#include "CrashHandler.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <dbghelp.h>
 #pragma comment(lib, "dbghelp.lib")
#endif

namespace crashlog
{
namespace
{
juce::File& target()
{
    static juce::File file;
    return file;
}

std::atomic<bool> reported { false };

void write(const juce::String& text)
{
    if (target() == juce::File())
        return;
    const auto now = juce::Time::getCurrentTime();
    target().appendText(now.formatted("%Y-%m-%d %H:%M:%S") + juce::String::formatted(".%03d", now.getMilliseconds())
                        + " [CRASH] " + text + "\n", false, false, "UTF-8");
}

#if JUCE_WINDOWS
juce::String hex(const juce::uint64 value)
{
    return "0x" + juce::String::toHexString(static_cast<juce::int64>(value)).paddedLeft('0', 8);
}

juce::String moduleForAddress(const void* address)
{
    HMODULE module = nullptr;
    if (! GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             static_cast<LPCWSTR>(address), &module) || module == nullptr)
        return hex(reinterpret_cast<juce::uint64>(address)) + " <unknown module>";
    wchar_t path[MAX_PATH] {};
    GetModuleFileNameW(module, path, MAX_PATH);
    const auto offset = static_cast<juce::uint64>(static_cast<const char*>(address) - reinterpret_cast<const char*>(module));
    return juce::String(path) + " +" + hex(offset);
}

const char* describe(const DWORD code) noexcept
{
    switch (code)
    {
        case EXCEPTION_ACCESS_VIOLATION:      return "access violation";
        case EXCEPTION_STACK_OVERFLOW:        return "stack overflow";
        case EXCEPTION_ILLEGAL_INSTRUCTION:   return "illegal instruction";
        case EXCEPTION_PRIV_INSTRUCTION:      return "privileged instruction";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "integer divide by zero";
        case EXCEPTION_IN_PAGE_ERROR:         return "in-page error";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "datatype misalignment";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
        case 0xC0000374:                      return "heap corruption";
        case 0xC0000409:                      return "stack buffer overrun / fast fail";
        case 0xE06D7363:                      return "unhandled C++ exception";
        default:                              return "unknown";
    }
}

bool isSevere(const DWORD code) noexcept
{
    switch (code)
    {
        case EXCEPTION_ACCESS_VIOLATION: case EXCEPTION_STACK_OVERFLOW: case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_PRIV_INSTRUCTION: case EXCEPTION_INT_DIVIDE_BY_ZERO: case EXCEPTION_IN_PAGE_ERROR:
        case 0xC0000374: case 0xC0000409:
            return true;
        default:
            return false;
    }
}

juce::String describeRecord(const EXCEPTION_RECORD& record)
{
    auto text = hex(record.ExceptionCode) + " (" + describe(record.ExceptionCode) + ") in "
              + moduleForAddress(record.ExceptionAddress) + ", thread " + juce::String(static_cast<int>(GetCurrentThreadId()));
    if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
    {
        const auto kind = record.ExceptionInformation[0];
        text << (kind == 0 ? ", reading " : kind == 1 ? ", writing " : ", executing ")
             << hex(static_cast<juce::uint64>(record.ExceptionInformation[1]));
    }
    return text;
}

// Walks the faulting thread's stack from the exception context, not the
// handler's own stack.
juce::String stackFrom(const CONTEXT& faultContext)
{
    auto context = faultContext;
    const auto process = GetCurrentProcess();
    const auto thread = GetCurrentThread();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    const auto haveSymbols = SymInitialize(process, nullptr, TRUE) != FALSE;

    STACKFRAME64 frame {};
   #if defined(_M_X64)
    constexpr DWORD machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip; frame.AddrFrame.Offset = context.Rbp; frame.AddrStack.Offset = context.Rsp;
   #elif defined(_M_ARM64)
    constexpr DWORD machine = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset = context.Pc; frame.AddrFrame.Offset = context.Fp; frame.AddrStack.Offset = context.Sp;
   #else
    constexpr DWORD machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip; frame.AddrFrame.Offset = context.Ebp; frame.AddrStack.Offset = context.Esp;
   #endif
    frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;

    juce::String text;
    for (int index = 0; index < 48; ++index)
    {
        if (! StackWalk64(machine, process, thread, &frame, &context, nullptr,
                          SymFunctionTableAccess64, SymGetModuleBase64, nullptr) || frame.AddrPC.Offset == 0)
            break;
        const auto address = frame.AddrPC.Offset;
        text << "  #" << index << " " << moduleForAddress(reinterpret_cast<const void*>(address));
        if (haveSymbols)
        {
            alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256] {};
            auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 255;
            DWORD64 displacement = 0;
            if (SymFromAddr(process, address, &displacement, symbol))
                text << "  " << juce::String(symbol->Name) << " +" << juce::String(static_cast<juce::int64>(displacement));
        }
        text << "\n";
    }
    if (haveSymbols)
        SymCleanup(process);
    return text;
}

void writeMinidump(EXCEPTION_POINTERS* info)
{
    const auto dump = target().getSiblingFile("Ton of Able crash " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S") + ".dmp");
    const auto file = CreateFileW(dump.getFullPathName().toWideCharPointer(), GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    MINIDUMP_EXCEPTION_INFORMATION exception { GetCurrentThreadId(), info, FALSE };
    const auto ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                                      static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory),
                                      info != nullptr ? &exception : nullptr, nullptr, nullptr);
    CloseHandle(file);
    write(ok ? "Minidump: " + dump.getFullPathName() : juce::String("Minidump could not be written."));
}

LONG WINAPI unhandledException(EXCEPTION_POINTERS* info)
{
    if (! reported.exchange(true))
    {
        write("Unhandled exception " + describeRecord(*info->ExceptionRecord));
        write("Stack of the crashing thread:\n" + stackFrom(*info->ContextRecord));
        writeMinidump(info);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

std::atomic<int> firstChanceReports { 0 };

LONG WINAPI firstChanceException(EXCEPTION_POINTERS* info)
{
    const auto& record = *info->ExceptionRecord;
    if (isSevere(record.ExceptionCode) && firstChanceReports.fetch_add(1) < 20)
        write("First-chance exception " + describeRecord(record)
              + " (fatal if nobody handles it; ASIO drivers sometimes catch their own)");
    return EXCEPTION_CONTINUE_SEARCH;
}

void pureCall()
{
    write("Pure virtual function call. Stack:\n" + juce::SystemStats::getStackBacktrace());
    std::abort();
}

void invalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
    write("Invalid parameter passed to a C runtime function. Stack:\n" + juce::SystemStats::getStackBacktrace());
}
#else
void signalCrash(void* signal)
{
    if (! reported.exchange(true))
        write("Fatal signal " + juce::String(static_cast<int>(reinterpret_cast<juce::pointer_sized_int>(signal)))
              + ". Stack:\n" + juce::SystemStats::getStackBacktrace());
}
#endif

void abortSignal(int)
{
    if (! reported.exchange(true))
        write("abort() called. Stack:\n" + juce::SystemStats::getStackBacktrace());
}

void terminateHandler()
{
    juce::String what = "no active exception";
    if (const auto current = std::current_exception())
    {
        try { std::rethrow_exception(current); }
        catch (const std::exception& error) { what = "exception: " + juce::String(error.what()); }
        catch (...) { what = "non-standard exception"; }
    }
    write("std::terminate called (" + what + "). Stack:\n" + juce::SystemStats::getStackBacktrace());
    reported = true;
    std::abort();
}
}

void install(const juce::File& logFile)
{
    target() = logFile;
   #if JUCE_WINDOWS
    SetUnhandledExceptionFilter(unhandledException);
    AddVectoredExceptionHandler(1, firstChanceException);
    _set_purecall_handler(pureCall);
    _set_invalid_parameter_handler(invalidParameter);
   #else
    juce::SystemStats::setApplicationCrashHandler(signalCrash);
   #endif
    std::signal(SIGABRT, abortSignal);
    std::set_terminate(terminateHandler);
}
}

#include "Logger.h"

#include <cstring>

Logger::Logger() : juce::Thread("NAM diagnostic logger") {}
Logger::~Logger() { stop(); }

juce::Result Logger::start(const juce::File& file)
{
    stop();
    if (! file.getParentDirectory().exists() && ! file.getParentDirectory().createDirectory())
        return juce::Result::fail("Log directory cannot be created.");
    logFile = file;
    accepting.store(true, std::memory_order_release);
    startThread();
    return juce::Result::ok();
}

void Logger::stop()
{
    accepting.store(false, std::memory_order_release);
    signalThreadShouldExit();
    notify();
    stopThread(2000);
}

void Logger::log(const Level level, juce::String message)
{
    if (! accepting.load(std::memory_order_acquire))
        return;
    {
        const std::scoped_lock lock(mutex);
        pending.push_back({ level, std::move(message), juce::Time::getCurrentTime() });
    }
    notify();
}

void Logger::logRealtime(const Level level, const char* message) noexcept
{
    if (! accepting.load(std::memory_order_relaxed) || message == nullptr)
        return;
    const auto write = realtimeWrite.load(std::memory_order_relaxed);
    const auto next = (write + 1) % realtimeCapacity;
    if (next == realtimeRead.load(std::memory_order_acquire))
    {
        droppedRealtimeMessages.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    auto& entry = realtimeQueue[write];
    entry.level = level;
    std::strncpy(entry.text.data(), message, entry.text.size() - 1);
    entry.text.back() = '\0';
    realtimeWrite.store(next, std::memory_order_release);
    notify();
}

std::vector<Logger::Entry> Logger::recentEntries() const
{
    const std::scoped_lock lock(mutex);
    return recent;
}

void Logger::run()
{
    for (;;)
    {
        if (! threadShouldExit())
            wait(200);
        std::vector<PendingEntry> work;
        {
            const std::scoped_lock lock(mutex);
            work.swap(pending);
        }
        auto read = realtimeRead.load(std::memory_order_relaxed);
        const auto write = realtimeWrite.load(std::memory_order_acquire);
        while (read != write)
        {
            const auto& item = realtimeQueue[read];
            work.push_back({ item.level, juce::String::fromUTF8(item.text.data()), juce::Time::getCurrentTime() });
            read = (read + 1) % realtimeCapacity;
        }
        realtimeRead.store(read, std::memory_order_release);
        for (const auto& item : work)
            append(item.level, item.text, item.timestamp);
        if (threadShouldExit())
            break;
    }
}

void Logger::append(const Level level, const juce::String& message, const juce::Time& timestamp)
{
    const auto line = timestamp.formatted("%Y-%m-%d %H:%M:%S") + " [" + levelName(level) + "] " + message + "\n";
    if (logFile != juce::File())
        logFile.appendText(line, false, false, "UTF-8");
    const std::scoped_lock lock(mutex);
    recent.push_back({ level, message, timestamp });
    constexpr size_t maximumRecentEntries = 500;
    if (recent.size() > maximumRecentEntries)
        recent.erase(recent.begin(), recent.begin() + static_cast<std::ptrdiff_t>(recent.size() - maximumRecentEntries));
}

const char* Logger::levelName(const Level level) noexcept
{
    switch (level) { case Level::debug: return "DEBUG"; case Level::info: return "INFO"; case Level::warning: return "WARN"; case Level::error: return "ERROR"; }
    return "INFO";
}

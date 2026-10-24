#pragma once

#include <juce_core/juce_core.h>

// Last-resort crash reporting. Fatal exceptions, std::terminate and abort()
// are written to the log file together with the faulting module (e.g. an ASIO
// driver DLL) and a stack; on Windows a minidump is saved next to the log.
// Severe first-chance exceptions are noted too, because drivers and plugins
// sometimes swallow them or replace the unhandled-exception filter.
namespace crashlog
{
void install(const juce::File& logFile);
}

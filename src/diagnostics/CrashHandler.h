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

// Breadcrumbs written into the crash report: what the audio callback was doing
// and which plugin it was inside. Lock- and allocation-free; the strings must
// stay valid (literals or storage owned by the caller).
void setAudioStage(const char* stage) noexcept;
void setCurrentPlugin(const char* name) noexcept;
}

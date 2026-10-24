#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "Logger.h"

// An AudioDeviceManager whose device types log every scan and device creation
// before handing over to the driver. Opening an ASIO device loads the driver
// DLL inside createDevice(), so the last line before a driver crash names the
// driver that was being loaded.
class LoggingAudioDeviceManager final : public juce::AudioDeviceManager
{
public:
    explicit LoggingAudioDeviceManager(Logger& logger) : log(logger) {}

    void createAudioDeviceTypes(juce::OwnedArray<juce::AudioIODeviceType>& types) override;

    /** Device types and their devices, one line each. */
    juce::String describeAvailableDevices();
    /** Current type, device, rate, buffer, active channels and the last error. */
    juce::String describeCurrentSetup();

private:
    Logger& log;
};

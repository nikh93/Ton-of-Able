#include "DeviceLogging.h"

namespace
{
// Forwards everything to the wrapped type. Devices are the inner type's own
// objects, so its getIndexOfDevice() still recognises them.
class LoggingDeviceType final : public juce::AudioIODeviceType, private juce::AudioIODeviceType::Listener
{
public:
    LoggingDeviceType(std::unique_ptr<juce::AudioIODeviceType> wrapped, Logger& logger)
        : juce::AudioIODeviceType(wrapped->getTypeName()), inner(std::move(wrapped)), log(logger)
    {
        inner->addListener(this);
    }

    ~LoggingDeviceType() override { inner->removeListener(this); }

    void scanForDevices() override
    {
        log.log(Logger::Level::info, "[Device] " + getTypeName() + ": scanning for devices ...");
        inner->scanForDevices();
        log.log(Logger::Level::info, "[Device] " + getTypeName() + ": outputs [" + inner->getDeviceNames(false).joinIntoString(", ")
                                     + "], inputs [" + inner->getDeviceNames(true).joinIntoString(", ") + "]");
    }

    juce::StringArray getDeviceNames(bool wantInputNames) const override { return inner->getDeviceNames(wantInputNames); }
    int getDefaultDeviceIndex(bool forInput) const override { return inner->getDefaultDeviceIndex(forInput); }
    int getIndexOfDevice(juce::AudioIODevice* device, bool asInput) const override { return inner->getIndexOfDevice(device, asInput); }
    bool hasSeparateInputsAndOutputs() const override { return inner->hasSeparateInputsAndOutputs(); }

    juce::AudioIODevice* createDevice(const juce::String& outputDeviceName, const juce::String& inputDeviceName) override
    {
        log.log(Logger::Level::info, "[Device] " + getTypeName() + ": creating device out=\"" + outputDeviceName
                                     + "\" in=\"" + inputDeviceName + "\" (loads the driver) ...");
        auto* device = inner->createDevice(outputDeviceName, inputDeviceName);
        if (device == nullptr)
        {
            log.log(Logger::Level::warning, "[Device] " + getTypeName() + ": device could not be created");
            return nullptr;
        }
        log.log(Logger::Level::info, "[Device] " + getTypeName() + ": created \"" + device->getName()
                                     + "\", outputs [" + device->getOutputChannelNames().joinIntoString(", ")
                                     + "], inputs [" + device->getInputChannelNames().joinIntoString(", ")
                                     + "], rates [" + joinNumbers(device->getAvailableSampleRates())
                                     + "], buffers [" + joinNumbers(device->getAvailableBufferSizes())
                                     + "], default buffer " + juce::String(device->getDefaultBufferSize()));
        return device;
    }

private:
    template <typename Array>
    static juce::String joinNumbers(const Array& values)
    {
        juce::StringArray text;
        for (const auto value : values)
            text.add(juce::String(value));
        return text.joinIntoString(" ");
    }

    void audioDeviceListChanged() override
    {
        log.log(Logger::Level::info, "[Device] " + getTypeName() + ": device list changed");
        callDeviceChangeListeners();
    }

    std::unique_ptr<juce::AudioIODeviceType> inner;
    Logger& log;
};
}

void LoggingAudioDeviceManager::createAudioDeviceTypes(juce::OwnedArray<juce::AudioIODeviceType>& types)
{
    juce::OwnedArray<juce::AudioIODeviceType> real;
    juce::AudioDeviceManager::createAudioDeviceTypes(real);
    while (! real.isEmpty())
    {
        std::unique_ptr<juce::AudioIODeviceType> type(real.removeAndReturn(0));
        log.log(Logger::Level::info, "[Device] available type: " + type->getTypeName());
        types.add(new LoggingDeviceType(std::move(type), log));
    }
}

juce::String LoggingAudioDeviceManager::describeAvailableDevices()
{
    juce::StringArray lines;
    for (auto* type : getAvailableDeviceTypes())
        lines.add(type->getTypeName() + ": " + type->getDeviceNames(false).joinIntoString(", "));
    return lines.joinIntoString("; ");
}

juce::String LoggingAudioDeviceManager::describeCurrentSetup()
{
    auto* device = getCurrentAudioDevice();
    if (device == nullptr)
        return "type \"" + getCurrentAudioDeviceType() + "\", no device open";
    const auto setup = getAudioDeviceSetup();
    return "type \"" + device->getTypeName() + "\", device \"" + device->getName()
         + "\", open " + juce::String(device->isOpen() ? "yes" : "no")
         + ", playing " + juce::String(device->isPlaying() ? "yes" : "no")
         + ", " + juce::String(device->getCurrentSampleRate()) + " Hz, buffer " + juce::String(device->getCurrentBufferSizeSamples())
         + ", active in " + device->getActiveInputChannels().toString(2) + " / out " + device->getActiveOutputChannels().toString(2)
         + ", requested in " + setup.inputChannels.toString(2) + " / out " + setup.outputChannels.toString(2)
         + ", latency in " + juce::String(device->getInputLatencyInSamples()) + " / out " + juce::String(device->getOutputLatencyInSamples())
         + (device->getLastError().isNotEmpty() ? ", last error: " + device->getLastError() : juce::String());
}

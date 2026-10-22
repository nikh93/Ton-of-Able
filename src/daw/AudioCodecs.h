#pragma once

#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>

namespace daw
{
/** Import/export formats. JUCE handles WAV, AIFF, FLAC, Ogg Vorbis and MP3
    decoding (plus Windows Media formats on Windows). Everything else - AAC,
    ALAC, Opus, AC3, and the audio of video containers - is converted with an
    external FFmpeg executable when one is available. */
namespace codecs
{
enum class ExportFormat
{
    wav16, wav24, wav32Float, aiff24, flac24, oggVorbis, mp3, aacM4a, opus
};

struct ExportFormatInfo
{
    ExportFormat format;
    const char* name;
    const char* extension;
    bool needsFfmpeg;
};

[[nodiscard]] const std::vector<ExportFormatInfo>& exportFormats();
[[nodiscard]] const ExportFormatInfo& infoFor(ExportFormat);

/** FFmpeg location: explicit setting, next to the executable, then PATH. */
void setFfmpegOverride(const juce::File& executable);
[[nodiscard]] juce::File findFfmpeg();

/** True if JUCE can decode the file directly. */
[[nodiscard]] bool canDecodeNatively(const juce::File& file);
/** All importable extensions (native + FFmpeg audio/video), for file choosers. */
[[nodiscard]] juce::String importWildcard();
[[nodiscard]] bool isVideoFile(const juce::File& file);

/** Extracts/decodes the first audio stream of any FFmpeg-readable file
    (including video) into a 32-bit float WAV. Blocking; worker threads only. */
juce::Result extractWithFfmpeg(const juce::File& input, const juce::File& outputWav);

/** Streaming writer for exports. For FFmpeg formats it writes a temporary
    float WAV and transcodes it in finish(). Worker threads only. */
class ExportWriter final
{
public:
    ExportWriter(const juce::File& target, ExportFormat format, double sampleRate, int numChannels);
    ~ExportWriter();
    [[nodiscard]] juce::Result openResult() const { return opened; }
    bool write(const float* const* channels, int numSamples);
    juce::Result finish();

private:
    juce::File target, temporary;
    ExportFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::Result opened = juce::Result::ok();
};
} // namespace codecs
} // namespace daw

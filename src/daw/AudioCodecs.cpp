#include "AudioCodecs.h"

#include "DawText.h"

namespace daw::codecs
{
namespace
{
juce::File& ffmpegOverride()
{
    static juce::File file;
    return file;
}

const char* const ffmpegAudioExtensions[] { "mp3", "m4a", "aac", "mp4a", "alac", "caf", "opus", "oga", "ogg", "wma", "ac3", "eac3",
                                            "dts", "ape", "wv", "tta", "amr", "au", "snd", "mka", "w64", "rf64", "flac", "aif", "aiff", "wav" };
const char* const videoExtensions[] { "mp4", "m4v", "mov", "mkv", "avi", "webm", "wmv", "flv", "mpg", "mpeg", "ts", "mts", "m2ts", "3gp", "vob", "ogv" };

juce::String runFfmpeg(const juce::StringArray& arguments, juce::Result& result)
{
    const auto executable = findFfmpeg();
    if (! executable.existsAsFile())
    {
        result = juce::Result::fail(text("Für dieses Format wird FFmpeg benötigt. Lege ffmpeg.exe neben \"Ton of Able.exe\", "
                                         "installiere es im PATH oder wähle es im Projektmenü unter \"FFmpeg auswählen\"."));
        return {};
    }
    juce::StringArray command { executable.getFullPathName(), "-hide_banner", "-nostdin", "-loglevel", "error", "-y" };
    command.addArray(arguments);
    juce::ChildProcess process;
    if (! process.start(command, juce::ChildProcess::wantStdErr | juce::ChildProcess::wantStdOut))
    {
        result = juce::Result::fail("FFmpeg konnte nicht gestartet werden: " + executable.getFullPathName());
        return {};
    }
    const auto output = process.readAllProcessOutput();
    if (! process.waitForProcessToFinish(10 * 60 * 1000))
    {
        process.kill();
        result = juce::Result::fail(text("FFmpeg hat nicht rechtzeitig geantwortet."));
        return output;
    }
    result = process.getExitCode() == 0 ? juce::Result::ok()
                                        : juce::Result::fail("FFmpeg-Fehler: " + output.trim().fromLastOccurrenceOf("\n", false, false));
    return output;
}

std::unique_ptr<juce::AudioFormat> nativeFormatFor(ExportFormat format, int& bits, int& quality, bool& floating)
{
    bits = 24;
    quality = 0;
    floating = false;
    switch (format)
    {
        case ExportFormat::wav16: bits = 16; return std::make_unique<juce::WavAudioFormat>();
        case ExportFormat::wav24: return std::make_unique<juce::WavAudioFormat>();
        case ExportFormat::wav32Float: bits = 32; floating = true; return std::make_unique<juce::WavAudioFormat>();
        case ExportFormat::aiff24: return std::make_unique<juce::AiffAudioFormat>();
        case ExportFormat::flac24: return std::make_unique<juce::FlacAudioFormat>();
       #if JUCE_USE_OGGVORBIS
        case ExportFormat::oggVorbis: bits = 16; quality = 8; return std::make_unique<juce::OggVorbisAudioFormat>(); // 256 kbps
       #endif
        default: break;
    }
    // FFmpeg formats are written as float WAV first.
    bits = 32;
    floating = true;
    return std::make_unique<juce::WavAudioFormat>();
}
} // namespace

const std::vector<ExportFormatInfo>& exportFormats()
{
    static const std::vector<ExportFormatInfo> formats {
        { ExportFormat::wav24, "WAV 24 Bit", "wav", false },
        { ExportFormat::wav16, "WAV 16 Bit", "wav", false },
        { ExportFormat::wav32Float, "WAV 32 Bit Float", "wav", false },
        { ExportFormat::aiff24, "AIFF 24 Bit", "aif", false },
        { ExportFormat::flac24, "FLAC 24 Bit", "flac", false },
        { ExportFormat::oggVorbis, "Ogg Vorbis 256 kbps", "ogg", false },
        { ExportFormat::mp3, "MP3 320 kbps (FFmpeg)", "mp3", true },
        { ExportFormat::aacM4a, "AAC/M4A 256 kbps (FFmpeg)", "m4a", true },
        { ExportFormat::opus, "Opus 192 kbps (FFmpeg)", "opus", true },
    };
    return formats;
}

const ExportFormatInfo& infoFor(ExportFormat format)
{
    for (const auto& info : exportFormats())
        if (info.format == format)
            return info;
    return exportFormats().front();
}

void setFfmpegOverride(const juce::File& executable) { ffmpegOverride() = executable; }

juce::File findFfmpeg()
{
    if (ffmpegOverride().existsAsFile())
        return ffmpegOverride();
   #if JUCE_WINDOWS
    const juce::String name = "ffmpeg.exe";
   #else
    const juce::String name = "ffmpeg";
   #endif
    const auto appDirectory = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    for (const auto& candidate : { appDirectory.getChildFile(name), appDirectory.getChildFile("ffmpeg").getChildFile(name),
                                   appDirectory.getChildFile("ffmpeg").getChildFile("bin").getChildFile(name) })
        if (candidate.existsAsFile())
            return candidate;
    const auto path = juce::SystemStats::getEnvironmentVariable("PATH", {});
    for (const auto& directory : juce::StringArray::fromTokens(path, juce::String(juce::File::getSeparatorString()) == "\\" ? ";" : ":", "\""))
        if (directory.isNotEmpty())
            if (const auto candidate = juce::File(directory).getChildFile(name); candidate.existsAsFile())
                return candidate;
    return {};
}

bool canDecodeNatively(const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    return std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file)) != nullptr;
}

bool isVideoFile(const juce::File& file)
{
    for (const auto* extension : videoExtensions)
        if (file.hasFileExtension(extension))
            return true;
    return false;
}

juce::String importWildcard()
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    juce::StringArray patterns;
    patterns.addTokens(formats.getWildcardForAllFormats(), ";", {});
    for (const auto* extension : ffmpegAudioExtensions)
        patterns.addIfNotAlreadyThere(juce::String("*.") + extension);
    for (const auto* extension : videoExtensions)
        patterns.addIfNotAlreadyThere(juce::String("*.") + extension);
    return patterns.joinIntoString(";");
}

juce::Result extractWithFfmpeg(const juce::File& input, const juce::File& outputWav)
{
    outputWav.getParentDirectory().createDirectory();
    auto result = juce::Result::ok();
    // -vn drops video; the first audio stream is decoded to float WAV.
    runFfmpeg({ "-i", input.getFullPathName(), "-vn", "-map", "0:a:0", "-c:a", "pcm_f32le", outputWav.getFullPathName() }, result);
    if (result.wasOk() && ! outputWav.existsAsFile())
        result = juce::Result::fail(text("Die Datei enthält keine Audiospur: ") + input.getFileName());
    return result;
}

ExportWriter::ExportWriter(const juce::File& file, ExportFormat exportFormat, double sampleRate, int numChannels)
    : target(file), format(exportFormat)
{
    const auto& info = infoFor(format);
    const auto destination = info.needsFfmpeg ? (temporary = file.getSiblingFile(file.getFileNameWithoutExtension() + ".export-tmp.wav")) : file;
    if (info.needsFfmpeg && ! findFfmpeg().existsAsFile())
    {
        opened = juce::Result::fail(text("Für ") + info.name + text(" wird FFmpeg benötigt (ffmpeg.exe neben die Anwendung legen oder im Projektmenü auswählen)."));
        return;
    }
    destination.getParentDirectory().createDirectory();
    destination.deleteFile();
    std::unique_ptr<juce::OutputStream> stream(destination.createOutputStream());
    if (stream == nullptr)
    {
        opened = juce::Result::fail("Exportdatei kann nicht geschrieben werden: " + destination.getFullPathName());
        return;
    }
    int bits = 24, quality = 0;
    bool floating = false;
    auto audioFormat = nativeFormatFor(format, bits, quality, floating);
    const auto options = juce::AudioFormatWriterOptions {}
        .withSampleRate(sampleRate)
        .withNumChannels(numChannels)
        .withBitsPerSample(bits)
        .withQualityOptionIndex(quality)
        .withSampleFormat(floating ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                   : juce::AudioFormatWriterOptions::SampleFormat::integral);
    writer = audioFormat->createWriterFor(stream, options);
    if (writer == nullptr)
        opened = juce::Result::fail(text("Exportformat wird mit diesen Einstellungen nicht unterstützt: ") + info.name);
}

ExportWriter::~ExportWriter()
{
    writer.reset();
    if (temporary != juce::File())
        temporary.deleteFile();
}

bool ExportWriter::write(const float* const* channels, int numSamples)
{
    return writer != nullptr && writer->writeFromFloatArrays(channels, writer->getNumChannels(), numSamples);
}

juce::Result ExportWriter::finish()
{
    if (writer == nullptr)
        return opened.failed() ? opened : juce::Result::fail("Export fehlgeschlagen.");
    writer.reset(); // flushes and finalises the header
    if (temporary == juce::File())
        return juce::Result::ok();

    juce::StringArray arguments { "-i", temporary.getFullPathName() };
    switch (format)
    {
        case ExportFormat::mp3: arguments.addArray({ "-c:a", "libmp3lame", "-b:a", "320k" }); break;
        case ExportFormat::aacM4a: arguments.addArray({ "-c:a", "aac", "-b:a", "256k" }); break;
        case ExportFormat::opus: arguments.addArray({ "-c:a", "libopus", "-b:a", "192k", "-ar", "48000" }); break;
        default: break;
    }
    arguments.add(target.getFullPathName());
    auto result = juce::Result::ok();
    runFfmpeg(arguments, result);
    temporary.deleteFile();
    return result;
}
} // namespace daw::codecs

#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

#include <juce_core/juce_core.h>

// Diagnostic logger. log() writes synchronously, so the last lines before a
// crash reach the file. logRealtime() is SPSC and bounded (audio callback
// producer, logger-thread consumer); it neither allocates nor locks.
class Logger final : private juce::Thread
{
public:
    enum class Level : unsigned char { debug, info, warning, error };

    struct Entry
    {
        Level level = Level::info;
        juce::String text;
        juce::Time timestamp;
    };

    Logger();
    ~Logger() override;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    juce::Result start(const juce::File& file);
    void stop();
    void log(Level level, juce::String message); // non-realtime callers, written immediately
    void logRealtime(Level level, const char* message) noexcept;
    std::vector<Entry> recentEntries() const;
    juce::File file() const { return logFile; }

private:
    static constexpr size_t realtimeCapacity = 256;
    static constexpr size_t realtimeMessageLength = 192;
    struct RealtimeEntry { Level level; std::array<char, realtimeMessageLength> text {}; };
    struct PendingEntry { Level level; juce::String text; juce::Time timestamp; };

    void run() override;
    void append(Level level, const juce::String& message, const juce::Time& timestamp);
    static const char* levelName(Level level) noexcept;

    std::atomic<bool> accepting { false };
    std::atomic<unsigned int> realtimeRead { 0 };
    std::atomic<unsigned int> realtimeWrite { 0 };
    std::array<RealtimeEntry, realtimeCapacity> realtimeQueue {};
    std::atomic<unsigned long long> droppedRealtimeMessages { 0 };

    std::mutex fileMutex;
    mutable std::mutex mutex;
    std::vector<Entry> recent;
    juce::File logFile;
};

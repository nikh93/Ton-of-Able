# Diagnostics

`PerformanceMonitor` exposes lock-free per-block timing aggregates for a UI timer. `Logger::logRealtime` is a bounded single-producer/single-consumer queue for audio-callback messages; ordinary callers use `log`, whose work is written by the logger thread.

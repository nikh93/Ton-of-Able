# Architekturentscheidungen

Stand: 15.09.2026 – Release-Build funktionsfähig

## Schichten und Audiopfad

`AudioEngine` besitzt den einzigen JUCE-`AudioIODeviceCallback` und routet einen vorallokierten Mono-Arbeitsbuffer durch eine feste, später umsortierbare Prozessorkette: Input Gain → NAM → IR → EQ → Auto-Wah → Chorus → Delay → Reverb → Output Gain. Bei Stereo-Hardware wird das bearbeitete Monosignal am Ausgang dupliziert.

GUI-Komponenten ändern keine DSP-Objekte direkt. Sie verwenden die thread-sicheren Setter von `AudioEngine`; Effektparameter werden in den Prozessoren geglättet bzw. über kurze Bypass-Ramps geführt. Nicht-realtime-sichere Bibliotheks-, Preset- und Dateivorgänge bleiben auf dem Message- oder Worker-Thread.

## Echtzeitgrenzen

Im Audiothread zulässig sind ausschließlich vorallokierte Buffer, bereits vorbereitete Prozessoren und lockfreie/atomare Parameter- und Meterübergaben. Verboten sind Allokationen, Dateizugriffe, Modell- oder IR-Ladevorgänge, JSON, GUI-Zugriffe, Mutexes und Festplattenlogging.

`NamProcessor` lädt `.nam`-Dateien außerhalb des Audiothreads, ruft `nam::get_dsp` auf und veröffentlicht vorbereitete Modelle über lockfreie SPSC-Queues. Der Audiothread übernimmt nur einen zuvor vorbereiteten immutable Modell-Slot an einer Blockgrenze; alte Modelle werden erst auf dem Loader-Thread freigegeben. IRs werden über JUCEs asynchronen Convolution-Loader geladen. A/B speichert zwei vollständige `SoundState`-Objekte.

## NAM- und Geräte-API-Basis

Die eingebundene NeuralAmpModelerCore-Revision ist `2563c0f`. Ihre relevante API ist `nam::get_dsp`, `nam::DSP::Reset`, `nam::DSP::process` und `nam::DSP::SetPrewarmOnReset`. `Reset` kann vorwärmen und ist daher nie im Audiocallback zulässig.

JUCE wird mit `JUCE_ASIO=1` und `JUCE_WASAPI=1` gebaut. `AudioDeviceManager` wird Geräte-Setup persistent verwalten; ASIO wird bevorzugt und WASAPI (shared-low-latency bzw. exclusive) bleibt als Fallback auswählbar.

## State und Persistenz

`SoundState` ist ein serialisierbarer, vollständiger Klangzustand. Presets und A/B verwenden denselben Typ, enthalten jedoch lediglich Dateireferenzen und Metadaten statt Modelldaten oder IR-Audiodaten. Fehlende Pfade bleiben valide Library-Einträge mit Status `missing` und können neu verknüpft werden.

## Verbleibende Ausbaustufen

Die Kernfunktionen – Audio-I/O, NAM/IR, EQ, vier Effekte, Bypass-Ramping, WAV-Player, Threaded Recording, Libraries, vollständige Presets/A-B, Persistenz, Metering, Clipping-Anzeige und Performance-Monitoring – sind implementiert und werden durch Debug- und Release-Tests geprüft. Die GUI verwendet dafür separate Library-, Preset-, Effekt- und Diagnosekomponenten; weitere Änderungen sind optionale UX-Politur und verändern die Echtzeitgrenzen nicht.

# Signalkette, VST3-Plugins und Routing

Die Signalkette oben im Rig (und im Spur-Effekt-Fenster jeder Arrangement-Spur) ist frei bearbeitbar.

## Aufbau

- **Obere Reihe:** der Signalweg von der Gitarre (**IN**) zum Ausgang (**OUT**). Alle eingebauten Effekte stehen hier genau einmal: Pitch, Kompressor, Wah, Drive, NAM-Pedal, Amp, Box/IR, Amp-EQ, Graphic EQ, Auto-Wah, Chorus, Delay, Reverb.
- **Untere Reihe „Neben der Kette“:** Blöcke, die nicht im Signalweg liegen, z. B. ein Synth. **KEYS** sind die MIDI-Keyboards und virtuellen MIDI-Ports (nur im Live-Rig).
- **Kurven** zeigen zusätzliche Verbindungen: orange = Audio (mit Pegel), blau gestrichelt = MIDI.

## Bedienung

- **Punkt** im Block: an/aus. **Klick** auf den Block: Einstellungen bzw. das Plugin-Fenster.
- **Ziehen:** Block an eine andere Stelle der Kette schieben. Plugins lassen sich auch in die untere Reihe ziehen.
- **+** am Ende einer Reihe: VST3-Effekt (auch Audio → MIDI wie TONIQ, MIDI Guitar, Transkr, MiGiC), VST3-Instrument (z. B. Vital) oder **MIDI transponieren** einfügen. Plugins werden mit „VST3-Plugins suchen“ in den Standardordnern gefunden (jedes in einem eigenen Prozess, 30 s Zeitlimit).
- **Rechtsklick** auf einen Block: *Audio senden an*, *MIDI senden an*, *Audio empfangen von* (auch „Gitarre (trocken)“), *MIDI empfangen von* (auch „MIDI-Keyboards“), *Verbindungen und Pegel …*, in die Kette / neben die Kette stellen, entfernen. Ein Haken im Menü bedeutet: Verbindung besteht – erneut wählen trennt sie.
- **Rechtsklick auf OUT:** *Pegel der Kette in den Ausgang* – „aus“ lässt nur die Verbindungen hören (z. B. nur den Synth).
- Mehrere Verbindungen in denselben Eingang werden addiert. Eingebaute Effekte arbeiten mono (Stereo davor wird zusammengemischt), Plugins und der Ausgang stereo.

## Beispiele

- **Gitarre → MIDI → Synth:** TONIQ neben der Kette einfügen (es hört automatisch die trockene Gitarre), Vital einfügen (geht automatisch in den Ausgang und hört die Keyboards), dann Rechtsklick auf TONIQ → *MIDI senden an* → Vital. Für eine Oktave tiefer **MIDI transponieren** dazwischen setzen. Nur Synth: Rechtsklick auf OUT → Pegel der Kette „aus“.
- **Parallel-Hall:** Rechtsklick auf Box/IR → *Audio senden an* → ein Hall-Plugin neben der Kette, dieses → *Audio senden an* → Ausgang, Pegel nach Geschmack.
- **Kompressor vor oder nach dem Amp:** ziehen – oder in der Effektkarte „VOR AMP“.

## Speichern

Reihenfolge, Plugins mit ihrem Zustand und alle Verbindungen gehören zum Sound: Sie werden mit Presets, A/B, Rigs („Zusatz-FX beim Laden AUS“ schaltet die Plugins aus) und beim Beenden gespeichert. Spur-Ketten werden mit dem Projekt gespeichert. Ältere Presets mit den früheren Slots „VST3 vor/nach dem Amp“ werden beim Laden in Blöcke an denselben Stellen umgewandelt.

Fehlende Plugins erscheinen gestrichelt als „nicht geladen“ und werden in der Statuszeile gemeldet; der Signalweg läuft an ihnen vorbei.

## Technisch

Der Signalweg ist ein `juce::AudioProcessorGraph`. Jedes Plugin läuft in einem `Vst3Slot` (Absturz- und NaN-Schutz, latenzkorrigiertes Bypass, Effekte, Instrumente und MIDI-Erzeuger). Geladene Plugins laufen im Audioprozess: Ein fehlerhaftes fremdes Plugin kann die Anwendung weiterhin abstürzen lassen. Umverdrahten lädt Plugins nicht neu; nur ein geänderter Plugin-Zustand (z. B. ein anderes Preset) lädt das Plugin.

Prüfung mit echten VST3-Plugins: `"Ton of Able" --chain-routing-test <tracker.vst3> <instrument.vst3> <report.xml>` (Gitarre → Audio-to-MIDI → Transponieren → Instrument → Ausgang, Umverdrahten ohne Neuladen, Ketten-Pegel).

## Frühere Tests der mitgelieferten Plugins

Teststand: John Mayer bestand alle 18 Kombinationen aus 44,1/48/96 kHz, 32/128/512 Samples und Mono/Stereo, einschließlich hörpfadwirksamer Signalverarbeitung und latenzkorrigiertem Bypass. Die zusätzlichen King-Of-Tone-Diagnosen zeigten Stille bei 96 kHz, auch beim direkten Plugin-Aufruf; nach einem solchen Wechsel kann auch 48 kHz stumm bleiben. Dann Plugin entfernen und neu laden. Für King Of Tone bei 48 kHz bleiben. `docs/byod-final-host-checks.xml` hält alle 91 bestandenen Prüfungen und die vier fehlgeschlagenen King-Of-Tone-Diagnosen ausdrücklich fest.

Die zuerst bereitgestellte FutureVerb Demo 1.0.2 wurde nicht als Ersatz installiert. Stattdessen wurde der anschließend vom Nutzer gelieferte, gültig von Valhalla DSP signierte Supermassive-5.0.0-Installer verwendet.

Der Scan läuft mit 30 Sekunden Zeitlimit pro Plugin in einem separaten Prozess. Geladene Plugins laufen im Audioprozess: Ein fehlerhaftes fremdes Plugin kann weiterhin die Anwendung abstürzen lassen. Pluginwechsel können kurz den trockenen Signalweg durchlassen; sie sind keine nahtlosen Live-Presetwechsel.

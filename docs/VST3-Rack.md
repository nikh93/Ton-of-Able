# VST3-Rack bedienen

Oben im Programm **VST3** anklicken. **VST3 importieren** akzeptiert Windows-x64 `.vst3`-Dateien und Bundles sowie einen Ordner zum Scannen. Import bedeutet Registrierung am vorhandenen Ort, kein Kopieren; Plugin-Dateien danach nicht verschieben. Standardordner: `D:\Amp Modeler\plugins` und `C:\Program Files\Common Files\VST3`.

- **VOR AMP / DRIVE:** Verzerrer wie BYOD. Reihenfolge: interner Drive, PRE-VST3, NAM, Cabinet/IR.
- **NACH AMP / STEREO:** Hall/Delay wie Supermassive, hinter den internen Effekten. Stereo bleibt bis Ausgabe und Aufnahme erhalten.
- Plugin im oberen Dropdown, Factory-Preset im darunterliegenden Dropdown auswählen. Nicht jedes Plugin veröffentlicht seine Presets über die Host-Schnittstelle; dann **Plugin / Presets oeffnen** verwenden.
- **ON** aktiviert den Slot. Beim ersten automatischen Laden bleibt er zunächst ausgeschaltet; BYOD wird auf John Mayer vorbereitet. BYODs Default-Preset kann eine leere Effektkette sein.
- Plugin-Zustände werden mit Sound-Presets, A/B und beim Beenden gespeichert. Fehlende Plugins erzeugen eine Meldung und setzen den betroffenen Slot auf Bypass.

Zum Vergleich zunächst internen Drive ausschalten. Für Dumble-Leads zuerst nur den NAM mit Cabinet hören, anschließend BYOD bei Bedarf sparsam dazuschalten. Die fünf persönlichen Captures stehen unter **Dumble-style / ODS #102 Clone / Lead**: Smooth, Bluesy, Open Robben, Open Larry und Carlosanta.

BYOD 1.3.0 und Valhalla Supermassive 5.0.0 sind unter `D:\Amp Modeler\plugins` installiert. Supermassive wird bei der ersten Erkennung im freien hinteren Slot vorbereitet, zunächst ausgeschaltet. Es veröffentlicht keine Factory-Programme über VST3: Seine Presets im Originalfenster über **Plugin / Presets oeffnen** auswählen. Beide Slots mit **ON** aktivieren.

Supermassives 204 Factory-Presets liegen ebenfalls auf D: und sind am erwarteten Windows-Pfad verknüpft. Der Hosttest bestand 71 Prüfungen ohne Fehler, einschließlich eines vier Sekunden langen Stereo-Impulstests mit messbarer Hallfahne. 19 nicht passende Host-Programmprüfungen wurden übersprungen, weil Supermassive keine Host-Programme veröffentlicht.

Teststand: John Mayer bestand alle 18 Kombinationen aus 44,1/48/96 kHz, 32/128/512 Samples und Mono/Stereo, einschließlich hörpfadwirksamer Signalverarbeitung und latenzkorrigiertem Bypass. Die zusätzlichen King-Of-Tone-Diagnosen zeigten Stille bei 96 kHz, auch beim direkten Plugin-Aufruf; nach einem solchen Wechsel kann auch 48 kHz stumm bleiben. Dann Plugin entfernen und neu laden. Für King Of Tone bei 48 kHz bleiben. `docs/byod-final-host-checks.xml` hält alle 91 bestandenen Prüfungen und die vier fehlgeschlagenen King-Of-Tone-Diagnosen ausdrücklich fest.

Die zuerst bereitgestellte FutureVerb Demo 1.0.2 wurde nicht als Ersatz installiert. Stattdessen wurde der anschließend vom Nutzer gelieferte, gültig von Valhalla DSP signierte Supermassive-5.0.0-Installer verwendet.

Der Scan läuft mit 30 Sekunden Zeitlimit pro Plugin in einem separaten Prozess. Geladene Plugins laufen im Audioprozess: Ein fehlerhaftes fremdes Plugin kann weiterhin die Anwendung abstürzen lassen. Pluginwechsel können kurz den trockenen Signalweg durchlassen; sie sind keine nahtlosen Live-Presetwechsel.

Plugin-Verwaltung und Korrekturen an Spur- und Send-Effekten für Windows 10/11 (64 Bit).

**Installation:** `Ton-of-Able-v0.2.2-setup.exe` ausführen – eine vorhandene Installation wird aktualisiert. Der Installer ist nicht signiert: Bei der SmartScreen-Warnung „Weitere Informationen“ → „Trotzdem ausführen“.

**Ohne Installation:** ZIP entpacken und `Ton of Able.exe` starten. Die exe, der Ordner `resources`, `ffmpeg.exe` und die Plugins müssen im selben Ordner bleiben.

**Neu**
- **Plugin-Verwaltung:** „+“ in der Signalkette → „Plugins verwalten / importieren …“. Zeigt alle VST3-Plugins mit Hersteller, Art und Datei. Einzelne `.vst3`-Dateien von beliebigen Orten importieren, eigene Suchordner hinzufügen, alles neu suchen, Plugins im Menü ausblenden. Plugins, die sich nicht laden lassen, stehen mit Grund in der Liste und lassen sich erneut prüfen oder entfernen.

**Behoben**
- Drum-Spuren: Die Spur-Effekte wirken jetzt auch auf das Drum-Instrument (bisher lief es an der Kette vorbei, auch beim Export).
- Spur- und Send-Effekte arbeiten in Stereo: Panorama und Stereobild der Spuren bleiben in Return-Spuren (Hall, Delay) und Spurketten erhalten, statt auf Mono zusammengemischt zu werden.

**Hinweise**
- Einstellungen und Plugin-Liste aus v0.2.1 werden übernommen.
- Fremde VST3-Plugins laufen im Programm selbst: Ein fehlerhaftes Plugin kann die Anwendung abstürzen lassen. Die Suche selbst läuft geschützt in eigenen Prozessen.

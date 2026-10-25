Plugin-Verwaltung und Korrekturen an Spur- und Send-Effekten für Windows 10/11 (64 Bit) und – neu – Linux.

**Installation:** `Ton-of-Able-v0.2.2-setup.exe` ausführen – eine vorhandene Installation wird aktualisiert. Der Installer ist nicht signiert: Bei der SmartScreen-Warnung „Weitere Informationen“ → „Trotzdem ausführen“.

**Ohne Installation:** ZIP entpacken und `Ton of Able.exe` starten. Die exe, der Ordner `resources`, `ffmpeg.exe` und die Plugins müssen im selben Ordner bleiben.

**Neu**
- **Stimmgerät „Ton Tuner“** als mitgeliefertes VST3-Plugin (auch für andere DAWs): Note, Oktave, Cent-Nadel, Kammerton 430–450 Hz, „Stumm beim Stimmen“. Am besten neben die Signalkette setzen.
- **Akkord-Leiste (Lead-Sheet)** über dem Lineal: Takt anklicken, Akkord tippen (`Am7`, `G/H` …), Tab springt zum nächsten Takt. Ein Akkord pro Takt oder – mit `Am G` bzw. Rechtsklick → „Halbe Takte“ – zwei pro Takt. Wird mit dem Projekt gespeichert.
- **Begleitung aus Akkorden:** Rechtsklick in die Akkordleiste → „Begleitung aus Akkorden erzeugen …“ legt sphärische Synth-Spuren an – Pad mit weicher Stimmführung, Sub-Bass und Shimmer-Arpeggio – samt Return-Spur „Atmo-Hall“ (Supermassive, Dragonfly oder eingebauter Hall). Die Spuren bleiben mit den Akkorden verknüpft und ändern sich mit; Noten von Hand bearbeiten löst die Verknüpfung. Neue Synth-Klänge Atmo, Swell, Sub und Glass (mit Stereo-Chorus).
- **MIDI-Spuren mit Piano-Roll:** Noten zeichnen, verschieben, in der Länge ändern, quantisieren, Anschlag bearbeiten. Klang vom eingebauten Synth (Pad, Keys, Bass, Lead) oder von VST3-Instrumenten wie Vital. Scharf geschaltete MIDI-Spuren werden vom MIDI-Keyboard gespielt und nehmen Noten auf.
- **Plugin-Verwaltung:** „+“ in der Signalkette → „Plugins verwalten / importieren …“. Zeigt alle VST3-Plugins mit Hersteller, Art und Datei. Einzelne `.vst3`-Dateien von beliebigen Orten importieren, eigene Suchordner hinzufügen, alles neu suchen, Plugins im Menü ausblenden. Plugins, die sich nicht laden lassen, stehen mit Grund in der Liste und lassen sich erneut prüfen oder entfernen.
- **Re-Amping:** Rechtsklick auf eine Audio-Spur → „Re-Amp: ins Live-Rig spielen“. Bei der Wiedergabe spielt die Spur statt der Gitarre durch das Live-Rig, die Effekte (auch Wah per Maus) lassen sich live verändern, eine scharfgeschaltete Rig-Spur nimmt das Ergebnis auf – mit Loop als Takes.
- **Wah per Maus in Spur-Effekten:** „MAUS“ beim Wah einer Spurkette lässt die Maus das Pedal auch bei der Wiedergabe dieser Spur steuern.
- **Neue mitgelieferte Plugins:** Dragonfly Reverb (Hall, Room, Plate, Early Reflections) für natürliche Räume, Valhalla Freq Echo und Valhalla Space Modulator (Flanger). In der Kette über „+“ einfügen.
- **Linux-Version:** AppImage und tar.gz für x86_64 (ALSA/JACK, PipeWire). BYOD und Dragonfly laufen nativ; Valhalla Supermassive und eigene Windows-VST3-Plugins laufen über das mitgelieferte yabridge, sobald Wine (Staging) installiert ist – Einrichtung automatisch, Details in `LIESMICH-Linux.txt`.
- **Loop-Bereich:** Anfang und Ende lassen sich im Lineal einzeln ziehen (bisher verschob sich immer der ganze Balken). Play und Aufnahme springen bei eingeschaltetem Loop an den Loop-Anfang, wenn die Abspielposition außerhalb liegt. Der Loop-Button nimmt beim Einschalten die ausgewählten Clips, sonst den sichtbaren bisherigen Bereich oder 4 Takte ab der Abspielposition.
- **Export mit Fortschritt:** Während des Exports zeigt ein Balken den Fortschritt in Prozent (bei MP3/AAC/Opus danach „Konvertiere …“), mit „Abbrechen“ – angefangene Dateien werden dann gelöscht.
- **Export-Bereich:** Neuer Balken unter den Spuren, dessen Anfang und Ende man zieht; er gilt für Mix, Stems und einzelne Spuren (ohne Einstellung: ganzer Song) und wird mit dem Projekt gespeichert. Der frühere Menüpunkt „Mix – nur Loop-Bereich“ entfällt.

**Behoben**
- Drum-Spuren: Die Spur-Effekte wirken jetzt auch auf das Drum-Instrument (bisher lief es an der Kette vorbei, auch beim Export).
- Spur- und Send-Effekte arbeiten in Stereo: Panorama und Stereobild der Spuren bleiben in Return-Spuren (Hall, Delay) und Spurketten erhalten, statt auf Mono zusammengemischt zu werden.

**Hinweise**
- Einstellungen und Plugin-Liste aus v0.2.1 werden übernommen.
- Fremde VST3-Plugins laufen im Programm selbst: Ein fehlerhaftes Plugin kann die Anwendung abstürzen lassen. Die Suche selbst läuft geschützt in eigenen Prozessen.

Neue Version für Windows 10/11 (64 Bit): moderne Oberfläche, frei verdrahtbare Signalkette mit VST3-Instrumenten, echte Drum-Samples und Loop-Aufnahme mit Takes.

**Installation:** `Ton-of-Able-v0.2.1-setup.exe` ausführen – eine vorhandene Installation wird aktualisiert. Der Installer ist nicht signiert: Bei der SmartScreen-Warnung „Weitere Informationen“ → „Trotzdem ausführen“.

**Ohne Installation:** ZIP entpacken und `Ton of Able.exe` starten. Die exe, der Ordner `resources`, `ffmpeg.exe` und die Plugins müssen im selben Ordner bleiben.

**Neu**
- **Signalkette frei bearbeiten:** Blöcke per Ziehen umsortieren, mit „+“ beliebig viele VST3-Effekte, Instrumente (z. B. Vital) und Audio→MIDI-Plugins (z. B. TONIQ, MIDI Guitar) einfügen – in die Kette oder daneben. Per Rechtsklick den Audio- oder MIDI-Ausgang jedes Blocks in jeden anderen leiten, mit Pegel pro Verbindung. Beispiel: Gitarre → TONIQ → MIDI → Vital, auch „nur Synth“. MIDI-Keyboards und virtuelle MIDI-Ports spielen mit. Die Spur-Effekte im Arrangement haben dieselbe Kette.
- **Loop-Aufnahme mit Takes:** Mit eingeschaltetem Loop wird jede Runde ein Take. „TAKE 3/5“ im Clip anklicken oder `[` / `]` drücken und den besten wählen; Clip teilen und pro Teil den besten Take wählen (Comping).
- **Echte Drum-Samples:** SM Drums (Stereo, bis zu 16 Anschlagstufen, wechselnde Schläge) als Standard für neue Drum-Spuren, dazu die AVL-Kits Black Pearl und Red Zeppelin; das Synth-Kit bleibt als elektronische Option.
- **Neue Oberfläche:** Reiter RIG / ARRANGEMENT / BIBLIOTHEK, die Signalkette als zentrale Bedienung des Rigs, flaches modernes Design mit eigenen Symbolen, überarbeitete Transportleiste und Spurköpfe im Arrangement, Effektkarten mit Schaltern.
- **PITCH:** Der latenzfreie Analog-Oktaver ist bei −12 jetzt eine wählbare Option statt fest eingestellt.

**Behoben**
- Einstellungen: Klicks landeten vertikal versetzt.
- Mute und Solo zeigen ihren Zustand sofort.
- Der Drumcomputer hört beim Schließen des Fensters auf vorzuhören.

**Hinweise**
- Presets und Einstellungen aus v0.2.0 werden übernommen; die früheren Plätze „VST3 vor/nach dem Amp“ werden zu Blöcken an derselben Stelle der Kette.
- Fremde VST3-Plugins laufen im Programm selbst: Ein fehlerhaftes Plugin kann die Anwendung abstürzen lassen.
- Die TONE3000-Dateien sind nur für den privaten Gebrauch gedacht. Details stehen in `LIESMICH.txt`.

# Arrangement (Mehrspur-Aufnahme und -Bearbeitung)

Das Arrangement ist die Mehrspur-Ansicht von Ton of Able, angelehnt an die Arrangement-Ansicht von Ableton Live.
Umschalten zwischen Live-Rig und Arrangement: Button **ARRANGEMENT / LIVE-RIG** oben rechts oder **Tab**.
Das Live-Rig (NAM, IR, Effekte, VST3) läuft in beiden Ansichten weiter.

## Spuren und Eingänge

Jede Spur hat eine Aufnahmequelle (Auswahlfeld im Spurkopf):

| Quelle | Was aufgenommen wird |
| --- | --- |
| **NAM-Rig (bearbeitet, Stereo)** | das fertige Signal des Live-Rigs |
| **In N: … (dry)** | der unbearbeitete Interface-Eingang N (Mono) |

Mehrere Eingänge aktivierst du unter **Einstellungen → Active input channels**. Das Rig nutzt immer den ersten aktiven Eingang.

Spurkopf: **●** scharf schalten · **M** Stumm · **S** Solo · **FX** eigene Effektkette · **IN** Eingang abhören · Lautstärke · Panorama · Pegelanzeige.
Rechtsklick auf den Spurkopf: Umbenennen, Farbe, Spur-Effekte, Spur exportieren, Spur löschen.

## Aufnehmen

1. Spur(en) scharf schalten (●). Ist keine Spur scharf, wird die ausgewählte Spur genommen.
2. **REC** oder **F9**. Mit **Count-In** zählt das Metronom einen Takt vor.
3. **Stop** oder **Leertaste** beendet die Aufnahme. Die Takes landen als WAV im Projektordner unter `Audio/`.

- Mehrere scharfe Spuren werden gleichzeitig aufgenommen (bis zu 32).
- Die Treiberlatenz und die Latenz der Effektkette werden automatisch ausgeglichen.
- Liegen Takes trotzdem leicht daneben: **Projekt → Aufnahme-Latenz feinjustieren**.
- Neue Takes überschreiben den darunterliegenden Bereich wie in Ableton. **Ctrl+Z** stellt ihn wieder her.

## Effekte pro Spur (NAM, IR, EQ, Graphic EQ, Effekte, VST3)

**FX** im Spurkopf öffnet die Effektkette der Spur. Jede Spur ist unabhängig einstellbar:

- NAM-Pedal, NAM-Amp, Cabinet-IR, Input/Bass/Mid/Treble/Output, Graphic EQ sowie Drive, Auto-Wah, Chorus, Delay und Reverb.
- **Live-Rig übernehmen** kopiert den aktuellen Live-Sound inklusive VST3-Zuständen in die Spur.
- **Insert (nachträglich)**: Die Spur wird trocken aufgenommen, die Kette läuft erst bei der Wiedergabe. Amp und Effekte bleiben jederzeit änderbar. So wendest du NAMs und Effekte auch im Nachhinein auf bestehende Spuren an.
- **Print (bei Aufnahme)**: Die Kette wird beim Aufnehmen in die Datei eingerechnet. Das Abhören läuft dabei über die Kette.

Rig-Spuren nehmen das Live-Rig auf. Ihre eigene Kette wirkt dort zusätzlich als Insert.

## Bearbeiten

| Aktion | Bedienung |
| --- | --- |
| Clip verschieben (auch auf andere Spur) | Clip ziehen |
| Kopie ziehen | **Ctrl** + ziehen |
| Kürzen (Trim) | linke/rechte Clipkante ziehen |
| Time-Stretch (Rubber Band) | **Ctrl** + Kante ziehen oder Werkzeug **Stretch (T)** |
| Fade-In/-Out | kleine Griffe oben links/rechts im Clip ziehen |
| Teilen | **Ctrl+E** an der Abspielposition, Werkzeug **Schere (C)**, Rechtsklick → Hier teilen |
| Ohne Raster | **Alt** beim Ziehen |
| Mehrfachauswahl | **Shift**-Klick oder Rahmen in freier Fläche ziehen |
| Duplizieren / Kopieren / Ausschneiden / Einfügen | **Ctrl+D / C / X / V** |
| Löschen | **Entf** / **Backspace** |
| Rückgängig / Wiederholen | **Ctrl+Z** / **Ctrl+Y** oder **Ctrl+Shift+Z** |
| Clip-Eigenschaften (Name, Gain, Stretch %, Transponieren) | Doppelklick auf den Clip |

Rechtsklick auf einen Clip öffnet Time-Stretch-Vorgaben (50 % … 200 %, **An ganze Takte anpassen**) und **Transponieren** (Tonhöhe ohne Tempoänderung).
Beides rechnet die Rubber Band Library (R3-Engine, Offline-Modus) im Hintergrund. Bis das Ergebnis fertig ist, hörst du eine Varispeed-Vorschau und im Clip steht „Rubber Band rechnet …“.

## Zoom, Navigation, Loop

- **Ctrl + Mausrad**: horizontal zoomen um die Mausposition, auch per Touchpad-Pinch, **Ctrl + / Ctrl -** oder die Buttons **+ / -**. **Alles** bzw. **Z** zeigt das ganze Arrangement.
- **Mausrad**: Spuren scrollen · **Shift + Mausrad**: horizontal scrollen · **Alt + Mausrad**: Spurhöhe.
- Klick ins Lineal oder in eine freie Fläche setzt die Abspielposition. **Pos1** springt zum Anfang. **Stop** zweimal = zurück zum Anfang. **F** schaltet das Mitlaufen der Ansicht an/aus.
- **Loop**: oberen Lineal-Streifen ziehen, Klick auf den Streifen bzw. **Ctrl+L** schaltet an/aus. Während einer Aufnahme läuft der Transport linear weiter.
- Raster (1 Takt … 1/32 oder aus), Tempo (Doppelklick auf BPM), Taktart, Metronom (**Click**).

## Import

**Import** (**Ctrl+I**) oder Dateien per Drag & Drop auf die Timeline ziehen. Mehrere Dateien landen auf neuen Spuren.

- Direkt, ohne Zusatzsoftware: WAV, AIFF, FLAC, Ogg Vorbis, MP3 und unter Windows zusätzlich WMA/Windows-Media-Formate.
- **Mit FFmpeg**: AAC/M4A, ALAC, Opus, AC3, APE, WavPack usw. und **Audio aus Videos** (MP4, MOV, MKV, AVI, WebM, WMV, FLV, MPG …). Die Tonspur wird als WAV in den Projektordner (`Audio/`) extrahiert.

FFmpeg wird nicht mitgeliefert. Ton of Able findet `ffmpeg.exe` neben `Ton of Able.exe`, in einem Unterordner `ffmpeg\` bzw. `ffmpeg\bin\`, im `PATH` oder über **Projekt → FFmpeg auswählen …**.

## Export

**Export** bietet:

- **Mix** (ganzes Arrangement oder nur der Loop-Bereich)
- **Alle Spuren einzeln (Stems)** in einen Ordner. Stems enthalten Lautstärke und Panorama der Spur und ignorieren Mute/Solo.
- **Ausgewählte Spur** (auch per Rechtsklick auf den Spurkopf)

Formate: WAV 16/24/32-Bit-Float, AIFF 24, FLAC 24, Ogg Vorbis sowie MP3 320 kbps, AAC/M4A 256 kbps und Opus 192 kbps über FFmpeg.
Ein Export ist erst möglich, wenn alle Rubber-Band-Berechnungen fertig sind.

## Projekte

- Ein Projekt ist ein Ordner mit `Name.tonproj` (XML) und dem Unterordner `Audio/` für Aufnahmen und extrahierte Audiodateien. Importierte Dateien außerhalb des Projekts werden referenziert.
- **Projekt → Speichern / Speichern unter / Öffnen / Neues Projekt.** Bei „Speichern unter“ werden die Dateien aus `Audio/` mitkopiert.
- Neue Projekte liegen unter `Dokumente\Ton of Able\Projekte\`. Beim Beenden und beim Projektwechsel wird automatisch gespeichert. Das zuletzt geöffnete Projekt wird beim Start wieder geladen.
- `Ton of Able.exe Pfad\zum\Projekt.tonproj` öffnet ein Projekt direkt, z. B. per Dateizuordnung.
- Ändert sich die Samplerate des Audiogeräts, bleiben die musikalischen Positionen erhalten und das Audio wird neu geladen.

## Drumcomputer

**+ Spur → Drum-Spur (Drumcomputer)** legt eine Drum-Spur an und öffnet den Step-Sequencer (später über den Button **DRUMCOMPUTER** im Spurkopf oder per Doppelklick auf einen Pattern-Clip).

- **8 Patterns (A–H)** mit Startvorlagen: Rock, Rock-Fill, Half-Time, Four on the Floor, Metal (Double Bass), Shuffle, Funk, leer.
- **12 Instrumente** in General-MIDI-Belegung: Kick 36, Snare 38, Rimshot 37, Clap 39, HiHat zu/Pedal/offen 42/44/46, Toms 45/47/50, Crash 49, Ride 51.
- **8 bis 64 Schritte** (Sechzehntel) und **Swing**.
- **Bedienung im Raster:** Klick schaltet einen Schritt an/aus, Ziehen malt, Shift setzt einen Akzent, Rechtsklick wechselt die Anschlagstärke (ghost/normal/Akzent/aus). Ein Klick auf den Instrumentnamen hört den Klang vor.
- **▶ Vorhören** spielt das Pattern bei gestopptem Transport in Schleife.
- **In Arrangement einfügen** legt einen Pattern-Clip über 1–32 Takte an der Abspielposition an. Pattern-Clips lassen sich verschieben, kopieren, kürzen, teilen und duplizieren wie Audio-Clips. Rechtsklick → **Pattern wechseln**. Sie folgen dem Projekttempo und werden deshalb nicht gestretcht.
- Mix, Stems und Einzelspur-Export rendern Drum-Spuren mit, auch mit VST3-Instrumenten. Dafür wird eine unabhängige Kopie des Instruments benutzt.

**Klangquelle** (Auswahl oben rechts im Drumcomputer):

- **Internes Synth-Kit:** synthetisierte 808/909-artige Sounds. Funktioniert sofort, ohne Samples oder Plugins.
- **VST3-Instrument:** z. B. das freie Open-Source-Drum-Plugin [JDrummer](https://github.com/jmantra/jdrummer) (GPL-3, 28 Soundfont-Kits). Den Ordner `jdrummer.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren, dann im Drumcomputer **VST3-Instrumente suchen …** und das Plugin auswählen. Das Kit wählst du im **Plugin-Editor**. Gesendet wird General MIDI auf Kanal 10, deshalb passen auch andere GM-Drum-Plugins.

Ton of Able teilt jeden Audio-Block an den Schlag-Positionen auf. Dadurch sitzen die Schläge sample-genau, auch bei Plugins, die MIDI nur am Blockanfang auswerten (JDrummer tut das). Mit JDrummer geprüft: Alle 12 Instrumente erklingen exakt an ihrer Position, auch bei 8192-Sample-Exportblöcken.

Für JDrummer oder jedes andere VST3-Drum-Instrument gibt es einen eigenen Test, der als separater Prozess läuft:

```powershell
& "Ton of Able.exe" --drum-instrument-test "C:\Program Files\Common Files\VST3\jdrummer.vst3" report.xml
```

## Live-Transponieren (PITCH, Rubber Band)

**PITCH** im Live-Rig (neben GRAPHIC EQ) und der Pitch-Bereich im Spur-Effektfenster transponieren das Live-Signal mit der Rubber Band Library, z. B. **eine Oktave tiefer** (Schnellwahl −24, −12, −7, −5, +5, +7, +12).

- Die Stufe sitzt am Anfang der Kette, vor Drive und NAM-Amp, wie ein Pitch-Pedal vor dem Verstärker.
- Einstellbar sind **Mix** (Original ↔ transponiert), **Formanten erhalten** und der Modus.
- **Genau −12 (eine Oktave tiefer) läuft automatisch latenzfrei** über einen analogen Oktaver nach dem Vorbild klassischer Oktavpedale (Flip-Flop-Teiler), nicht über Rubber Band. Der Modus ist dann ausgegraut.
  - Arbeitsweise: Das Signal wird auf den Grundton gefiltert, ein Schmitt-Trigger macht daraus ein Rechteck, ein Flip-Flop teilt es durch zwei. Das Ergebnis folgt der Anschlagsdynamik und wird über ein Tonfilter abgerundet.
  - Grundton-Erkennung: Fünf parallele Tiefpässe im Oktavabstand laufen gleichzeitig. Der Trigger hört auf das tiefste Band, das noch Signal führt. So wird vom tiefen E bis zum 22. Bund der hohen E-Saite sauber geteilt, ohne dass Obertöne doppelt schalten.
  - Es wird nichts gepuffert, also keine Latenz außer der Phasenverschiebung der Filter (1–2 ms), und es gibt keinen Latenzausgleich.
  - Wie beim Pedal einstimmig: Einzelnoten und Riffs funktionieren, Akkorde zerfallen in Störgeräusche. Der Klang ist synthartiger als Rubber Band, der Mix-Regler mischt das Original dazu.
- **Niedrige Latenz** (R2-Echtzeit-Engine, kurzes Fenster), für alle anderen Intervalle: gemessen eine Oktave tiefer bei 48 kHz etwa **28 ms** mit Interface-Puffern von 32/64 Samples, **37 ms** bei 128 und **43–46 ms** bei 256–1024 Samples.
- **Hohe Qualität** (R3 `RubberBandLiveShifter`): sauberer, aber etwa **87 ms**.
- Originalsignal und transponiertes Signal laufen zeitgleich, es gibt also kein Doppeln oder Flammen im Mix. Aufnahmen werden um die Verzögerung ausgeglichen.
- Die Verzögerung ist dennoch hörbar. Für knackiges Live-Spielen eher kleine Interface-Puffer verwenden, gut geeignet ist es z. B. für Bass-Oktaven zum Schichten.
- Zwei Oktaven (±24) klingen im Niedrig-Latenz-Modus hörbar ungenauer.

## Graphic EQ

10 Bänder nach ISO-Oktaven (31,5 Hz … 16 kHz, ±12 dB) mit Frequenzgang-Anzeige und Presets.
Im Live-Rig über den Button **GRAPHIC EQ** (wird mit Presets/A-B gespeichert), pro Spur im FX-Fenster.
Er sitzt in der Kette nach dem Amp-EQ und vor Auto-Wah/Chorus/Delay/Reverb.
Technisch sind es konstant-Q-RBJ-Peaking-Filter aus JUCEs `dsp::IIR::ArrayCoefficients`, die ohne Speicherallokation auskommen und damit echtzeitsicher sind.

## Lizenzen

- **Rubber Band Library** (Breakfast Quay / Particular Programs, <https://breakfastquay.com/rubberband/>) liegt als Submodul unter `third_party/rubberband` (v4.0.0) und wird über den Single-File-Build einkompiliert. Lizenz: GPL-2.0-or-later. Wer die Anwendung unter anderen Bedingungen als der GPL verbreiten will, braucht eine kommerzielle Rubber-Band-Lizenz.
- **FFmpeg** wird nur als externes Programm aufgerufen und nicht mitgeliefert. Für eine eigene Weitergabe gelten die Lizenzbedingungen des verwendeten FFmpeg-Builds (LGPL/GPL).

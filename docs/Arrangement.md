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

## Send-Effekte (Return-Spuren A–D)

Statt jeder Spur einen eigenen Hall zu geben, teilen sich mehrere Spuren einen Effekt. Das klingt zusammenhängender und spart Rechenleistung.

- **+ Spur → Return-Spur (Send-Effekt)** oder **Strg+Umschalt+R** legt bis zu vier Return-Spuren an. Sie stehen immer unten.
- Die Effektvorgabe ist 100 % nass: A = Hall, B = Delay, C = Plate, D = Slapback. Über **FX** lässt sich jede Kette wie eine Spurkette bearbeiten, inklusive NAM, IR, EQ und Kompressor.
- **SEND** im Spurkopf (oder Rechtsklick → **Sends …**) öffnet je Return einen Regler (aus … +6 dB). Der Button leuchtet, sobald ein Send aktiv ist.
- Sends sind **post-Fader**: Lautstärke, Panorama und Mute der Spur gelten auch für den Send.
- Return-Spuren haben Lautstärke, Panorama, Mute, Solo und Pegelanzeige. Sie nehmen keine Clips auf. Beim Solo einer anderen Spur bleiben sie hörbar, so bleibt der Hall der Solo-Spur erhalten.
- Beim Export enthält der Mix die Returns. Bei Stems bekommt jede Return-Spur eine eigene Datei mit dem Effektanteil, die normalen Spuren sind dann trocken.

## Spuren einfrieren (Freeze)

Spuren mit rechenintensiven NAMs, VST3-Effekten oder VST3-Drums lassen sich vorberechnen:

- Rechtsklick auf den Spurkopf → **Spur einfrieren** oder **Strg+Umschalt+F** für die ausgewählte Spur.
- Die Spur wird im Hintergrund durch Instrument und Effektkette in `Audio/Freeze/` gerendert, mit 4 s Nachlauf für Hall und Delay. Danach spielt sie diese Datei statt der Kette. Der Spurkopf zeigt ❄, die Clips einen hellblauen Schleier.
- Lautstärke, Panorama, Mute/Solo und Sends wirken weiter, weil sie nach dem Einfrieren kommen.
- **Bearbeiten taut automatisch auf**: Wer Clips, Patterns, Instrument, Effekte oder Tempo ändert, hört wieder die Live-Kette, und die Statuszeile meldet es. Rückgängig stellt den eingefrorenen Zustand wieder her.
- **Auftauen**: Klick auf ❄, Rechtsklick → **Spur auftauen** oder **Strg+Umschalt+U**.
- **Einfrieren und in Audio umwandeln** ersetzt die Clips durch das Ergebnis und schaltet die Kette aus. Die Einstellungen bleiben dabei erhalten.
- Eingefrorene Spuren nehmen nicht auf. Return-Spuren lassen sich nicht einfrieren.

## Kompressor

Im Effekt-Rack (Live-Rig und jede Spur- bzw. Return-Kette) gibt es die Karte **KOMPRESSOR** mit Threshold, Ratio, Attack, Release, Knee, Makeup und Mix.

- **VOR AMP** setzt ihn wie ein Kompressor-Pedal vor Drive und NAM, für mehr Sustain. Aus heißt nach Amp, Box und EQs, wie im Studio.
- Vorgaben: **Sanft** (2,5:1), **Clean Sustain** (Pedal), **Funk Squash** (12:1, schnell) und **Parallel** (Mix 45 % für New-York-Kompression).
- Technik: Feed-Forward, Peak-Detektor über alle Kanäle, Soft Knee, Attack/Release auf der Gainreduktion in dB.

## Tastenkürzel

**Tasten** in der Werkzeugleiste oder **F1** öffnet die Übersicht aller Befehle, nach Bereich sortiert.

- **Klick auf eine Taste**: ändern oder entfernen. **+**: weitere Taste für denselben Befehl.
- Ist eine Taste schon belegt, fragt das Programm, ob sie umgelegt werden soll.
- **Standard wiederherstellen** setzt alles zurück. Änderungen gelten sofort und werden gespeichert.
- Ohne Standardtaste und frei belegbar sind z. B. Metronom, Count-In, In Audio umwandeln und Sends.
- Die Mausgesten stehen unten in der Übersicht, sie sind fest.

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
- **Alle Spuren einzeln (Stems)** in einen Ordner. Stems enthalten Lautstärke und Panorama der Spur und ignorieren Mute/Solo. Return-Spuren erscheinen als eigene Stems.
- **Ausgewählte Spur** (auch per Rechtsklick auf den Spurkopf)

Formate: WAV 16/24/32-Bit-Float, AIFF 24, FLAC 24, Ogg Vorbis sowie MP3 320 kbps, AAC/M4A 256 kbps und Opus 192 kbps über FFmpeg.
Ein Export ist erst möglich, wenn alle Rubber-Band-Berechnungen fertig sind.
Die Effektketten der Spuren (Insert) und der Return-Spuren werden mitgerendert. Solange der Export läuft, spielen diese Spuren live ohne Effekte.

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
Er sitzt in der Kette nach dem Amp-EQ und vor Kompressor (Stellung „nach Amp“), Auto-Wah, Chorus, Delay und Reverb.
Technisch sind es konstant-Q-RBJ-Peaking-Filter aus JUCEs `dsp::IIR::ArrayCoefficients`, die ohne Speicherallokation auskommen und damit echtzeitsicher sind.

## Lizenzen

- **Rubber Band Library** (Breakfast Quay / Particular Programs, <https://breakfastquay.com/rubberband/>) liegt als Submodul unter `third_party/rubberband` (v4.0.0) und wird über den Single-File-Build einkompiliert. Lizenz: GPL-2.0-or-later. Wer die Anwendung unter anderen Bedingungen als der GPL verbreiten will, braucht eine kommerzielle Rubber-Band-Lizenz.
- **FFmpeg** wird als externes Programm aufgerufen. Das Windows-Paket aus dem Release-Workflow legt einen statischen LGPL-Build (`ffmpeg.exe`) neben die exe, die Lizenz steht in `licenses/FFmpeg-LICENSE.txt`. Bei eigener Weitergabe gelten die Bedingungen des verwendeten Builds (LGPL/GPL).

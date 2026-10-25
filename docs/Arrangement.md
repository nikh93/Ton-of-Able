# Arrangement (Mehrspur-Aufnahme und -Bearbeitung)

Das Arrangement ist die Mehrspur-Ansicht von Ton of Able, angelehnt an die Arrangement-Ansicht von Ableton Live.
Umschalten über die Reiter **RIG / ARRANGEMENT / BIBLIOTHEK** oben oder **Tab** (Rig ↔ Arrangement).
Das Live-Rig (NAM, IR, Effekte, VST3) läuft in beiden Ansichten weiter.

Aufbau der Oberfläche:
- **Transportleiste** (oben): Projekt-Button mit Projektname (• = ungespeichert; Neu, Öffnen, Speichern), Transport (Anfang, Play, Stop, Aufnahme), Display mit Position in Takten und Zeit, Tempo (Doppelklick zum Ändern) und Taktart, dann Loop, Metronom und Vorzähler, rechts Master-Lautstärke und Tastenkürzel.
- **Bearbeitungsleiste**: Werkzeuge Zeiger, Schere, Stretch, Raster, Zoom, Rückgängig/Wiederholen, rechts Import und Export.
- **Spurspalte**: „+ Spur“ legt Audio-, Drum- oder Return-Spuren an. Jeder Spurkopf zeigt Name, Aufnahme-Scharfschaltung, Mute, Solo, Quelle mit Abhören (Kopfhörer), FX und SEND, Lautstärke, Panorama und Pegel. Alle Buttons erklären sich per Tooltip.

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

### Loop-Aufnahme mit Takes

Loop einschalten (**Ctrl+L**, Bereich oben im Lineal ziehen) und aufnehmen (**F9**): Der Loop läuft weiter und jede Runde wird ein **Take**. Nach dem Stopp liegt ein Clip über dem Loop-Bereich, der den zuletzt vollständig gespielten Take abspielt.

- Das Feld **TAKE 3/5 ▾** im Clip öffnet die Liste der Takes – einfach den besten anklicken. Während der Loop weiterläuft, hört man den Wechsel sofort.
- **]** / **[** (frei belegbar): nächster / vorheriger Take der ausgewählten Clips – auf mehreren Spuren zugleich.
- **Comping:** Clip mit der Schere (oder **Ctrl+E**) teilen, dann für jeden Teil den besten Take wählen – z. B. Takt 1–2 aus Take 2, Takt 3–4 aus Take 5. Fades an den Schnitten glätten die Übergänge.
- **Andere Takes verwerfen** (im Take-Menü) behält nur den gewählten.
- Steht die Abspielposition außerhalb des Loops, beginnen Wiedergabe und Aufnahme am Loop-Anfang (mit Vorzähler einen Takt davor). Innerhalb des Loops beginnen sie an der Abspielposition; diese angefangene erste Runde wird kein Take, ebenso eine letzte Runde unter einer halben Loop-Länge. Wird die Aufnahme bei laufender Wiedergabe vor dem Loop gestartet, bleibt der Vorlauf ein eigener Clip.
- Alle Takes liegen in derselben Aufnahmedatei; Wechseln kostet nichts und ist rückgängig machbar. Takes werden mit dem Projekt gespeichert.

## Effekte pro Spur (NAM, IR, EQ, Graphic EQ, Effekte, VST3)

**FX** im Spurkopf öffnet die Effektkette der Spur. Jede Spur ist unabhängig einstellbar:

- NAM-Pedal, NAM-Amp, Cabinet-IR, Input/Bass/Mid/Treble/Output, Graphic EQ sowie Drive, Auto-Wah, Chorus, Delay und Reverb.
- **Live-Rig übernehmen** kopiert den aktuellen Live-Sound inklusive VST3-Zuständen in die Spur.
- **Insert (nachträglich)**: Die Spur wird trocken aufgenommen, die Kette läuft erst bei der Wiedergabe. Amp und Effekte bleiben jederzeit änderbar. So wendest du NAMs und Effekte auch im Nachhinein auf bestehende Spuren an.
- **Print (bei Aufnahme)**: Die Kette wird beim Aufnehmen in die Datei eingerechnet. Das Abhören läuft dabei über die Kette.

Rig-Spuren nehmen das Live-Rig auf. Ihre eigene Kette wirkt dort zusätzlich als Insert.

**Was wirkt bei der Wiedergabe?** Nur die Kette der Spur selbst. Das Live-Rig (oben im Tab RIG) verarbeitet ausschließlich das, was gerade am Eingang hereinkommt; Aufnahmen laufen nie noch einmal hindurch. Bei einer Rig-Spur ist der Live-Sound schon in der Aufnahme enthalten: Ein Wah, das beim Einspielen aus war, lässt sich danach nicht mehr über das Rig hinzuschalten – auch kein anderer Rig-Effekt. Wer den Sound nachträglich ändern will, nimmt die Gitarre trocken auf (Audio-Spur mit Eingang, Insert) und baut den Sound in der Spurkette, z. B. mit **Live-Rig übernehmen** – oder spielt sie per Re-Amping erneut durch das Live-Rig.

### Re-Amping

Rechtsklick auf eine Audio-Spur → **Re-Amp: ins Live-Rig spielen**. Bei der Wiedergabe spielt die Spur dann statt der Gitarre durch das Live-Rig: Wah per Maus, Amp, Plugins, Presets – alles lässt sich live verändern. Eine scharfgeschaltete Rig-Spur nimmt das Ergebnis auf, mit Loop auch als mehrere Takes.

- Die Spur hört man dabei nur über das Rig, nicht direkt. Ihr Fader und Mute wirken nicht auf das Re-Amping (wie ein Re-Amp-Ausgang vor dem Fader).
- Im Spurkopf leuchtet **RE-AMP**; ein Klick darauf schaltet aus. Im Tab RIG steht der Rig-Eingang über der Quellenwahl.
- Gestoppt spielt wieder die Gitarre ins Rig.
- Die Aufnahme wird nur um die Latenz des Rigs verschoben, nicht um die des Audio-Interfaces – das Signal verlässt das Programm nicht.
- Am besten eignen sich trockene Aufnahmen (DI). Eingefrorene Spuren erst auftauen. Die Einstellung gilt für die Sitzung und wird nicht im Projekt gespeichert.

Die Spurkette bearbeitet Clips und das Drum-Instrument einer Drum-Spur gleichermaßen und arbeitet in Stereo: Ausgeschaltete eingebaute Effekte lassen das Stereobild durch, eingeschaltete eingebaute Effekte sind mono, VST3-Plugins stereo. Return-Spuren erhalten so auch das Panorama der sendenden Spuren.

## Send-Effekte (Return-Spuren A–D)

Statt jeder Spur einen eigenen Hall zu geben, teilen sich mehrere Spuren einen Effekt. Das klingt zusammenhängender und spart Rechenleistung.

- **+ Spur** (oben in der Spurspalte) **→ Return-Spur (Send-Effekt)** oder **Strg+Umschalt+R** legt bis zu vier Return-Spuren an. Sie stehen immer unten.
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

## Wah-Pedal mit Maussteuerung

Im Effekt-Rack gibt es die Karte **WAH-PEDAL**: ein klassisches Wah wie Cry Baby oder Vox. Es ist ein resonanter Bandpass, der beim Durchtreten exponentiell von **HEEL** (dunkel) nach **TOE** (hell) wandert. In der Kette sitzt es nach dem Kompressor (Stellung „vor Amp“) und vor Drive und NAM-Amp.

- **MAUS** (oder Taste **W**, frei belegbar) macht die Maus zum Pedal. Die vertikale Mausposition auf dem Bildschirm steuert das Pedal: **oben = Toe** (hell, offen), **unten = Heel** (dunkel). Klicken muss man nicht, die Maus kann über jedem Fenster stehen.
- Oben und unten sind je 5 % des Bildschirms Totzone, damit die Endstellungen leicht zu treffen sind. Der POSITION-Regler zeigt die aktuelle Stellung.
- Regler: **POSITION** (fest eingestellt ergibt sich ein „Cocked Wah“), **HEEL**, **TOE**, **Q** (Resonanz) und **MIX**.
- Vorgaben: **Cry Baby** (350–2200 Hz), **Vox** (450–1800 Hz), **Bass-Wah** (180–1100 Hz) und **Fester Wah** (ohne Maus, halb getreten).
- Die Maussteuerung wirkt auf das Live-Rig, auch während der Aufnahme im Arrangement, und auf jede Spurkette, in der beim Wah **MAUS** eingeschaltet ist (bei der Wiedergabe hörbar). Die Taste W schaltet nur das Live-Rig um.

## Tastenkürzel

Das Tastatur-Symbol rechts in der Transportleiste oder **F1** öffnet die Übersicht aller Befehle, nach Bereich sortiert.

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

- **Ctrl + Mausrad**: horizontal zoomen um die Mausposition, auch per Touchpad-Pinch, **Ctrl + / Ctrl -** oder die Lupen-Buttons. Der dritte Zoom-Button bzw. **Z** zeigt das ganze Arrangement.
- **Mausrad**: Spuren scrollen · **Shift + Mausrad**: horizontal scrollen · **Alt + Mausrad**: Spurhöhe.
- Klick ins Lineal oder in eine freie Fläche setzt die Abspielposition. **Pos1** springt zum Anfang. **Stop** zweimal = zurück zum Anfang. **F** schaltet das Mitlaufen der Ansicht an/aus.
- **Loop**: im oberen Lineal-Streifen außerhalb des Balkens ziehen legt einen neuen Bereich an; den Balken in der Mitte ziehen verschiebt ihn, **an Anfang oder Ende ziehen** (Mauszeiger ↔) verändert nur diese Grenze. Alles rastet am Raster ein (Alt: frei). Klick auf den Streifen schaltet an/aus. Der **Loop-Button** bzw. **Ctrl+L** nimmt beim Einschalten die ausgewählten Clips als Bereich; ohne Auswahl den bisherigen Bereich, wenn er im Bild ist, sonst 4 Takte ab dem Takt der Abspielposition (bzw. ab dem ersten sichtbaren Takt). Auch während einer Aufnahme läuft der Loop weiter – jede Runde wird ein Take. Play außerhalb des Loops springt an den Loop-Anfang.
- Raster (1 Takt … 1/32 oder aus), Tempo (Doppelklick auf BPM), Taktart, Metronom (**Click**).

## Import

**Import** (**Ctrl+I**) oder Dateien per Drag & Drop auf die Timeline ziehen. Mehrere Dateien landen auf neuen Spuren.

- Direkt, ohne Zusatzsoftware: WAV, AIFF, FLAC, Ogg Vorbis, MP3 und unter Windows zusätzlich WMA/Windows-Media-Formate.
- **Mit FFmpeg**: AAC/M4A, ALAC, Opus, AC3, APE, WavPack usw. und **Audio aus Videos** (MP4, MOV, MKV, AVI, WebM, WMV, FLV, MPG …). Die Tonspur wird als WAV in den Projektordner (`Audio/`) extrahiert.

FFmpeg wird nicht mitgeliefert. Ton of Able findet `ffmpeg.exe` neben `Ton of Able.exe`, in einem Unterordner `ffmpeg\` bzw. `ffmpeg\bin\`, im `PATH` oder über **Projekt → FFmpeg auswählen …**.

## Export

**Export-Bereich:** Der Balken **EXPORT-BEREICH** unter den Spuren legt fest, was exportiert wird – für Mix, Stems und einzelne Spuren. Ohne eigene Einstellung (blass, „Ganzer Song“) ist es das ganze Arrangement. Anfang und Ende ziehen (Mauszeiger ↔), die Mitte verschiebt den Bereich, außerhalb des Balkens ziehen legt einen neuen an; alles rastet am Raster ein (Alt: frei). Doppelklick = wieder ganzer Song. Der Bereich wird mit dem Projekt gespeichert und ist unabhängig vom Loop.

**Export** bietet (das Menü zeigt oben den aktuellen Bereich):

- **Mix**
- **Alle Spuren einzeln (Stems)** in einen Ordner. Stems enthalten Lautstärke und Panorama der Spur und ignorieren Mute/Solo. Return-Spuren erscheinen als eigene Stems.
- **Ausgewählte Spur** (auch per Rechtsklick auf den Spurkopf)

Formate: WAV 16/24/32-Bit-Float, AIFF 24, FLAC 24, Ogg Vorbis sowie MP3 320 kbps, AAC/M4A 256 kbps und Opus 192 kbps über FFmpeg.
Ein Export ist erst möglich, wenn alle Rubber-Band-Berechnungen fertig sind.
Die Effektketten der Spuren (Insert) und der Return-Spuren werden mitgerendert. Solange der Export läuft, spielen diese Spuren live ohne Effekte.

Während des Exports ersetzt ein Fortschrittsbalken die Buttons Import/Export (bei MP3, AAC und Opus folgt „Konvertiere …“ mit FFmpeg). **Abbrechen** stoppt den Export und löscht die angefangenen Dateien.

## Projekte

- Ein Projekt ist ein Ordner mit `Name.tonproj` (XML) und dem Unterordner `Audio/` für Aufnahmen und extrahierte Audiodateien. Importierte Dateien außerhalb des Projekts werden referenziert.
- **Projekt → Speichern / Speichern unter / Öffnen / Neues Projekt.** Bei „Speichern unter“ werden die Dateien aus `Audio/` mitkopiert.
- Neue Projekte liegen unter `Dokumente\Ton of Able\Projekte\`. Beim Beenden und beim Projektwechsel wird automatisch gespeichert. Das zuletzt geöffnete Projekt wird beim Start wieder geladen.
- `Ton of Able.exe Pfad\zum\Projekt.tonproj` öffnet ein Projekt direkt, z. B. per Dateizuordnung.
- Ändert sich die Samplerate des Audiogeräts, bleiben die musikalischen Positionen erhalten und das Audio wird neu geladen.

## Akkorde (Lead-Sheet)

Die Leiste **AKKORDE** über dem Lineal hält die Akkordfolge des Songs fest – pro Takt ein Akkord oder pro halbem Takt zwei.

- **Takt anklicken und tippen**, z. B. `Am7`, `G/H`, `F#m7b5`, `Dsus4`, `N.C.`. **Enter** übernimmt, **Tab** übernimmt und springt in den nächsten Takt (so tippt man eine ganze Folge durch), **Esc** bricht ab, ein leeres Feld löscht den Akkord.
- **Zwei Akkorde in einem Takt:** einfach mit Leerzeichen tippen (`Am G`) – der Takt wird in zwei halbe Takte geteilt. **Shift+Klick** bearbeitet gezielt eine Takthälfte; in einem geteilten Takt bearbeitet ein Klick die angeklickte Hälfte.
- **Rechtsklick:** *Ganzer Takt (ein Akkord)*, *Halbe Takte (zwei Akkorde)*, *Akkord löschen*, *Alle Akkorde löschen*.
- Grundton und Bass werden großgeschrieben (`am` → `Am`, `c/e` → `C/E`); H ist der deutsche Name für B, beide werden akzeptiert. Nicht erkannte Namen erscheinen rot.
- Akkorde hängen an Takten, nicht an Sekunden: Bei Tempowechseln bleiben sie auf ihrem Takt. Sie werden mit dem Projekt gespeichert, jede Änderung lässt sich rückgängig machen.

## Akkordfolgen-Editor, Bibliothek und Auto-Modus

Rechtsklick in die Akkordleiste → **Akkordfolgen-Editor & Bibliothek …** (oder Klick auf „AKKORDE · Editor …“ links). Der Editor arbeitet mit **Abschnitten**: eine Folge über einen Taktbereich, die dort wiederholt wird – z. B. „Strophe“ Takt 1–8 und „Refrain“ Takt 9–16. So muss man nichts doppelt eintippen.

- **Abschnitte** (links): *+ Abschnitt*, *Duplizieren* (gleiche Folge direkt dahinter, z. B. die zweite Strophe), *Löschen*. Für den markierten Abschnitt: Name, *Ab Takt*, *Takte* und die **Folge** als Akkordnamen, z. B. `Am | F | C G/H | Dm7 G`. Takte werden mit `|` getrennt, zwei Akkorde in einem Takt sind halbe Takte, `%` wiederholt den Takt davor, `-` ist eine Pause. Unbekannte Akkordnamen werden sofort rot angezeigt.
- **Bibliothek** (rechts): **Grundton** und **Tonart** wählen – Ionisch (Dur), Dorisch, Phrygisch, Lydisch, Mixolydisch, Äolisch (Moll), Lokrisch, Harmonisch Moll, Melodisch Moll. Die Liste zeigt jede Folge als **Stufen** (z. B. `i | bVI | bIII | bVII`) und darunter als Akkorde in der gewählten Tonart (`Am | F | C | G`). Filter nach Länge (2, 4, 8, 12, 16 Takte) oder nur Kadenzen. Doppelklick oder *← In Abschnitt übernehmen* schreibt die Folge in den markierten Abschnitt.
- **Stufen-Schreibweise:** bezogen auf die Dur-Tonleiter wie in Pop und Jazz üblich – Großbuchstaben Dur, Kleinbuchstaben Moll, ° vermindert, ø halbvermindert, + übermäßig, b/# erniedrigt/erhöht. Also z. B. `bVII` in Mixolydisch und Moll, `II` in Lydisch, `bII` in Phrygisch, `V7` in harmonisch Moll, `i(maj7)` in melodisch Moll.
- **Auto-Modus** (unten): *Würfeln* wählt eine Tonart – zufällig (meist Dur oder Moll, seltener die anderen Modi; gitarrenfreundliche Grundtöne häufiger) oder die eingestellte, wenn „Tonart zufällig“ aus ist – und füllt die gewünschte Zahl Takte ab dem markierten Abschnitt mit Phrasen aus vier Takten: zwei Takte Eröffnung aus der Bibliothek plus zwei Takte Kadenz. Form A · A' · B · A'': Halbschluss in der Mitte, Trug- oder Ganzschluss in A', ein vollständiger Schluss auf der Tonika am Ende. Übrige Takte werden mit Tonika bzw. Schlusskadenz gefüllt. Gefällt das Ergebnis nicht: nochmal würfeln.
- **In Akkordleiste schreiben** überträgt alle Abschnitte (ein Schritt für Rückgängig). Die Abschnitte werden im Projekt gespeichert und stehen beim nächsten Öffnen wieder da. Verknüpfte Begleitspuren ziehen sofort mit.

**Bibliothek (Auswahl):**

| Tonart | Folgen (Stufen) | Kadenzen |
|---|---|---|
| Ionisch (Dur) | I–IV · I–V–vi–IV · I–vi–IV–V · vi–IV–I–V · I–vi–ii–V · IVmaj7–V7–iii7–vi7 · ii7–V7–Imaj7 · Kanon I–V–vi–iii–IV–I–IV–V · Rhythm Changes · Blues 12 Takte · 16-Takt-Song | V–I · ii–V–I · IV V–I · IV–I · Halbschluss ii–V / IV–V · Trugschluss V–vi |
| Dorisch | i7–IV7 · i7–ii7 · i–IV–i–IV · i–bVII–IV–i · i–bIII–IV–i · 8 und 16 Takte | IV–i · bVII–i · v–i |
| Phrygisch | i–bII · i–bII–bIII–bII · i–iv–bII–i · i–bvii–bVI–bII | bII–i · bvii–i · iv–i |
| Lydisch | I–II · Imaj7–II · I–II–iii–II · vi–vii–II–I · I–V–II–I | II–I · vii–I · V–I |
| Mixolydisch | I–bVII · I–bVII–IV–I · I–IV–bVII–IV · I–v–IV–I · I7–IV–bVII–I7 | bVII–I · bVII IV–I · v–I |
| Äolisch (Moll) | i–bVII · i–bVI–bIII–bVII · i–bVII–bVI–bVII · i–iv–bVII–bIII · i–iv–v–i · Moll-Quintfall 8 Takte · 16 Takte | bVII–i · bVI bVII–i · iv–i · iv V–i |
| Lokrisch | i°–bII · i°–bII–biii–bII · i°–bV–bII–i° | bII–i° · bvii–i° |
| Harmonisch Moll | i–V · i–iv–V–i · Andalusische Kadenz i–bVII–bVI–V · i–bVI–V–i · iiø7–V7–i · Flamenco 8 Takte | V7–i · iv V–i · iiø7 V7–i · Trugschluss V–bVI |
| Melodisch Moll | i(maj7)–IV7 · i–IV · i(maj7)–ii7–V7–i(maj7) · i–bIII+–IV–V | V7–i · IV7 V7–i · ii7 V7–i |

## Begleitung aus Akkorden

Rechtsklick in die Akkordleiste → **Begleitung aus Akkorden erzeugen …** macht aus den Akkorden eine sphärische Synth-Begleitung. Jede gewählte Ebene wird eine eigene MIDI-Spur:

| Ebene | Was sie spielt | Klang |
|---|---|---|
| **Pad** | volle Akkorde in offener Lage, Stimmen bewegen sich so wenig wie möglich, gemeinsame Töne klingen über den Akkordwechsel weiter | Synth „Atmo“ (breiter Stereo-Chorus, langsam atmender Filter) bzw. „Swell“ |
| **Sub-Bass** | Grundton, bei Slash-Akkorden der Basston (`G/H` → H) | Synth „Sub“ |
| **Shimmer** | die Akkordtöne hoch oben auf und ab in Vierteln, Achteln oder Sechzehnteln, jede Note klingt nach | Synth „Glass“ |

- **Lage** (tief/mittel/hoch), **Farben** (wie notiert · dezent mit add9 · reich mit 7, 9, 11, 13) und der Shimmer-Rhythmus gelten für alle Ebenen. **Swell** lässt das Pad bei jedem Akkord langsam anschwellen.
- **Raum:** Die Ebenen gehen per Send in eine Return-Spur **„Atmo-Hall“** – mit Valhalla Supermassive, wenn installiert, sonst Dragonfly Hall, sonst der eingebaute Hall (jeweils 100 % nass). Weitere Begleitungen nutzen dieselbe Return-Spur.
- **Verknüpft:** Die Spuren tragen im Kopf das grüne Schild **AKKORDE**. Solange es da ist, folgen sie jeder Änderung der Akkordleiste (auch einer neuen Taktart): neuer Akkord, halbe Takte, gelöschter Akkord – die Noten werden sofort neu gesetzt. Klick auf das Schild ändert den Stil der Spur.
- **Von Hand bearbeiten:** Wer Noten in der Piano-Roll ändert oder die Clips verschiebt/kürzt, löst die Verknüpfung automatisch – die eigenen Änderungen bleiben dann erhalten, das Schild verschwindet. Rechtsklick auf den Spurkopf: *Wieder aus den Akkorden erzeugen* (überschreibt die Änderungen) bzw. *Von den Akkorden lösen*.
- Takte ohne Akkord, `N.C.` und rot markierte (unbekannte) Akkorde sind Pausen; zwischen solchen Lücken entstehen getrennte Clips. Statt des eingebauten Synths kann jede Spur über den Instrument-Knopf auch ein VST3-Instrument spielen – die Noten bleiben verknüpft.
- Erzeugen, Ändern und jedes Nachziehen nach einer Akkordänderung sind ein gemeinsamer Schritt für Rückgängig (Strg+Z).

## MIDI-Spuren und Piano-Roll

**+ Spur → MIDI-Spur** legt eine Instrumentspur an. Ihr Klang kommt vom Knopf im Spurkopf: der **eingebaute Synth** (Pad, Keys, Bass, Lead – funktioniert ohne Plugins) oder ein **VST3-Instrument** wie Vital oder Surge XT (dort auch „Plugin-Fenster öffnen“). VST3-Instrumente empfangen die Noten auf MIDI-Kanal 1.

- **MIDI-Clip anlegen:** Doppelklick in die leere Spur (4 Takte ab diesem Takt) oder Rechtsklick → *MIDI-Clip einfügen*. Clips lassen sich wie Audio verschieben, kürzen, teilen, kopieren und einfrieren; sie folgen dem Tempo.
- **Piano-Roll** (Doppelklick auf den Clip): links die Tastatur (Klick spielt den Ton), in der Mitte die Noten, unten der Anschlag.
  - Klick ins Leere zeichnet eine Note, Ziehen bestimmt ihre Länge. Die nächste Note bekommt die zuletzt verwendete Länge.
  - Note ziehen verschiebt (Zeit und Tonhöhe), ihr rechter Rand ändert die Länge; mehrere ausgewählte Noten bewegen sich gemeinsam.
  - Shift+Ziehen wählt mit einem Rahmen aus, Shift+Klick ergänzt die Auswahl, Strg+A wählt alles.
  - Doppelklick, Rechtsklick oder Entf löscht. ↑/↓ transponiert (mit Shift um eine Oktave), ←/→ verschiebt um ein Raster, Strg+D dupliziert, **Q** quantisiert die Auswahl (ohne Auswahl alles).
  - Im Anschlag-Streifen ziehen ändert die Lautstärke der Note (bzw. der ausgewählten Noten).
  - Raster 1/4 bis 1/32 und Triolen; Alt schaltet das Raster beim Zeichnen und Ziehen aus. Strg+Mausrad zoomt, Shift+Mausrad scrollt waagerecht.
  - Noten außerhalb des Clips (grau hinterlegt) werden nicht gespielt. Jede Bearbeitung ist ein Schritt für Rückgängig (Strg+Z).
- **Live spielen und aufnehmen:** Eine scharf geschaltete MIDI-Spur wird von den MIDI-Keyboards (und virtuellen MIDI-Ports) gespielt – das Live-Rig bekommt die Keyboards dann nicht. Mit **Aufnahme** werden die gespielten Noten zu einem MIDI-Clip (auf ganze Takte gerundet). Im Loop entsteht ein Clip über den Loop, in dem alle Runden zusammenkommen.
- Wiedergabe-Stopp, Sprünge und Loop-Wiederholungen beenden alle Noten, es bleibt nichts hängen.

## Drumcomputer

**+ Spur → Drum-Spur (Drumcomputer)** legt eine Drum-Spur an und öffnet den Step-Sequencer (später über den Button **Drumcomputer** im Spurkopf oder per Doppelklick auf einen Pattern-Clip).

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
- **Modus** wählbar wie bei jedem Intervall: **Niedrige Latenz (R2)**, **Hohe Qualität (R3)** und, nur bei genau −12, zusätzlich **Analog-Oktaver (Flip-Flop, 0 ms)**. Steht der Analog-Oktaver auf einem anderen Intervall, läuft dort Rubber Band mit niedriger Latenz, und die Infozeile sagt es.
  - Analog-Oktaver: Nachbau klassischer Oktavpedale (Flip-Flop-Teiler). Das Signal wird auf den Grundton gefiltert, ein Schmitt-Trigger macht daraus ein Rechteck, ein Flip-Flop teilt es durch zwei. Das Ergebnis folgt der Anschlagsdynamik und wird über ein Tonfilter abgerundet.
  - Grundton-Erkennung: Fünf parallele Tiefpässe im Oktavabstand laufen gleichzeitig. Der Trigger hört auf das tiefste Band, das noch Signal führt. So wird vom tiefen E bis zum 22. Bund der hohen E-Saite sauber geteilt, ohne dass Obertöne doppelt schalten.
  - Es wird nichts gepuffert, also keine Latenz außer der Phasenverschiebung der Filter (1–2 ms), und es gibt keinen Latenzausgleich.
  - Wie beim Pedal einstimmig: Einzelnoten und Riffs funktionieren, Akkorde zerfallen. Der Klang ist synthartiger als Rubber Band, der Mix-Regler mischt das Original dazu.
- **Niedrige Latenz** (R2-Echtzeit-Engine, kurzes Fenster): gemessen eine Oktave tiefer bei 48 kHz etwa **28 ms** mit Interface-Puffern von 32/64 Samples, **37 ms** bei 128 und **43–46 ms** bei 256–1024 Samples.
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

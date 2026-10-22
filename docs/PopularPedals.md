# Zehn NAM-Verzerrer-Pakete (persoenliche Installation)

Stand: 15.09.2026. 10 Pakete mit zusammen 267 unveraenderten A1-NAM-Dateien.
Dies ist eine kuratierte Auswahl bekannter, haeufig genutzter Pedale, **keine belegte globale Top-10-Rangliste**.
Ausgangspunkt: [TONE3000-Pedalauswahl](https://www.tone3000.com/blog/best-distortion-pedals-nam-captures), ergaenzt um Timmy und King of Tone. Rockman wurde nicht als einfacher vorgeschalteter Verzerrer uebernommen.

## Benutzung

Programm neu starten. Im Bereich **NAM PEDAL > NAM AMP** erscheinen zehn Eintraege mit `Popular -` am Anfang. Modell auswaehlen und bei Bedarf einschalten. Der Download schaltet keine Pedale oder weiteren Effekte automatisch ein und aendert keine bestehenden Sounds/Rigs.

Je eine Startvariante liegt in:
- `D:\Amp Modeler\resources\pedal-models`
- `D:\Amp Modeler\install\share\NamTestProgram\resources\pedal-models`

Alle weiteren Gain-/Tone-Stufen und die Original-ZIPs liegen in `D:\Amp Modeler\downloads\popular-pedals`, jeweils nach Paket getrennt. Weitere Varianten ueber den bestehenden Pedal-Import laden. Die urspruenglichen Gilmour-Pedale bleiben erhalten.

## Auswahl und Quellen

Downloadzahlen sind Momentaufnahmen der jeweiligen Paket-Seite, gerundete Anzeigen bleiben Naeherungswerte. Sie beweisen keine Klangqualitaet. Die Startvarianten sind unsere Auswahl, keine vom Autor zugesicherten optimalen Einstellungen.

| Paket | Autor | Downloads ca. | NAMs | Startvariante |
| --- | --- | ---: | ---: | --- |
| [Ibanez TS9](https://www.tone3000.com/tones/ibanez-ts9-tube-screamer-36827) | StudioAmpCaptures | 34.130 | 9 | Drive 8, Tone 4, Level 5 |
| [Klon Centaur Silver](https://www.tone3000.com/tones/klon-centaur-silver-2599) | seanashe | 8.686 | 1 | Low Gain / Clean Boost, nicht starke Eigenverzerrung |
| [EHX Op-Amp Big Muff](https://www.tone3000.com/tones/electro-harmonix-op-amp-big-muff-37818) | OutmodedElectronics | 7.788 | 70 | Volume 6, Tone 5, Sustain 5; Standard statt Feather |
| [Dunlop Hendrix Fuzz Face](https://www.tone3000.com/tones/dunlop-jimi-hendrix-fuzz-face-distortion-1531) | matty | 4.262 | 1 | Original-Capture, Einstellungen nicht verifiziert |
| [Boss DS-1](https://www.tone3000.com/tones/boss-ds-1-distortion-38560) | reimenseimen | 3.556 | 12 | Tone 5, Volume 10, Gain 4 |
| [ProCo RAT](https://www.tone3000.com/tones/proco-rat-36551) | romanrecords | 3.365 | 49 | Distortion 4, Filter 4 |
| [Boss BD-2 Pack](https://www.tone3000.com/tones/boss-blues-driver-bd-2-pack-5054) | kosmicaudio | 1.631 | 66 | Tone/Gain 12 Uhr, Standard; modifiziertes Pedal fuer waermeren Klang |
| [MXR Timmy](https://www.tone3000.com/tones/mxr-timmy-30891) | OutmodedElectronics | 1.621 | 36 | Gain 12 Uhr, Clipping Center, Volume 1 Uhr, Bass 3 Uhr, Treble Max |
| [King of Tone v4](https://www.tone3000.com/tones/king-of-tone-ver-4-pedal-both-sides-on-29600) | mikefont5150 | 979 | 1 | Beide Seiten an, hohe Volume-Einstellung im Capture |
| [Fulltone OCD v1.5](https://www.tone3000.com/tones/fulltone-ocd-v15-pack-5197) | kosmicaudio | 954 | 22 | LP, Drive/Tone 12 Uhr, Volume 5 Uhr |

## Pruefung und Grenzen

- Alle 267 Dateien als NAM-JSON gelesen; Architektur und nichtleere endliche Gewichte geprueft. Keine Modellbytes veraendert.
- Zehn Dropdown-Dateien sind bytegleiche Kopien der dokumentierten Originale, nur mit besser lesbaren Dateinamen.
- Alle zehn Startvarianten im installierten NAM-Pedal-Prozessor bei 48 kHz geladen und verarbeitet: endlicher, nichtstummer, veraenderter Ausgang; Bypass, Format-/Blockgroessenwechsel und Fehler-Rueckfall geprueft. Kein subjektiver Hoervergleich.
- Acht Varianten: 19/19 Checks erfolgreich.
- Klon und Fuzz Face: 18/19 erfolgreich. Der verbleibende Check setzt explizite 48-kHz-Metadaten voraus und erwartet eine Ablehnung bei 96 kHz. Diese beiden Legacy-Dateien haben keine Sampleraten-Angabe; deshalb greift die Ablehnung nicht. Kein Lade-/Audiotestfehler bei 48 kHz, aber die urspruengliche Capture-Rate bleibt unbekannt. Die Originaldateien wurden nicht nachtraeglich mit einer erfundenen Rate versehen.
- Pakete mit bekannter Rate geben 48 kHz an. Pegel und Kalibrierung unterscheiden sich; beim Einschalten vorsichtig mit Pedal Output beginnen, insbesondere King of Tone. Keine automatische Normalisierung.

Maschinenlesbares Inventar mit Quellen, Originalnamen und SHA-256 je Datei: `D:\Amp Modeler\downloads\popular-pedals\manifest.json`.
Einzelne Laufzeitberichte: Unterordner `checks` daneben.

## Lizenz

Die Paket-Seiten nennen T3K: persoenliches Herunterladen und Verwendung in Software sowie Veroeffentlichung resultierender Audios sind gestattet; Weiterverteilung der Modelldateien braucht die Erlaubnis des Autors. Diese lokale Zusammenstellung ist nur fuer diese persoenliche Installation bestimmt, nicht fuer einen oeffentlich verteilten Installer. [TONE3000-Richtlinie](https://www.tone3000.com/guides/tone-sharing-guidelines).

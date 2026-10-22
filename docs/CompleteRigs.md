# Komplette Rigs

Oben die Rig-Vorlage auswählen und **Rig laden** drücken. Die Auswahl allein verändert den laufenden Sound nicht. Laden aktiviert die zusammengehörigen NAM-/Cabinet-Stufen, setzt Amp-EQ und Pedal-Trims neutral und schaltet alle zusätzlichen internen Effekte sowie beide VST3-Slots aus. Master-Input und -Output bleiben erhalten.

**Details** zeigt die Bestandteile und Capture-Einschränkungen. Die rechte Seite ist als **Optionale Effekte** gekennzeichnet.

Keine automatischen Delay-/Hall-Startwerte. Bereits im Capture enthaltene Effekte bleiben natürlich enthalten; der Katalog kennzeichnet sie, statt einen zweiten Effekt einzuschalten. Optionale Effekte können nach dem Laden wieder manuell aktiviert und mit dem Sound gespeichert werden.

## Enthaltene Gilmour-inspirierte Kombinationen

- **Big Muff + Hiwatt**: die beiden Original-Captures aus pinkflors GILMOUR TONE-Paket.
- **Tube Driver + Hiwatt**: der alternative Butler Tube Driver Boost mit demselben Original-Hiwatt.

Alle drei Originaldateien bleiben unverändert erhalten und gemeinsam zugeordnet. Es handelt sich um von uns zusammengestellte Abhörvarianten aus einer Originalsammlung, nicht um nachgewiesene albumgetreue Gilmour-Presets oder eine einzige gemeinsame Aufnahme. Die Quelle nennt keine Reihenfolge für beide Pedale zusammen; deshalb werden nicht willkürlich beide gestapelt.

**Cabinet-Einschränkung:** Der Hiwatt-Capture enthält keine Angaben dazu, ob eine Box mit aufgenommen wurde. Die Varianten ergänzen zunächst die bereits vorhandene Jester-Emerald-IR. Diese Ergänzung ist keine Original-Gilmour-Box; mit dem **IR**-Schalter kann sie abgeschaltet werden. Die Beschreibung der Rig-Auswahl (Tooltip) weist auf diese Unklarheit hin. Technischer Test bei 48 kHz; die Originaldateien enthalten selbst keine Sampleratenangabe.

## Routing-Regeln

- `pedal-amp-cab`: separates Pedal-NAM → Amp-NAM → IR.
- `amp-cab`: gemeinsamer Pedal/Amp-Capture oder Amp-Capture → IR; separater Pedal-Slot aus.
- `full-rig`: ein Capture inklusive Cabinet; zusätzlicher Pedal-Slot und IR aus.

Der Katalog `resources/rig-models/catalog.json` ordnet Dateien und Quelle zu. Unzulässige Doppel-Cabinet-Zuordnungen und fehlende Dateien werden abgelehnt. Die zuletzt bearbeitete Rig-Einstellung wird beim Beenden gespeichert; Sound-Presets und A/B behalten ebenfalls die Rig-Zuordnung.

Quellen und Datei-Hashes: [PedalModels.md](PedalModels.md). Herstellerbeleg zur Auswahl der Pedaltypen: [Pete Cornish – Gilmours 1994er Rig](https://petecornish.co.uk/pfrig.html), das unter anderem Tube Driver und Sovtek Big Muff aufführt. Das belegt die Pedalrichtung, nicht die Authentizität dieser Community-Captures.

Technisch geprüft: 37 Rig-Zuordnungsprüfungen, 18 Audio-Kettenprüfungen und 34 allgemeine Testabschnitte ohne Fehler. UI-Prüfung bestätigt ausgeschaltete Zusatz-Effekte nach dem Laden. Berichte: `docs/rig-checks.xml`, `docs/rig-audio-checks.xml` und `docs/complete-rig-preview/rig-ui-check.xml`. Kein subjektiver Hörtest durchgeführt.

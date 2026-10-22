# NAM-Pedalmodelle vor dem Amp

Oben **Pedals / VST3** öffnen. Die zusätzliche Karte **NAM PEDAL > NAM AMP** lädt eine separate `.nam`-Datei für das Pedal. Das Amp-Modell und seine Regler bleiben unverändert.

1. **Pedal-NAM importieren**: eine Pedal-only-Aufnahme wählen, ohne Amp oder Lautsprecherbox. Import registriert den bestehenden Dateipfad; Datei danach nicht verschieben.
2. **ON** aktivieren. Neue Imports bleiben zunächst ausgeschaltet.
3. **PEDAL INPUT** verändert die Ansteuerung des Pedals. **PEDAL OUTPUT** verändert den Pegel, mit dem das Pedal den Amp anfährt. Beide Regler reichen von -24 bis +24 dB und werden geglättet.
4. Über das Dropdown zwischen importierten Pedal-Captures wechseln. **Entfernen** leert den Slot, ohne Dateien zu löschen.

Reihenfolge: Input → interner Drive → PRE-VST3 → NAM-Pedal → NAM-Amp → Cabinet/IR → interne Effekte → POST-VST3 → Output. Zum ersten Vergleich internen Drive und BYOD ausschalten, damit nicht mehrere Verzerrer gleichzeitig arbeiten. Eine Cabinet-IR gehört hinter den Amp, nicht zwischen Pedal und Amp.

Das NAM-Pedal ist eine Aufnahme einer bestimmten Einstellung. Input/Output verändern Pegel; sie simulieren nicht die ursprünglichen Tone-/Gain-Regler des echten Pedals. Für andere Hardware-Einstellungen andere Captures importieren. Der Dateiinhalt kann nicht zuverlässig allein am Dateinamen als Pedal oder Amp erkannt werden.

Die Audio-Samplerate muss zur im Pedalmodell angegebenen Rate passen (häufig 48 kHz). Abweichungen werden mit Dry-Fallback gemeldet, nicht mit falscher Geschwindigkeit abgespielt. Es wird in diesem Slot kein automatisches Resampling vorgenommen. Nach Korrektur einer defekten Datei erneut importieren; dadurch wird der Ladeversuch wiederholt.

Pedal-Dateipfad, Bypass und beide Pegel werden mit Sound-Presets und A/B gespeichert. Die Importliste und letzte Pedal-Einstellung werden beim Beenden gespeichert. Alte Presets ohne Pedalfelder laden einen ausgeschalteten leeren Slot. Modellvorbereitung erfolgt im Hintergrund; fehlende oder ungültige Dateien dürfen kein altes Pedal unbemerkt weiter aktivieren.

Inzwischen sind Big Muff und Butler Tube Driver aus dem ursprünglichen GILMOUR TONE-Paket mitgeliefert. Der zugehörige Hiwatt bleibt gemeinsam mit ihnen über die vollständige Rig-Auswahl verfügbar; siehe `docs/CompleteRigs.md`. Die zuvor importierten Dumble-Captures sind Amp-Modelle, keine Pedal-Captures.

## Technischer Teststand

34 allgemeine Testabschnitte und 19 Pedal-Audioprüfungen bestanden, auch mit der installierten EXE. Bericht: `docs/nam-pedal-installed-checks.xml`. Für den technischen Loader-/Bypass-Test wurde der vorhandene 48-kHz-Dumble-Capture verwendet; das ist kein Pedal-Klangtest und registriert diesen Capture nicht als Pedal. Geprüft wurden unter anderem wirksame Verarbeitung, kurze Blöcke, Bypass ohne Trims, fehlende Dateien, veraltete Ladeanforderungen und Sampleratenfehler mit anschließender Erholung.

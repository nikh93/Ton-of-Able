# Projekt: Standalone Neural Amp Modeler Testprogramm für Windows

Entwickle eine eigenständige Windows-Desktopanwendung zum Testen und Verwenden von Neural-Amp-Modeler-Modellen in Echtzeit.

Das Programm soll kein VST3-Plugin sein. Es soll eine eigenständige Anwendung für Windows 10 und Windows 11 werden.

## 1. Technischer Stack

Verwende:

- C++20
- CMake
- JUCE für GUI, Audio-I/O, Geräteverwaltung, DSP und WAV-Verarbeitung
- NeuralAmpModelerCore als offizielle NAM-DSP-Bibliothek
- möglichst wenige zusätzliche Dependencies

Bevor du APIs von NeuralAmpModelerCore verwendest, prüfe den aktuellen Stand des offiziellen Repositories und orientiere dich an der tatsächlich vorhandenen API.

Das Projekt soll modular aufgebaut werden und langfristig wartbar bleiben.

Vorgeschlagene Hauptmodule:

- `AudioEngine`
- `NamProcessor`
- `IRProcessor`
- `EQProcessor`
- `ReverbProcessor`
- `DelayProcessor`
- `ChorusProcessor`
- `AutoWahProcessor`
- `LibraryManager`
- `PresetManager`
- `FilePlayer`
- `Recorder`
- `PerformanceMonitor`
- `Logger`
- `MainComponent`
- spezialisierte GUI-Komponenten

GUI und DSP müssen sauber voneinander getrennt sein.

---

# 2. Audio-Grundarchitektur

Priorität hat eine möglichst niedrige und stabile Echtzeitlatenz.

Unter Windows:

1. ASIO bevorzugen
2. WASAPI als Fallback anbieten

Der Benutzer muss auswählen können:

- Audiointerface
- Eingangskanal
- Ausgangskanal
- Samplerate
- Buffergröße

Das Programm verarbeitet zunächst ausschließlich MONO.

Grundlegender Signalweg:

`Input Gain → NAM → IR → EQ → Auto-Wah → Chorus → Delay → Reverb → Output Gain`

Die Effektarchitektur soll modular aufgebaut werden, sodass die Reihenfolge später relativ einfach geändert oder erweitert werden kann.

Jeder Effekt benötigt einen eigenen Bypass.

Falls ein physisches Stereo-Ausgabegerät verwendet wird, darf das Monosignal identisch auf L und R ausgegeben werden.

---

# 3. Samplerate und Buffer

Samplerate muss auswählbar sein.

Unterstütze die vom jeweiligen Audiogerät angebotenen Sampleraten, insbesondere:

- 44,1 kHz
- 48 kHz
- 88,2 kHz
- 96 kHz

Bevorzugtes Default:

**48 kHz**

Buffergröße ebenfalls über die vom Treiber angebotenen Werte auswählbar machen.

Bevorzugtes Default:

**64 Samples**

Falls 64 Samples nicht verfügbar oder instabil sind, soll 128 Samples als sinnvoller nächster Wert empfohlen werden.

Keine festen Werte erzwingen, die das Interface nicht unterstützt.

---

# 4. Strikte Echtzeitanforderungen

Der Audio-Callback muss real-time-safe aufgebaut sein.

Im Audiothread insbesondere vermeiden:

- Speicherallokationen
- Dateizugriffe
- Modellladen
- IR-Laden
- GUI-Operationen
- Locks/Mutexes, soweit vermeidbar
- Logging auf Festplatte
- JSON-Verarbeitung
- blockierende Operationen

NAM-Modelle und IRs müssen außerhalb des Audiothreads geladen und vorbereitet werden.

Auch Effektparameter müssen so übertragen werden, dass Änderungen während des Spielens keine Locks oder unnötige Allokationen verursachen.

Parameteränderungen, die hörbare Sprünge verursachen könnten, sollen geglättet werden.

Ziel:

- möglichst niedrige Latenz
- keine Dropouts
- keine Knackser
- deterministische Blockverarbeitung

---

# 5. NAM-Bibliothek

Der Benutzer kann beliebige `.nam`-Dateien importieren.

Beim Import wird die Datei NICHT kopiert.

Gespeichert werden:

- Dateipfad
- Anzeigename
- Metadaten
- Tags/Kategorien

Die Bibliothek bleibt zwischen Programmstarts erhalten.

Fehlende Dateien:

- nicht abstürzen
- Eintrag als fehlend markieren
- Benutzer informieren
- neuen Dateipfad zuweisen können

NAMs müssen:

- importiert
- umbenannt
- gelöscht
- kategorisiert/getaggt

werden können.

Mögliche Tags:

- Clean
- Crunch
- Overdrive
- High Gain
- Bass
- Other

---

# 6. Default-NAMs

Das Programm soll einige populäre und qualitativ gute kostenlose NAM-Modelle bereits anbieten.

Recherchiere dafür geeignete aktuelle Modelle und integriere eine sinnvolle Auswahl als Defaults.

Dokumentiere:

- Modellname
- Quelle
- Ersteller
- kurze Beschreibung
- ungefähre Klangkategorie

Die Default-Auswahl soll möglichst verschiedene Klangbereiche abdecken, beispielsweise:

- Clean
- Crunch
- Classic Rock
- High Gain

---

# 7. Impulse Responses

Implementiere eine IR/Cabinet-Sektion.

Unterstütze WAV-Impulse-Responses.

Eigene IRs können importiert werden.

Auch diese werden nur VERLINKT.

Bibliotheksfunktionen:

- Import
- Auswahl
- Umbenennen
- Löschen
- Tags
- persistente Dateipfade
- fehlende Dateien behandeln

Mögliche Kategorien:

- 1x12
- 2x12
- 4x12
- Open Back
- Closed Back
- Other

IR-Verarbeitung muss abschaltbar sein.

Recherchiere drei hochwertige kostenlose IRs als Defaults.

Die drei IRs sollten klanglich sinnvoll unterschiedlich sein und beispielsweise verschiedene Cabinet-Typen repräsentieren.

Dokumentiere:

- Name
- Quelle
- Cabinet-Typ
- kurze Klangbeschreibung

---

# 8. 3-Band-EQ

Implementiere:

- Bass
- Mid
- Treble

Keine unnötig komplexe parametrische Oberfläche.

Der EQ soll musikalisch sinnvoll reagieren.

---

# 9. Auto-Wah

Implementiere ein echtes envelope-gesteuertes Auto-Wah.

Es soll auf die Anschlagstärke bzw. Hüllkurve des Eingangssignals reagieren.

Übliche Parameter sollen einstellbar sein.

Mindestens:

- Bypass
- Sensitivity
- Attack
- Release/Decay
- Minimum Frequency
- Maximum Frequency / Range
- Resonance / Q
- Mix

Falls sinnvoll zusätzlich:

- Direction: Up / Down

Die Parameter sollen musikalisch sinnvolle Grenzen besitzen.

Der Envelope-Follower muss für Echtzeitaudio optimiert sein.

---

# 10. Chorus

Implementiere einen klassischen Chorus-Effekt.

Einstellbare Parameter mindestens:

- Bypass
- Rate
- Depth
- Delay / Center Delay
- Feedback, falls für die Implementierung sinnvoll
- Mix

Da das Projekt intern zunächst Mono arbeitet, soll der Chorus auch in Mono sinnvoll funktionieren.

Die Architektur soll eine spätere Stereo-Erweiterung nicht unnötig erschweren.

---

# 11. Delay

Implementiere einen klassischen Delay-Effekt.

Parameter mindestens:

- Bypass
- Delay Time
- Feedback
- Mix

Zusätzlich sinnvoll:

- Low Cut oder Tone
- High Cut oder Damping

Delay-Zeit zunächst in Millisekunden einstellbar.

Architektur so vorbereiten, dass später optional Tempo-Sync ergänzt werden könnte, ohne ihn jetzt implementieren zu müssen.

Änderungen der Delay-Zeit sollen möglichst keine harten Klicks verursachen.

---

# 12. Reverb

Implementiere einen hochwertigen, aber CPU-effizienten Reverb.

Geeignete Algorithmen aus JUCE dürfen verwendet werden, sofern Qualität und Echtzeitverhalten ausreichend sind.

Parameter mindestens:

- Bypass
- Size
- Decay
- Damping
- Pre-Delay
- Mix

Optional sinnvoll:

- Low Cut
- High Cut

Der Reverb darf die niedrige Gesamtlatenz nicht unnötig verschlechtern.

---

# 13. Effekt-Bypass

Direkt zugänglich:

- NAM Bypass
- IR Bypass
- Auto-Wah Bypass
- Chorus Bypass
- Delay Bypass
- Reverb Bypass

Bypass-Operationen sollen möglichst keine Knackser erzeugen.

Geeignete kurze Crossfades oder Parameter-Ramping verwenden, wenn erforderlich.

---

# 14. Hauptregler

Direkt zugänglich:

- Input Gain
- Output Gain
- Bass
- Mid
- Treble

Die wichtigsten Parameter der Effekte sollen schnell erreichbar sein.

Erweiterte Parameter können innerhalb des jeweiligen Effektpanels untergebracht werden.

---

# 15. Pegelanzeigen

Deutlich sichtbare Echtzeit-Pegelmeter:

- Input
- Output

Mindestens:

- aktueller Pegel
- Peak
- Clipping-Anzeige

GUI niemals direkt aus dem Audiothread aktualisieren.

---

# 16. Gesamtlatenz

Zeige eine verständliche geschätzte Gesamtlatenz:

`Estimated Latency: 4.2 ms`

Berücksichtige soweit zuverlässig möglich:

- Input-Latenz
- Buffer
- DSP
- Output-Latenz

Keine pseudogenauen Werte erfinden.

---

# 17. Performance-Monitoring

Intern erfassen:

- Audioblock-Verarbeitungszeit
- Average Processing Time
- Peak Processing Time
- Realtime Load
- Dropouts/Xruns soweit erfassbar

Keinen großen permanenten Diagnosebereich anzeigen.

Technische Werte nur dezent oder in einem optionalen Fenster.

---

# 18. Presets

Ein Preset speichert die komplette Klangkonfiguration:

- NAM
- IR
- Input Gain
- Output Gain
- Bass
- Mid
- Treble
- NAM Bypass
- IR Bypass

sowie sämtliche Parameter und Bypass-Zustände von:

- Auto-Wah
- Chorus
- Delay
- Reverb

Presets müssen:

- erstellt
- geladen
- umbenannt
- gelöscht

werden können.

Persistente Speicherung zwischen Programmstarts.

---

# 19. A/B-Vergleich

Implementiere zwei vollständige Klangzustände:

- A
- B

Beide enthalten die komplette Klangkonfiguration inklusive:

- NAM
- IR
- EQ
- Gains
- Auto-Wah
- Chorus
- Delay
- Reverb
- sämtliche Effektparameter

Schnelles Umschalten ermöglichen.

Modelle dürfen dabei nicht unkontrolliert im Echtzeitthread geladen werden.

Falls notwendig, beide NAMs vorbereitet im Speicher halten.

---

# 20. WAV-Testplayer

Zusätzlich zum Live-Eingang:

`LIVE | WAV`

WAV-Dateien müssen:

- geladen
- abgespielt
- pausiert
- gestoppt
- geloopt

werden können.

Die WAV-Datei läuft durch exakt dieselbe DSP-Kette:

`WAV → Input Gain → NAM → IR → EQ → Auto-Wah → Chorus → Delay → Reverb → Output Gain`

Der Benutzer soll während eines Loops komfortabel:

- NAM
- IR
- Presets
- Effekte
- A/B

vergleichen können.

---

# 21. Recording / WAV Export

Das vollständig verarbeitete Ausgangssignal aufnehmen können.

Funktionen:

- Record Start
- Record Stop
- Zielpfad wählen
- WAV speichern

Aufgezeichnet wird der komplette Signalweg inklusive aller Effekte.

Keine synchronen Festplattenzugriffe im Audio-Callback.

Buffered/Threaded Writer verwenden.

---

# 22. Logging

Internes Logsystem implementieren.

Beispiele:

- Audio device opened
- ASIO activated
- WASAPI fallback activated
- Sample rate changed
- Buffer size changed
- NAM loaded
- IR loaded
- missing library file
- preset loaded
- recording started/stopped
- audio dropout
- initialization error

Kein synchrones Logging aus dem Audiothread.

Optionales kleines Logfenster.

---

# 23. GUI und Design

Die Anwendung soll wie eine hochwertige moderne Gitarren-Audioanwendung aussehen.

Designrichtung:

- warme Holztöne
- Bernstein
- dunkle warme Hintergründe
- dezente metallische oder glasartige Akzente
- klare moderne Typografie
- hochwertige Drehregler
- subtile Tiefenwirkung
- moderne Meter
- keine kitschige Vintage-Amp-Kopie
- keine übertriebene Skeuomorphie

Ziel:

`modernes Audio-Plugin + hochwertiges Gitarren-Equipment + warme Holz/Amber-Ästhetik`

UX ist genauso wichtig wie Optik.

---

# 24. GUI-Struktur

## Oberer Bereich

Kompakt:

- Audiointerface
- Input
- Output
- Sample Rate
- Buffer
- geschätzte Latenz

## Hauptbereich

Prominente Auswahl für:

- NAM
- IR

Zentrale Klangsektion:

- Input Gain
- Bass
- Mid
- Treble
- Output Gain

Input-/Output-Meter gut sichtbar.

## Effektsektion

Vier moderne Effektmodule:

### Auto-Wah
### Chorus
### Delay
### Reverb

Jedes Effektmodul besitzt:

- gut sichtbaren On/Off-Button
- wichtigste Parameter direkt
- klare visuelle Rückmeldung, ob aktiv
- optional aufklappbare zusätzliche Parameter

Die Oberfläche darf durch die zusätzlichen Effekte nicht überladen wirken.

Bevorzugt modulare Cards oder Panels verwenden.

## Preset-Bereich

- Preset-Auswahl
- Save
- Save As
- Delete
- A/B

## Testbereich

`LIVE | WAV`

Bei WAV:

- Open
- Play
- Pause
- Stop
- Loop
- Timeline

## Recording

Gut erreichbarer Record-Button.

Während Aufnahme klarer visueller Zustand.

## Status

Dezent:

- Samplerate
- Buffer
- Gesamtlatenz
- Realtime Load
- Dropouts

---

# 25. UX-Anforderungen

Sauber behandeln:

- Audiointerface entfernt
- vorheriges Interface nicht vorhanden
- NAM-Datei verschoben
- IR-Datei verschoben
- ungültige NAM-Datei
- ungültige WAV-Datei
- nicht unterstützte Samplerate
- nicht unterstützte Buffergröße
- Aufnahmeziel nicht beschreibbar
- Preset verweist auf fehlende Datei
- ASIO nicht verfügbar

Keine kryptischen Exceptions.

WASAPI bleibt als Fallback verfügbar.

---

# 26. Persistente Einstellungen

Zwischen Programmstarts speichern:

- Audiointerface
- Input/Output
- Samplerate
- Buffergröße
- NAM-Bibliothek
- IR-Bibliothek
- Tags
- Presets
- Effektparameter
- zuletzt verwendetes Preset
- Fenstergröße/Position
- sinnvolle UI-Einstellungen

---

# 27. Projektstruktur

Orientierung:

```text
src/
    audio/
        AudioEngine.*
        NamProcessor.*
        IRProcessor.*
        EQProcessor.*
        AutoWahProcessor.*
        ChorusProcessor.*
        DelayProcessor.*
        ReverbProcessor.*
        AudioMeter.*
        Recorder.*

    library/
        LibraryManager.*
        NamLibrary.*
        IRLibrary.*

    presets/
        PresetManager.*

    playback/
        FilePlayer.*

    diagnostics/
        PerformanceMonitor.*
        Logger.*

    ui/
        MainComponent.*
        NamPanel.*
        IRPanel.*
        EQPanel.*
        EffectsPanel.*
        AutoWahPanel.*
        ChorusPanel.*
        DelayPanel.*
        ReverbPanel.*
        MeterComponent.*
        PresetPanel.*
        AudioSettingsPanel.*
        TransportPanel.*
        Theme.*

    config/
        AppSettings.*

third_party/
    NeuralAmpModelerCore/

resources/

tests/
```

Die Struktur darf angepasst werden, wenn eine bessere Lösung begründet werden kann.

---

# 28. Entwicklungsvorgehen

Nicht alles gleichzeitig implementieren.

Das Projekt muss nach jeder Phase buildbar bleiben.

## Phase 1
Projektgrundgerüst:

- CMake
- JUCE
- NeuralAmpModelerCore
- Windows-Build
- grundlegende moderne GUI

## Phase 2
Audio-I/O:

- ASIO
- WASAPI
- Geräteauswahl
- Kanäle
- Samplerate
- Buffer
- Mono passthrough

## Phase 3
NAM:

- NAM laden
- vorbereiten
- Echtzeitverarbeitung
- Bypass
- Input/Output Gain

## Phase 4
IR und EQ.

## Phase 5
Effektarchitektur erstellen und anschließend implementieren:

1. Auto-Wah
2. Chorus
3. Delay
4. Reverb

Nach jedem Effekt:

- Build
- Audiofunktion prüfen
- CPU-Last prüfen
- Bypass prüfen
- Parameter-Ramping prüfen

## Phase 6
NAM-/IR-Libraries.

## Phase 7
Presets und A/B.

## Phase 8
WAV Test Player und Loop.

## Phase 9
Recording / WAV Export.

## Phase 10
Metering, Latenzanzeige, Performanceüberwachung und Logging.

## Phase 11
GUI/UX vollständig ausarbeiten und visuell polieren.

## Phase 12
Recherche und Integration geeigneter Default-NAMs und Default-IRs.

---

# 29. Qualitätssicherung

Nach jeder Entwicklungsphase:

1. Projekt kompilieren.
2. Compilerfehler beheben.
3. Anwendung starten, soweit möglich.
4. Regressionen prüfen.
5. bestehende Funktionen nicht unnötig verändern.
6. Änderungen klein und nachvollziehbar halten.

Tests insbesondere für:

- Preset Serialization
- Library Serialization
- fehlende Dateipfade
- Audio-State-Transitions
- Parametergrenzen
- WAV Player
- Recorder
- Delay Buffer
- Envelope Follower des Auto-Wah
- Bypass-Verhalten
- Parameter-Smoothing

---

# 30. Wichtige Designregel

Funktionalität und Echtzeitstabilität haben Vorrang vor visueller Komplexität.

Die finale Anwendung darf trotzdem nicht wie ein Entwickler-Prototyp aussehen.

Der normale Workflow:

1. Interface auswählen
2. NAM auswählen
3. IR auswählen
4. Grundsound mit EQ einstellen
5. gewünschte Effekte einschalten
6. spielen

Effekte sollen leicht verständlich sein, ohne den Benutzer mit technischen DSP-Details zu erschlagen.

---

# 31. Erstes Vorgehen

Beginne NICHT sofort damit, hunderte Dateien zu erzeugen.

Zuerst:

1. Aktuelles NeuralAmpModelerCore-Repository und API prüfen.
2. Aktuelle JUCE-Integration für ASIO und WASAPI prüfen.
3. interne Audioarchitektur planen.
4. Thread-Grenzen definieren.
5. festlegen, welche Objekte im Audiothread verwendet werden dürfen.
6. DSP-Chain modular planen.
7. State-Verwaltung für Presets und A/B planen.
8. Parameterverwaltung und Parameter-Smoothing festlegen.
9. kurze Architekturübersicht erstellen.
10. danach Phase 1 beginnen.

Treffe bei kleineren Implementierungsdetails selbst vernünftige Entscheidungen.

Vermeide unnötiges Overengineering.

Dokumentiere wichtige Architekturentscheidungen kurz im Repository.

Das langfristige Ziel ist eine elegante, sehr latenzarme und stabile Windows-Anwendung zum Spielen, Testen und Vergleichen von Neural-Amp-Modeler-Modellen inklusive praxisgerechter Gitarreneffekte.
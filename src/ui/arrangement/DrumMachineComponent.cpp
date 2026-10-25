#include "DrumMachineComponent.h"

#include <cmath>

#include "daw/DawText.h"
#include "daw/SampleKit.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
namespace
{
using Theme = AmberTheme;
using daw::text;
constexpr int builtInId = 1;
constexpr int rescanId = 2;
constexpr int firstKitId = 10;
constexpr int firstPluginId = 100;
constexpr int laneWidth = 120;
} // namespace

DrumMachineComponent::DrumMachineComponent(daw::ArrangementController& c, const juce::Uuid& id, int patternIndex, Host h)
    : controller(c), trackId(id), host(std::move(h)), selected(juce::jlimit(0, 7, patternIndex))
{
    title.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, Theme::text());
    info.setColour(juce::Label::textColourId, Theme::subduedText());
    info.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(title);
    addAndMakeVisible(info);

    source.setTooltip(text("Klangquelle: eingebautes Kit oder ein VST3-Instrument (General-MIDI-Drums, z. B. JDrummer)"));
    source.onChange = [this]
    {
        const auto choice = source.getSelectedId();
        juce::Result result = juce::Result::ok();
        if (choice == rescanId)
        {
            info.setText(text("Suche VST3-Instrumente …"), juce::dontSendNotification);
            if (host.rescan)
                host.rescan([safe = juce::Component::SafePointer<DrumMachineComponent>(this)]
                {
                    if (safe != nullptr)
                        safe->refreshSources();
                });
            refreshSources();
            return;
        }
        if (! host.setSource)
            return;
        if (choice == builtInId)
            result = host.setKit ? host.setKit(trackId, {}) : host.setSource(trackId, nullptr);
        else if (const auto& kits = daw::SampleKit::bundledKits(); juce::isPositiveAndBelow(choice - firstKitId, static_cast<int>(kits.size())) && host.setKit)
            result = host.setKit(trackId, kits[static_cast<size_t>(choice - firstKitId)].id);
        else if (juce::isPositiveAndBelow(choice - firstPluginId, instrumentList.size()))
            result = host.setSource(trackId, &instrumentList.getReference(choice - firstPluginId));
        if (result.failed())
            info.setText(result.getErrorMessage(), juce::dontSendNotification);
        refreshSources();
    };
    addAndMakeVisible(source);
    editor.setTooltip(text("Oberfläche des VST3-Instruments öffnen (Kit-Auswahl usw.)"));
    editor.onClick = [this] { if (host.openEditor) host.openEditor(trackId); };
    addAndMakeVisible(editor);
    arrangement.setButtonText(text("Drum-Arrangement …"));
    arrangement.setTooltip(text("Abschnitte mit Grooves, Fills und Crashes über ganze Teile des Songs legen – mit Groove-Bibliothek und Auto-Modus"));
    arrangement.onClick = [this] { if (host.openArrangement) host.openArrangement(trackId); };
    addAndMakeVisible(arrangement);
    morePatterns.setTooltip(text("Weitere Patterns dieser Spur, z. B. aus der Groove-Bibliothek des Drum-Arrangements"));
    morePatterns.onChange = [this]
    {
        if (const auto id = morePatterns.getSelectedId(); id > 0)
            selectPattern(id - 1);
    };
    addChildComponent(morePatterns);

    for (int index = 0; index < 8; ++index)
    {
        auto* button = patternButtons.add(new juce::TextButton(juce::String::charToString(static_cast<juce::juce_wchar>('A' + index))));
        button->setClickingTogglesState(true);
        button->setRadioGroupId(9001);
        button->setConnectedEdges((index > 0 ? juce::Button::ConnectedOnLeft : 0) | (index < 7 ? juce::Button::ConnectedOnRight : 0));
        button->onClick = [this, index] { selectPattern(index); };
        addAndMakeVisible(button);
    }

    for (const auto steps : { 8, 12, 16, 24, 32, 48, 64 })
        length.addItem(juce::String(steps) + " Schritte", steps);
    length.setTooltip(text("Pattern-Länge in Sechzehnteln (16 = ein 4/4-Takt)"));
    length.onChange = [this]
    {
        const auto steps = length.getSelectedId();
        if (steps > 0)
            editPattern(text("Pattern-Länge"), [steps](daw::DrumPattern& p) { p.steps = steps; });
    };
    addAndMakeVisible(length);

    swing.setRange(0.0, 60.0, 1.0);
    swing.setTextValueSuffix(" %");
    swing.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    const auto commitSwing = [this]
    {
        const auto value = static_cast<float>(swing.getValue() / 100.0);
        if (const auto* p = pattern(); p != nullptr && std::abs(p->swing - value) > 0.001f)
            editPattern("Swing", [value](daw::DrumPattern& pat) { pat.swing = value; });
    };
    swing.onDragEnd = commitSwing;
    swing.onValueChange = [this, commitSwing] { if (! swing.isMouseButtonDown()) commitSwing(); };
    swingLabel.setColour(juce::Label::textColourId, Theme::subduedText());
    addAndMakeVisible(swingLabel);
    addAndMakeVisible(swing);

    audition.setButtonText(text("▶ Vorhören"));
    audition.setTooltip(text("Pattern in Schleife abspielen (bei gestopptem Transport)"));
    audition.setClickingTogglesState(true);
    audition.onClick = [this]
    {
        auditioning = audition.getToggleState();
        controller.setPatternAudition(auditioning ? trackId : juce::Uuid::null(), selected);
    };
    clear.onClick = [this] { editPattern("Pattern leeren", [](daw::DrumPattern& p) { for (auto& lane : p.velocity) lane.fill(0); }); };
    addAndMakeVisible(audition);
    addAndMakeVisible(clear);

    for (const auto count : { 1, 2, 4, 8, 16, 32 })
        bars.addItem(juce::String(count) + (count == 1 ? " Takt" : " Takte"), count);
    bars.setSelectedId(4, juce::dontSendNotification);
    insert.setButtonText(text("In Arrangement einfügen"));
    insert.setTooltip(text("Fügt das Pattern als Clip an der Abspielposition (auf den Takt gerundet) ein"));
    insert.onClick = [this]
    {
        const auto start = controller.grid().snap(controller.engine().position(), 1.0);
        controller.insertPatternClip(trackId, selected, start, std::max(1, bars.getSelectedId()));
    };
    addAndMakeVisible(bars);
    addAndMakeVisible(insert);

    refreshSources();
    selectPattern(selected);
    setSize(1040, 600);
    startTimerHz(30);
}

DrumMachineComponent::~DrumMachineComponent()
{
    if (auditioning)
        controller.setPatternAudition(juce::Uuid::null(), 0);
}

const daw::Track* DrumMachineComponent::track() const
{
    return controller.state().findTrack(trackId);
}

const daw::DrumPattern* DrumMachineComponent::pattern() const
{
    const auto* t = track();
    return t != nullptr && juce::isPositiveAndBelow(selected, static_cast<int>(t->patterns.size())) ? &t->patterns[static_cast<size_t>(selected)] : nullptr;
}

void DrumMachineComponent::selectPattern(int index)
{
    const auto* t = track();
    const auto count = t != nullptr ? static_cast<int>(t->patterns.size()) : 8;
    selected = juce::jlimit(0, std::max(0, count - 1), index);
    if (selected < patternButtons.size())
    {
        patternButtons[selected]->setToggleState(true, juce::dontSendNotification);
        morePatterns.setSelectedId(0, juce::dontSendNotification);
    }
    else
    {
        for (auto* button : patternButtons)
            button->setToggleState(false, juce::dontSendNotification);
        morePatterns.setSelectedId(selected + 1, juce::dontSendNotification);
    }
    if (auditioning)
        controller.setPatternAudition(trackId, selected);
    refreshControls();
    repaint();
}

void DrumMachineComponent::refreshSources()
{
    instrumentList = host.instruments ? host.instruments() : juce::Array<juce::PluginDescription>();
    source.clear(juce::dontSendNotification);
    source.addSectionHeading(text("Eingebaute Kits"));
    const auto& kits = daw::SampleKit::bundledKits();
    for (size_t index = 0; index < kits.size(); ++index)
        source.addItem(text("Akustik: ") + text(kits[index].name), firstKitId + static_cast<int>(index));
    source.addItem(text("Synth-Kit (elektronisch, 808/909)"), builtInId);
    source.addSectionHeading(text("VST3-Instrumente"));
    for (int index = 0; index < instrumentList.size(); ++index)
        source.addItem("VST3: " + instrumentList[index].name, firstPluginId + index);
    source.addSeparator();
    source.addItem(text("VST3-Instrumente suchen …"), rescanId);

    const auto* t = track();
    const auto settings = t != nullptr ? t->instrument->settings : juce::ValueTree();
    const auto isPlugin = settings.getProperty("type").toString() == "vst3";
    source.setSelectedId(builtInId, juce::dontSendNotification);
    const auto isKit = settings.getProperty("type").toString() == "kit";
    if (isKit)
        for (size_t index = 0; index < kits.size(); ++index)
            if (settings.getProperty("kit").toString() == kits[index].id)
                source.setSelectedId(firstKitId + static_cast<int>(index), juce::dontSendNotification);
    if (isPlugin)
    {
        const auto name = settings.getProperty("name").toString();
        bool found = false;
        for (int index = 0; index < instrumentList.size(); ++index)
            if (instrumentList[index].name == name)
            {
                source.setSelectedId(firstPluginId + index, juce::dontSendNotification);
                found = true;
            }
        if (! found)
            source.setText("VST3: " + name, juce::dontSendNotification);
    }
    editor.setEnabled(isPlugin);
    info.setText(isPlugin ? text("Gesendet wird General MIDI (Kanal 10) – das Kit wählst du im Plugin-Editor.")
                 : isKit ? (settings.getProperty("kit").toString() == "sm-drums"
                                ? text("Echte Stereo-Drum-Samples mit bis zu 16 Anschlagstufen und wechselnden Schlägen: SM Drums (Scott McLean, Tod Stillwell, Suleiman Ali).")
                                : text("Echte Drum-Samples mit 5 Anschlagstufen: AVL Drumkits von Glen MacArthur (bandshed.net), CC-BY-SA 3.0."))
                         : text("Synthetisiertes Kit im 808/909-Stil. Für echte Drums oben ein Akustik-Kit wählen."),
                 juce::dontSendNotification);
}

void DrumMachineComponent::refreshMorePatterns()
{
    const auto* t = track();
    const auto count = t != nullptr ? static_cast<int>(t->patterns.size()) : 0;
    if (count == morePatternCount)
        return;
    morePatternCount = count;
    morePatterns.clear(juce::dontSendNotification);
    for (int index = patternButtons.size(); index < count; ++index)
        morePatterns.addItem(t->patterns[static_cast<size_t>(index)].name, index + 1);
    morePatterns.setTextWhenNothingSelected(text("Weitere: ") + juce::String(std::max(0, count - patternButtons.size())));
    morePatterns.setVisible(count > patternButtons.size());
    if (selected >= patternButtons.size())
        morePatterns.setSelectedId(selected + 1, juce::dontSendNotification);
}

void DrumMachineComponent::refreshControls()
{
    refreshMorePatterns();
    const auto* t = track();
    title.setText(text("Drumcomputer: ") + (t != nullptr ? t->name : juce::String()), juce::dontSendNotification);
    if (const auto* p = pattern())
    {
        length.setSelectedId(p->steps, juce::dontSendNotification);
        if (! swing.isMouseButtonDown())
            swing.setValue(std::round(p->swing * 100.0), juce::dontSendNotification);
        for (int index = 0; index < patternButtons.size() && t != nullptr; ++index)
            if (index < static_cast<int>(t->patterns.size()))
                patternButtons[index]->setTooltip(t->patterns[static_cast<size_t>(index)].name);
    }
}

void DrumMachineComponent::editPattern(const juce::String& description, const std::function<void(daw::DrumPattern&)>& change)
{
    const auto id = trackId;
    const auto index = selected;
    controller.perform(description, [&](daw::ArrangementState& state)
    {
        if (auto* t = state.findTrack(id); t != nullptr && juce::isPositiveAndBelow(index, static_cast<int>(t->patterns.size())))
            change(t->patterns[static_cast<size_t>(index)]);
    });
    refreshControls();
    repaint();
}

juce::Rectangle<int> DrumMachineComponent::cellBounds(int lane, int step) const
{
    const auto* p = pattern();
    const auto steps = p != nullptr ? p->steps : 16;
    const auto cellWidth = static_cast<float>(gridArea.getWidth()) / static_cast<float>(steps);
    const auto cellHeight = gridArea.getHeight() / daw::numDrumLanes;
    return juce::Rectangle<float>(static_cast<float>(gridArea.getX()) + cellWidth * static_cast<float>(step),
                                  static_cast<float>(gridArea.getY() + lane * cellHeight), cellWidth, static_cast<float>(cellHeight))
        .toNearestInt();
}

bool DrumMachineComponent::cellAt(juce::Point<int> point, int& lane, int& step) const
{
    const auto* p = pattern();
    if (p == nullptr || ! gridArea.contains(point))
        return false;
    lane = juce::jlimit(0, daw::numDrumLanes - 1, (point.y - gridArea.getY()) / std::max(1, gridArea.getHeight() / daw::numDrumLanes));
    step = juce::jlimit(0, p->steps - 1, static_cast<int>(static_cast<float>(point.x - gridArea.getX()) * static_cast<float>(p->steps) / static_cast<float>(gridArea.getWidth())));
    return true;
}

void DrumMachineComponent::applyPainted(daw::ArrangementState& state) const
{
    if (auto* t = state.findTrack(trackId); t != nullptr && juce::isPositiveAndBelow(selected, static_cast<int>(t->patterns.size())))
        for (const auto& [cell, velocity] : painted)
            t->patterns[static_cast<size_t>(selected)].velocity[static_cast<size_t>(cell.first)][static_cast<size_t>(cell.second)] = velocity;
}

void DrumMachineComponent::mouseDown(const juce::MouseEvent& event)
{
    if (laneArea.contains(event.getPosition()))
    {
        const auto lane = (event.y - laneArea.getY()) / std::max(1, laneArea.getHeight() / daw::numDrumLanes);
        if (juce::isPositiveAndBelow(lane, daw::numDrumLanes))
            controller.previewNote(trackId, daw::drumLanes()[static_cast<size_t>(lane)].note, 110);
        return;
    }
    int lane = 0, step = 0;
    const auto* p = pattern();
    if (p == nullptr || ! cellAt(event.getPosition(), lane, step))
        return;
    const auto current = p->velocity[static_cast<size_t>(lane)][static_cast<size_t>(step)];
    if (event.mods.isPopupMenu() || event.mods.isCommandDown())
    {
        // Cycle the velocity of one hit: ghost, normal, accent, off.
        const juce::uint8 next = current == 0 ? 45 : current < 70 ? 100 : current < 115 ? 127 : 0;
        editPattern("Anschlag", [lane, step, next](daw::DrumPattern& pat) { pat.velocity[static_cast<size_t>(lane)][static_cast<size_t>(step)] = next; });
        if (next > 0)
            controller.previewNote(trackId, daw::drumLanes()[static_cast<size_t>(lane)].note, next);
        return;
    }
    paintVelocity = current > 0 ? 0 : (event.mods.isShiftDown() ? 127 : 100);
    painted.clear();
    painted[{ lane, step }] = paintVelocity;
    lastLane = lane;
    lastStep = step;
    painting = true;
    controller.beginGesture();
    controller.updateGesture([this](daw::ArrangementState& state) { applyPainted(state); });
    if (paintVelocity > 0)
        controller.previewNote(trackId, daw::drumLanes()[static_cast<size_t>(lane)].note, paintVelocity);
    repaint();
}

void DrumMachineComponent::mouseDrag(const juce::MouseEvent& event)
{
    int lane = 0, step = 0;
    if (! painting || ! cellAt(event.getPosition(), lane, step) || (lane == lastLane && step == lastStep))
        return;
    // Fill every cell between the previous and the current position, so fast
    // drags do not leave gaps.
    const auto distance = std::max(std::abs(lane - lastLane), std::abs(step - lastStep));
    for (int i = 1; i <= distance; ++i)
    {
        const auto l = lastLane + static_cast<int>(std::lround(static_cast<double>(lane - lastLane) * i / distance));
        const auto s = lastStep + static_cast<int>(std::lround(static_cast<double>(step - lastStep) * i / distance));
        painted[{ l, s }] = paintVelocity;
    }
    lastLane = lane;
    lastStep = step;
    controller.updateGesture([this](daw::ArrangementState& state) { applyPainted(state); });
    repaint();
}

void DrumMachineComponent::mouseUp(const juce::MouseEvent&)
{
    if (! painting)
        return;
    painting = false;
    controller.endGesture("Pattern bearbeiten");
    painted.clear();
    repaint();
}

void DrumMachineComponent::visibilityChanged()
{
    if (! isShowing() && auditioning)
    {
        auditioning = false;
        audition.setToggleState(false, juce::dontSendNotification);
        controller.setPatternAudition(juce::Uuid::null(), 0);
    }
}

int DrumMachineComponent::currentStep() const
{
    auto& engine = controller.engine();
    const auto* p = pattern();
    if (p == nullptr)
        return -1;
    if (! engine.isPlaying())
        return auditioning ? engine.auditionStep() : -1;
    const auto* t = track();
    const auto position = engine.position();
    const auto stepLength = controller.grid().sampleRate * 60.0 / controller.grid().bpm / 4.0;
    for (const auto& clip : t->clips)
        if (clip.pattern == selected && position >= clip.start && position < clip.end())
            return static_cast<int>(std::floor((static_cast<double>(position - clip.start) + clip.offset) / stepLength)) % std::max(1, p->steps);
    return -1;
}

void DrumMachineComponent::timerCallback()
{
    // Closing the window only hides it (visibilityChanged() is not called for
    // the content), so the preview is stopped here once nothing shows it.
    if (auditioning && ! isShowing())
        visibilityChanged();
    if (track() == nullptr)
        return;
    if (const auto step = currentStep(); step != playingStep)
    {
        playingStep = step;
        repaint(gridArea.expanded(2));
    }
    if (! painting)
        refreshControls();
}

void DrumMachineComponent::paint(juce::Graphics& g)
{
    g.fillAll(Theme::background());
    const auto* p = pattern();
    if (p == nullptr)
        return;
    const auto* t = track();
    const auto colour = t != nullptr ? t->colour : Theme::amber();

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    const auto laneHeight = gridArea.getHeight() / daw::numDrumLanes;
    for (int lane = 0; lane < daw::numDrumLanes; ++lane)
    {
        auto label = juce::Rectangle<int>(laneArea.getX(), laneArea.getY() + lane * laneHeight, laneArea.getWidth(), laneHeight).reduced(2, 1);
        g.setColour(Theme::surfaceRaised());
        g.fillRoundedRectangle(label.toFloat(), 4.0f);
        g.setColour(Theme::text());
        g.drawText(daw::drumLanes()[static_cast<size_t>(lane)].name, label.reduced(8, 0), juce::Justification::centredLeft);
    }

    for (int step = 0; step < p->steps; ++step)
    {
        for (int lane = 0; lane < daw::numDrumLanes; ++lane)
        {
            const auto cell = cellBounds(lane, step).reduced(2).toFloat();
            const auto beat = (step / 4) % 2 == 0;
            g.setColour(beat ? juce::Colour(0xff2a221b) : juce::Colour(0xff221c17));
            g.fillRoundedRectangle(cell, 3.0f);
            const auto velocity = p->velocity[static_cast<size_t>(lane)][static_cast<size_t>(step)];
            if (velocity > 0)
            {
                g.setColour(colour.withAlpha(0.35f + 0.65f * static_cast<float>(velocity) / 127.0f));
                g.fillRoundedRectangle(cell.reduced(1.0f), 3.0f);
                if (velocity >= 120)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.8f));
                    g.drawRoundedRectangle(cell.reduced(1.5f), 3.0f, 1.2f);
                }
            }
        }
        if (step == playingStep)
        {
            g.setColour(Theme::amberBright().withAlpha(0.25f));
            g.fillRect(cellBounds(0, step).getUnion(cellBounds(daw::numDrumLanes - 1, step)));
        }
    }
    // Beat numbers above the grid.
    g.setColour(Theme::subduedText());
    g.setFont(juce::FontOptions(11.0f));
    for (int step = 0; step < p->steps; step += 4)
        g.drawText(juce::String(step / 4 + 1), cellBounds(0, step).withY(gridArea.getY() - 16).withHeight(14), juce::Justification::centredLeft);

    g.setColour(Theme::subduedText());
    g.drawText(text("Klick: an/aus · Ziehen: malen · Shift: Akzent · Rechtsklick: Anschlagstärke · Name anklicken: vorhören"),
               getLocalBounds().removeFromBottom(24).reduced(14, 0), juce::Justification::centredLeft);
}

void DrumMachineComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    auto top = area.removeFromTop(30);
    editor.setBounds(top.removeFromRight(120));
    top.removeFromRight(8);
    source.setBounds(top.removeFromRight(300));
    top.removeFromRight(8);
    arrangement.setBounds(top.removeFromRight(160));
    top.removeFromRight(8);
    morePatterns.setBounds(top.removeFromRight(180));
    title.setBounds(top);
    area.removeFromTop(4);
    info.setBounds(area.removeFromTop(20));
    area.removeFromTop(8);

    auto controls = area.removeFromTop(30);
    for (auto* button : patternButtons)
    {
        button->setBounds(controls.removeFromLeft(32));
    }
    controls.removeFromLeft(12);
    length.setBounds(controls.removeFromLeft(130));
    controls.removeFromLeft(12);
    swingLabel.setBounds(controls.removeFromLeft(48));
    swing.setBounds(controls.removeFromLeft(juce::jlimit(90, 170, controls.getWidth() - 470)));
    controls.removeFromLeft(12);
    audition.setBounds(controls.removeFromLeft(110));
    controls.removeFromLeft(6);
    clear.setBounds(controls.removeFromLeft(70));
    insert.setBounds(controls.removeFromRight(170));
    controls.removeFromRight(6);
    bars.setBounds(controls.removeFromRight(100));

    area.removeFromTop(26);
    area.removeFromBottom(28);
    laneArea = area.removeFromLeft(laneWidth);
    area.removeFromLeft(6);
    gridArea = area;
}
} // namespace nam::ui

#include "ChordTrackBar.h"

#include <cmath>

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using daw::text;
using Theme = AmberTheme;

/** The text field over a bar; Tab moves on instead of leaving the field. */
class ChordTrackBar::Editor final : public juce::TextEditor
{
public:
    std::function<void(bool moveOn)> onCommit;
    std::function<void()> onCancel;
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::tabKey || key == juce::KeyPress::returnKey)
        {
            if (onCommit) onCommit(key == juce::KeyPress::tabKey);
            return true;
        }
        if (key == juce::KeyPress::escapeKey)
        {
            if (onCancel) onCancel();
            return true;
        }
        return TextEditor::keyPressed(key);
    }
    void focusLost(FocusChangeType) override
    {
        if (onCommit) onCommit(false);
    }
};

ChordTrackBar::ChordTrackBar(daw::ArrangementController& c, const TimelineComponent& t, int width)
    : controller(c), timeline(t), labelWidth(width)
{
    setTooltip(text("Lead-Sheet: Takt anklicken und Akkord tippen (z. B. Am7, G/H). Zwei Akkorde wie „Am G“ teilen den Takt. "
                    "Enter übernimmt, Tab springt zum nächsten Takt, Shift+Klick wählt eine Takthälfte, Rechtsklick: ganzer/halber Takt, löschen, Akkordfolgen-Editor mit Bibliothek und Auto-Modus, Begleitung erzeugen."));
}

ChordTrackBar::~ChordTrackBar() = default;

double ChordTrackBar::samplesPerBar() const { return controller.grid().samplesPerBar(); }
float ChordTrackBar::barToX(double bar) const { return static_cast<float>(labelWidth) + timeline.sampleToX(bar * samplesPerBar()); }
double ChordTrackBar::xToBar(float x) const { return timeline.xToSampleExact(x - static_cast<float>(labelWidth)) / samplesPerBar(); }

std::pair<double, double> ChordTrackBar::slotAt(float x, bool forceHalf) const
{
    const auto position = std::max(0.0, xToBar(x));
    const auto bar = std::floor(position);
    const auto half = position - bar >= 0.5 ? 0.5 : 0.0;
    // A bar that is already split edits the clicked half.
    const auto* chord = daw::edit::chordAt(controller.state(), bar + half);
    const auto split = forceHalf || (chord != nullptr && chord->length < 0.99)
                    || (chord == nullptr && daw::edit::chordAt(controller.state(), bar + 0.5 - half) != nullptr
                        && daw::edit::chordAt(controller.state(), bar + 0.5 - half)->length < 0.99);
    return split ? std::make_pair(bar + half, 0.5) : std::make_pair(bar, 1.0);
}

size_t ChordTrackBar::chordsHash() const
{
    // Undo, redo and loading change chords without this bar knowing.
    size_t hash = controller.state().chords.size();
    for (const auto& chord : controller.state().chords)
        hash = hash * 31u + std::hash<double>()(chord.bar * 7.0 + chord.length) + static_cast<size_t>(chord.name.hashCode64());
    return hash;
}

void ChordTrackBar::update()
{
    const auto now = std::make_tuple(timeline.viewStart(), timeline.samplesPerPixel(), chordsHash(), samplesPerBar());
    if (now != painted)
        repaint();
}

void ChordTrackBar::paint(juce::Graphics& g)
{
    const auto& state = controller.state();
    painted = std::make_tuple(timeline.viewStart(), timeline.samplesPerPixel(), chordsHash(), samplesPerBar());
    g.fillAll(Theme::panel());
    auto label = getLocalBounds().removeFromLeft(labelWidth).reduced(10, 0);
    g.setColour(Theme::subduedText());
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(text("AKKORDE"), label, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(text("Editor …"), label, juce::Justification::centredRight);

    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(getLocalBounds().withTrimmedLeft(labelWidth));
    const auto first = std::max(0.0, std::floor(xToBar(static_cast<float>(labelWidth))));
    const auto last = xToBar(static_cast<float>(getWidth())) + 1.0;
    const auto height = static_cast<float>(getHeight());
    // Empty bars as faint cells, so it is clear where to click.
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    for (auto bar = first; bar < last; bar += 1.0)
        g.drawRect(juce::Rectangle<float>(barToX(bar), 3.0f, barToX(bar + 1.0) - barToX(bar), height - 6.0f), 1.0f);
    for (const auto& chord : state.chords)
    {
        if (chord.end() < first || chord.bar > last)
            continue;
        const auto cell = juce::Rectangle<float>(barToX(chord.bar), 3.0f, barToX(chord.end()) - barToX(chord.bar), height - 6.0f).reduced(1.0f, 0.0f);
        const auto valid = daw::edit::isChordName(chord.name);
        g.setColour(valid ? Theme::amber().withAlpha(0.22f) : Theme::danger().withAlpha(0.25f));
        g.fillRoundedRectangle(cell, 3.0f);
        g.setColour(valid ? Theme::amberBright() : Theme::danger());
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(chord.name, cell.reduced(6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, true);
    }
}

void ChordTrackBar::resized()
{
    if (editor != nullptr)
        editor->setBounds(juce::Rectangle<float>(barToX(editBar), 2.0f, std::max(70.0f, barToX(editBar + editLength) - barToX(editBar)),
                                                 static_cast<float>(getHeight()) - 4.0f).toNearestInt());
}

void ChordTrackBar::mouseDown(const juce::MouseEvent& event)
{
    if (event.position.x < static_cast<float>(labelWidth))
    {
        // The label opens the progression editor.
        if (onOpenEditor)
            onOpenEditor(std::max(0, static_cast<int>(std::floor(xToBar(static_cast<float>(labelWidth))))));
        return;
    }
    const auto [bar, length] = slotAt(event.position.x, event.mods.isShiftDown());
    if (event.mods.isPopupMenu())
        showMenu(std::floor(bar));
    else
        startEditing(bar, length);
}

void ChordTrackBar::startEditing(double bar, double length)
{
    editBar = bar;
    editLength = length;
    editor = std::make_unique<Editor>();
    editor->setFont(juce::FontOptions(13.0f, juce::Font::bold));
    editor->setColour(juce::TextEditor::backgroundColourId, Theme::well());
    editor->setColour(juce::TextEditor::textColourId, Theme::amberBright());
    editor->setColour(juce::TextEditor::outlineColourId, Theme::amber());
    editor->setColour(juce::TextEditor::focusedOutlineColourId, Theme::amber());
    // A whole bar shows both halves of a split bar ("Am G").
    juce::StringArray names;
    for (const auto& chord : controller.state().chords)
        if (chord.bar >= bar - 1.0e-9 && chord.end() <= bar + length + 1.0e-9)
            names.add(chord.name);
    editor->setText(names.joinIntoString(" "), false);
    editor->setSelectAllWhenFocused(true); // typing replaces the chord
    editor->selectAll();
    editor->onCommit = [this](bool moveOn) { commit(moveOn); };
    editor->onCancel = [this] { cancel(); };
    addAndMakeVisible(*editor);
    resized();
    editor->grabKeyboardFocus();
}

void ChordTrackBar::commit(bool moveOn)
{
    if (editor == nullptr)
        return;
    const auto typed = juce::StringArray::fromTokens(editor->getText(), " \t", {});
    auto names = typed;
    names.removeEmptyStrings();
    const auto bar = editBar, length = editLength;
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ChordTrackBar>(this)] { if (safe != nullptr) safe->editor.reset(); });
    editor->onCommit = nullptr; // focus loss while closing must not commit twice
    controller.perform(text("Akkord"), [&](daw::ArrangementState& state)
    {
        if (names.size() >= 2 && length > 0.75)
        {
            // "Am G": two half bars.
            daw::edit::setChord(state, bar, 0.5, names[0]);
            daw::edit::setChord(state, bar + 0.5, 0.5, names[1]);
        }
        else
            daw::edit::setChord(state, bar, length, names.joinIntoString(""));
    });
    repaint();
    if (moveOn)
    {
        const auto next = bar + length;
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ChordTrackBar>(this), next]
        {
            if (safe != nullptr)
            {
                const auto x = safe->barToX(next) + 2.0f;
                const auto [b, l] = safe->slotAt(x, false);
                safe->startEditing(b, l);
            }
        });
    }
}

void ChordTrackBar::cancel()
{
    if (editor == nullptr)
        return;
    editor->onCommit = nullptr;
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ChordTrackBar>(this)] { if (safe != nullptr) safe->editor.reset(); });
}

void ChordTrackBar::showMenu(double bar)
{
    const auto* first = daw::edit::chordAt(controller.state(), bar);
    const auto* second = daw::edit::chordAt(controller.state(), bar + 0.5);
    const auto split = (first != nullptr && first->length < 0.99) || (second != nullptr && second->length < 0.99);
    juce::PopupMenu menu;
    menu.addItem(text("Akkord eingeben …"), [this, bar] { startEditing(bar, 1.0); });
    menu.addItem(text("Ganzer Takt (ein Akkord)"), true, ! split, [this, bar, first]
    {
        const auto name = first != nullptr ? first->name : juce::String();
        controller.perform(text("Akkord: ganzer Takt"), [&](daw::ArrangementState& state) { daw::edit::setChord(state, bar, 1.0, name); });
    });
    menu.addItem(text("Halbe Takte (zwei Akkorde)"), true, split, [this, bar, first]
    {
        const auto name = first != nullptr ? first->name : juce::String();
        controller.perform(text("Akkord: halbe Takte"), [&](daw::ArrangementState& state)
        {
            daw::edit::setChord(state, bar, 0.5, name);
            daw::edit::setChord(state, bar + 0.5, 0.5, name);
        });
        startEditing(bar + 0.5, 0.5);
    });
    menu.addSeparator();
    menu.addItem(text("Akkordfolgen-Editor & Bibliothek …"), onOpenEditor != nullptr, false, [this, bar] { if (onOpenEditor) onOpenEditor(static_cast<int>(bar)); });
    menu.addItem(text("Begleitung aus Akkorden erzeugen …"), onAccompaniment != nullptr, false, [this] { if (onAccompaniment) onAccompaniment(); });
    menu.addSeparator();
    menu.addItem(text("Akkord löschen"), first != nullptr || second != nullptr, false, [this, bar]
    {
        controller.perform(text("Akkord löschen"), [&](daw::ArrangementState& state) { daw::edit::setChord(state, bar, 1.0, {}); });
    });
    menu.addItem(text("Alle Akkorde löschen"), ! controller.state().chords.empty(), false, [this]
    {
        controller.perform(text("Alle Akkorde löschen"), [](daw::ArrangementState& state) { state.chords.clear(); });
    });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(),
                       [safe = juce::Component::SafePointer<ChordTrackBar>(this)](int) { if (safe != nullptr) safe->repaint(); });
}
} // namespace nam::ui

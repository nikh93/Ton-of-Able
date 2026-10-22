#include "NamPedalPanel.h"

NamPedalPanel::NamPedalPanel(AudioEngine& e) : engine(e)
{
    title.setText("NAM PEDAL > NAM AMP",juce::dontSendNotification);
    hint.setText("Nur Pedal-Captures ohne Amp/Cab. Input treibt das Pedal, Output den Amp.",juce::dontSendNotification);
    inputLabel.setText("PEDAL INPUT",juce::dontSendNotification);
    outputLabel.setText("PEDAL OUTPUT",juce::dontSendNotification);
    for (auto* l : {&title,&hint,&status,&inputLabel,&outputLabel}) addAndMakeVisible(*l);
    addAndMakeVisible(models); addAndMakeVisible(enabled);
    addAndMakeVisible(importButton); addAndMakeVisible(removeButton);
    for (auto* s : {&input,&output})
    {
        addAndMakeVisible(*s); s->setRange(-24,24,.1); s->setValue(0);
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight,false,72,24); s->setTextValueSuffix(" dB");
        s->onValueChange=[this]{apply();};
    }
    models.setTextWhenNothingSelected("Pedal-Modell importieren (.nam)");
    models.onChange=[this]
    {
        const int n=models.getSelectedId()-1;
        if (juce::isPositiveAndBelow(n,paths.size())) {path=paths[n]; apply();}
    };
    enabled.onClick=[this]{apply();};
    removeButton.onClick=[this]
    {
        path.clear(); enabled.setToggleState(false,juce::dontSendNotification);
        models.setSelectedId(0,juce::dontSendNotification); apply();
    };
    importButton.onClick=[this]
    {
        chooser=std::make_shared<juce::FileChooser>("Pedal-only NAM-Capture importieren",juce::File("D:/Amp Modeler"),"*.nam");
        const juce::Component::SafePointer<NamPedalPanel> self(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [self](const juce::FileChooser& fc)
            {
                if (!self || !fc.getResult().existsAsFile()) return;
                self->engine.setNamPedal({},true,0.f,0.f); // Reimport explicitly retries a repaired file.
                self->path=fc.getResult().getFullPathName();
                self->paths.addIfNotAlreadyThere(self->path); self->rebuildChoices();
                // Import alone never unexpectedly boosts or distorts a live signal.
                self->enabled.setToggleState(false,juce::dontSendNotification); self->apply();
            });
    };
    startTimerHz(4);
}
void NamPedalPanel::rebuildChoices()
{
    models.clear(juce::dontSendNotification);
    for (int i=0;i<paths.size();++i) models.addItem(juce::File(paths[i]).getFileNameWithoutExtension(),i+1);
    models.setSelectedId(paths.indexOf(path)+1,juce::dontSendNotification);
}
void NamPedalPanel::addBundledModels(const juce::File& folder)
{
    bundledFolder=folder;
    juce::StringArray bundled;
    for (const auto& f : juce::RangedDirectoryIterator(folder,false,"*.nam",juce::File::findFiles))
        bundled.add(f.getFile().getFullPathName());
    bundled.sort(true);
    for (const auto& p : bundled) paths.addIfNotAlreadyThere(p);
    rebuildChoices();
}
juce::String NamPedalPanel::resolveBundledPath(const juce::String& p) const
{
    if (juce::File::isAbsolutePath(p) && p.replaceCharacter('\\','/').containsIgnoreCase("/resources/pedal-models/"))
    {
        const auto relocated=bundledFolder.getChildFile(juce::File(p).getFileName());
        if (relocated.existsAsFile()) return relocated.getFullPathName();
    }
    return p;
}
void NamPedalPanel::capture(SoundState& s) const
{
    s.namPedalPath=path; s.namPedalBypassed=!enabled.getToggleState();
    s.namPedalInputDb=(float)input.getValue(); s.namPedalOutputDb=(float)output.getValue();
}
void NamPedalPanel::restore(const SoundState& s)
{
    path=resolveBundledPath(s.namPedalPath);
    if (path.isNotEmpty()) paths.addIfNotAlreadyThere(path);
    rebuildChoices(); enabled.setToggleState(!s.namPedalBypassed,juce::dontSendNotification);
    input.setValue(s.namPedalInputDb,juce::dontSendNotification);
    output.setValue(s.namPedalOutputDb,juce::dontSendNotification); apply();
}
void NamPedalPanel::apply()
{
    engine.setNamPedal(path,!enabled.getToggleState(),(float)input.getValue(),(float)output.getValue());
    timerCallback();
}
void NamPedalPanel::timerCallback()
{
    enabled.setEnabled(path.isNotEmpty());
    const auto error=engine.namPedalError();
    const auto name=engine.activeNamPedalName();
    status.setText(path.isEmpty() ? "Kein Pedal geladen: trockener Durchgang."
        : error.isNotEmpty() ? "Dry-Fallback: "+error
        : name.isEmpty() ? "Pedal wird im Hintergrund geladen ..."
        : "Pedal: "+name+" | "+(enabled.getToggleState()?"ON":"OFF"),juce::dontSendNotification);
}
juce::ValueTree NamPedalPanel::save()
{
    SoundState s; capture(s); juce::ValueTree t("namPedal");
    t.addChild(s.toValueTree(),-1,nullptr);
    for (const auto& p : paths) {juce::ValueTree f("model"); f.setProperty("path",p,nullptr); t.addChild(f,-1,nullptr);}
    return t;
}
void NamPedalPanel::restoreSaved(const juce::ValueTree& t)
{
    for (const auto& f : t) if(f.hasType("model")) paths.addIfNotAlreadyThere(resolveBundledPath(f["path"].toString()));
    if (auto s=SoundState::fromValueTree(t.getChild(0))) restore(*s);
}
void NamPedalPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff35443c)); g.fillRoundedRectangle(getLocalBounds().toFloat(),9.f);
}
void NamPedalPanel::resized()
{
    const int w=getWidth(); title.setBounds(12,5,w-100,25); enabled.setBounds(w-70,5,60,25);
    models.setBounds(12,36,w-335,28); importButton.setBounds(w-310,36,185,28); removeButton.setBounds(w-115,36,100,28);
    inputLabel.setBounds(12,72,110,22); input.setBounds(120,70,w/2-132,26);
    outputLabel.setBounds(w/2+8,72,115,22); output.setBounds(w/2+125,70,w/2-140,26);
    hint.setBounds(12,103,w-24,22); status.setBounds(12,130,w-24,28);
}

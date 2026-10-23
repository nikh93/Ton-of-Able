#include "PluginRackPanel.h"
#include "ui/components/AmberTheme.h"

namespace
{
juce::File appDirectory()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
}

void addVst3Files(juce::Array<juce::File>& files, const juce::File& root, bool recursive)
{
    if (!root.isDirectory()) return;
    for (const auto& entry : juce::RangedDirectoryIterator(root,recursive,"*.vst3",juce::File::findFilesAndDirectories))
        if (!entry.getFile().getParentDirectory().getFullPathName().containsIgnoreCase(".vst3")) files.addIfNotAlreadyThere(entry.getFile());
}

class EditorWindow final : public juce::DocumentWindow
{
public:
    explicit EditorWindow(juce::AudioPluginInstance& plugin)
        : DocumentWindow(plugin.getName(), juce::Colour(0xff252b27), closeButton)
    {
        setUsingNativeTitleBar(true);
        auto* editor = plugin.createEditorAndMakeActive();
        if (!editor) editor = new juce::GenericAudioProcessorEditor(plugin);
        setContentOwned(editor, true);
        setResizable(editor->isResizable(), false);
        centreWithSize(juce::jmax(420,editor->getWidth()), juce::jmax(250,editor->getHeight()));
        setVisible(true);
    }
    void closeButtonPressed() override { setVisible(false); }
};
}

PluginRackPanel::PluginRackPanel(AudioEngine& engine) : preSlot(engine.prePlugin()), postSlot(engine.postPlugin()), pedal(engine)
{
    addAndMakeVisible(pedal);
    heading.setText("PEDALE & VST3-EFFEKTE", juce::dontSendNotification);
    heading.setFont(juce::FontOptions(22.f, juce::Font::bold));
    route.setText("Drive > PRE VST3 > NAM PEDAL > NAM AMP > IR / Effekte > POST VST3 (Stereo)", juce::dontSendNotification);
    for (auto* c : { &heading, &route, &status }) addAndMakeVisible(*c);
    addAndMakeVisible(importButton); addAndMakeVisible(scanButton);
    importButton.onClick = [this] { choosePlugin(); };
    scanButton.onClick = [this] { scanInstalled(); };
    for (int i=0;i<2;++i)
    {
        addAndMakeVisible(names[i]); addAndMakeVisible(choices[i]); addAndMakeVisible(enabled[i]);
        addAndMakeVisible(editors[i]); addAndMakeVisible(remove[i]);
        addAndMakeVisible(programs[i]);
        programs[i].setTextWhenNothingSelected("Presets im Plugin-Fenster");
        programs[i].onChange = [this,i]
        {
            if (auto* plugin = slot(i).instance())
            {
                const int index = programs[i].getSelectedId()-1;
                if (juce::isPositiveAndBelow(index,plugin->getNumPrograms()))
                {
                    const juce::ScopedLock lock(plugin->getCallbackLock());
                    plugin->setCurrentProgram(index);
                }
            }
        };
        choices[i].setTextWhenNothingSelected(juce::String(juce::CharPointer_UTF8("Plugin ausw\xc3\xa4hlen")));
        choices[i].onChange = [this,i]
        {
            const int selected = choices[i].getSelectedId()-1;
            if (!juce::isPositiveAndBelow(selected,descriptions.size())) return;
            windows[i].reset();
            const auto result = slot(i).load(descriptions[selected]);
            status.setText(result.wasOk() ? "Geladen: " + slot(i).name() : result.getErrorMessage(), juce::dontSendNotification);
            refresh();
        };
        enabled[i].setButtonText("AN");
        enabled[i].onClick = [this,i] { slot(i).setBypassed(!enabled[i].getToggleState()); };
        editors[i].setButtonText(juce::String(juce::CharPointer_UTF8("Plugin / Presets \xc3\xb6" "ffnen")));
        editors[i].onClick = [this,i] { openEditor(i); };
        remove[i].setButtonText("Entfernen");
        remove[i].onClick = [this,i] { windows[i].reset(); slot(i).clear(); choices[i].setSelectedId(0,juce::dontSendNotification); refresh(); };
    }
    refresh(); setSize(840,610); startTimerHz(2);
}

PluginRackPanel::~PluginRackPanel()
{
    stopTimer(); scanPool.removeAllJobs(true,35000); closeEditors();
}
void PluginRackPanel::closeEditors() { for (auto& window : windows) window.reset(); }
void PluginRackPanel::refresh()
{
    for (int i=0;i<2;++i)
    {
        names[i].setText(juce::String(i==0 ? "VOR AMP / DRIVE: " : "NACH AMP / STEREO: ") + slot(i).name(),juce::dontSendNotification);
        enabled[i].setEnabled(slot(i).instance()!=nullptr);
        enabled[i].setToggleState(slot(i).instance()!=nullptr && !slot(i).isBypassed(),juce::dontSendNotification);
        editors[i].setEnabled(slot(i).instance()!=nullptr);
        auto* plugin = slot(i).instance();
        int selectedPlugin=0;
        if (plugin)
        {
            juce::PluginDescription active;
            plugin->fillInPluginDescription(active);
            for (int n=0;n<descriptions.size();++n)
                if (active.isDuplicateOf(descriptions[n])) { selectedPlugin=n+1; break; }
        }
        choices[i].setSelectedId(selectedPlugin,juce::dontSendNotification);
        const int count = plugin ? plugin->getNumPrograms() : 0;
        bool changed = programs[i].getNumItems()!=count;
        for (int n=0; !changed && n<count; ++n)
            changed = programs[i].getItemText(n) != plugin->getProgramName(n);
        if (changed)
        {
            programs[i].clear(juce::dontSendNotification);
            for (int n=0;n<count;++n)
                programs[i].addItem(plugin->getProgramName(n).isEmpty() ? "Preset " + juce::String(n+1) : plugin->getProgramName(n),n+1);
        }
        programs[i].setEnabled(count>1);
        programs[i].setSelectedId(plugin && count>0 ? plugin->getCurrentProgram()+1 : 0,juce::dontSendNotification);
    }
}
void PluginRackPanel::timerCallback() { refresh(); }
void PluginRackPanel::openEditor(int i)
{
    if (windows[i]) { windows[i]->setVisible(true); windows[i]->toFront(true); return; }
    if (auto* plugin = slot(i).instance()) windows[i] = std::make_unique<EditorWindow>(*plugin);
}
void PluginRackPanel::choosePlugin()
{
    chooser=std::make_shared<juce::FileChooser>(juce::String(juce::CharPointer_UTF8("VST3-Datei oder Bundle ausw\xc3\xa4hlen")),appDirectory(),"*.vst3");
    const juce::Component::SafePointer<PluginRackPanel> self(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectDirectories,
        [self](const juce::FileChooser& selected)
        {
            if (!self || !selected.getResult().exists()) return;
            const auto file=selected.getResult();
            juce::Array<juce::File> files;
            if (file.hasFileExtension("vst3")) files.add(file);
            else addVst3Files(files, file, true);
            self->scan(files,false);
        });
}
void PluginRackPanel::scanInstalled(bool defaults)
{
    juce::Array<juce::File> files;
    // Portable copies: plugins placed directly next to the exe or in a "plugins" folder beside it.
    addVst3Files(files, appDirectory(), false);
    addVst3Files(files, appDirectory().getChildFile("plugins"), true);
    addVst3Files(files, juce::File("D:/Amp Modeler/plugins"), true);
    addVst3Files(files, juce::File("C:/Program Files/Common Files/VST3"), true);
    scan(files, defaults);
}
void PluginRackPanel::scan(const juce::Array<juce::File>& files, bool defaults)
{
    if (scanning) return;
    scanning=true; scanButton.setEnabled(false); importButton.setEnabled(false);
    status.setText(juce::String(juce::CharPointer_UTF8("VST3-Pr\xc3\xbc" "fung in separatem Prozess \xe2\x80\xa6")),juce::dontSendNotification);
    const juce::Component::SafePointer<PluginRackPanel> self(this);
    scanPool.addJob([self,files,defaults]
    {
        juce::Array<juce::PluginDescription> found;
        juce::StringArray errors;
        for (const auto& file : files)
        {
            const auto report = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("nam-vst-scan",".xml");
            juce::ChildProcess child;
            const juce::StringArray args {juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),"--scan-vst3",file.getFullPathName(),report.getFullPathName()};
            if (child.start(args) && child.waitForProcessToFinish(30000) && child.getExitCode()==0)
            {
                if (auto xml=juce::XmlDocument::parse(report))
                    for (auto* item=xml->getFirstChildElement(); item; item=item->getNextElement())
                    {
                        juce::PluginDescription description;
                        if (description.loadFromXml(*item)) found.add(description);
                    }
            }
            else { child.kill(); errors.add(file.getFileName()); }
            report.deleteFile();
        }
        juce::MessageManager::callAsync([self,found,errors,defaults]
        {
            if (!self) return;
            bool firstSupermassive = false;
            for (const auto& description : found)
            {
                bool exists=false;
                for (const auto& old : self->descriptions) exists |= old.isDuplicateOf(description);
                if (!exists)
                {
                    self->descriptions.add(description);
                    firstSupermassive |= description.name.containsIgnoreCase("Supermassive");
                }
            }
            for (int i=0;i<2;++i)
            {
                self->choices[i].clear(juce::dontSendNotification);
                for (int index=0;index<self->descriptions.size();++index)
                    self->choices[i].addItem(self->descriptions[index].name,index+1);
                if ((defaults || (i==1 && firstSupermassive)) && !self->slot(i).instance())
                    for (int index=0;index<self->descriptions.size();++index)
                        if (self->descriptions[index].name.containsIgnoreCase(i==0 ? "BYOD" : "Supermassive"))
                        {
                            self->choices[i].setSelectedId(index+1,juce::sendNotificationSync);
                            if (i==0)
                                if (auto* plugin=self->slot(i).instance())
                                    for (int p=0;p<plugin->getNumPrograms();++p)
                                        if (plugin->getProgramName(p)=="John Mayer")
                                        {
                                            const juce::ScopedLock lock(plugin->getCallbackLock());
                                            plugin->setCurrentProgram(p); break;
                                        }
                            self->slot(i).setBypassed(true);
                            break;
                        }
            }
            self->scanning=false; self->scanButton.setEnabled(true); self->importButton.setEnabled(true);
            self->status.setText(juce::String(self->descriptions.size())+" Plugins verfuegbar. "
                +(errors.isEmpty() ? juce::String(juce::CharPointer_UTF8("AN aktiviert; \xe2\x80\x9ePlugin \xc3\xb6" "ffnen\xe2\x80\x9c zeigt dessen Presets.")) : "Scan fehlgeschlagen: "+errors.joinIntoString(", ")),juce::dontSendNotification);
            self->refresh();
        });
    });
}
juce::ValueTree PluginRackPanel::saveRack()
{
    juce::ValueTree rack("vst3Rack"), pre("pre"), post("post");
    pre.addChild(preSlot.saveState(),-1,nullptr); post.addChild(postSlot.saveState(),-1,nullptr);
    rack.addChild(pre,-1,nullptr); rack.addChild(post,-1,nullptr);
    juce::ValueTree known("knownVst3");
    for (const auto& description : descriptions)
        known.addChild(juce::ValueTree::fromXml(*description.createXml()),-1,nullptr);
    rack.addChild(known,-1,nullptr);
    rack.addChild(pedal.save(),-1,nullptr);
    return rack;
}
void PluginRackPanel::restoreRack(const juce::ValueTree& rack)
{
    for (const auto& entry : rack.getChildWithName("knownVst3"))
    {
        juce::PluginDescription description;
        if (description.loadFromXml(*entry.createXml())) descriptions.add(description);
    }
    restore(rack.getChildWithName("pre").getChild(0),rack.getChildWithName("post").getChild(0));
    if (auto state=rack.getChildWithName("namPedal"); state.isValid()) pedal.restoreSaved(state);
}
void PluginRackPanel::restore(const juce::ValueTree& pre,const juce::ValueTree& post)
{
    closeEditors();
    auto a=juce::Result::ok(), b=juce::Result::ok();
    if (pre.isValid()) a=preSlot.restoreState(pre); else preSlot.clear();
    if (post.isValid()) b=postSlot.restoreState(post); else postSlot.clear();
    if (a.failed()) preSlot.setBypassed(true);
    if (b.failed()) postSlot.setBypassed(true);
    if (a.failed()||b.failed()) status.setText(a.getErrorMessage()+" "+b.getErrorMessage(),juce::dontSendNotification);
    refresh();
}
void PluginRackPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff202923));
    for (int i=0;i<2;++i)
    {
        g.setColour(juce::Colour(0xff35443c));
        g.fillRoundedRectangle(16.f,285.f+i*115.f,getWidth()-32.f,105.f,9.f);
    }
}
void PluginRackPanel::resized()
{
    heading.setBounds(20,10,getWidth()-40,30); route.setBounds(20,42,getWidth()-40,24);
    importButton.setBounds(20,70,160,26); scanButton.setBounds(190,70,210,26);
    pedal.setBounds(16,102,getWidth()-32,170);
    for (int i=0;i<2;++i)
    {
        const int top=293+i*115;
        names[i].setBounds(28,top,getWidth()-110,24); enabled[i].setBounds(getWidth()-80,top,60,24);
        choices[i].setBounds(28,top+32,getWidth()-355,30);
        programs[i].setBounds(28,top+68,getWidth()-355,25);
        editors[i].setBounds(getWidth()-315,top+32,180,30); remove[i].setBounds(getWidth()-125,top+32,98,30);
    }
    status.setBounds(20,530,getWidth()-40,65);
}

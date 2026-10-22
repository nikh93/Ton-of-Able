#include <juce_gui_extra/juce_gui_extra.h>

#include "ui/MainComponent.h"
#include "audio/plugins/PluginInstrument.h"
#include "../tests/PluginHostChecks.h"
#include "../tests/NamPedalChecks.h"
#include "../tests/RigChecks.h"
#include "../tests/RigAudioChecks.h"
#include "../tests/DrumInstrumentChecks.h"

class NamTestProgramApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Ton of Able"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        auto args = juce::StringArray::fromTokens(commandLine, true);
        for (auto& arg : args) arg=arg.unquoted();
        if (args.size()==3 && args[0]=="--rig-audio-test")
        {
            setApplicationReturnValue(runRigAudioChecks(juce::File(args[1]),juce::File(args[2])));
            quit(); return;
        }
        if (args.size()==3 && args[0]=="--rig-test")
        {
            setApplicationReturnValue(runRigChecks(juce::File(args[1]),juce::File(args[2])));
            quit(); return;
        }
        // Device-free integration checks for the independent pedal slot.
        if (args.size()==3 && args[0]=="--nam-pedal-test")
        {
            setApplicationReturnValue(runNamPedalChecks(juce::File(args[1]),juce::File(args[2])));
            quit(); return;
        }
        if (args.size()==3 && args[0]=="--vst3-test")
        {
            setApplicationReturnValue(runPluginHostChecks(juce::File(args[1]),juce::File(args[2])));
            quit(); return;
        }
        if (args.size()==3 && args[0]=="--drum-instrument-test")
        {
            setApplicationReturnValue(runDrumInstrumentChecks(juce::File(args[1]),juce::File(args[2])));
            quit(); return;
        }
        if (args.size()==3 && args[0]=="--scan-vst3-instruments")
        {
            juce::XmlElement report("plugins");
            for (const auto& description : PluginInstrument::scanFile(juce::File(args[1])))
                report.addChildElement(description.createXml().release());
            report.writeTo(juce::File(args[2]));
            quit(); return;
        }
        if (args.size()==3 && args[0]=="--scan-vst3")
        {
            juce::XmlElement report("plugins");
            for (const auto& description : Vst3Slot::scanFile(juce::File(args[1])))
                report.addChildElement(description.createXml().release());
            report.writeTo(juce::File(args[2]));
            quit(); return;
        }
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
        if (args.size() == 1 && args[0].endsWithIgnoreCase(".tonproj"))
            if (auto* c = dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
                c->openProject(juce::File::getCurrentWorkingDirectory().getChildFile(args[0]));
        if (commandLine.startsWith("--design-preview="))
        {
            auto folder = juce::File(commandLine.fromFirstOccurrenceOf("=", false, false).unquoted());
            juce::Timer::callAfterDelay(1800, [this, folder] {
                if (auto* c = dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
                    c->exportDesignPreviews(folder);
                quit();
            });
        }
    }

    void shutdown() override { mainWindow.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name, juce::Colour(0xff17130f), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setResizeLimits(1020, 860, 2600, 1800);
            auto* content = new MainComponent();
            setContentOwned(content, true);
            if (const auto savedBounds = content->appSettings().loadWindowBounds())
                setBounds(savedBounds->withSize(juce::jmax(1020, savedBounds->getWidth()), juce::jmax(860, savedBounds->getHeight())));
            else
                centreWithSize(1280, 900);
            setVisible(true);
        }

        ~MainWindow() override
        {
            if (auto* content = dynamic_cast<MainComponent*>(getContentComponent()))
                content->appSettings().saveWindowBounds(getBounds());
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(NamTestProgramApplication)

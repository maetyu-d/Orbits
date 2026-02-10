#include <JuceHeader.h>
#include "MainComponent.h"

class SpiralTimelineApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Orbits"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name)
            : juce::DocumentWindow(name,
                                   juce::Desktop::getInstance().getDefaultLookAndFeel()
                                       .findColour(juce::ResizableWindow::backgroundColourId),
                                   juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setResizeLimits(920, 640, 3000, 2000);
            setContentOwned(new MainComponent(), true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(SpiralTimelineApplication)

#pragma once

#include <JuceHeader.h>

#include "../common/LookAndFeel.h"

using namespace juce;


namespace MoTool {

class MainWindow final : public DocumentWindow
{
public:
    MainWindow()
        : DocumentWindow("", Colors::Theme::backgroundAlt, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setSize(1024, 740);
        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    ~MainWindow() override {
        clearContentComponent();
    }

    void closeButtonPressed() override {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};


} // namespace MoTool

#pragma once

#include <JuceHeader.h>
#include <functional>

using namespace juce;

namespace MoTool {

class MainWindow final : public DocumentWindow
{
public:
    MainWindow(tracktion::Engine& engine);

    ~MainWindow() override;

    void setCloseHandler(std::function<void()> handler);
    void closeButtonPressed() override;
    void resized() override;

private:
    void saveWindowBounds();
    void restoreWindowBounds();

    tracktion::Engine& engine_;
    bool windowBoundsRestored_ = false;
    ComponentBoundsConstrainer constrainer_;
    std::function<void()> closeHandler_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};


} // namespace MoTool

#pragma once

#include <JuceHeader.h>

namespace MoTool {

namespace te = tracktion;

class AYPluginSidePanel : public juce::Component,
                          private juce::ChangeListener
{
public:
    AYPluginSidePanel(te::Edit& edit, te::SelectionManager& selectionManager);
    ~AYPluginSidePanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void updatePluginUI();

    te::Edit& edit_;
    te::SelectionManager& selectionManager_;
    std::unique_ptr<juce::Component> pluginUI_;
    juce::Label noPluginLabel_ { {}, "No AY plugin" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYPluginSidePanel)
};

}  // namespace MoTool

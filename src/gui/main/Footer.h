#pragma once

#include <JuceHeader.h>

#include <common/Utilities.h>  // from Tracktion


using namespace juce;

namespace MoTool {

class FooterBar: public Component {
public:
    explicit FooterBar(te::Engine& engine);
    void resized() override;

private:
    te::Engine& engine_;

    TextButton audioSettingsButton_ { "Audio" },
               pluginListButton_    { "Plugins" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FooterBar)
};

}  // namespace MoTool

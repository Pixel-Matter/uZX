#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>

#include "layout/Layout.h"

using namespace juce;
namespace lo = Layout;

class FooterBar: public Component {
public:

    explicit FooterBar(te::Engine& engine)
        : engine_ {engine}
        // , edit_ {edit}
    {
        Helpers::addAndMakeVisible (*this, {
            &pluginListButton_,
            &audioSettingsButton_
        });
        audioSettingsButton_.onClick = [this] { EngineHelpers::showAudioDeviceSettings(engine_); };

        // Show the plugin scan dialog
        // If you're loading an Edit with plugins in, you'll need to perform a scan first
        pluginListButton_.onClick = [this]
        {
            DialogWindow::LaunchOptions o;
            o.dialogTitle                   = TRANS("Plugins");
            o.dialogBackgroundColour        = juce::Colours::black;
            o.escapeKeyTriggersCloseButton  = true;
            o.useNativeTitleBar             = true;
            o.resizable                     = true;
            o.useBottomRightCornerResizer   = true;

            auto v = new PluginListComponent (engine_.getPluginManager().pluginFormatManager,
                                              engine_.getPluginManager().knownPluginList,
                                              engine_.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal"),
                                              std::addressof (engine_.getPropertyStorage().getPropertiesFile()));
            v->setSize (800, 600);
            o.content.setOwned (v);
            o.launchAsync();
        };

    }

    void resized() override {
        // TODO use layout system, but first we need empty space to fill
        auto b = getLocalBounds();
        int w = 80;
        pluginListButton_.setBounds(b.removeFromRight(w).reduced(2));
        audioSettingsButton_.setBounds(b.removeFromRight(w).reduced(2));
    }

private:
    te::Engine& engine_;
    // te::Edit& edit_;

    TextButton audioSettingsButton_ { "Audio" },
               pluginListButton_    { "Plugins" };
    // lo::HorizontalLayout layout_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FooterBar)
};

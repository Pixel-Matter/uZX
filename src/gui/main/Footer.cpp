#include "Footer.h"

using namespace juce;

namespace MoTool {

FooterBar::FooterBar(te::Engine& engine)
    : engine_ {engine}
{
    Helpers::addAndMakeVisible (*this, {
        &pluginListButton_,
        &audioSettingsButton_
    });
    // Route through our UIBehaviour so the dialog uses the same input/output validation
    // (see ExtUIBehaviour::showSettingsScreen) as the menu/transport entry points.
    audioSettingsButton_.onClick = [] { te::AppFunctions::showSettingsScreen(); };

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

void FooterBar::resized() {
    // TODO use layout system, but first we need empty space to fill
    auto b = getLocalBounds();
    int w = 80;
    pluginListButton_.setBounds(b.removeFromRight(w).reduced(2));
    audioSettingsButton_.setBounds(b.removeFromRight(w).reduced(2));
}

}  // namespace MoTool
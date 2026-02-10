#include "AYPluginSidePanel.h"

#include "../../plugins/uZX/aychip/AYPlugin.h"
#include "../../plugins/uZX/aychip/AYPluginEditor.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

AYPluginSidePanel::AYPluginSidePanel(te::Edit& edit, te::SelectionManager& selectionManager)
    : edit_(edit)
    , selectionManager_(selectionManager)
{
    selectionManager_.addChangeListener(this);

    noPluginLabel_.setJustificationType(juce::Justification::centred);
    noPluginLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(noPluginLabel_);

    updatePluginUI();
}

AYPluginSidePanel::~AYPluginSidePanel() {
    selectionManager_.removeChangeListener(this);
}

void AYPluginSidePanel::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);
}

void AYPluginSidePanel::resized() {
    auto r = getLocalBounds();
    if (pluginUI_)
        pluginUI_->setBounds(r);
    noPluginLabel_.setBounds(r);
}

void AYPluginSidePanel::changeListenerCallback(juce::ChangeBroadcaster*) {
    updatePluginUI();
}

void AYPluginSidePanel::updatePluginUI() {
    // Find selected track
    te::AudioTrack* selectedTrack = nullptr;
    auto sel = selectionManager_.getSelectedObject(0);
    selectedTrack = dynamic_cast<te::AudioTrack*>(sel);

    // If no track selected, try to find first audio track
    if (selectedTrack == nullptr) {
        auto tracks = te::getAudioTracks(edit_);
        if (!tracks.isEmpty())
            selectedTrack = tracks.getFirst();
    }

    // Find AYChipPlugin on the track
    te::Plugin::Ptr ayPlugin;
    if (selectedTrack != nullptr) {
        for (auto& p : selectedTrack->pluginList) {
            if (dynamic_cast<uZX::AYChipPlugin*>(p)) {
                ayPlugin = p;
                break;
            }
        }
    }

    // Check if we already show this plugin
    if (pluginUI_ != nullptr) {
        if (auto* existingUI = dynamic_cast<uZX::AYPluginUI*>(pluginUI_.get())) {
            if (existingUI->getPlugin() == ayPlugin)
                return;
        }
    }

    // Remove old UI
    pluginUI_.reset();

    if (ayPlugin != nullptr) {
        auto ui = std::make_unique<uZX::AYPluginUI>(ayPlugin);
        addAndMakeVisible(ui.get());
        pluginUI_ = std::move(ui);
        noPluginLabel_.setVisible(false);
    } else {
        noPluginLabel_.setVisible(true);
    }

    resized();
}

}  // namespace MoTool

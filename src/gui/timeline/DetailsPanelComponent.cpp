#include "DetailsPanelComponent.h"

#include "../common/LookAndFeel.h"
#include "PsgParamEditorComponent.h"
#include "TimelineGrid.h"
#include "../devices/TrackDevicesPanel.h"

namespace MoTool {

//==============================================================================
// PsgParamWrapper implementation
DetailsPanelComponent::PsgParamWrapper::PsgParamWrapper(EditViewState& evs,
                                                        PsgParamEditorComponent* editor,
                                                        TabbedComponent* tabbedComp)
    : editViewState_(evs), tabbedComponent_(tabbedComp), editor_(editor) {
    addAndMakeVisible(editor_.get());
}

void DetailsPanelComponent::PsgParamWrapper::resized() {
    auto bounds = getLocalBounds();
    const int headerWidth = editViewState_.getTrackHeaderWidth();
    const int tabBarWidth = tabbedComponent_ ? tabbedComponent_->getTabBarDepth() : 0;
    // Align plot with track content: remove (headerWidth - tabBarWidth) since tabBar already provides tabBarWidth
    // offset
    bounds.removeFromLeft(headerWidth - tabBarWidth - 8);
    editor_->setBounds(bounds);
}

//==============================================================================
DetailsPanelComponent::DetailsPanelComponent(EditViewState& evs, TimelineGrid& g) : editViewState(evs) {
    tabbedComponent.setOutline(0);
    tabbedComponent.addTab("Clip Parameters", Colors::Theme::background,
                           new PsgParamWrapper(evs, new PsgParamEditorComponent(evs, g), &tabbedComponent), true);
    tabbedComponent.addTab("Track Devices", Colors::Theme::background, new TrackDevicesPanel(evs), true);

    addAndMakeVisible(tabbedComponent);

    editViewState.selectionManager.addChangeListener(this);

    updateTabVisibility();
}

DetailsPanelComponent::~DetailsPanelComponent() {
    editViewState.selectionManager.removeChangeListener(this);
}

void DetailsPanelComponent::paint(Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);
}

void DetailsPanelComponent::resized() {
    auto bounds = getLocalBounds();
    bounds.removeFromLeft(8);
    tabbedComponent.setBounds(bounds.reduced(0, 8));
}

void DetailsPanelComponent::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &editViewState.selectionManager) {
        updateTabVisibility();
    }
}

void DetailsPanelComponent::updateTabVisibility() {
    auto* psgClip = editViewState.selectionManager.getFirstItemOfType<PsgClip>();
    auto* track = editViewState.selectionManager.getFirstItemOfType<tracktion::Track>();

    if (psgClip) {
        // auto parametersTabName = psgClip->getName().isNotEmpty() ? psgClip->getName() : "Clip Parameters";
        // tabbedComponent.setTabName(0, parametersTabName);
        tabbedComponent.setCurrentTabIndex(0, true);
    } else if (track) {
        // auto devicesTabName = track->getName().isNotEmpty() ? track->getName() : "Track Devices";
        // tabbedComponent.setTabName(1, devicesTabName);
        tabbedComponent.setCurrentTabIndex(1, true);
    }
}

}  // namespace MoTool
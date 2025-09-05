#include "DetailsPanelComponent.h"

#include "../common/LookAndFeel.h"
#include "PsgParamEditorComponent.h"
#include "TimelineGrid.h"
#include "TrackDevicesComponent.h"

namespace MoTool {

DetailsPanelComponent::DetailsPanelComponent(EditViewState& evs, TimelineGrid& g)
    : editViewState(evs)
{
    tabbedComponent.addTab("Parameters", Colors::Theme::backgroundAlt, new PsgParamEditorComponent(evs, g), true);
    tabbedComponent.addTab("Devices", Colors::Theme::backgroundAlt, new TrackDevicesComponent(evs), true);

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
    bounds.removeFromLeft(editViewState.showHeaders ? editViewState.headersWidth : 0);
    bounds.removeFromLeft(-tabbedComponent.getTabBarDepth());

    tabbedComponent.setBounds(bounds.reduced(0, 8));
}

void DetailsPanelComponent::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &editViewState.selectionManager) {
        updateTabVisibility();
    }
}

void DetailsPanelComponent::updateTabVisibility() {
    bool hasPsgClipSelected = editViewState.selectionManager.getFirstItemOfType<PsgClip>() != nullptr;
    bool hasTrackSelected = editViewState.selectionManager.getFirstItemOfType<tracktion::Track>() != nullptr;

    if (hasPsgClipSelected) {
        tabbedComponent.setCurrentTabIndex(0, true);
    } else if (hasTrackSelected) {
        tabbedComponent.setCurrentTabIndex(1, true);
    }
    // If nothing is selected, keep the current tab visible
}

}  // namespace MoTool
#include "DetailsPanelComponent.h"

#include "../common/LookAndFeel.h"
#include "PsgParamEditorComponent.h"
#include "TimelineGrid.h"
#include "../devices/TrackDevicesPanel.h"

namespace MoTool {

DetailsPanelComponent::DetailsPanelComponent(EditViewState& evs, TimelineGrid& g)
    : editViewState(evs)
{
    tabbedComponent.addTab("Clip Parameters", Colors::Theme::backgroundAlt, new PsgParamEditorComponent(evs, g), true);
    tabbedComponent.addTab("Track Devices", Colors::Theme::backgroundAlt, new TrackDevicesPanel(evs), true);

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
    auto* psgClip = editViewState.selectionManager.getFirstItemOfType<PsgClip>();
    auto* track = editViewState.selectionManager.getFirstItemOfType<tracktion::Track>();

    if (psgClip) {
        auto parametersTabName = psgClip->getName().isNotEmpty() ? psgClip->getName() : "Clip Parameters";
        tabbedComponent.setTabName(0, parametersTabName);
        tabbedComponent.setCurrentTabIndex(0, true);
    } else if (track) {
        auto devicesTabName = track->getName().isNotEmpty() ? track->getName() : "Track Devices";
        tabbedComponent.setTabName(1, devicesTabName);
        tabbedComponent.setCurrentTabIndex(1, true);
    }
}

}  // namespace MoTool
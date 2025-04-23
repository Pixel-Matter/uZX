#include <JuceHeader.h>

#include "EditComponent.h"
#include "../common/Components.h"
#include "TrackComponents.h"

namespace MoTool {


//==============================================================================
EditComponent::EditComponent(te::Edit& e, EditViewState& evs)
    : edit(e)
    , editViewState(evs)
{
    editViewState.state.addListener(this);
    // editViewState.selectionManager.addChangeListener(this);
    // editViewState.zoom.addListener(this);

    playhead.setAlwaysOnTop(true);

    trackViewport.setViewedComponent(&tracksContainer, false);
    trackViewport.setScrollBarsShown(true, false);

    addAndMakeVisible(playhead);
    addAndMakeVisible(ruler);
    addAndMakeVisible(detailsPanel);
    addAndMakeVisible(trackViewport);
}

EditComponent::~EditComponent() {
    // editViewState.zoom.removeListener(this);
    editViewState.state.removeListener(this);
    // editViewState.selectionManager.removeChangeListener(this);
}

void EditComponent::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) {
    if (v.hasType(IDs::EDITVIEWSTATE)) {
        if (i == IDs::showHeaders || i == IDs::showFooters || i == IDs::headersWidth) {
            markAndUpdate(updateSizes);
        }
    }
}

// void EditComponent::zoomChanged() {
//     // markAndUpdate(updateZoom);
// }

void EditComponent::handleAsyncUpdate() {
    if (compareAndReset(updateSizes)) {
        resized();
    }
}

void EditComponent::resized() {
    // also get called on updated zoom
    const int rulerHeight = 32;
    const int footerWidth = editViewState.showFooters ? 100 : 0;
    const auto headerWidth = editViewState.showHeaders ? editViewState.headersWidth : 0;
    auto r = getLocalBounds();

    detailsPanel.setBounds(r.removeFromBottom(300));
    playhead.setBounds(r.withTrimmedLeft(headerWidth).withTrimmedRight(footerWidth));
    ruler.setBounds(r.removeFromTop(rulerHeight).withTrimmedLeft(headerWidth).withTrimmedRight(footerWidth));
    trackViewport.setBounds(r);
    tracksContainer.setBounds(r.withHeight(tracksContainer.getIdealHeight()));
}

// void EditComponent::componentMovedOrResized(Component& /*component*/, bool /*wasMoved*/, bool /*wasResized*/) {
//     // if trackHeaderOverlay mas resized, we need to update the bounds of ruler and playhead
//     // resized();
// }

// void EditComponent::mouseDown(const MouseEvent& e) {
// }

}  // namespace MoTool

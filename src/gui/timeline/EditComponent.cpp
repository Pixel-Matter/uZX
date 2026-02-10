#include <JuceHeader.h>

#include "EditComponent.h"
#include "TrackComponents.h"
#include "PlayheadComponent.h"
#include "../../controllers/App.h"

namespace MoTool {


//==============================================================================
EditComponent::EditComponent(te::Edit& e, EditViewState& evs, EditComponentOptions opts)
    : options(opts)
    , edit(e)
    , editViewState(evs)
{
    editViewState.state.addListener(this);

    playhead.setAlwaysOnTop(true);

    trackViewport.setViewedComponent(&tracksContainer, false);
    trackViewport.setScrollBarsShown(true, false);

    addAndMakeVisible(playhead);
    addAndMakeVisible(ruler);
    addAndMakeVisible(trackViewport);
    if (options.showDetailsPanel)
        addAndMakeVisible(detailsPanel);
}

EditComponent::~EditComponent() {
    editViewState.state.removeListener(this);
}

void EditComponent::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) {
    if (v.hasType(IDs::EDITVIEWSTATE)) {
        if (i == IDs::showHeaders || i == IDs::headersWidth) {
            markAndUpdate(updateSizes);
        }
    }
}

void EditComponent::handleAsyncUpdate() {
    if (compareAndReset(updateSizes)) {
        resized();
    }
}

void EditComponent::paint(Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
}

void EditComponent::resized() {
    // also get called on updated zoom
    const int rulerHeight = 24;
    const auto headerWidth = editViewState.getTrackHeaderWidth();
    auto r = getLocalBounds();

    editViewState.zoom.setViewWidthPx(r.getWidth() - headerWidth);
    if (options.showDetailsPanel) {
        detailsPanel.setBounds(r.removeFromBottom(300));
        detailsPanel.resized();  // for internal components
    }
    playhead.setBounds(r.withTrimmedLeft(headerWidth));
    ruler.setBounds(r.removeFromTop(rulerHeight).withTrimmedLeft(headerWidth));
    trackViewport.setBounds(r);
    tracksContainer.setBounds(r.withHeight(jmax(tracksContainer.getIdealHeight(), r.getHeight())));
}

void EditComponent::ZoomableViewport::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) {
    if (e.mods.isCommandDown()) {
        auto headerWidth = editViewState.getTrackHeaderWidth();
        editViewState.zoom.zoomAroundX(wheel.deltaY, e.x - headerWidth);
    } else {
        Viewport::mouseWheelMove(e, wheel);
    }
}

}  // namespace MoTool

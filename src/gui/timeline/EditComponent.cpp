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

    playhead.setAlwaysOnTop(true);

    trackViewport.setViewedComponent(&tracksContainer, false);
    trackViewport.setScrollBarsShown(true, false);

    addAndMakeVisible(playhead);
    addAndMakeVisible(ruler);
    addAndMakeVisible(detailsPanel);
    addAndMakeVisible(trackViewport);
}

EditComponent::~EditComponent() {
    editViewState.state.removeListener(this);
}

void EditComponent::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) {
    if (v.hasType(IDs::EDITVIEWSTATE)) {
        if (i == IDs::showHeaders || i == IDs::showFooters || i == IDs::headersWidth) {
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
    const int rulerHeight = 32;
    const int footerWidth = editViewState.showFooters ? 100 : 0;
    const auto headerWidth = editViewState.showHeaders ? editViewState.headersWidth : 0;
    auto r = getLocalBounds();

    detailsPanel.setBounds(r.removeFromBottom(300));
    playhead.setBounds(r.withTrimmedLeft(headerWidth).withTrimmedRight(footerWidth));
    ruler.setBounds(r.removeFromTop(rulerHeight).withTrimmedLeft(headerWidth).withTrimmedRight(footerWidth));
    trackViewport.setBounds(r);
    tracksContainer.setBounds(r.withHeight(jmax(tracksContainer.getIdealHeight(), r.getHeight())));
}

}  // namespace MoTool

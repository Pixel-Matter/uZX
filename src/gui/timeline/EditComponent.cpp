#include <JuceHeader.h>

#include "EditComponent.h"
#include "../common/Components.h"

namespace MoTool {

//==============================================================================
EditComponent::EditComponent(te::Edit& e, EditViewState& evs)
    : edit (e)
    , editViewState (evs)
{
    edit.state.addListener(this);
    editViewState.selectionManager.addChangeListener(this);
    editViewState.state.addListener(this);
    editViewState.zoom.addListener(this);

    addAndMakeVisible(playhead);
    addAndMakeVisible(ruler);
    markAndUpdate(updateTracks);
}

EditComponent::~EditComponent() {
    editViewState.zoom.removeListener(this);
    editViewState.state.removeListener(this);
    editViewState.selectionManager.removeChangeListener(this);
    edit.state.removeListener(this);
}

void EditComponent::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) {
    // FIXME abstraction leaked. Change to EditViewState::Listener
    if (v.hasType(IDs::EDITVIEWSTATE)) {
        if (i == IDs::showHeaders
                 || i == IDs::showFooters) {
            markAndUpdate(updateZoom);
        } else if (i == IDs::drawWaveforms) {
            repaint();
        }
    }
}

void EditComponent::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& c) {
    if (te::TrackList::isTrack(c))
        markAndUpdate(updateTracks);
}

void EditComponent::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& c, int) {
    if (te::TrackList::isTrack(c))
        markAndUpdate(updateTracks);
}

void EditComponent::valueTreeChildOrderChanged (juce::ValueTree& v, int a, int b) {
    if (te::TrackList::isTrack(v.getChild(a)))
        markAndUpdate(updateTracks);
    else if (te::TrackList::isTrack(v.getChild(b)))
        markAndUpdate(updateTracks);
}

void EditComponent::handleAsyncUpdate() {
    // DBG("EditComponent::handleAsyncUpdate");
    if (compareAndReset(updateTracks)) {
        buildTracks();
    }
    if (compareAndReset(updateZoom)) {
        resized();
        ruler.repaint();
    }
}

void EditComponent::resized() {
    const int trackHeight = 160, trackGap = 2, rulerHeight = 32;
    const int headerWidth = editViewState.showHeaders ? editViewState.headersWidth : 0;
    const int footerWidth = editViewState.showFooters ? 100 : 0;

    playhead.setBounds(getLocalBounds().withTrimmedLeft(headerWidth).withTrimmedRight(footerWidth));

    int y = roundToInt(editViewState.zoom.getViewY());

    ruler.setBounds(headerWidth, y, getWidth() - headerWidth - footerWidth, rulerHeight);
    y += rulerHeight;

    for (auto t : trackRows) {
        // TODO get trackHeight from trackViewState
        t->setBounds(0, y, getWidth(), trackHeight);
        t->resized();
        y += trackHeight + trackGap;
    }
}

void EditComponent::buildTracks() {
    trackRows.clear();

    for (auto t : getAllTracks(edit)) {
        TrackRowComponent* c = nullptr;

        if (t->isMasterTrack()) {
            if (editViewState.showMasterTrack)
                c = new TrackRowComponent(editViewState, t);
        } else if (t->isTempoTrack()) {
            if (editViewState.showGlobalTrack)
                c = new TrackRowComponent(editViewState, t);
        } else if (t->isMarkerTrack()) {
            if (editViewState.showMarkerTrack)
                c = new TrackRowComponent(editViewState, t);
        } else if (t->isChordTrack()) {
            if (editViewState.showChordTrack)
                c = new TrackRowComponent(editViewState, t);
        } else if (t->isArrangerTrack()) {
            if (editViewState.showArrangerTrack)
                c = new TrackRowComponent(editViewState, t);
        } else {
            c = new TrackRowComponent(editViewState, t);
        }

        if (c != nullptr) {
            trackRows.add(c);
            addAndMakeVisible(c);
        }
    }

    playhead.toFront(false);
    resized();
}

void EditComponent::mouseDown(const MouseEvent& e) {
    editViewState.selectionManager.deselectAll();

    // NOTE can't move this functionality to PlayheadComponent because we should be able to select clips and tracks as well
    auto rulerRect = ruler.getBounds();
    if (e.x > rulerRect.getX() && e.x < rulerRect.getX() + rulerRect.getWidth())
        ruler.repositionTransportToX(e.x - ruler.getX());
}

}  // namespace MoTool

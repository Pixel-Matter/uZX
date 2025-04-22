#include <JuceHeader.h>

#include "EditComponent.h"
#include "../common/Components.h"
#include "TrackComponents.h"

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
    addAndMakeVisible(trackHeaderOverlay);
    trackHeaderOverlay.setAlwaysOnTop(true);
    trackHeaderOverlay.addComponentListener(this);

    markAndUpdate(updateTracks);
}

EditComponent::~EditComponent() {
    editViewState.zoom.removeListener(this);
    editViewState.state.removeListener(this);
    editViewState.selectionManager.removeChangeListener(this);
    edit.state.removeListener(this);
}

void EditComponent::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) {
    if (v.hasType(IDs::EDITVIEWSTATE)) {
        if (i == IDs::showHeaders || i == IDs::showFooters) {
            markAndUpdate(updateZoom);
        } else if (i == IDs::drawWaveforms) {
            // TODO move to track body?
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
    if (compareAndReset(updateTracks)) {
        buildTracks();
    }
    if (compareAndReset(updateZoom)) {
        resized();
        ruler.repaint();
    }
}

void EditComponent::resized() {
    // also get called on updated zoom
    const int trackGap = 0, rulerHeight = 32;
    const int footerWidth = editViewState.showFooters ? 100 : 0;

    const auto headerWidth = trackHeaderOverlay.getWidth();
    auto r = getLocalBounds();
    playhead.setBounds(r.withTrimmedLeft(headerWidth).withTrimmedRight(footerWidth));

    ruler.setBounds(r.removeFromTop(rulerHeight).withTrimmedLeft(headerWidth).withTrimmedRight(footerWidth));
    trackHeaderOverlay.setBounds(r.withWidth(headerWidth));

    int y = roundToInt(editViewState.zoom.getViewY());
    y += rulerHeight;

    for (auto t : trackRows) {
        t->setBounds(0, y, getWidth(), t->getTrackHeight());
        // do not remove t->resized(); because it triggers repainting on zoom/scroll change when bounds not change
        t->resized();
        y += t->getTrackHeight() + trackGap;
    }
}

void EditComponent::componentMovedOrResized(Component& component, bool /*wasMoved*/, bool /*wasResized*/) {
    resized();
}

void EditComponent::buildTracks() {
    for (auto tr : trackRows) {
        tr->removeComponentListener(this);
        removeChildComponent(tr);
    }
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
            c->addComponentListener(this);
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

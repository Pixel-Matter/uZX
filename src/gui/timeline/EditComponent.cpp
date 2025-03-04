#include <JuceHeader.h>

#include "../common/Utilities.h"
#include "../common/Components.h"
#include "EditComponent.h"

namespace MoTool {

//==============================================================================
EditComponent::EditComponent(te::Edit& e, te::SelectionManager& sm)
    : edit (e)
    , editViewState (e, sm)
{
    edit.state.addListener(this);
    editViewState.selectionManager.addChangeListener(this);

    addAndMakeVisible(playhead);
    addAndMakeVisible(ruler);
    markAndUpdate(updateTracks);
}

EditComponent::~EditComponent() {
    editViewState.selectionManager.removeChangeListener(this);
    edit.state.removeListener(this);
}

void EditComponent::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) {
    if (v.hasType(IDs::EDITVIEWSTATE)) {
        if (i == IDs::viewX1
            || i == IDs::viewX2
            || i == IDs::viewY) {
            markAndUpdate(updateZoom);
        } else if (i == IDs::showHeaders
                 || i == IDs::showFooters) {
            markAndUpdate(updateZoom);
        } else if (i == IDs::drawWaveforms) {
            repaint();
        }
    }
}

void EditComponent::zoomTracksHorizontally(te::TimePosition pos, double factor) {
    editViewState.zoomHorizontally(pos, factor);
    markAndUpdate(updateZoom);
}

void EditComponent::zoomToFit() {
    auto range = Helpers::getEffectiveClipsTimeRange(edit);
    if (!range.isEmpty()) {
        editViewState.viewX1 = range.getStart();
        editViewState.viewX2 = range.getEnd();
        markAndUpdate(updateZoom);
    }
}

void EditComponent::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& c) {
    if (te::TrackList::isTrack (c))
        markAndUpdate(updateTracks);
}

void EditComponent::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& c, int) {
    if (te::TrackList::isTrack (c))
        markAndUpdate(updateTracks);
}

void EditComponent::valueTreeChildOrderChanged (juce::ValueTree& v, int a, int b) {
    if (te::TrackList::isTrack(v.getChild(a)))
        markAndUpdate(updateTracks);
    else if (te::TrackList::isTrack(v.getChild(b)))
        markAndUpdate(updateTracks);
}

void EditComponent::handleAsyncUpdate() {
    if (compareAndReset(updateTracks))
        buildTracks();
    if (compareAndReset(updateZoom))
        resized();
}

void EditComponent::resized() {
    jassert (headers.size() == tracks.size());

    const int trackHeight = 100, trackGap = 4, rulerHeight = 32;
    const int headerWidth = editViewState.showHeaders ? editViewState.headersWidth : 0;
    const int footerWidth = editViewState.showFooters ? 100 : 0;

    playhead.setBounds(getLocalBounds().withTrimmedLeft(headerWidth).withTrimmedRight(footerWidth));

    int y = roundToInt(editViewState.viewY.get());

    ruler.setBounds(headerWidth, y, getWidth() - headerWidth - footerWidth, rulerHeight);
    y += rulerHeight;

    for (int i = 0; i < jmin(headers.size(), tracks.size()); i++) {
        auto h = headers[i];
        auto t = tracks[i];
        auto f = footers[i];

        h->setBounds(0, y, headerWidth, trackHeight);
        t->setBounds(headerWidth, y, getWidth() - headerWidth - footerWidth, trackHeight);
        f->setBounds(getWidth() - footerWidth, y, footerWidth, trackHeight);

        y += trackHeight + trackGap;
    }

    for (auto t : tracks)
        t->resized();
}

void EditComponent::buildTracks() {
    tracks.clear();
    headers.clear();
    footers.clear();

    for (auto t : getAllTracks (edit)) {
        TrackComponent* c = nullptr;

        if (t->isMasterTrack()) {
            if (editViewState.showMasterTrack)
                c = new TrackComponent(editViewState, t);
        } else if (t->isTempoTrack()) {
            if (editViewState.showGlobalTrack)
                c = new TrackComponent(editViewState, t);
        } else if (t->isMarkerTrack()) {
            if (editViewState.showMarkerTrack)
                c = new TrackComponent(editViewState, t);
        } else if (t->isChordTrack()) {
            if (editViewState.showChordTrack)
                c = new TrackComponent(editViewState, t);
        } else if (t->isArrangerTrack()) {
            if (editViewState.showArrangerTrack)
                c = new TrackComponent(editViewState, t);
        } else {
            c = new TrackComponent(editViewState, t);
        }

        if (c != nullptr) {
            tracks.add(c);
            addAndMakeVisible(c);

            auto h = new TrackHeaderComponent(editViewState, t);
            headers.add(h);
            addAndMakeVisible(h);

            auto f = new TrackFooterComponent(editViewState, t);
            footers.add(f);
            addAndMakeVisible(f);
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

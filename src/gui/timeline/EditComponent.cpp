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
    edit.state.addListener(this);  // view state, and clips for the scroll range
    editViewState.zoom.addListener(this);

    playhead.setAlwaysOnTop(true);

    editViewState.transparentClips = options.transparentClips;
    editViewState.disableClipSelection = options.disableClipSelection;
    tracksContainer.setAutoFitTrackHeights(options.autoFitTrackHeights);

    trackViewport.setViewedComponent(&tracksContainer, false);
    tracksContainer.addMouseListener(this, true);
    trackViewport.setScrollBarsShown(!options.autoFitTrackHeights, false);

    addAndMakeVisible(playhead);
    addAndMakeVisible(ruler);
    addAndMakeVisible(trackViewport);
    timeScrollBar.setAutoHide(false);
    timeScrollBar.addListener(this);
    addAndMakeVisible(timeScrollBar);
    if (options.showDetailsPanel)
        addAndMakeVisible(detailsPanel);
}

EditComponent::~EditComponent() {
    tracksContainer.removeMouseListener(this);
    edit.state.removeListener(this);
    editViewState.zoom.removeListener(this);
}

void EditComponent::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) {
    if (v.hasType(IDs::EDITVIEWSTATE)) {
        if (i == IDs::showHeaders || i == IDs::headersWidth) {
            markAndUpdate(updateSizes);
        }
    } else if (te::Clip::isClipState(v) && (i == te::IDs::start || i == te::IDs::length)) {
        markAndUpdate(updateScrollRange);
    }
}

void EditComponent::valueTreeChildAdded(ValueTree&, ValueTree& c) {
    if (te::Clip::isClipState(c) || te::TrackList::isTrack(c))
        markAndUpdate(updateScrollRange);
}

void EditComponent::valueTreeChildRemoved(ValueTree&, ValueTree& c, int) {
    if (te::Clip::isClipState(c) || te::TrackList::isTrack(c))
        markAndUpdate(updateScrollRange);
}

void EditComponent::zoomChanged() {
    updateScrollBar();
}

void EditComponent::updateScrollBar() {
    const auto view = editViewState.zoom.getRange();
    const auto end = jmax(te::toPosition(edit.getLength()), view.getEnd());
    timeScrollBar.setRangeLimits(0.0, end.inSeconds(), dontSendNotification);
    timeScrollBar.setCurrentRange(view.getStart().inSeconds(), view.getLength().inSeconds(), dontSendNotification);
}

void EditComponent::scrollBarMoved(ScrollBar*, double newRangeStart) {
    editViewState.zoom.setStart(te::TimePosition::fromSeconds(newRangeStart));
}

void EditComponent::handleAsyncUpdate() {
    if (compareAndReset(updateSizes)) {
        resized();
    }
    if (compareAndReset(updateScrollRange))
        updateScrollBar();
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
    timeScrollBar.setBounds(r.removeFromBottom(getLookAndFeel().getDefaultScrollbarWidth()).withTrimmedLeft(headerWidth));
    playhead.setBounds(r.withTrimmedLeft(headerWidth));
    ruler.setBounds(r.removeFromTop(rulerHeight).withTrimmedLeft(headerWidth));
    trackViewport.setBounds(r);
    if (options.autoFitTrackHeights)
        tracksContainer.setBounds(r);
    else
        tracksContainer.setBounds(r.withHeight(jmax(tracksContainer.getIdealHeight(), r.getHeight())));
}

void EditComponent::mouseDown(const MouseEvent& e) {
    middleDragging = e.mods.isMiddleButtonDown();
    if (middleDragging)
        editViewState.zoom.beginDrag();
}

void EditComponent::mouseDrag(const MouseEvent& e) {
    if (middleDragging) {
        auto r = e.getEventRelativeTo(&ruler);  // ruler x is timeline x
        editViewState.zoom.dragBy(r.getMouseDownX(), r.getDistanceFromDragStartX(), r.getDistanceFromDragStartY());
    }
}

void EditComponent::mouseUp(const MouseEvent&) {
    if (middleDragging)  // whatever buttons are reported now, or following stays off
        editViewState.zoom.endDrag();
    middleDragging = false;
}

void EditComponent::TimeScrollBar::mouseDown(const MouseEvent& e) {
    zoom.beginDrag();
    ScrollBar::mouseDown(e);
}

void EditComponent::TimeScrollBar::mouseUp(const MouseEvent& e) {
    ScrollBar::mouseUp(e);
    zoom.endDrag();
}

void EditComponent::TimeScrollBar::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) {
    ruler.mouseWheelMove(e.getEventRelativeTo(&ruler), wheel);  // same x as the ruler above
}

void EditComponent::TimeScrollBar::mouseMagnify(const MouseEvent& e, float scale) {
    ruler.mouseMagnify(e.getEventRelativeTo(&ruler), scale);
}

void EditComponent::ZoomableViewport::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) {
    if (e.mods.isCommandDown()) {
        auto headerWidth = editViewState.getTrackHeaderWidth();
        editViewState.zoom.zoomAroundX(wheel.deltaY, e.x - headerWidth);
    } else if (e.mods.isShiftDown() || !isVerticalScrollBarShown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY)) {
        // sideways swipes, and any wheel where the tracks never scroll vertically (Player), scroll the timeline
        editViewState.zoom.scrollByWheel(wheel);
    } else {
        Viewport::mouseWheelMove(e, wheel);
    }
}

void EditComponent::ZoomableViewport::mouseMagnify(const MouseEvent& e, float scale) {
    editViewState.zoom.magnifyAroundX(scale, e.x - editViewState.getTrackHeaderWidth());
}

}  // namespace MoTool

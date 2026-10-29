#include <JuceHeader.h>

#include "PlayheadComponent.h"
#include "TimelineGrid.h"

#include "../common/LookAndFeel.h"

using namespace tracktion::literals;

namespace MoTool {


//==============================================================================
PlayheadComponent::PlayheadComponent(te::Edit& e, EditViewState& evs, TimelineGrid& g)
    : edit (e)
    , editViewState (evs)
    , grid (g)
{
    // edit.getTransport().state.addListener(this);
    editViewState.zoom.addListener(this);
}

PlayheadComponent::~PlayheadComponent() {
    // edit.getTransport().state.removeListener(this);
    editViewState.zoom.removeListener(this);
}

void PlayheadComponent::paint(Graphics& g) {
    g.setColour(Colors::Theme::success);
    g.drawRect(xPosition, 0, 2, getHeight());
}

bool PlayheadComponent::hitTest(int x, int) {
    if (std::abs(x - xPosition) <= 3)
        return true;

    return false;
}

void PlayheadComponent::mouseEnter(const MouseEvent&) {
    // TODO On Linux, don't set the mouse cursor until after the Component has appeared
    setMouseCursor(MouseCursor::LeftRightResizeCursor);
}

void PlayheadComponent::mouseDown(const MouseEvent&) {
    editViewState.zoom.beginDrag();  // the view does not follow the playhead being dragged
    edit.getTransport().setUserDragging(true);
}

void PlayheadComponent::mouseUp(const MouseEvent&) {
    edit.getTransport().setUserDragging(false);
    editViewState.zoom.endDrag();
}

// The playhead line sits on top of the timeline: pass wheel and pinch on to what is under it
Component* PlayheadComponent::getSiblingUnder(const MouseEvent& e) const {
    if (auto* parent = getParentComponent()) {
        auto posInParent = e.getEventRelativeTo(parent).getPosition();
        for (int i = parent->getNumChildComponents() - 1; i >= 0; --i)
            if (auto* child = parent->getChildComponent(i); child != this && child->getBounds().contains(posInParent))
                return child;
    }
    return nullptr;
}

void PlayheadComponent::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) {
    if (auto* sibling = getSiblingUnder(e))
        sibling->mouseWheelMove(e.getEventRelativeTo(sibling), wheel);
}

void PlayheadComponent::mouseMagnify(const MouseEvent& e, float scale) {
    if (auto* sibling = getSiblingUnder(e))
        sibling->mouseMagnify(e.getEventRelativeTo(sibling), scale);
}

void PlayheadComponent::mouseDrag(const MouseEvent& e) {
    // Past either edge, scroll by the overshoot, again and again while the mouse is held there
    auto r = getLocalBounds();
    auto x = jlimit(r.getX(), r.getRight() - 1, e.x);
    beginDragAutoRepeat(x != e.x ? 40 : 0);
    if (x != e.x)
        editViewState.zoom.scrollBy(e.x - x);
    auto t = jmax(0_tp, editViewState.zoom.xToTime(x));
    if (editViewState.shouldSnapPlayheadToGrid())
        t = grid.snapTimeToVisibleGrid(t);
    // DBG("PlayheadComponent::mouseDrag, mouseX: " << e.x << " clampedX: " << x << " time: " << t.inSeconds());
    edit.getTransport().setPosition(t);
    // DBG("PlayheadComponent::mouseDrag, transportPos after setPosition: " << edit.getTransport().getPosition().inSeconds());
    // fix for Playhead painting while dragging
    checkRepaint();
}

void PlayheadComponent::zoomOrPosChanged() {
    checkRepaint();
}

void PlayheadComponent::zoomChanged() {
    // needed to redraw playhead position if zooming out/in
    checkRepaint();
}

void PlayheadComponent::checkRepaint() {
    int newX = roundToInt(editViewState.zoom.timeToX(editViewState.zoom.getPlayheadPosition()));
    // DBG("PlayheadComponent::checkRepaint, pos: " << edit.getTransport().getPosition().inSeconds()
        // << " newX: " << newX << " oldX: " << xPosition);
    if (newX == xPosition)
        return;

    // The peer flushes deferred repaints at vblank; a mouse event landing between the
    // flush and paint() moves xPosition past the dirty region, so pad the rect by twice
    // the current step to keep the line inside the next frame's clip.
    const int step = std::abs(newX - xPosition);
    const int pad = 2 * step + 8;
    const int left = jmin(newX, xPosition) - pad;
    const int right = jmax(newX, xPosition) + 2 + pad;
    repaint(left, 0, right - left, getHeight());
    xPosition = newX;
}

}  // namespace MoTool

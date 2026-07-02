#include <JuceHeader.h>

#include "PlayheadComponent.h"

#include "../common/LookAndFeel.h"

using namespace tracktion::literals;

namespace MoTool {


//==============================================================================
PlayheadComponent::PlayheadComponent(te::Edit& e, EditViewState& evs)
    : edit (e)
    , editViewState (evs)
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
    edit.getTransport().setUserDragging(true);
}

void PlayheadComponent::mouseUp(const MouseEvent&) {
    edit.getTransport().setUserDragging(false);
}

void PlayheadComponent::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) {
    if (auto* parent = getParentComponent()) {
        auto posInParent = e.getEventRelativeTo(parent).getPosition();
        for (int i = parent->getNumChildComponents() - 1; i >= 0; --i) {
            auto* child = parent->getChildComponent(i);
            if (child != this && child->getBounds().contains(posInParent)) {
                child->mouseWheelMove(e.getEventRelativeTo(child), wheel);
                return;
            }
        }
    }
}

void PlayheadComponent::mouseDrag(const MouseEvent& e) {
    // TODO start horizontal scroll instead (if mouse is outside of the component)
    // limit x to LocalBounds
    auto r = getLocalBounds();
    auto x = jmax(jmin(e.x, r.getRight() - 1), r.getX());
    auto t = jmax(0_tp, editViewState.zoom.xToTime(x));
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
    int newX = roundToInt(editViewState.zoom.timeToX(edit.getTransport().getPosition()));
    // DBG("PlayheadComponent::checkRepaint, pos: " << edit.getTransport().getPosition().inSeconds()
        // << " newX: " << newX << " oldX: " << xPosition);
    if (newX != xPosition) {
        if (edit.getTransport().isUserDragging()) {
            // During a drag, mouse events can land between the peer's deferred-repaint
            // flush and the actual paint, so paint() runs with a dirty region that
            // predates the latest xPosition and narrow strips would clip the line out
            // entirely at high drag speeds. Invalidating everything keeps the line
            // inside every frame's clip region.
            repaint();
        } else {
            // Playback moves the line ~1px per update, so two narrow strips
            // (erase old, draw new) are enough and keep repaints cheap.
            repaint(xPosition - 1, 0, 4, getHeight());
            repaint(newX - 1, 0, 4, getHeight());
        }
        xPosition = newX;
    }
}

}  // namespace MoTool

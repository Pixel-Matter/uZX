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
    // TODO change to project framerate
    startTimerHz(30);
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

void PlayheadComponent::mouseDown(const MouseEvent&) {
    edit.getTransport().setUserDragging(true);
}

void PlayheadComponent::mouseUp(const MouseEvent&) {
    edit.getTransport().setUserDragging(false);
}

void PlayheadComponent::mouseDrag(const MouseEvent& e) {
    // TODO start horizontal scroll instead (if mouse is outside of the component)
    // limit x to LocalBounds
    auto r = getLocalBounds();
    auto x = jmax(jmin(e.x, r.getRight() - 1), r.getX());
    auto t = jmax(0_tp, editViewState.zoom.xToTime(x, getWidth()));
    edit.getTransport().setPosition(t);
    timerCallback();
}

void PlayheadComponent::timerCallback() {
    if (firstTimer) {
        // On Linux, don't set the mouse cursor until after the Component has appeared
        firstTimer = false;
        setMouseCursor(MouseCursor::LeftRightResizeCursor);
    }

    int newX = editViewState.zoom.timeToX(edit.getTransport().getPosition(), getWidth());
    if (newX != xPosition) {
        repaint(jmin(newX, xPosition) - 1, 0, jmax(newX, xPosition) - jmin(newX, xPosition) + 3, getHeight());
        xPosition = newX;
    }
}

}  // namespace MoTool

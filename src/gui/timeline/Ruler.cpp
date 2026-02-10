#include "Ruler.h"

namespace MoTool {

RulerComponent::RulerComponent(te::Edit& ed, EditViewState& evs, TimelineGrid& g)
    : te::TempoSequence::Listener {ed.tempoSequence}
    , edit {ed}
    , editViewState {evs}
    , grid {g}
{
    edit.tempoSequence.addListener(this);
    editViewState.state.addListener(this);
    editViewState.zoom.addListener(this);
    // cached value is ValueTree::Listener
    timecodeFormat.referTo(edit.state, te::IDs::timecodeFormat, nullptr, TimecodeDisplayFormatExt {TimecodeTypeExt::barsBeatsFps50});
}

RulerComponent::~RulerComponent() {
    edit.tempoSequence.removeListener(this);
    editViewState.state.removeListener(this);
    editViewState.zoom.removeListener(this);
}

void RulerComponent::zoomChanged() {
    repaint();
}

void RulerComponent::paint(Graphics& g) {
    auto ticks = grid.getTicks();
    auto bounds = getLocalBounds();
    g.fillAll(Colors::Theme::background);
    g.setFont(13.0f);

    for (const auto& tick : ticks) {
        if (tick.x > bounds.getWidth())
            continue;
        g.setColour(Colors::Timeline::trackGridTickColors[tick.level]);
        auto y = (float) bounds.getHeight() - (float) bounds.getHeight() / (float) (3 - tick.level);
        g.drawVerticalLine(tick.x, y, (float)bounds.getHeight());
        if (tick.label.isNotEmpty()) {
            g.drawSingleLineText(tick.label, tick.x + 2, 10, Justification::left);
        }
    }
}

void RulerComponent::mouseDown(const MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        PopupMenu m;
        m.addItem("Zoom in", [] {
            te::AppFunctions::zoomIn();
        });
        m.addItem("Zoom out", [] {
            te::AppFunctions::zoomOut();
        });
        m.addItem("Zoom to selection", [this] {
            edit.engine.getUIBehaviour().zoomToSelection();
        });
        m.addItem("Zoom fit", [this] {
            edit.engine.getUIBehaviour().zoomToFitHorizontally();
        });
        m.showMenuAsync({});
    } else {
        isDragging = false;
        dragStartViewStart = editViewState.zoom.getStart();
        dragStartTimePerPixel = editViewState.zoom.getTimePerPixel();
    }
}

void RulerComponent::mouseDrag(const MouseEvent& e) {
    if (e.mods.isPopupMenu())
        return;

    if (!isDragging && e.getDistanceFromDragStart() >= dragThreshold)
        isDragging = true;

    if (!isDragging)
        return;

    auto deltaX = e.getDistanceFromDragStartX();
    auto deltaY = e.getDistanceFromDragStartY();

    auto newTimePerPixel = dragStartTimePerPixel * std::pow(2.0, deltaY * 0.01);
    if (newTimePerPixel < 0.0005s) newTimePerPixel = te::TimeDuration::fromSeconds(0.0005);
    if (newTimePerPixel > 2s) newTimePerPixel = te::TimeDuration::fromSeconds(2.0);

    auto anchorTime = dragStartViewStart + dragStartTimePerPixel * (double) e.getMouseDownX();
    auto newStart = anchorTime - newTimePerPixel * (double)(e.getMouseDownX() + deltaX);
    newStart = jmax(te::TimePosition(), newStart);
    auto newSpan = newTimePerPixel * editViewState.zoom.getViewWidthPx();
    editViewState.zoom.setRange({newStart, newSpan});
}

void RulerComponent::mouseUp(const MouseEvent& e) {
    if (e.mods.isPopupMenu())
        return;

    if (!isDragging)
        repositionTransportToX(e.x);

    isDragging = false;
}

void RulerComponent::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) {
    if (e.mods.isCommandDown()) {
        editViewState.zoom.zoomAroundX(wheel.deltaY, e.x);
    } else {
        auto scrollAmount = editViewState.zoom.getTimePerPixel() * wheel.deltaY * 200.0;
        auto newStart = editViewState.zoom.getStart() - scrollAmount;
        editViewState.zoom.setStart(jmax(te::TimePosition(), newStart));
    }
}

void RulerComponent::repositionTransportToX(int x) {
    auto pos = editViewState.zoom.xToTime(x);
    edit.getTransport().setPosition(pos);
}

void RulerComponent::timerCallback() {
    repaint();
}

void RulerComponent::selectableObjectChanged(te::Selectable*) {
    repaint();
}

} // namespace MoTool
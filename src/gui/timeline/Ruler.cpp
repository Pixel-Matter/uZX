#include "Ruler.h"

#include "../../controllers/MainCommands.h"
#include "../../controllers/PlayWholeEdit.h"
#include "../../models/EditUtilities.h"
#include "../common/Utilities.h"

namespace MoTool {

RulerComponent::RulerComponent(te::Edit& ed, EditViewState& evs, TimelineGrid& g)
    : edit {ed}
    , editViewState {evs}
    , grid {g}
{
    editViewState.zoom.addListener(this);
    grid.addListener(this);
    edit.getTransport().state.addListener(this);
}

RulerComponent::~RulerComponent() {
    editViewState.zoom.removeListener(this);
    grid.removeListener(this);
    edit.getTransport().state.removeListener(this);
}

void RulerComponent::viewRangeChanged() {
    repaint();
}

void RulerComponent::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& prop) {
    if (prop == te::IDs::loopPoint1 || prop == te::IDs::loopPoint2 || prop == te::IDs::looping)
        repaint();
}

void RulerComponent::paint(Graphics& g) {
    auto ticks = grid.getTicks();
    auto bounds = getLocalBounds();
    g.fillAll(Colors::Theme::background);

    const auto bottom = (float) bounds.getHeight();
    const auto stripTop = (float) loopStripTop();

    // The loop range, under the ticks, with a triangle at each end pointing in. It spans whole pixels, from the
    // column where the grid draws a line at its start to the one of its end, as the loop lines down the tracks do.
    auto& transport = edit.getTransport();
    const auto loop = transport.getLoopRange();
    if (!loop.isEmpty()) {
        const auto& zoom = editViewState.zoom;
        const auto x1 = (float) roundToInt(zoom.timeToX(loop.getStart()));
        const auto x2 = (float) roundToInt(zoom.timeToX(loop.getEnd())) + 1.0f;
        g.setColour(transport.looping ? Colors::Timeline::loopOn : Colors::Timeline::loopOff);
        g.fillRect(Rectangle<float>::leftTopRightBottom(x1, stripTop, x2, bottom));

        const auto mid = (stripTop + bottom) * 0.5f;
        const auto size = jmin((bottom - stripTop) * 0.75f, (x2 - x1) / 3.0f);  // narrow ranges get smaller ones
        Path ends;
        ends.addTriangle(x1, stripTop, x1, bottom, x1 + size, mid);
        ends.addTriangle(x2, stripTop, x2, bottom, x2 - size, mid);
        g.setColour(transport.looping ? Colors::Timeline::loopEndOn : Colors::Timeline::loopEndOff);
        g.fillPath(ends);
    }

    g.setFont(13.0f);

    // The finest ticks fill the loop strip, the next reach halfway up the rest, the coarsest the full height
    for (const auto& tick : ticks) {
        if (tick.x > bounds.getWidth())
            continue;
        g.setColour(Colors::Timeline::trackGridTickColors[tick.level]);
        const auto top = stripTop * (float) (2 - (int) tick.level) / 2.0f;
        g.drawVerticalLine(tick.x, top, bottom);
    }

    // Resolve text collisions here, where the actual font is known; structural labels win.
    RectangleList<int> occupiedLabels;
    for (size_t level = Colors::Timeline::trackGridLabelColors.size(); level-- > 0;) {
        for (const auto& tick : ticks) {
            if (tick.level != level || tick.label.isEmpty() || tick.x > bounds.getWidth())
                continue;

            const int width = GlyphArrangement::getStringWidthInt(g.getCurrentFont(), tick.label);
            const Rectangle<int> labelBounds(tick.x + 2, 0, width + 4, bounds.getHeight());
            if (occupiedLabels.intersectsRectangle(labelBounds))
                continue;

            occupiedLabels.addWithoutMerging(labelBounds);
            g.setColour(Colors::Timeline::trackGridLabelColors[tick.level]);
            g.drawSingleLineText(tick.label, tick.x + 2, 10, Justification::left);
        }
    }
}

PopupMenu RulerComponent::createMenu() {
    PopupMenu m;
    if (auto* manager = edit.engine.getUIBehaviour().getApplicationCommandManager())
        m.addSubMenu("Zoom", Commands::MainAppCommands::createZoomMenu(manager, !editViewState.disableClipSelection));
    m.addSeparator();
    m.addSubMenu("Timecode Format", Helpers::buildTimecodeFormatMenu(edit));
    m.addSeparator();
    m.addItem("Clear Loop", isLoopSet(), false, [ruler = SafePointer<RulerComponent>(this)] {
        if (ruler != nullptr)
            ruler->clearLoop();
    });
    if (auto* manager = edit.engine.getUIBehaviour().getApplicationCommandManager()) {
        m.addSeparator();
        m.addCommandItem(manager, Commands::MainAppCommands::transportFollow);
        m.addCommandItem(manager, Commands::MainAppCommands::transportFollowByPage);
        m.addCommandItem(manager, Commands::MainAppCommands::transportFollowByScroll);
        m.addCommandItem(manager, Commands::MainAppCommands::viewSnapPlayheadToGrid);
        if (manager->getCommandForID(Commands::MainAppCommands::transportPreservePsgTiming) != nullptr)  // Studio's
            m.addCommandItem(manager, Commands::MainAppCommands::transportPreservePsgTiming);
    }
    return m;
}

bool RulerComponent::isLoopSet() const {
    auto& transport = edit.getTransport();
    return transport.looping || !PlayWholeEdit::isWholeEdit(transport.getLoopRange(), PlayWholeEdit::getEnd(edit));
}

void RulerComponent::clearLoop() {
    auto& transport = edit.getTransport();
    transport.setLoopRange({te::TimePosition(), PlayWholeEdit::getEnd(edit)});
    transport.looping = false;
}

void RulerComponent::mouseDown(const MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        createMenu().showMenuAsync({});
    } else {
        isDragging = false;
        loopDrag = isInLoopStrip(e) ? loopDragAt(e.x) : LoopDrag::none;
        loopAtDragStart = edit.getTransport().getLoopRange();
        dragStartTime = snappedTimeAt(e.x, e.mods);
    }
}

void RulerComponent::mouseDrag(const MouseEvent& e) {
    if (e.mods.isPopupMenu())
        return;

    if (!isDragging && e.getDistanceFromDragStart() >= dragThreshold) {
        isDragging = true;
        if (loopDrag == LoopDrag::none)
            editViewState.zoom.beginDrag();
    }

    if (!isDragging)
        return;

    if (loopDrag == LoopDrag::none) {
        editViewState.zoom.dragBy(e.getMouseDownX(), e.getDistanceFromDragStartX(), e.getDistanceFromDragStartY());
        return;
    }

    const auto loop = draggedLoop(loopDrag, loopAtDragStart, dragStartTime, snappedTimeAt(e.x, e.mods));
    if (loop.isEmpty())
        return;

    auto& transport = edit.getTransport();
    transport.setLoopRange(loop);
    if (loopDrag == LoopDrag::create)
        transport.looping = true;
}

void RulerComponent::mouseUp(const MouseEvent& e) {
    if (isDragging) {
        if (loopDrag == LoopDrag::none)  // whatever modifiers are held now, or following stays off
            editViewState.zoom.endDrag();
    } else if (!e.mods.isPopupMenu()) {
        edit.getTransport().setPosition(grid.playheadTimeAtX(e.x));
    }

    isDragging = false;
    loopDrag = LoopDrag::none;
}

void RulerComponent::mouseDoubleClick(const MouseEvent& e) {
    // Only where a drag would draw a new range: not on a range that could be moved, or on its edges
    if (e.mods.isPopupMenu() || !isInLoopStrip(e) || loopDragAt(e.x) != LoopDrag::create)
        return;

    if (const auto loop = loopOfView(); !loop.isEmpty()) {
        auto& transport = edit.getTransport();
        transport.setLoopRange(loop);
        transport.looping = true;
    }
}

void RulerComponent::mouseMove(const MouseEvent& e) {
    const auto drag = isInLoopStrip(e) ? loopDragAt(e.x) : LoopDrag::none;
    setMouseCursor(drag == LoopDrag::start || drag == LoopDrag::end ? MouseCursor::LeftRightResizeCursor
                   : drag == LoopDrag::move                         ? MouseCursor::DraggingHandCursor
                                                                    : MouseCursor::NormalCursor);
}

RulerComponent::LoopDrag RulerComponent::loopDragAt(int x) const {
    const auto loop = edit.getTransport().getLoopRange();
    if (loop.isEmpty())
        return LoopDrag::create;

    const auto& zoom = editViewState.zoom;
    return loopDragAt((float) x, zoom.timeToX(loop.getStart()), zoom.timeToX(loop.getEnd()), (float) getWidth(),
                      PlayWholeEdit::isWholeEdit(loop, PlayWholeEdit::getEnd(edit)));
}

RulerComponent::LoopDrag RulerComponent::loopDragAt(float x, float x1, float x2, float width, bool wholeProject) {
    const auto startDistance = std::abs(x - x1), endDistance = std::abs(x - x2);
    if (std::min(startDistance, endDistance) <= (float) loopEdgeWidth)
        return startDistance < endDistance ? LoopDrag::start : LoopDrag::end;

    // Half a pixel of slack: zoomed to a range, its ends are on the view's borders, give or take rounding
    const bool inView = x1 >= -0.5f && x2 <= width + 0.5f;
    return x > x1 && x < x2 && inView && !wholeProject ? LoopDrag::move : LoopDrag::create;
}

te::TimeRange RulerComponent::loopOfView() const {
    const auto& zoom = editViewState.zoom;
    const auto view = zoom.getRange();
    // Half a pixel of slack, as in loopDragAt: a line on the view's border is in view, whatever the rounding
    const auto slack = zoom.getTimePerPixel() * 0.5;
    const auto viewStart = jmax(te::TimePosition(), view.getStart() - slack), viewEnd = view.getEnd() + slack;
    const auto projectEnd = PlayWholeEdit::getEnd(edit);

    const auto start = grid.snapUpToVisibleGrid(viewStart);
    // The project's end ends the loop when in view, on the grid or not: zoomed to the project, all of it loops
    const auto end = viewEnd >= projectEnd ? projectEnd : grid.snapDownToVisibleGrid(viewEnd);
    return start < end ? te::TimeRange(start, end) : te::TimeRange();
}

te::TimePosition RulerComponent::snappedTimeAt(int x, const ModifierKeys& mods) const {
    const auto time = jmax(te::TimePosition(), editViewState.zoom.xToTime(x));
    if (!mods.isCommandDown())
        return grid.snapTimeToVisibleGrid(time);
    const auto fps = Helpers::getProjectFps(edit);
    return te::TimePosition::fromSeconds(std::round(time.inSeconds() * fps) / fps);
}

te::TimeRange RulerComponent::draggedLoop(LoopDrag drag, te::TimeRange original, te::TimePosition from,
                                          te::TimePosition to) {
    switch (drag) {
        case LoopDrag::create:
            return te::TimeRange::between(from, to);
        case LoopDrag::start:
            return te::TimeRange::between(to, original.getEnd());
        case LoopDrag::end:
            return te::TimeRange::between(original.getStart(), to);
        case LoopDrag::move: {
            const auto moved = original + (to - from);
            return moved.getStart() < te::TimePosition() ? moved + (te::TimePosition() - moved.getStart()) : moved;
        }
        case LoopDrag::none:
            return original;
    }
    jassertfalse;
    return original;
}

void RulerComponent::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) {
    if (e.mods.isCommandDown())
        editViewState.zoom.zoomAroundX(wheel.deltaY, e.x);
    else
        editViewState.zoom.scrollByWheel(wheel);
}

void RulerComponent::mouseMagnify(const MouseEvent& e, float scale) {
    editViewState.zoom.magnifyAroundX(scale, e.x);
}

void RulerComponent::gridChanged() {
    repaint();
}

} // namespace MoTool

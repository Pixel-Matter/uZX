#include "Ruler.h"

#include "../../controllers/MainCommands.h"
#include "../../models/EditUtilities.h"

namespace MoTool {

RulerComponent::RulerComponent(te::Edit& ed, EditViewState& evs, TimelineGrid& g)
    : edit {ed}
    , editViewState {evs}
    , grid {g}
{
    editViewState.zoom.addListener(this);
    grid.addListener(this);
}

RulerComponent::~RulerComponent() {
    editViewState.zoom.removeListener(this);
    grid.removeListener(this);
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
        m.addSeparator();
        m.addSubMenu("Timecode Format", Helpers::buildTimecodeFormatMenu(edit));
        if (auto* manager = edit.engine.getUIBehaviour().getApplicationCommandManager()) {
            m.addSeparator();
            m.addCommandItem(manager, Commands::MainAppCommands::viewSnapPlayheadToGrid);
            m.addCommandItem(manager, Commands::MainAppCommands::transportPreservePsgTiming);
        }
        m.showMenuAsync({});
    } else {
        isDragging = false;
    }
}

void RulerComponent::mouseDrag(const MouseEvent& e) {
    if (e.mods.isPopupMenu())
        return;

    if (!isDragging && e.getDistanceFromDragStart() >= dragThreshold) {
        isDragging = true;
        editViewState.zoom.beginDrag();
    }

    if (isDragging)
        editViewState.zoom.dragBy(e.getMouseDownX(), e.getDistanceFromDragStartX(), e.getDistanceFromDragStartY());
}

void RulerComponent::mouseUp(const MouseEvent& e) {
    if (isDragging)  // whatever modifiers are held now, or following stays off
        editViewState.zoom.endDrag();
    else if (!e.mods.isPopupMenu())
        edit.getTransport().setPosition(grid.playheadTimeAtX(e.x));

    isDragging = false;
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

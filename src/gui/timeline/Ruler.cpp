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
    g.setFont(12.0f);

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
        repositionTransportToX(e.x);
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
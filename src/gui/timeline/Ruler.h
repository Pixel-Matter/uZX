#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "TimelineGrid.h"


namespace MoTool {

/** The time ruler. Its upper half seeks on a click and scrolls and zooms on a drag; its lower half
    is the loop strip, which shows the loop range and seeks on a click too. */
class RulerComponent : public Component,
                       private ZoomViewState::Listener,
                       private TimelineGrid::Listener,
                       private juce::ValueTree::Listener
{
public:
    RulerComponent(te::Edit& ed, EditViewState& evs, TimelineGrid& g);
    ~RulerComponent() override;

    void viewRangeChanged() override;
    void paint(Graphics& g) override;
    void mouseMove(const MouseEvent& e) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;
    void mouseMagnify(const MouseEvent& e, float scale) override;

    /** What a drag in the loop strip changes: a new range, an edge, or the whole range. */
    enum class LoopDrag { none, create, start, end, move };

    /** The loop range a drag from `from` to `to` (both snapped) makes of `original`, the range when
        the drag started. A moved range stays at or after zero. */
    static te::TimeRange draggedLoop(LoopDrag, te::TimeRange original, te::TimePosition from, te::TimePosition to);

private:
    void gridChanged() override;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;

    int loopStripTop() const { return getHeight() / 2; }
    bool isInLoopStrip(const MouseEvent& e) const { return e.y >= loopStripTop(); }
    LoopDrag loopDragAt(int x) const;
    /** On the visible grid; on whole project frames while Cmd is held. */
    te::TimePosition snappedTimeAt(int x, const ModifierKeys& mods) const;

    te::Edit& edit;
    EditViewState& editViewState;
    TimelineGrid& grid;

    bool isDragging = false;
    LoopDrag loopDrag = LoopDrag::none;
    te::TimeRange loopAtDragStart;
    te::TimePosition dragStartTime;
    static constexpr int dragThreshold = 4;
    static constexpr int loopEdgeWidth = 4;  // pixels either side of a loop edge that grab it
};

} // namespace MoTool

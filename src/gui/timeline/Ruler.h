#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "TimelineGrid.h"


namespace MoTool {

/** The time ruler. Its upper half seeks on a click and scrolls and zooms on a drag; its lower half
    is the loop strip, which shows the loop range and seeks on a click too. A drag on the strip sets the
    range, and so does a double-click, to what is in view; a range made anew is looped at once. */
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
    void mouseDoubleClick(const MouseEvent& e) override;
    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;
    void mouseMagnify(const MouseEvent& e, float scale) override;

    /** The right-click menu: zoom, timecode format, Clear Loop, Follow and snap options. */
    PopupMenu createMenu();

    /** What a drag in the loop strip changes: a new range, an edge, or the whole range. */
    enum class LoopDrag { none, create, start, end, move };

    /** What a drag from x changes, in a strip `width` wide that draws the loop range from x1 to x2: an edge
        when x is on one; the range when x is inside it, both its ends are in view and it isn't the whole
        project; otherwise a new range, drawn over a range that is only the strip's background there. */
    static LoopDrag loopDragAt(float x, float x1, float x2, float width, bool wholeProject);

    /** The loop range a drag from `from` to `to` (both snapped) makes of `original`, the range when
        the drag started. A moved range stays at or after zero. */
    static te::TimeRange draggedLoop(LoopDrag, te::TimeRange original, te::TimePosition from, te::TimePosition to);

private:
    void gridChanged() override;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;

    int loopStripTop() const { return getHeight() / 2; }
    bool isInLoopStrip(const MouseEvent& e) const { return e.y >= loopStripTop(); }
    LoopDrag loopDragAt(int x) const;
    /** What a double-click loops: the grid lines in view, up to the project's end when that is in view. */
    te::TimeRange loopOfView() const;
    /** Whether there is a loop to clear: looping is on, or the range isn't the whole project. */
    bool isLoopSet() const;
    /** Back to where a project starts: the range over the whole project, following its end, and looping off. */
    void clearLoop();
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

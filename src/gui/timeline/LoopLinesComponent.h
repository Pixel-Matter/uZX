#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

//==============================================================================
/** The two ends of the loop range as lines down the tracks, so the range shows among the notes and not on
    the ruler alone: in the loop's colour while looping, grey while not, like the triangles on the loop strip.
    Lies over the tracks and lets the mouse through. */
class LoopLinesComponent : public Component, private ZoomViewState::Listener, private juce::ValueTree::Listener
{
public:
    LoopLinesComponent(tracktion::Edit& e, EditViewState& evs) : edit{e}, editViewState{evs} {
        setInterceptsMouseClicks(false, false);
        editViewState.zoom.addListener(this);
        edit.getTransport().state.addListener(this);
    }

    ~LoopLinesComponent() override {
        editViewState.zoom.removeListener(this);
        edit.getTransport().state.removeListener(this);
    }

    void paint(Graphics& g) override {
        auto& transport = edit.getTransport();
        const auto loop = transport.getLoopRange();
        if (loop.isEmpty())
            return;

        g.setColour(transport.looping ? Colors::Timeline::loopEndOn : Colors::Timeline::loopEndOff);
        // Rounded as the grid rounds its lines, so an end on a grid line is drawn on it and not beside it
        for (auto time : {loop.getStart(), loop.getEnd()})
            g.drawVerticalLine(roundToInt(editViewState.zoom.timeToX(time)), 0.0f, (float) getHeight());
    }

private:
    // ponytail: repaints its whole area, and so the tracks under it. A view change repaints those anyway; a loop
    // edge dragged over a slow timeline doesn't: if that stutters, repaint only the lines' old and new columns.
    void viewRangeChanged() override { repaint(); }

    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& prop) override {
        if (prop == tracktion::IDs::loopPoint1 || prop == tracktion::IDs::loopPoint2 || prop == tracktion::IDs::looping)
            repaint();
    }

    tracktion::Edit& edit;
    EditViewState& editViewState;
};

}  // namespace MoTool

/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

#pragma once

#include "tracktion_core/utilities/tracktion_Time.h"
#include "tracktion_core/utilities/tracktion_TimeRange.h"
#include <JuceHeader.h>
#include <common/Utilities.h>  // from Tracktion

namespace MoTool {

namespace IDs
{
    #define DECLARE_ID(name)  const juce::Identifier name(#name);
    DECLARE_ID(EDITVIEWSTATE)
    DECLARE_ID(showMasterTrack)
    DECLARE_ID(showGlobalTrack)
    DECLARE_ID(showMarkerTrack)
    DECLARE_ID(showChordTrack)
    DECLARE_ID(showMidiDevices)
    DECLARE_ID(showWaveDevices)
    DECLARE_ID(drawWaveforms)
    DECLARE_ID(showHeaders)
    DECLARE_ID(showFooters)
    DECLARE_ID(showArranger)
    DECLARE_ID(headersWidth)
    DECLARE_ID(ZOOMVIEWSTATE)
    DECLARE_ID(viewX1)
    DECLARE_ID(viewX2)
    DECLARE_ID(viewY)
    #undef DECLARE_ID
}

namespace te = tracktion;

//==============================================================================
class ZoomViewState {
public:
    ZoomViewState(te::Edit& e, ValueTree& st)
        : edit(e)
        , state(st)
    {
        state = edit.state.getOrCreateChildWithName(IDs::ZOOMVIEWSTATE, nullptr);
        auto um = &edit.getUndoManager();
        viewX1.referTo(state, IDs::viewX1, um, 0s);   // time of the left edge of the view
        viewX2.referTo(state, IDs::viewX2, um, 60s);  // time of the right edge of the view
        viewY.referTo(state, IDs::viewY, um, 0);      // not used yet
    }

    te::TimeRange getRange() const {
        return {viewX1, viewX2};
    }

    void setRange(te::TimeRange range) {
        viewX1 = range.getStart();
        viewX2 = range.getEnd();
    }

    te::TimePosition getRangeStart() const {
        return viewX1;
    }

    te::TimePosition getRangeEnd() const {
        return viewX2;
    }

    double getViewY() const {
        return viewY;
    }

    te::TimeDuration viewLength() const {
        return viewX2 - viewX1;
    }

    te::TimePosition beatToTime(te::BeatPosition b) const {
        auto& ts = edit.tempoSequence;
        return ts.toTime(b);
    }

    int timeToX(te::TimePosition time, int width) const {
        return roundToInt(((time - viewX1) * width) / viewLength());
    }

    te::TimePosition xToTime(int x, int width) const {
        return toPosition(viewLength() * (double (x) / width)) + toDuration(viewX1.get());
    }

    float durationToPixels(te::TimeDuration duration, int width) const {
        return (float)(duration * width / viewLength());
    }

    float pixelsPerBeat(te::TimeDuration beatDur, int width) const {
        return durationToPixels(beatDur, width);
    }

    float pixelsPerBeat(double beatDur, int width) const {
        return durationToPixels(te::TimeDuration::fromSeconds(beatDur), width);
    }

    void zoomHorizontally(te::TimePosition pos, double factor) {
        auto range = viewLength();
        auto newHalfRange = range * factor / 2.0;
        // limit zoom to 1s to 1 year
        if (newHalfRange > 0.5s && newHalfRange < 600s) {
            viewX1 = pos - newHalfRange;
            viewX1 = jmax(te::TimePosition(), viewX1.get());
            viewX2 = viewX1 + newHalfRange * 2.0;
        }
    }

private:
    te::Edit& edit;
    CachedValue<te::TimePosition> viewX1, viewX2;
    CachedValue<double> viewY;
    ValueTree state;
};


class EditViewState {
public:
    EditViewState (te::Edit& e, te::SelectionManager& s)
        : state(e.state.getOrCreateChildWithName(IDs::EDITVIEWSTATE, nullptr))
        , zoom(e, state)
        , selectionManager(s)
        , edit(e)
    {
        auto um = &edit.getUndoManager();
        showMasterTrack.referTo(state, IDs::showMasterTrack, um, false);
        showGlobalTrack.referTo(state, IDs::showGlobalTrack, um, false);
        showMarkerTrack.referTo(state, IDs::showMarkerTrack, um, false);
        showChordTrack.referTo(state, IDs::showChordTrack, um, false);
        showArrangerTrack.referTo(state, IDs::showArranger, um, false);
        drawWaveforms.referTo(state, IDs::drawWaveforms, um, true);
        showHeaders.referTo(state, IDs::showHeaders, um, true);
        showFooters.referTo(state, IDs::showFooters, um, false);
        showMidiDevices.referTo(state, IDs::showMidiDevices, um, false);
        showWaveDevices.referTo(state, IDs::showWaveDevices, um, true);

        headersWidth.referTo(state, IDs::headersWidth, nullptr, 150);
    }

    CachedValue<bool> showMasterTrack, showGlobalTrack, showMarkerTrack, showChordTrack, showArrangerTrack,
                      drawWaveforms, showHeaders, showFooters, showMidiDevices, showWaveDevices;
    CachedValue<int> headersWidth;

    ValueTree state;
    ZoomViewState zoom;
    te::SelectionManager& selectionManager;

private:
    te::Edit& edit;
};

}  // namespace MoTool

/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

#pragma once

#include "juce_events/juce_events.h"
#include "tracktion_core/utilities/tracktion_Time.h"
#include "tracktion_core/utilities/tracktion_TimeRange.h"
#include "tracktion_engine/tracktion_engine.h"
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
class ZoomViewState :
        private Timer,
        // private te::TransportControl::Listener,
                      private ChangeListener {
public:

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void zoomChanged() = 0;
    };

    ZoomViewState(te::Edit& e, ValueTree& st)
        : edit(e)
        , state(st)
    {
        state = edit.state.getOrCreateChildWithName(IDs::ZOOMVIEWSTATE, nullptr);
        auto um = &edit.getUndoManager();
        viewX1.referTo(state, IDs::viewX1, um, 0s);   // time of the left edge of the view
        viewX2.referTo(state, IDs::viewX2, um, 60s);  // time of the right edge of the view
        viewY.referTo(state, IDs::viewY, um, 0);      // not used yet
        // edit.getTransport().addListener(this);
        edit.getTransport().addChangeListener(this);
    }

    ~ZoomViewState() override {
        edit.getTransport().removeChangeListener(this);
        // edit.getTransport().removeListener(this);
    }

    void addListener(Listener* l) {
        listeners.add(l);
    }

    void removeListener(Listener* l) {
        listeners.remove(l);
    }

    te::TimeRange getRange() const {
        return {viewX1, viewX2};
    }

    void setRange(te::TimeRange range) {
        viewX1 = range.getStart();
        viewX2 = range.getEnd();
        listeners.call(&Listener::zoomChanged);
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

    void zoomHorizontally(double factor) {
        double scaleFactor = std::pow(2.0, -factor * 5.0);
        auto pos = edit.getTransport().getPosition();
        auto range = viewLength();
        auto newHalfRange = range * scaleFactor / 2.0;
        // limit zoom to 1s .. 1 hour
        if (newHalfRange > 0.5s && newHalfRange < 600s) {
            viewX1 = jmax(te::TimePosition(), pos - newHalfRange);
            viewX2 = viewX1 + newHalfRange * 2.0;
            listeners.call(&Listener::zoomChanged);
        }
    }

    te::Edit& edit;
    ValueTree state;
private:
    CachedValue<te::TimePosition> viewX1, viewX2;
    CachedValue<double> viewY;
    ListenerList<Listener> listeners;

    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &edit.getTransport()) {
            if (edit.getTransport().isPlaying() || edit.getTransport().isRecording()) {
                startTimerHz(60);
            } else {
                stopTimer();
            }
        }
    }

    void timerCallback() override {
        handlePlaybackScrolling();
    }

    void handlePlaybackScrolling() {
        if (edit.getTransport().isPlaying() || edit.getTransport().isRecording()) {
            auto pos = edit.getTransport().getPosition();
            auto range = getRange();
            auto leftRange = range.getLength() / 3.0;
            // for paging in the middle of the view
            // if (pos < viewX1 + leftRange || pos > viewX2 - leftRange) {

            // for continuous scrolling
            if (pos < viewX1 || pos > viewX1 + leftRange) {
                auto newX1 = jmax(te::TimePosition {}, pos - leftRange);
                if (newX1 != viewX1)
                    setRange({newX1, newX1 + range.getLength()});
            }
        }
    }
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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditViewState)
};

}  // namespace MoTool

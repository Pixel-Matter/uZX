#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>  // from Tracktion

namespace MoTool {

namespace IDs
{
    #define DECLARE_ID(name)  const Identifier name(#name);
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

    DECLARE_ID(VIEWSTATE)
    DECLARE_ID(height)
    #undef DECLARE_ID
}

namespace te = tracktion;

//==============================================================================
class ZoomViewState :
        private Timer,
        private te::TransportControl::Listener,
        private ChangeListener {
public:

    // Actually we do not need to use listeners in every ValueTree state wrapper,
    // IDs and ValueTree::Listener are enough.
    // But with Listener we can achieve encapsulation of the state properties
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
        edit.getTransport().addListener(this);
        edit.getTransport().addChangeListener(this);
    }

    ~ZoomViewState() override {
        edit.getTransport().removeChangeListener(this);
        edit.getTransport().removeListener(this);
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

    void setStart(te::TimePosition start) {
        setRange({start, start + viewLength()});
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

    bool scrollToPosition(te::TimePosition pos) {
        if (pos < viewX1 || pos > viewX2) {
            auto range = viewLength();
            auto newViewX1 = jmax(te::TimePosition(), pos - range / 2.0);
            setStart(newViewX1);
            return true;
        }
        return false;
    }

    bool scrollToCurrentPosition() {
        auto pos = edit.getTransport().getPosition();
        return scrollToPosition(pos);
    }

    void zoomHorizontally(double factor) {
        double scaleFactor = std::pow(2.0, -factor * 5.0);
        auto pos = edit.getTransport().getPosition();
        auto range = viewLength();
        auto newHalfRange = range * scaleFactor / 2.0;
        // TODO limit must be relative to pixels
        // limit zoom to
        if (newHalfRange > 0.5s && newHalfRange < 600s) {
            viewX1 = jmax(te::TimePosition(), pos - newHalfRange);
            viewX2 = viewX1 + newHalfRange * 2.0;
            listeners.call(&Listener::zoomChanged);
        }
    }

    te::Edit& edit;
private:
    ValueTree state;
    CachedValue<te::TimePosition> viewX1, viewX2;
    CachedValue<double> viewY;
    ListenerList<Listener> listeners;

    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &edit.getTransport()) {
            // DBG("ZoomViewState::changeListenerCallback");
            if (edit.getTransport().isPlaying() || edit.getTransport().isRecording()) {
                startTimerHz(30);
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
        headersWidth.referTo(state, IDs::headersWidth, nullptr, 110);
    }

    CachedValue<bool> showMasterTrack, showGlobalTrack, showMarkerTrack, showChordTrack, showArrangerTrack,
                      drawWaveforms, showHeaders, showFooters, showMidiDevices, showWaveDevices;
    CachedValue<int> headersWidth;

    ValueTree state;
    ZoomViewState zoom;
    // TODO actually it is better to make EditContext class and put EditViewState and selection managers there
    te::SelectionManager& selectionManager;
    te::Edit& edit;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditViewState)
};


//==============================================================================
/**
    Wrapper for storing the view state of a single track,
    similar to EditViewState, but tied to a specific track's ValueTree.
*/
//==============================================================================
class TrackViewState : private ValueTree::Listener {
public:
    /**
        @param trackState  ValueTree with type TRACK, inside which VIEWSTATE is created or extracted
        @param undoManager  UndoManager from EditViewState
    */
    TrackViewState(ValueTree trackState, UndoManager* undoManager)
        : state(ensure(trackState))
        , height(state, IDs::height, undoManager, 160)
    {
        constrainer.setMinimumHeight(56);
        constrainer.setMaximumHeight(600);
        state.addListener(this);
    }

    ~TrackViewState() override {
        state.removeListener(this);
    }

    int getHeight() const noexcept {
        return height.get();
    }

    void setTrackHeight(int h) {
        height = h;
    }

    class Listener {
        public: virtual ~Listener() = default;
        virtual void trackViewStateChanged() = 0;
    };

    void addListener(Listener* l)    {
        listeners.add(l);
    }

    void removeListener(Listener* l) {
        listeners.remove(l);
    }

    ComponentBoundsConstrainer& getConstrainer() {
        return constrainer;
    }

private:
    ValueTree state;
    CachedValue<int> height;
    ComponentBoundsConstrainer constrainer;
    ListenerList<Listener> listeners;

    static ValueTree ensure(ValueTree trackState) {
        if (auto existing = trackState.getChildWithName(IDs::VIEWSTATE); existing.isValid()) {
            return existing;
        }
        auto vt = ValueTree(IDs::VIEWSTATE);
        trackState.addChild(vt, -1, nullptr);
        return vt;
    }

    void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override {
        if (v == state && id == IDs::height) {
            listeners.call(&Listener::trackViewStateChanged);
        }
    }
};

}  // namespace MoTool

#pragma once

#include <JuceHeader.h>

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

    ZoomViewState(te::Edit& e, ValueTree& st);
    ~ZoomViewState() override;

    void addListener(Listener* l);
    void removeListener(Listener* l);

    te::TimeRange getRange() const;
    void setRange(te::TimeRange range);

    void setStart(te::TimePosition start);

    te::TimePosition getRangeStart() const;
    te::TimePosition getRangeEnd() const;

    double getViewY() const;

    te::TimeDuration viewLength() const;

    te::TimePosition beatToTime(te::BeatPosition b) const;

    int timeToX(te::TimePosition time, int width) const;
    te::TimePosition xToTime(int x, int width) const;

    float durationToPixels(te::TimeDuration duration, int width) const;

    float pixelsPerBeat(te::TimeDuration beatDur, int width) const;
    float pixelsPerBeat(double beatDur, int width) const;

    bool scrollToPosition(te::TimePosition pos);
    bool scrollToCurrentPosition();

    void zoomHorizontally(double factor);

    te::Edit& edit;
private:
    ValueTree state;
    CachedValue<te::TimePosition> viewX1, viewX2;
    CachedValue<double> viewY;
    ListenerList<Listener> listeners;

    void changeListenerCallback(ChangeBroadcaster* source) override;
    void playbackContextChanged() override;
    void timerCallback() override;
    void handlePlaybackScrolling();
};


class EditViewState {
public:
    EditViewState(te::Edit& e, te::SelectionManager& s);

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
    TrackViewState(ValueTree trackState, UndoManager* undoManager);
    ~TrackViewState() override;

    int getHeight() const noexcept;
    void setTrackHeight(int h);

    class Listener {
        public: virtual ~Listener() = default;
        virtual void trackViewStateChanged() = 0;
    };

    void addListener(Listener* l);
    void removeListener(Listener* l);

    ComponentBoundsConstrainer& getConstrainer();

private:
    ValueTree state;
    CachedValue<int> height;
    ComponentBoundsConstrainer constrainer;
    ListenerList<Listener> listeners;

    static ValueTree ensure(ValueTree trackState);

    void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override;
};

}  // namespace MoTool

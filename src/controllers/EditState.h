#pragma once

#include <JuceHeader.h>

#include <common/Utilities.h>  // from Tracktion, for FlaggedAsyncUpdater

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
    DECLARE_ID(viewSpan)
    DECLARE_ID(viewY)

    DECLARE_ID(VIEWSTATE)
    DECLARE_ID(height)
    #undef DECLARE_ID
}

namespace te = tracktion;

//==============================================================================
class ZoomViewState :
        private FlaggedAsyncUpdater,
        private ValueTree::Listener {
public:

    // Actually we do not need to use listeners in every ValueTree state wrapper,
    // IDs and ValueTree::Listener are enough.
    // But with Listener we can achieve encapsulation of the state properties
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void zoomChanged() = 0;
        virtual void zoomOrPosChanged() {}
    };

    ZoomViewState(te::Edit& e, ValueTree& st);
    ~ZoomViewState() override;

    void addListener(Listener* l);
    void removeListener(Listener* l);

    bool jumpToPosition(te::TimePosition pos);
    bool jumpToCurrentPosition();
    void zoomHorizontally(double factor);

    te::TimeRange getRange() const;
    void setRange(te::TimeRange range);

    int timeToX(te::TimePosition time, int width) const;
    te::TimePosition xToTime(int x, int width) const;
    float durationToPixels(te::TimeDuration duration, int width) const;
    double getViewY() const;

    static bool isZoomProperty(const juce::Identifier& id);

    te::Edit& edit;

private:
    ValueTree state;
    CachedValue<te::TimePosition> viewX1;
    CachedValue<te::TimeDuration> viewSpan;
    CachedValue<double> viewY;
    ListenerList<Listener> listeners;

    bool updateZoom = false, updatePos = false;

    void valueTreePropertyChanged(ValueTree&, const Identifier& prop) override;
    void handleAsyncUpdate() override;
    void handlePlaybackScrolling();

    te::TimeDuration getViewSpan() const;
    void setStart(te::TimePosition start);
};


class EditViewState {
public:
    EditViewState(te::Edit& e, te::SelectionManager& s);

    CachedValue<bool> showMasterTrack, showGlobalTrack, showMarkerTrack, showChordTrack, showArrangerTrack,
                      drawWaveforms, showHeaders, showFooters, showMidiDevices, showWaveDevices;
    CachedValue<int> headersWidth;

    ValueTree state;
    ZoomViewState zoom;
    te::SelectionManager& selectionManager;
    te::Edit& edit;

    te::TimeDuration getBeatLengthFor(double bpm) const;
    double getFramesPerBeatFor(double bpm) const;
    double getCurrentFramesPerBeat() const;
    // for note lengths: whole (divider=1), half (divider=2), quarter (divider=4), eighth (divider=8), etc.
    double getFramesPerNote(size_t divider) const;
    double getBpmForBeatLength(te::TimeDuration beatLen) const;
    double getBpmSnappedToFps(double bpm) const;
    double setBpmSnappedToFps(double bpm);

    void setBeatLength(te::TimeDuration beatLen);
    void setFramesPerBeat(int fpb);

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

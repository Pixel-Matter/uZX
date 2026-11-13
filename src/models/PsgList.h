#pragma once

#include <JuceHeader.h>

#include "../formats/psg/PsgData.h"
#include "PsgParameter.h"

#include <algorithm>
#include <cstdint>

namespace te = tracktion;

namespace MoTool {

class PsgClip;


class PsgParamFrame {
public:
    static juce::ValueTree createPsgFrameValueTree(int frameIndex, const PsgParamFrameData& data);

    PsgParamFrame(const juce::ValueTree&);
    PsgParamFrame(PsgParamFrame&&) = default;

    //==============================================================================
    /** Canonical machine-frame index — the tempo-independent timestamp and single
        source of truth. -1 means the frame predates the frame-index model and still
        needs one-time migration from a legacy beat value. */
    int getFrameIndex() const noexcept                                        { return frameIndex; }
    bool hasFrameIndex() const noexcept                                       { return frameIndex >= 0; }
    void setFrameIndex(int, juce::UndoManager*);

    /** Back-fills the frame index for a legacy beat-only frame by inverting its
        current beat through the tempo map at the given frame rate. No-op if the
        frame already has an index. */
    void migrateFrameIndexFromBeat(const PsgClip&, double frameRate, juce::UndoManager*);
    void removeLegacyBeatProperty(juce::UndoManager*);

    /** Frame position converted to beat space. This is derived on demand from the
        owning list's frames-per-beat metadata; PSG frame events are not stored in
        beat space. */
    te::BeatPosition getFrameBeatPosition(const PsgClip&) const;

    /** Raw edit position, ignoring quantising/groove. */
    te::BeatPosition getRawEditBeats(const PsgClip&) const;
    te::TimePosition getRawEditTime(const PsgClip&) const;

    /** This takes into account quantising, groove templates, clip offset, etc */
    te::BeatPosition getEditBeats(const PsgClip&) const;
    te::TimePosition getEditTime(const PsgClip&) const;

    //==============================================================================
    inline const PsgParamFrameData& getData() const noexcept {
        return data;
    }

    inline std::optional<uint16_t> getParam(PsgParamType type) const noexcept {
        return data[type];
    }
    // std::vector<std::pair<PsgParamType, int>> getPsgParams() const noexcept;

//     int getType() const noexcept                { return type; }
//     void setType (int type, juce::UndoManager* um);

    // static juce::String getParameterName(PsgParamType type) noexcept;

    juce::ValueTree state;

private:
    //==============================================================================
    friend class PsgList;

    int frameIndex = -1;
    PsgParamFrameData data;

    void updatePropertiesFromState() noexcept;

    PsgParamFrame() = delete;
    PsgParamFrame(const PsgParamFrame&) = delete;

    JUCE_LEAK_DETECTOR(PsgParamFrame)
//     // ----------------------------------------
};


class PsgList {
public:
    PsgList();
    PsgList(const juce::ValueTree&, juce::UndoManager*);
    ~PsgList();

    static juce::ValueTree createPsgList();

    /** Loads from Psg data, for a clip starting at clipStart: its frames per beat follow the tempo there. */
    void loadFrom(const uZX::PsgData &data, te::Edit& edit, te::TimePosition clipStart, juce::UndoManager*);

    //==============================================================================
    /** Returns the frames sorted array, each frame holding the accumulated register
        state. No copy made. Any change to the frame trees, undo/redo included, is
        picked up here. */
    const juce::Array<PsgParamFrame*>& getFrames() const;

    template <typename Visitor>
    void visitFrames(Visitor&& visitor) const {
        for (auto* frame : getFrames())
            visitor(*frame);
    }

    //==============================================================================
    /** Advances whenever the frame data changes; restarts with each new list. */
    int getDataVersion() const                                      { getFrames(); return dataVersion_; }

    //==============================================================================
    /** Source machine frame rate (Hz) the frame indices came from. 0 if unknown
        (legacy data loaded before the frame-index model). */
    double getFrameRate() const noexcept;
    void setFrameRate(double, juce::UndoManager*);

    /** Dense PSG source frames that fall on one musical beat. This controls the
        current PSG tempo without touching per-frame integer indices. */
    double getFramesPerBeat() const noexcept;
    void setFramesPerBeat(double, juce::UndoManager*);

    /** Current effective PSG frame rate at this clip's start: framesPerBeat divided
        by the current beat length. */
    double getEffectiveFps(const PsgClip&) const;
    bool hasFpsMismatch(const PsgClip&, double editFps) const;

    /** One-time upgrade for legacy lists that have frames but no frame rate:
        adopts the given frame rate and back-fills every frame's index from its
        stored beat, so old data gains tempo-stable PSG timing. Any legacy beat
        property is removed after migration. */
    void migrateToFrameIndicesIfNeeded(const PsgClip&, double frameRate, juce::UndoManager*);

    //==============================================================================
    bool isAttachedToClip() const noexcept                          { return ! state.getParent().hasType(te::IDs::NA); }

    //==============================================================================
    /** Gets the list's midi channel number. Value is 1 to 16. */
    te::MidiChannel getMidiChannel() const                              { return midiChannel; }

    /** Gives the list a channel number that it'll use when generating real midi messages. Value is 1 to 16. */
    void setMidiChannel(te::MidiChannel chanNum);

    //==============================================================================
    bool isEmpty() const noexcept                                   { return state.getNumChildren() == 0; }

    void clear(juce::UndoManager* = nullptr);

    //==============================================================================
    int getNumFrames() const                                        { return getFrames().size(); }

    PsgParamFrame* getFrame(int index) const                  { return getFrames()[index]; }
    const PsgParamFrame* getFrameAtIndex(int frameIndex) const;

    PsgParamFrame* addFrameEvent(int frameIndex, const PsgParamFrameData&, juce::UndoManager*);

    /** Get time according to MIDI timing */
    double getTimeInBase(const PsgParamFrame& frame, PsgClip& clip, te::MidiList::TimeBase timeBase) const;

    //==============================================================================
    // Where a machine frame of the clip's list plays, by its index: for frames, and for anything else that
    // is on the list's frame grid.
    static te::BeatPosition getFrameBeatPosition(const PsgClip&, int frameIndex);
    static te::BeatPosition getRawEditBeats(const PsgClip&, int frameIndex);
    static te::BeatPosition getEditBeats(const PsgClip&, int frameIndex);
    static te::TimePosition getEditTime(const PsgClip&, int frameIndex);

    /** A frame's time in a playback sequence's time base. */
    static double getFrameTimeInBase(int frameIndex, const PsgClip&, te::MidiList::TimeBase);

    /** True if the frame is before the clip's start: trimmed off, or the earlier part of a split. In the
        time bases counted from the clip's start such a frame has no time of its own. (In raw beats Tracktion
        takes the offset off and chases the controllers itself.) */
    static bool isFrameBeforeClip(int frameIndex, const PsgClip&, te::MidiList::TimeBase);

    /** Creates a juce::MidiMessageSequence from the list in order to be played back
        The sequence will be in terms of edit time, either in seconds or beats
        @param PsgClip      The clip boundries to use
        @param TimeBase     The format the exported MIDI event times will be in
    */
    [[nodiscard]] juce::MidiMessageSequence exportToPlaybackMidiSequence(PsgClip&, te::MidiList::TimeBase) const;

    //==============================================================================
    template <typename Type>
    static void sortEventsByFrameIndex(juce::Array<Type>& events) {
        std::stable_sort(events.begin(), events.end(),
                         [] (const Type& a, const Type& b) { return a->getFrameIndex() < b->getFrameIndex(); });
    }

    //==============================================================================
    juce::ValueTree state;

private:
    //==============================================================================
    juce::CachedValue<te::MidiChannel> midiChannel;
    juce::CachedValue<double> frameRate_;
    juce::CachedValue<double> framesPerBeat_;
    mutable int dataVersion_ = 0;


    void initialise(juce::UndoManager*);
    void recomputeAccumulatedState(const juce::Array<PsgParamFrame*>&) const;

    template<typename EventType>
    struct EventDelegate {
        static bool isSuitableType(const juce::ValueTree&);
        /** Return true if the order may have changed. */
        static bool updateObject(EventType&, const juce::Identifier&);
        static void removeFromSelection(EventType*);
    };

    template<typename EventType>
    struct EventList : public te::ValueTreeObjectList<EventType> {
        EventList (const juce::ValueTree& v)
            : te::ValueTreeObjectList<EventType> (v)
        {
            te::ValueTreeObjectList<EventType>::rebuildObjects();
        }

        ~EventList() override {
            te::ValueTreeObjectList<EventType>::freeObjects();
        }

        EventType* getEventFor(const juce::ValueTree& v) {
            for (auto m : te::ValueTreeObjectList<EventType>::objects)
                if (m->state == v)
                    return m;

            return {};
        }

        bool isSuitableType(const juce::ValueTree& v) const override   { return EventDelegate<EventType>::isSuitableType(v); }
        EventType* createNewObject(const juce::ValueTree& v) override  { return new EventType (v); }
        void deleteObject(EventType* m) override                       { delete m; }
        void newObjectAdded(EventType*) override                       { triggerSort(); dataChanged = true; }
        void objectRemoved(EventType* m) override                      { EventDelegate<EventType>::removeFromSelection(m); triggerSort(); dataChanged = true; }
        void objectOrderChanged() override                             { triggerSort(); }

        void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i) override {
            if (auto e = getEventFor(v)) {
                dataChanged = true;
                if (EventDelegate<EventType>::updateObject(*e, i))
                    triggerSort();
            }
        }

        void triggerSort() {
            const juce::ScopedLock sl (lock);
            needsSorting = true;
        }

        const juce::Array<EventType*>& getSortedList() {
            TRACKTION_ASSERT_MESSAGE_THREAD

            const juce::ScopedLock sl (lock);

            if (needsSorting) {
                needsSorting = false;
                sortedEvents = te::ValueTreeObjectList<EventType>::objects;
                sortEventsByFrameIndex(sortedEvents);
            }

            return sortedEvents;
        }

        bool needsSorting = true;
        bool dataChanged = true;  // the owning list rebuilds derived state; message thread only
        juce::Array<EventType*> sortedEvents;
        juce::CriticalSection lock;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventList)
    };

    std::unique_ptr<EventList<PsgParamFrame>> framesList;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsgList)};

}  // namespace MoTool

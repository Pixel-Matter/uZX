#pragma once

#include <JuceHeader.h>

#include "../formats/psg/PsgFile.h"
#include "../util/enumchoice.h"

#include <initializer_list>

namespace te = tracktion;

namespace MoTool {

class PsgClip;

struct PsgParamTypeEnum {
    enum Enum {
        VolumeA = 0,
        VolumeB,
        VolumeC,
        TonePeriodA,
        TonePeriodB,
        TonePeriodC,
        ToneIsOnA,
        ToneIsOnB,
        ToneIsOnC,
        NoiseIsOnA,
        NoiseIsOnB,
        NoiseIsOnC,
        EnvelopeIsOnA,
        EnvelopeIsOnB,
        EnvelopeIsOnC,
        NoisePeriod,
        EnvelopePeriod,
        EnvelopeShape,
        RetriggerToneA,
        RetriggerToneB,
        RetriggerToneC,
        RetriggerEnvelope,
    };

    static inline constexpr std::string_view labels[] {
        "Volume A",
        "Volume B",
        "Volume C",
        "Tone Period A",
        "Tone Period B",
        "Tone Period C",
        "Tone Is On A",
        "Tone Is On B",
        "Tone Is On C",
        "Noise Is On A",
        "Noise Is On B",
        "Noise Is On C",
        "Envelope Is On A",
        "Envelope Is On B",
        "Envelope Is On C",
        "Noise Period",
        "Envelope Period",
        "Envelope Shape",
        "Retrigger Tone A",
        "Retrigger Tone B",
        "Retrigger Tone C",
        "Retrigger Envelope",
    };
};
using PsgParamType = MoTool::Util::EnumChoice<PsgParamTypeEnum>;

class PsgParamFrameData {
public:

    PsgParamFrameData() noexcept = default;
    PsgParamFrameData(const PsgParamFrameData&) = default;
    PsgParamFrameData(PsgParamFrameData&&) = default;
    PsgParamFrameData& operator=(const PsgParamFrameData&) = default;
    PsgParamFrameData& operator=(PsgParamFrameData&&) = default;
    ~PsgParamFrameData() = default;

    PsgParamFrameData(std::initializer_list<std::pair<PsgParamType, uint16_t>> params) noexcept {
        for (const auto& [type, value] : params) {
            set(type, value);
        }
    }

    PsgParamFrameData(const std::vector<std::pair<PsgParamType, uint16_t>>& params) noexcept {
        for (const auto& [type, value] : params) {
            set(type, value);
        }
    }

    PsgParamFrameData(const std::vector<std::tuple<PsgParamType, uint16_t, bool>>& params) noexcept {
        for (const auto& [type, value, isSet] : params) {
            values[static_cast<size_t>(type)] = value;
            masks[static_cast<size_t>(type)] = isSet;
        }
    }

    explicit PsgParamFrameData(const uZX::PsgRegsFrame& regs) noexcept {
        for (size_t i = 0; i < 3; ++i) {
            if (regs.hasVolumeOrEnvModSet(i)) {
                set(PsgParamType(int(PsgParamType::VolumeA) + int(i)), regs.getVolume(i));
                set(PsgParamType(int(PsgParamType::EnvelopeIsOnA) + int(i)), regs.getEnvMod(i));
            }
            if (regs.hasTonePeriodSet(i)) {
                set(PsgParamType(int(PsgParamType::TonePeriodA) + int(i)), regs.getTonePeriod(i));
            }
        }
        if (regs.hasMixerSet()) {
            set(PsgParamType::ToneIsOnA,  regs.getToneOn(0));
            set(PsgParamType::ToneIsOnB,  regs.getToneOn(1));
            set(PsgParamType::ToneIsOnC,  regs.getToneOn(2));
            set(PsgParamType::NoiseIsOnA, regs.getNoiseOn(0));
            set(PsgParamType::NoiseIsOnB, regs.getNoiseOn(1));
            set(PsgParamType::NoiseIsOnC, regs.getNoiseOn(2));
        }
        if (regs.hasNoisePeriodSet()) {
            set(PsgParamType::NoisePeriod, regs.getNoisePeriod());
        }
        if (regs.hasEnvelopePeriodSet()) {
            set(PsgParamType::EnvelopePeriod, regs.getEnvelopePeriod());
        }
        if (regs.hasEnvelopeShapeSet()) {
            set(PsgParamType::EnvelopeShape, regs.getEnvelopeShape());
        }
    }

    void update(const uZX::PsgRegsFrame& regs) noexcept {
        update(PsgParamFrameData {regs});
    }

    void update(const PsgParamFrameData& data) noexcept {
        // track what params was changed actually, compare with current values
        for (size_t i = 0; i < PsgParamType::size(); ++i) {
            masks[i] = data.masks[i] && data.values[i] != values[i];
            values[i] = data.values[i];
        }
    }

    void debugPrint() const noexcept {
        for (size_t i = 0; i < PsgParamType::size(); ++i) {
            DBG("PsgParamFrameData: " << static_cast<PsgParamType>(static_cast<int>(i)) << ": " << values[i] << " " << (masks[i] ? "true" : "false"));
        }
    }

    inline std::optional<uint16_t> operator [](size_t type) const noexcept {
        return masks[type] ? std::optional<uint16_t> {values[type]} : std::nullopt;
    }

    inline std::optional<uint16_t> operator [](PsgParamType type) const noexcept {
        return operator[](static_cast<size_t>(type));
    }

    inline bool isSet(size_t type) const noexcept {
        return masks[type];
    }

    inline bool isSet(PsgParamType type) const noexcept {
        return isSet(static_cast<size_t>(type));
    }

    inline uint16_t& set(size_t type, uint16_t value) noexcept {
        masks[type] = true;
        values[type] = value;
        return values[type];
    }

    inline uint16_t& set(PsgParamType type, uint16_t value) noexcept {
        return set(static_cast<size_t>(type), value);
    }

    inline void remove(size_t type) noexcept {
        masks[type] = false;
    }

    inline void remove(PsgParamType type) noexcept {
        remove(static_cast<size_t>(type));
    }

    std::vector<std::pair<PsgParamType, uint16_t>> getParams() const {
        // TODO use ranges?
        std::vector<std::pair<PsgParamType, uint16_t>> result;
        result.reserve(std::size(values));
        for (size_t i = 0; i < std::size(values); ++i) {
            if (masks[i]) {
                result.emplace_back(static_cast<PsgParamType>(static_cast<int>(i)), values[i]);
            }
        }
        result.shrink_to_fit();
        return result;
    }

    friend class PsgParamFrame;
    friend class PsgParamsChangeTracker;

private:
    // not map or vector to avoid dynamic allocations
    std::array<uint16_t, static_cast<size_t>(PsgParamType::size())> values {};
    // TODO or use bitmask for 21 bits
    // sizeof(std::vector<bool>) is 24 bytes
    std::array<bool,     static_cast<size_t>(PsgParamType::size())> masks {};
};


class PsgParamFrame {
public:
    static juce::ValueTree createPsgFrameValueTree(te::BeatPosition, const PsgParamFrameData& data);
    // static juce::ValueTree createPsgFrameValueTree(te::BeatPosition, const uZX::PsgRegsFrame& regs);
    static juce::ValueTree createPsgFrame(const PsgParamFrame&, te::BeatPosition);

    PsgParamFrame(const juce::ValueTree&);
    PsgParamFrame(PsgParamFrame&&) = default;

//     //==============================================================================
//     int getControllerValue() const noexcept                         { return value; }
//     void setControllerValue(int newValue, juce::UndoManager*);

//     static int getControllerDefautValue(PsgParamType type);

//     int getMetadata() const noexcept                                { return metadata; }
//     void setMetadata (int metaValue, juce::UndoManager*);

    //==============================================================================
    te::BeatPosition getBeatPosition() const noexcept                         { return beatNumber; }
    void setBeatPosition (te::BeatPosition, juce::UndoManager*);

    /** This takes into account quantising, groove templates, clip offset, etc */
    te::BeatPosition getEditBeats(const PsgClip&) const;
    te::TimePosition getEditTime(const PsgClip&) const;

//     juce::String getLevelDescription (te::MidiClip*) const;

//     //==============================================================================
    inline const PsgParamFrameData& getData() const noexcept {
        return data;
    }

    inline std::optional<uint16_t> getParam(PsgParamType type) const noexcept {
        return data[static_cast<size_t>(type)];
    }
    // std::vector<std::pair<PsgParamType, int>> getPsgParams() const noexcept;

//     int getType() const noexcept                { return type; }
//     void setType (int type, juce::UndoManager* um);

    // static juce::String getParameterName(PsgParamType type) noexcept;

    juce::ValueTree state;

private:
    //==============================================================================
    friend class PsgList;

    te::BeatPosition beatNumber;
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

    /** Clears the current list and copies the others contents and properties. */
    void copyFrom(const PsgList&, juce::UndoManager*);

    /** Clears the current list and moves the others contents and properties. */
    void moveFrom(PsgList&, juce::UndoManager*);

    /** Adds copies of the events in another list to this one. */
    void addFrom(const PsgList&, juce::UndoManager*);

    /** Loads from Psg data. */
    void loadFrom(const uZX::PsgData &data, te::Edit& edit, juce::UndoManager*);

    /** Loads from Psg file. */
    void loadFrom(const uZX::PsgFile &psgFile, te::Edit& edit, juce::UndoManager*);

    //==============================================================================
    const juce::Array<PsgParamFrame*>& getFrames() const;

    //==============================================================================
    bool isAttachedToClip() const noexcept                          { return ! state.getParent().hasType(te::IDs::NA); }

    //==============================================================================
    /** Gets the list's midi channel number. Value is 1 to 16. */
    te::MidiChannel getMidiChannel() const                              { return midiChannel; }

    /** Gives the list a channel number that it'll use when generating real midi messages. Value is 1 to 16. */
    void setMidiChannel(te::MidiChannel chanNum);

    /** If the data was pulled from a PSG file then this may have a useful name describing its purpose. */
    juce::String getImportedPsgTrackName() const noexcept           { return importedName; }

    /** Set the imported file name if you want it to appear on the clip */
    juce::String getImportedFileName() const noexcept               { return importedFileName; }
    void setImportedFileName (const juce::String& n)                { importedFileName = n; }

    //==============================================================================
    bool isEmpty() const noexcept                                   { return state.getNumChildren() == 0; }

    void clear(juce::UndoManager* = nullptr);
    void trimOutside(te::BeatPosition firstBeat, te::BeatPosition lastBeat, juce::UndoManager*);
    void moveAllBeatPositions(te::BeatDuration deltaBeats, juce::UndoManager*);
    void rescale(double factor, juce::UndoManager*);

    //==============================================================================
    int getNumFrames() const                                        { return getFrames().size(); }

    /** Beat number of first event in the list */
    te::BeatPosition getFirstBeatNumber() const;

    /** Beat number of last event in the list */
    te::BeatPosition getLastBeatNumber() const;

    PsgParamFrame* getFrame(int index) const                  { return getFrames()[index]; }
    PsgParamFrame* getParamEventAt(te::BeatPosition, PsgParamType paramType) const;

    PsgParamFrame* addFrameEvent(const PsgParamFrame&, juce::UndoManager*);
    PsgParamFrame* addFrameEvent(te::BeatPosition, const PsgParamFrameData&, juce::UndoManager*);
    PsgParamFrame* addFrameEvent(te::BeatPosition, const uZX::PsgRegsFrame&, juce::UndoManager*);

    void removeFrameEvent(PsgParamFrame&, juce::UndoManager*);
    void removeAllFrames(juce::UndoManager*);

    void setControllerValueAt(PsgParamType paramType, te::BeatPosition beatNumber, int newValue, juce::UndoManager*);
    void removeControllersBetween(PsgParamType paramType, te::BeatPosition beatNumberStart, te::BeatPosition beatNumberEnd, juce::UndoManager*);

    /** Adds controller values over a specified time, at an even interval */
    void insertRepeatedParamValue (PsgParamType paramType, int startVal, int endVal,
                                   te::BeatRange rangeBeats,
                                   te::BeatDuration intervalBeats, juce::UndoManager*);

    //==============================================================================
    /** Adds the contents of a MidiMessageSequence to this list.
        If an Edit is provided, it'll be used to convert the timestamps from possible seconds to beats
    */
    void importPsgRegsData(const juce::MidiMessageSequence&, te::Edit*,
                           te::TimePosition editTimeOfListTimeZero, juce::UndoManager*);

    // /** Adds the contents of a MidiSequence to this list assigning MPE expression changes to EXP expression. */
    // void importFromEditTimeSequenceWithNoteExpression (const juce::MidiMessageSequence&, Edit*,
    //                                                    te::TimePosition editTimeOfListTimeZero, juce::UndoManager*);

    /** Determines MIDI event timing. */
    enum class TimeBase
    {
        seconds,    /** Event times will be in seconds relative to the Edit timeline. */
        beats,      /** Event times will be in beats relative to the Edit timeline. */
        beatsRaw    /** Event times will be in beats with no quantisation or groove. */
    };

    /** Get time according to MIDI timing */
    double getTimeInBeats(const PsgParamFrame& frame, PsgClip& clip, PsgList::TimeBase timeBase) const;

    /** Creates a juce::MidiMessageSequence from the list in order to be played back
        The sequence will be in terms of edit time, either in seconds or beats
        @param PsgClip      The clip boundries to use
        @param TimeBase     The format the exported MIDI event times will be in
    */
    juce::MidiMessageSequence exportToPlaybackMidiSequence(PsgClip&, TimeBase) const;

    //==============================================================================
    template <typename Type>
    static void sortEventsByTime(juce::Array<Type>& events) {
        std::sort(events.begin(), events.end(),
                  [] (const Type& a, const Type& b) { return a->getBeatPosition() < b->getBeatPosition(); });
    }

    //==============================================================================
    juce::ValueTree state;

private:
    //==============================================================================
    juce::CachedValue<te::MidiChannel> midiChannel;

    juce::String importedFileName;
    juce::String importedName;

    void initialise(juce::UndoManager*);

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
        void newObjectAdded(EventType*) override                       { triggerSort(); }
        void objectRemoved(EventType* m) override                      { EventDelegate<EventType>::removeFromSelection(m); triggerSort(); }
        void objectOrderChanged() override                             { triggerSort(); }

        void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i) override {
            if (auto e = getEventFor(v))
                if (EventDelegate<EventType>::updateObject(*e, i))
                    triggerSort();
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
                sortEventsByTime(sortedEvents);
            }

            return sortedEvents;
        }

        bool needsSorting = true;
        juce::Array<EventType*> sortedEvents;
        juce::CriticalSection lock;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventList)
    };

    std::unique_ptr<EventList<PsgParamFrame>> framesList;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsgList)};

}  // namespace MoTool
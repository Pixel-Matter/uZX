#pragma once

#include "JuceHeader.h"

#include "../../models/tuning/TuningSystem.h"
#include "../../models/tuning/Scales.h"
#include "../../plugins/uZX/aychip/aychip.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include <cmath>

namespace MoTool {


struct TuningNoteName {
    int noteNumber;        // 0-based note number in 12-semitone system, ie C is 0, C# is 1, etc.
    bool isInScale;        // Whether this note is part of the scale
    Interval interval;     // Interval from the root note in semitones or ratio
    String name;           // Note name (e.g., "C", "A#")
};

struct TuningNote {
    int midiNote;          // can be greater than MIDI note range (0-127) for presentation purposes
    bool isInScale;        // Whether this note is part of the scale
    bool isDefined;        // If this note period is defined in generated or custom table
    double offtune;        // Offtune in cents (positive or negative)
    double frequency;      // Calculated frequency in Hz
    int period;            // Chip divider value (for AY-3-8910, etc.)
    String name;           // Note name (e.g., "C3", "A#4")

    inline static constexpr int trackerNoteOffset = 12; // Tracker note number offset from MIDI number
    inline static constexpr int trackerNotesNum = 96;

    bool isInMidiRange() const {
        return midiNote >= 0 && midiNote <= 127; // MIDI note range check
    }

    bool isInTrackerRange() const {
        return midiNote >= trackerNoteOffset && midiNote < trackerNotesNum + trackerNoteOffset; // Tracker note range check
    }

    int getTrackerNote() const {
        if (!isInTrackerRange()) {
            return -1; // Invalid Tracker note number
        }
        return midiNote - trackerNoteOffset; // Convert MIDI note to Tracker note
    }

    String getTooltip() const {
        String midiInfo = isInMidiRange() ? String::formatted("%d", midiNote) : String("no");
        auto trackerNote = getTrackerNote();
        String trackerInfo = trackerNote != -1 ? String::formatted("%d", trackerNote) : String("out of range");
        String freqInfo;
        if (frequency >= 1000.0) {
            freqInfo = String::formatted("%.3f kHz", frequency / 1000.0);
        } else {
            freqInfo = String::formatted("%.2f Hz", frequency);
        }
        String hearableInfo;
        if (frequency < 20.0) {
            hearableInfo = "Clicks only";
        } else if (frequency < 15000.0) {
            hearableInfo = "Audible";
        } else if (frequency < 17000.0) {
            hearableInfo = "High freq (age-dependent)";
        } else if (frequency < 20000.0) {
            hearableInfo = "Very high (age-dependent)";
        } else {
            hearableInfo = "Ultrasonic (inaudible)";
        }

        return String::formatted("%s: MIDI %s\nTracker note: %s\nPeriod: %d\nFrequency: %s\n%s\nOfftune: %+.1f cents",
                                name.toUTF8(),
                                midiInfo.toUTF8(),
                                trackerInfo.toUTF8(),
                                period,
                                freqInfo.toUTF8(),
                                hearableInfo.toUTF8(),
                                offtune);
    }
};

class TuningViewModel : public ChangeBroadcaster {
public:
    // Any UI component can now listen to tuning system changes by implementing ChangeListener
    // and calling viewModel.addChangeListener(this) in their constructor
    TuningViewModel()
        : transientState("TuningView")
        , chipCapabilities {1750000, 16, Range<int>(1, 4096)}
        // , chipCapabilities {1773400, 16, Range<int>(1, 4096)}
    {
        // Initialize transient view state
        chipClock.referTo(transientState, "chipClock", nullptr,
            uZX::ChipClockChoice(uZX::ChipClockEnum::Pentagon_1_75_MHz));
            // uZX::ChipClockChoice(uZX::ChipClockEnum::ZX_Spectrum_1_77_MHz));
        a4Frequency.referTo(transientState, "A4", nullptr, 440.0);

        // Initialize with ProTracker tuning
        updateTuningSystem();
    }

    std::vector<TuningNoteName> getColumnNoteNames() const {
        return {
            {0, true,   Interval::fromSemitones(0),  "C" },
            {1, false,  Interval::fromSemitones(1),  "C#"},
            {2, true,   Interval::fromSemitones(2),  "D" },
            {3, false,  Interval::fromSemitones(3),  "D#"},
            {4, true,   Interval::fromSemitones(4),  "E" },
            {5, true,   Interval::fromSemitones(5),  "F" },
            {6, false,  Interval::fromSemitones(6),  "F#"},
            {7, true,   Interval::fromSemitones(7),  "G" },
            {8, false,  Interval::fromSemitones(8),  "G#"},
            {9, true,   Interval::fromSemitones(9),  "A" },
            {10, false, Interval::fromSemitones(10), "A#"},
            {11, true,  Interval::fromSemitones(11), "B" }
        };
    }

    int getNumColumns() const {
        return 12; // 12 semitones in an octave
    }

    Range<int> getOctaveRange() const {
        return Range<int>(0, 13);
    }

    int getNumRows() const {
        return getOctaveRange().getLength();
    }

    // Octave -1 is subcontroctave
    std::vector<TuningNote> getOctaveNotes(int octave) const {
        std::vector<TuningNote> notes;
        notes.reserve((size_t) getNumColumns());
        for (auto noteName : getColumnNoteNames()) {
            TuningNote note;
            note.midiNote = (octave + 1) * 12 + noteName.noteNumber; // MIDI note number
            note.isInScale = noteName.isInScale;
            if (!tuningSystem) {
                note.frequency = 0.0; // Default frequency
                note.period = 0;
                note.offtune = 0.0; // Default detune if no tuning system is set
            } else {
                note.period = tuningSystem->midiNoteToPeriod(note.midiNote); // Calculate period for the chip
                note.frequency = tuningSystem->periodToFrequency(note.period); // Calculate frequency from period
                note.offtune = tuningSystem->getOfftune(note.midiNote); // Get detune from tuning system
            }
            // note.detune = 0.0; // Default detune, can be adjusted later

            note.name = noteName.name + String(octave); // Note name with octave
            notes.push_back(note);
        }
        return notes;
    }

    // get values in the range -0.5...0.5 representing offtunes of all periods around this note
    std::vector<double> getTicksAroundNote(const TuningNote& note) const {
        jassert(tuningSystem != nullptr);
        std::vector<double> ticks;
        auto lowerNote = (double) note.midiNote - 0.5f;
        auto upperPeriod = tuningSystem->midiNoteToPeriod(lowerNote);
        auto actualLowerNote = tuningSystem->periodToMidiNote(upperPeriod);
        if (actualLowerNote > note.midiNote + 0.5f) {
            return ticks; // No ticks available
        }
        if (actualLowerNote < note.midiNote - 0.5f) {
            --upperPeriod;
        }
        auto upperNote = (double) note.midiNote + 0.5f;
        auto lowerPeriod = tuningSystem->midiNoteToPeriod(upperNote);
        auto actualUpperNote = tuningSystem->periodToMidiNote(lowerPeriod);
        if (actualUpperNote < note.midiNote - 0.5f) {
            return ticks; // No ticks available
        }
        if (actualUpperNote > note.midiNote + 0.5f) {
            ++lowerPeriod;
        }
        auto ticksNum = upperPeriod - lowerPeriod + 1;
        if (ticksNum <= 0) {
            return ticks; // No ticks available
        }
        // find suitable step for power of 10
        auto step = std::log2(ticksNum);
        if (step < 0.0) {
            step = std::pow(10.0, std::ceil(step));
        } else {
            step = std::pow(10.0, std::floor(step));
        }
        // DBG("Ticks around note " << note.name
        //     << ": lowerPeriod = " << lowerPeriod
        //     << ", upperPeriod = " << upperPeriod
        //     << ", note = " << note.midiNote
        //     << ", actualLowerNote = " << actualLowerNote
        //     << ", actualUpperNote = " << actualUpperNote
        //     << ", ticksNum = " << ticksNum
        //     << ", step = " << step);
        ticks.reserve(static_cast<size_t>(ticksNum));
        int intStep = static_cast<int>(step);
        for (int p = upperPeriod; p >= lowerPeriod; p -= intStep) {
            auto n = tuningSystem->periodToMidiNote(p);
            ticks.push_back(n - note.midiNote);
            // DBG("Tick for period " << p << ": note = " << n << ", offtune = " << (n - note.midiNote));
        }
        return ticks;
    }

    // std::vector<Interval> getIntervals() const;

    String getScaleName() const {
        return "C Major";  // Placeholder, should be set based on the tuning system or scale
    }

    String getTuningTypeName() const {
        return tuningSystem ? String(std::string(tuningSystem->getType().getLabel())) : "Unknown tuning system type";
    }

    String getTuningName() const {
        return tuningSystem ? tuningSystem->getName() : "Default tuning";
    }

    Range<int> getChipDividerRange() const {
        return chipCapabilities.registerRange;
    }

    StringArray getChipClockLabels() const {
        StringArray labels;
        for (const auto& label : uZX::ChipClockEnum::longLabels) {
            labels.add(String(label.data(), label.size()));
        }
        return labels;
    }

    int getCurrentChipClockIndex() const {
        return static_cast<int>(chipClock.get().value);
    }

    void setChipClockChoice(int index) {
        DBG("Setting chip clock choice to index: " << index);
        if (index >= 0 && index < static_cast<int>(uZX::ChipClockChoice::size())) {
            chipClock = uZX::ChipClockChoice(static_cast<uZX::ChipClockEnum::Enum>(index));
            // Call updateChipCapabilities directly since Value::Listener doesn't work reliably with custom types
            updateChipCapabilities();
        }
    }

    double getA4Frequency() const {
        return a4Frequency.get();
    }

    void seta4Frequency(double frequency) {
        DBG("Setting A4 frequency to: " << frequency << " Hz");
        if (frequency >= 220.0 && frequency <= 880.0) { // Reasonable range for A4
            a4Frequency = frequency;
            updateChipCapabilities();
        }
    }

    // // State persistence methods
    // ValueTree getState() const {
    //     return transientState.createCopy();
    // }

    // void setState(const ValueTree& newState) {
    //     transientState.copyPropertiesFrom(newState, nullptr);
    // }

    // TODO Tuning table editing? What bindings to use?

private:
    void updateTuningSystem() {
        // TODO scale and root note

        /* Delphi code
        PT3NoteTable_PT: PT3ToneTable = (
            $0C22, $0B73, $0ACF, $0A33, $09A1, $0917, $0894, $0819, $07A4, $0737, $06CF, $066D,
            $0611, $05BA, $0567, $051A, $04D0, $048B, $044A, $040C, $03D2, $039B, $0367, $0337,
            $0308, $02DD, $02B4, $028D, $0268, $0246, $0225, $0206, $01E9, $01CE, $01B4, $019B,
            $0184, $016E, $015A, $0146, $0134, $0123, $0112, $0103, $00F5, $00E7, $00DA, $00CE,
            $00C2, $00B7, $00AD, $00A3, $009A, $0091, $0089, $0082, $007A, $0073, $006D, $0067,
            $0061, $005C, $0056, $0052, $004D, $0049, $0045, $0041, $003D, $003A, $0036, $0033,
            $0031, $002E, $002B, $0029, $0027, $0024, $0022, $0020, $001F, $001D, $001B, $001A,
            $0018, $0017, $0016, $0014, $0013, $0012, $0011, $0010, $000F, $000E, $000D, $000C);
        */
        auto proTrackerTuning0 = std::make_unique<CustomTuning>(
            chipCapabilities,
            24, // Starting at MIDI note 24 (C1)
            std::vector<int> {
                // Octave 1 (C1-B1): $0C22-$066D
                3106, 2931, 2767, 2611, 2465, 2327, 2196, 2073, 1956, 1847, 1743, 1645,
                // Octave 2 (C2-B2): $0611-$0337
                1553, 1466, 1383, 1306, 1232, 1163, 1098, 1036, 978, 923, 871, 823,
                // Octave 3 (C3-B3): $0308-$019B
                776, 733, 692, 653, 616, 582, 549, 518, 489, 462, 436, 411,
                // Octave 4 (C4-B4): $0184-$00CE
                388, 366, 346, 326, 308, 291, 274, 259, 245, 231, 218, 206,
                // Octave 5 (C5-B5): $00C2-$0067
                194, 183, 173, 163, 154, 145, 137, 130, 122, 115, 109, 103,
                // Octave 6 (C6-B6): $0061-$0033
                97, 92, 86, 82, 77, 73, 69, 65, 61, 58, 54, 51,
                // Octave 7 (C7-B7): $0031-$001A
                49, 46, 43, 41, 39, 36, 34, 32, 31, 29, 27, 26,
                // Octave 8 (C8-B8): $0018-$000C
                24, 23, 22, 20, 19, 18, 17, 16, 15, 14, 13, 12
            },
            "ProTracker Tuning #0"
        );

        // auto equalTemperamentTuning = std::make_unique<EqualTemperamentTuning>(chipCapabilities, a4Frequency.get());

        tuningSystem = std::move(proTrackerTuning0);
    }

    void updateChipCapabilities() {
        auto choice = chipClock.get();
        int index = static_cast<int>(choice.value);

        if (index != uZX::ChipClockEnum::Custom) {
            double clockFreq = uZX::ChipClockEnum::clockValues[index];
            chipCapabilities.clockFrequency = clockFreq;

            DBG("Updating tuning system with new clock frequency: " << clockFreq << " Hz and A4 frequency: " << a4Frequency.get() << " Hz");

            // Recreate tuning system with new capabilities and current A4 frequency
            updateTuningSystem();

            // Notify all registered listeners that the tuning system has changed
            DBG("Broadcasting tuning system change to all listeners");
            sendChangeMessage();
        }
    }

    // Transient view state
    ValueTree transientState;
    CachedValue<uZX::ChipClockChoice> chipClock;
    CachedValue<double> a4Frequency;
    ChipCapabilities chipCapabilities; // Chip capabilities for the tuning system
    std::unique_ptr<TuningSystem> tuningSystem;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningViewModel)
};

}

namespace juce {

using namespace MoTool::uZX;

template<>
struct VariantConverter<MoTool::uZX::ChipClockChoice> {
    static MoTool::uZX::ChipClockChoice fromVar(const var& v) {
        return MoTool::uZX::ChipClockChoice(v.toString().toStdString());
    }

    static var toVar(MoTool::uZX::ChipClockChoice choice) {
        const std::string_view label = choice.getLabel();
        return String {label.data(), label.size()};
    }
};

} // namespace juce
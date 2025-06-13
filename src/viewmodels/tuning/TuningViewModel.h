#pragma once

#include <JuceHeader.h>

#include "../../models/tuning/TuningSystem.h"
#include "../../models/tuning/TuningRegistry.h"
#include "../../models/tuning/Ratios.h"
#include "../../models/tuning/Scales.h"
#include "../../plugins/uZX/aychip/aychip.h"

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
                                offtune
                            );
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
        initTuningSystem();
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
        return Range<int>(-1, 10);
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
        // DBG("Setting chip clock choice to index: " << index);
        if (index >= 0 && index < static_cast<int>(uZX::ChipClockChoice::size())) {
            chipClock = uZX::ChipClockChoice(static_cast<uZX::ChipClockEnum::Enum>(index));
            // Call updateChipCapabilities directly since Value::Listener doesn't work reliably with custom types
            updateParameters();
        }
    }

    double getA4Frequency() const {
        return a4Frequency.get();
    }

    void setA4Frequency(double frequency) {
        DBG("Setting A4 frequency to: " << frequency << " Hz");
        if (frequency >= 220.0 && frequency <= 880.0) { // Reasonable range for A4
            a4Frequency = frequency;
            updateParameters();
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
    void initTuningSystem() {
        // TODO scale and root note

        chipCapabilities.divider = 16 * 16;
        tuningSystem = makeEqualTemperamentTuning(chipCapabilities);

        a4Frequency = tuningSystem->getA4Frequency();
    }

    void updateTuningSystem() {
        tuningSystem->setA4Frequency(a4Frequency.get());
    }

    void updateParameters() {
        auto choice = chipClock.get();
        int index = static_cast<int>(choice.value);

        if (index != uZX::ChipClockEnum::Custom) {
            double clockFreq = uZX::ChipClockEnum::clockValues[index];
            chipCapabilities.clockFrequency = clockFreq;

            // DBG("Updating tuning system with new clock frequency: " << clockFreq << " Hz and A4 frequency: " << a4Frequency.get() << " Hz");

            // Recreate tuning system with new capabilities and current A4 frequency
            updateTuningSystem();

            // Notify all registered listeners that the tuning system has changed
            // DBG("Broadcasting tuning system change to all listeners");
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
#pragma once

#include <JuceHeader.h>

#include "../../models/tuning/TemperamentSystem.h"
#include "../../models/tuning/TuningSystemBase.h"
#include "../../models/tuning/AutoTuning.h"
#include "../../models/tuning/TuningTable.h"
#include "../../models/tuning/TuningRegistry.h"
#include "../../models/tuning/Ratios.h"
#include "../../models/tuning/Scales.h"
#include "../../plugins/uZX/aychip/aychip.h"

#include <cmath>
#include <set>
#include <array>
#include <limits>

namespace MoTool {

enum class Key {
    C = 0, CSharp, D, DSharp, E, F, FSharp, G, GSharp, A, ASharp, B
};

struct TuningNoteName {
    int noteNumber;        // 0-based note number in 12-semitone system, ie C is 0, C# is 1, etc.
    bool isInScale;        // Whether this note is part of the scale
    Interval interval;     // Interval from the root note in cents or ratio
    String name;           // Note name (e.g., "C", "A#")
    String stepName;       // Note step name (e.g., "1", "♭2", "2", etc.)
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
        , chipCapabilities {16, Range<int>(1, 4096)}
        // , chipCapabilities {1773400, 16, Range<int>(1, 4096)}
        , currentScale(Scale::ScaleType::IonianOrMajor)
        , currentKey(Key::C)
    {
        // Initialize transient view state
        // TODO handle Custom option to chipClock
        chipClock.referTo(transientState, "chipClock", nullptr,
            uZX::ChipClockChoice(uZX::ChipClockEnum::ZX_Spectrum_1_77_MHz));
        // Make it editable only when using custom clock
        clockFrequency.referTo(transientState, "chipFrequency", nullptr, 1773400.0);
        a4Frequency.referTo(transientState, "A4", nullptr, 440.0);

        // Initialize with ProTracker tuning
        initTuningSystem();
    }

    std::vector<TuningNoteName> getColumnNoteNames() const {
        std::vector<TuningNoteName> noteNames;
        noteNames.reserve(12);

        // Get the scale intervals
        auto scaleIntervals = currentScale.getIntervals();

        // Convert to set for fast lookup
        std::set<int> scaleNotes;
        for (int interval : scaleIntervals) {
            int transposedInterval = (interval + static_cast<int>(currentKey)) % 12;
            scaleNotes.insert(transposedInterval);
        }

        // Note steps array
        static constexpr std::array<std::string, 12> noteStepsArray = {
            "1", "♭2", "2", "♭3", "3", "4", "♭5", "5", "♭6", "6", "♭7", "7"
        };

        // Note names array
        static const std::array<String, 12> noteNamesArray = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };

        // Build the result
        for (int i = 0; i < 12; ++i) {
            noteNames.emplace_back(
                i,
                scaleNotes.count(i) > 0,  // isInScale
                Interval::fromSemitones(i),
                noteNamesArray[(size_t) i],
                String::fromUTF8(noteStepsArray[(size_t) i].data())
            );
        }

        return noteNames;
    }

    int getNumColumns() const {
        return 12; // 12 semitones in an octave
    }

    Range<int> getOctaveRange() const {
        // TODO depends on divider, for envelope use -1, 9
        return Range<int>(0, 10);
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
        return getKeyName(currentKey) + " " + currentScale.getName();
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

    // Scale and Key selection methods
    Scale::ScaleType getCurrentScale() const {
        return currentScale.getType();
    }

    void setCurrentScale(Scale::ScaleType scaleType) {
        currentScale = Scale(scaleType);
        sendChangeMessage();
    }

    Key getCurrentKey() const {
        return currentKey;
    }

    void setCurrentKey(Key key) {
        currentKey = key;
        sendChangeMessage();
    }

    StringArray getScaleTypeNames() const {
        StringArray names;
        for (auto scaleType : Scale::getAllScaleTypes()) {
            if (scaleType != Scale::ScaleType::User) {
                names.add(Scale::getNameForType(scaleType));
            }
        }
        return names;
    }

    StringArray getKeyNames() const {
        StringArray names;
        for (int i = 0; i < 12; ++i) {
            names.add(getKeyName(static_cast<Key>(i)));
        }
        return names;
    }

    static String getKeyName(Key key) {
        static const std::array<String, 12> keyNames = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        return keyNames[static_cast<size_t>(key)];
    }

    // Tuning table selection methods
    StringArray getTuningTableNames() const {
        StringArray names;
        names.add("Equal Temperament");
        CustomTuningType::forEach([&](auto&& tableType) {
            names.add(CustomTuningType(tableType).getLongLabel().data());
        });
        return names;
    }

    int getCurrentTuningTableIndex() const {
        return currentTuningTableIndex;
    }

    void setCurrentTuningTable(int index) {
        if (index != currentTuningTableIndex && index >= 0 && index < getTuningTableNames().size()) {
            currentTuningTableIndex = index;
            updateTuningSystem(true, true); // Reset UI values when changing tuning table
            sendChangeMessage();
        }
    }

    StringArray getChipClockLabels() const {
        StringArray labels;
        uZX::ChipClockChoice::forEach([&](auto&& clockType) {
            labels.add(uZX::ChipClockChoice(clockType).getLongLabel().data());
        });
        return labels;
    }

    int getCurrentChipClockIndex() const {
        return static_cast<int>(chipClock.get().value);
    }

    void setChipClockChoice(int index) {
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
        setParameterWithValidation(a4Frequency, frequency, 220.0, 880.0);
    }

    double getClockFrequency() const {
        return clockFrequency.get();
    }

    void setClockFrequency(double frequency) {
        setParameterWithValidation(clockFrequency, frequency, 1000000.0, 2000000.0);
    }

    bool isCustomClockEnabled() const {
        return chipClock.get().value == uZX::ChipClockEnum::Custom;
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
        chipCapabilities.divider = 16;
        currentTuningTableIndex = 0; // Equal Temperament
        updateTuningSystem(true);
    }

    void updateTuningSystem(bool recreate = false, bool resetUIValues = false) {
        if (recreate) {
            if (currentTuningTableIndex == 0) {
                tuningSystem = makeEqualTemperamentTuning(chipCapabilities, clockFrequency.get(), a4Frequency.get());
            } else {
                // Custom tuning table
                int customIndex = currentTuningTableIndex - 1;
                tuningSystem = makeCustomTableTuning(static_cast<CustomTuningType>(customIndex), chipCapabilities);

                if (resetUIValues) {
                    // When changing tuning table, reset UI to match tuning defaults
                    a4Frequency = tuningSystem->getA4Frequency();
                    clockFrequency = tuningSystem->getClockFrequency();
                    chipClock = uZX::ChipClockChoice(static_cast<uZX::ChipClockEnum::Enum>(findBestMatchingClockPreset(clockFrequency.get())));
                } else {
                    // When updating from user changes, preserve user settings
                    tuningSystem->setA4Frequency(a4Frequency.get());
                    tuningSystem->setClockFrequency(clockFrequency.get());
                }
            }
        } else {
            // Apply current UI values to the existing tuning system
            tuningSystem->setA4Frequency(a4Frequency.get());
            tuningSystem->setClockFrequency(clockFrequency.get());
        }
    }

    void updateParameters() {
        auto choice = chipClock.get();
        int index = static_cast<int>(choice.value);

        if (index != uZX::ChipClockEnum::Custom) {
            clockFrequency = uZX::ChipClockEnum::clockValues[index];
        }

        // Always update tuning system when parameters change
        updateTuningSystem(true);

        // Notify all registered listeners that the tuning system has changed
        sendChangeMessage();
    }

    // Helper methods
    template<typename T>
    void setParameterWithValidation(CachedValue<T>& parameter, T value, T minValue, T maxValue) {
        if (value >= minValue && value <= maxValue) {
            parameter = value;
            updateTuningSystem();
            sendChangeMessage();
        }
    }

    int findBestMatchingClockPreset(double frequency) {
        int bestMatch = static_cast<int>(uZX::ChipClockEnum::Custom); // Default to Custom
        double bestDiff = std::numeric_limits<double>::max();

        for (int i = 0; i < static_cast<int>(uZX::ChipClockEnum::Custom); ++i) {
            double presetFreq = uZX::ChipClockEnum::clockValues[i];
            double diff = std::abs(frequency - presetFreq);
            if (diff < bestDiff && diff < 1000.0) { // Within 1kHz tolerance
                bestMatch = i;
                bestDiff = diff;
            }
        }

        return bestMatch;
    }

    // Transient view state
    ValueTree transientState;
    CachedValue<uZX::ChipClockChoice> chipClock;
    CachedValue<double> clockFrequency; // Current chip clock frequency in Hz
    CachedValue<double> a4Frequency;

    ChipCapabilities chipCapabilities; // Chip capabilities for the tuning system
    std::unique_ptr<TuningSystem> tuningSystem;

    // Scale and Key selection
    Scale currentScale;
    Key currentKey;

    // Tuning table selection
    int currentTuningTableIndex = 0;

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
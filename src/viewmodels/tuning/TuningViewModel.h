#pragma once

#include "JuceHeader.h"

#include "../../models/tuning/TuningSystem.h"
#include "../../plugins/uZX/aychip/aychip.h"

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
    double offtune;        // Offtune in cents (positive or negative)
    double frequency;      // Calculated frequency in Hz
    int period;            // Chip divider value (for AY-3-8910, etc.)
    String name;           // Note name (e.g., "C3", "A#4")

    bool isInMidiRange() const {
        return midiNote >= 0 && midiNote <= 127; // MIDI note range check
    }
};

class TuningViewModel {
public:
    TuningViewModel()
        : chipCapabilities {1773400, 16, Range<int>(1, 4096)}
        , tuningSystem {std::make_unique<EqualTemperamentTuning>(chipCapabilities)}
    {}

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
        return Range<int>(0, 14);
    }

    int getNumRows() const {
        return getOctaveRange().getLength();
    }

    std::vector<TuningNote> getOctaveNotes(int octave) const {
        std::vector<TuningNote> notes;
        notes.reserve((size_t) getNumColumns());
        for (auto noteName : getColumnNoteNames()) {
            TuningNote note;
            note.midiNote = octave * 12 + noteName.noteNumber; // MIDI note number
            note.isInScale = noteName.isInScale;
            if (!tuningSystem) {
                note.frequency = 0.0; // Default frequency
                note.period = 0;
                note.offtune = 0.0; // Default detune if no tuning system is set
            } else {
                note.frequency = tuningSystem->midiNoteToFrequency(note.midiNote); // Calculate frequency
                note.period = tuningSystem->midiNoteToPeriod(note.midiNote); // Calculate period for the chip
                note.offtune = tuningSystem->getOfftune(note.midiNote); // Get detune from tuning system
            }
            // note.detune = 0.0; // Default detune, can be adjusted later

            note.name = noteName.name + String(octave); // Note name with octave
            notes.push_back(note);
        }
        return notes;
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

    String getChipClock() const {
        return String::formatted("%s %f Hz", "ZX Spectrum", 1.7734);
        // return String::formatted("%d Hz", tuningSystem.getChipClock());
    }

    StringArray getChipClockLabels() const {
        StringArray labels;
        for (const auto& label : uZX::ChipClockEnum::labels) {
            labels.add(String(label.data(), label.size()));
        }
        return labels;
    }

    int getCurrentChipClockIndex() const {
        return static_cast<int>(chipClockChoice.value);
    }

    void setChipClockChoice(int index) {
        if (index >= 0 && index < static_cast<int>(uZX::ChipClockChoice::size())) {
            chipClockChoice = uZX::ChipClockChoice(static_cast<uZX::ChipClockEnum::Enum>(index));
            // Update chip capabilities with new clock frequency
            if (index != uZX::ChipClockEnum::Custom) {
                double clockFreq = uZX::ChipClockEnum::clockValues[index];
                chipCapabilities.clockFrequency = clockFreq;
                // Recreate tuning system with new capabilities
                tuningSystem = std::make_unique<EqualTemperamentTuning>(chipCapabilities);
            }
        }
    }

    // TODO Tuning table editing? What bindings to use?

private:
    ChipCapabilities chipCapabilities; // Chip capabilities for the tuning system
    std::unique_ptr<TuningSystem> tuningSystem;
    uZX::ChipClockChoice chipClockChoice {uZX::ChipClockEnum::ZX_Spectrum_1_77_MHz};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningViewModel)
};

}
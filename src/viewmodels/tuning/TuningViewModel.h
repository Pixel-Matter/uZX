#pragma once

#include "JuceHeader.h"

#include "../../models/tuning/TuningSystem.h"

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
    double detune;         // Detune in cents (positive or negative)
    double frequency;      // Calculated frequency in Hz
    int divider;           // Chip divider value (for AY-3-8910, etc.)
    String name;           // Note name (e.g., "C3", "A#4")
};

class TuningViewModel {
public:
    TuningViewModel() {}

    std::vector<TuningNoteName> getColumnNoteNames() const {
        return {
            {0, true, Interval::fromSemitones(0), "C"},
            {1, true, Interval::fromSemitones(1), "C#"},
            {2, true, Interval::fromSemitones(2), "D"},
            {3, true, Interval::fromSemitones(3), "D#"},
            {4, true, Interval::fromSemitones(4), "E"},
            {5, true, Interval::fromSemitones(5), "F"},
            {6, true, Interval::fromSemitones(6), "F#"},
            {7, true, Interval::fromSemitones(7), "G"},
            {8, true, Interval::fromSemitones(8), "G#"},
            {9, true, Interval::fromSemitones(9), "A"},
            {10, true, Interval::fromSemitones(10), "A#"},
            {11, true, Interval::fromSemitones(11), "B"}
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
            note.detune = 0.0; // Default detune, can be adjusted later
            // note.frequency = TuningSystem::getFrequencyForMidiNote(note.midiNote); // Calculate frequency
            // note.divider = TuningSystem::getChipDividerForMidiNote(note.midiNote); // Get chip divider
            note.name = noteName.name + String(octave); // Note name with octave
            notes.push_back(note);
        }
        return notes;
    }

    std::vector<Interval> getIntervals() const;
    String getScaleName() const;

    Range<int> getChipDividerRange() const;

    String getChipClock() const {
        return String::formatted("%s %f Hz", "ZX Spectrum", 1.7734);
        // return String::formatted("%d Hz", tuningSystem.getChipClock());
    }

    // TODO Tuning table editing? What bindings to use?

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningViewModel)
};

}
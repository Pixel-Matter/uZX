#pragma once

#include <JuceHeader.h>

#include "../../models/tuning/TemperamentSystem.h"
#include "../../models/tuning/TuningSystemBase.h"
#include "../../models/tuning/TuningRegistry.h"
#include "../../models/tuning/Scales.h"
#include "../../plugins/uZX/aychip/aychip.h"
#include "../../util/convert.h"
#include "../../controllers/ParamAttachments.h"

#include <cmath>
#include <array>
#include <limits>

namespace MoTool {

using namespace uZX;

struct TuningNoteName {
    int noteNumber;        // 0-based note number in 12-semitone system, ie C is 0, C# is 1, etc.
    bool isInScale;        // Whether this note is part of the scale
    String name;           // Note pitch class name (e.g., "C", "A#")
    ScaleDegree degree;    // Scale degree representation
    String tuning;         // Tuning representation
    bool isRootNote;
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

    bool isSafeForEnvelope() const {
        return period % 16 == 0;
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

        return name + String::formatted(": MIDI %s\nTracker note: %s\nPeriod: %d\nFrequency: %s\n%s\nOfftune: %+.1f cents",
                                midiInfo.toUTF8(),
                                trackerInfo.toUTF8(),
                                period,
                                freqInfo.toUTF8(),
                                hearableInfo.toUTF8(),
                                offtune
                            );
    }
};


namespace IDs {
    #define DECLARE_ID(name)  const Identifier name(#name);
    DECLARE_ID(TUNINGVIEWSTATE)
    DECLARE_ID(tuningTable)
    DECLARE_ID(chipClock)
    DECLARE_ID(key)
    DECLARE_ID(scale)
    DECLARE_ID(a4Freq)
    DECLARE_ID(clockFreq)

    #undef DECLARE_ID
}

class TuningViewModel : public ChangeBroadcaster, private Value::Listener {
public:
    // Any UI component can now listen to tuning system changes by implementing ChangeListener
    // and calling viewModel.addChangeListener(this) in their constructor
    TuningViewModel(UndoManager* um = nullptr)
        : transientState(IDs::TUNINGVIEWSTATE)
        , undoManager(um)
                                                                 // no undo for this control
        , selectedTuningTable(transientState, IDs::tuningTable, nullptr, BuiltinTuningType::EqualTemperament)
        , selectedChip       (transientState, IDs::chipClock,   ChipClockChoice::getLongLabels(), um, ChipClockChoice::ZX_Spectrum_1_77_MHz)
        , selectedTonic      (transientState, IDs::key,         Scale::getAllKeyNames(),          um, Scale::Key::C)
        , selectedScale      (transientState, IDs::scale,                                         um, Scale::ScaleType::IonianOrMajor)
        , a4Frequency        (transientState, IDs::a4Freq,      {220.0, 880.0, 0.1},              um, 440.0)
        , clockFrequencyMhz  (transientState, IDs::clockFreq,   {1.0, 2.0, 0.001},                um, 1.7734) // MHz
        // objects
        , currentScale(Scale::ScaleType::IonianOrMajor)
        , chipCapabilities {16, Range<int>(1, 4096)}
    {
        // Set up Value listeners for bidirectional sync
        selectedTuningTable.addListener(this);
        selectedChip.addListener(this);
        selectedTonic.addListener(this);
        selectedScale.addListener(this);
        a4Frequency.addListener(this);
        clockFrequencyMhz.addListener(this);

        recreateTuningSystem();
    }

    std::vector<TuningNoteName> getColumnNoteNames() const {
        std::vector<TuningNoteName> noteNames;
        noteNames.reserve(12);

        auto& scale = getCurrentScale();
        auto scaleIntervals = scale.getIntervals();
        const auto scaleChromDegrees = scale.getChromaticDegrees();
        auto refTuning = tuningSystem ? tuningSystem->getReferenceTuning() : nullptr;

        // Build the result
        const auto keyIdx = static_cast<int>(getCurrentKey());
        for (int i = 0; i < 12; ++i) {
            const int semitonesFromKey = (i - keyIdx + 12) % 12;
            noteNames.emplace_back(
                i,
                scale.isIntervalInScale(semitonesFromKey),
                // scaleNotes.count(i) > 0,  // isInScale
                Scale::getKeyName(Scale::Key(i)),
                scaleChromDegrees[(size_t) semitonesFromKey],
                refTuning ? refTuning->getDegreeRepresentation(semitonesFromKey) : String(),
                semitonesFromKey == 0 // isRootNote
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

    TuningSystem* getTuningSystem() const {
        return tuningSystem.get();
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
        // // DBG("Ticks around note " << note.name
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
            // // DBG("Tick for period " << p << ": note = " << n << ", offtune = " << (n - note.midiNote));
        }
        return ticks;
    }

    String getScaleName() const {
        return Scale::getKeyName(getCurrentKey()) + " " + Scale(getCurrentScaleType()).getName();
    }

    String getTuningTypeName() const {
        return tuningSystem ? String(std::string(tuningSystem->getType().getLabel())) : "Unknown tuning system type";
    }

    String getTuningDescription() const {
        return tuningSystem ? tuningSystem->getDescription() : "Default tuning";
    }

    Range<int> getChipDividerRange() const {
        return chipCapabilities.registerRange;
    }

    Scale::ScaleType getCurrentScaleType() const {
        auto scaleType = selectedScale.get();
        if (currentScale.getType() != scaleType) {
            currentScale = Scale(scaleType); // Update cache only when needed
        }
        return scaleType;
    }

    const Scale& getCurrentScale() const {
        getCurrentScaleType(); // update cache if needed
        return currentScale;
    }

    void setCurrentScaleType(Scale::ScaleType scaleType) {
        // DBG("setCurrentScaleType: " << Scale::getNameForType(scaleType));
        selectedScale = scaleType;
        // TODO double update after assigning to scaleIndex1?
        currentScale = Scale(scaleType); // Update cache
        // no need to update tuning system scale
        // DBG("setCurrentScaleType sendChangeMessage");
        sendChangeMessage();
    }

    Scale::Key getCurrentKey() const {
        return selectedTonic.get();
    }

    void setCurrentKey(Scale::Key key) {
        // DBG("setCurrentKey: " << Scale::getKeyName(key));
        selectedTonic = key;
        // TODO double update after assigning to keyIndex1?
        tuningSystem->setTonic(key); // Update tuning system tonic
        // DBG("setCurrentKey sendChangeMessage");
        sendChangeMessage();
    }

    static StringArray getScaleTypeNames() {
        StringArray names;
        for (auto scaleType : Scale::getAllScaleTypes()) {
            if (scaleType != Scale::ScaleType::User) {
                names.add(Scale::getNameForType(scaleType));
            }
        }
        return names;
    }

    // Tuning table selection methods
    StringArray getTuningTableNames() const {
        return toStringArray(BuiltinTuningType::getLongLabels());
    }

    ChipClockChoice getChipChoice() const {
        return selectedChip.get();
    }

    // Not used yet
    void setChipChoice(ChipClockChoice clockChoice) {
        // DBG("setChipChoice: " << clockChoice.getLongLabel().data());
        selectedChip = clockChoice;
        clockFrequencyMhz = clockChoice.getClockValue() / MHz; // Store as MHz
        tuningSystem->setClockFrequency(clockChoice.getClockValue()); // Update tuning system clock frequency
        // DBG("setChipChoice sendChangeMessage");
        sendChangeMessage();
    }

    // used in tests only
    void setA4Frequency(double frequency) {
        // DBG("setA4Frequency: " << frequency);
        if (frequency >= 220.0 && frequency <= 880.0) {
            a4Frequency = frequency;
            tuningSystem->setA4Frequency(frequency); // Update tuning system A4 frequency
            // DBG("setA4Frequency sendChangeMessage");
            sendChangeMessage();
        }
    }

    // Not used yet
    void setClockFrequency(double frequency) {
        // DBG("setClockFrequencyHz: " << frequency);
        if (frequency >= 1.0 * MHz && frequency <= 2.0 * MHz) {
            clockFrequencyMhz = frequency / MHz; // Store as MHz
            tuningSystem->setClockFrequency(frequency);
            // DBG("setClockFrequencyHz sendChangeMessage");
            sendChangeMessage();
        }
    }

    bool isCustomClockEnabled() const {
        return getChipChoice() == ChipClockChoice::Custom;
    }

    String exportToCSV() const {
        String csv;

        // CSV Header with metadata
        // csv += "# Tuning System Export\n";
        // csv += "# Tuning: " + getTuningName() + "\n";
        // csv += "# Type: " + getTuningTypeName() + "\n";
        // csv += "# Scale: " + getScaleName() + "\n";
        // csv += "# Clock Frequency: " + String(getClockFrequency(), 0) + " Hz\n";
        // csv += "# A4 Frequency: " + String(getA4Frequency(), 1) + " Hz\n";
        // csv += "#\n";

        // CSV Data Header
        csv += "MIDI,Note,Period\n";

        // Export data for each octave
        auto octaveRange = getOctaveRange();
        for (int octave = octaveRange.getStart(); octave < octaveRange.getEnd(); ++octave) {
            auto octaveNotes = getOctaveNotes(octave);
            for (const auto& note : octaveNotes) {
                // Skip notes beyond MIDI range
                if (!note.isInMidiRange()) {
                    continue;
                }

                // MIDI note
                csv += String(note.midiNote) + ",";

                // Note name - convert to ASCII compatible format
                String asciiNoteName = note.name;
                asciiNoteName = asciiNoteName.replace(String::fromUTF8("♯"), "#");
                asciiNoteName = asciiNoteName.replace(String::fromUTF8("♭"), "b");
                if (asciiNoteName.length() == 2) {
                    // insert dash between 0th and 1st character for compatibility
                    asciiNoteName = asciiNoteName.substring(0, 1) + "-" + asciiNoteName.substring(1);
                }
                csv += asciiNoteName + ",";

                // Period
                csv += String(note.period) + "\n";
            }
        }

        return csv;
    }

    String getDefaultExportFilename() const {
        // Create a descriptive filename from tuning details
        jassert(tuningSystem != nullptr);

        String filename = tuningSystem->getReferenceTuning()->getTypeName() + " " + tuningSystem->getTypeName() + " " + getScaleName();

        // Add clock frequency info
        filename += " " + String(clockFrequencyMhz.get(), 2).replace(".", "_") + "MHz";

        // Add A4 frequency
        filename += " A4=" + String(a4Frequency.get(), 0);

        // Replace invalid filename characters
        filename = filename.replaceCharacter('/', '-')
                          .replaceCharacter('\\', '-')
                          .replaceCharacter(':', '-')
                          .replaceCharacter('*', '-')
                          .replaceCharacter('?', '-')
                          .replaceCharacter('"', '\'')
                          .replaceCharacter('<', '-')
                          .replaceCharacter('>', '-')
                          .replaceCharacter('|', '-')
                          .replaceCharacter('#', '-');

        // Remove multiple consecutive spaces and dashes
        while (filename.contains("  ")) {
            filename = filename.replace("  ", " ");
        }
        while (filename.contains("--")) {
            filename = filename.replace("--", "-");
        }

        // Add extension
        filename += ".csv";

        return filename;
    }

    //-------------------------------------------------------------------------
private:
    // Transient view state
    ValueTree transientState;
    // te::Edit edit;             // Edit for undo/redo support, and for previewing tuning
    UndoManager* undoManager;

public:
    // ParamAttachment objects - single source of truth for all persisted types
    ChoiceParamAttachment<BuiltinTuningType> selectedTuningTable;
    ChoiceParamAttachment<ChipClockChoice>   selectedChip;
    ChoiceParamAttachment<Scale::Key>        selectedTonic;
    ChoiceParamAttachment<Scale::ScaleType>  selectedScale;
    RangedParamAttachment<double>            a4Frequency;        // Hz - authoritative source
    RangedParamAttachment<double>            clockFrequencyMhz;  // MHz - authoritative source

private:
    // Cached objects derived from values (performance optimization) - only for complex conversions
    mutable Scale currentScale;            // Scale object cache
    ChipCapabilities chipCapabilities; // Chip capabilities for the tuning system
    std::unique_ptr<TuningSystem> tuningSystem;

    // Value::Listener implementation for bidirectional sync
    void valueChanged(Value& value) override {
        if (value.refersToSameSourceAs(selectedScale.getValue())) {
            // DBG("Scale changed from valueChanged");
            auto newScale = selectedScale.get();
            if (newScale != currentScale.getType()) {
                // circular dependency if call setCurrentScaleType?
                // setCurrentScaleType(newScale);
                currentScale = Scale(newScale); // Update Scale object cache
                // no need to update tuning system scale here
                // DBG("Scale changed from valueChanged sendChangeMessage");
                sendChangeMessage();
            }
        } else if (value.refersToSameSourceAs(selectedTonic.getValue())) {
            // DBG("Key changed from valueChanged");
            tuningSystem->setTonic(getCurrentKey());
            // DBG("Key changed from valueChanged sendChangeMessage");
            sendChangeMessage();
        } else if (value.refersToSameSourceAs(a4Frequency.getValue())) {
            // DBG("A4 frequency changed from valueChanged");
            double newFreq = value.getValue();
            if (newFreq >= 220.0 && newFreq <= 880.0) {
                tuningSystem->setA4Frequency(newFreq);
                // DBG("A4 frequency changed from valueChanged sendChangeMessage");
                sendChangeMessage();
            }
        } else if (value.refersToSameSourceAs(clockFrequencyMhz.getValue())) {
            // DBG("Clock frequency changed from valueChanged");
            double newFreqMHz = value.getValue();
            if (newFreqMHz >= 1.0 && newFreqMHz <= 2.0) {
                tuningSystem->setClockFrequency(newFreqMHz * MHz);
                // DBG("Clock frequency changed from valueChanged sendChangeMessage");
                sendChangeMessage();
            }
        } else if (value.refersToSameSourceAs(selectedChip.getValue())) {
            // DBG("chipIndex0 changed from valueChanged " << value.toString());
            auto chip = selectedChip.get();
            if (chip != ChipClockChoice::Custom) {
                // Update clock frequency when preset is selected
                // DBG("chipIndex0 =" << chip.getLabel().data());
                auto clock = chip.getClockValue();
                clockFrequencyMhz = clock / MHz; // Update CachedValue (MHz)
                tuningSystem->setClockFrequency(clock); // Update tuning system clock frequency
                // Notify all registered listeners that the tuning system has changed
            }
            // if chip == ChipClockChoice::Custom we should send change message too
            // DBG("chipIndex0 changed from valueChanged sendChangeMessage");
            sendChangeMessage();
        } else if (value.refersToSameSourceAs(selectedTuningTable.getValue())) {
            // DBG("Tuning table index changed from valueChanged: " << static_cast<int>(value.getValue()));
            recreateTuningSystem(); // Reset to tuning defaults when changing tuning table
            // DBG("Tuning table index changed from valueChanged sendChangeMessage");
            sendChangeMessage();
        }
    }

    void recreateTuningSystem() {
        // DBG("Recreating tuning system with index: " << tuningTableIndex0.get());
        auto tuningType = selectedTuningTable.get();

        TuningOptions options {
            .tableType = tuningType,
            .capabilities = chipCapabilities,
            .temperamentType = TemperamentType::EqualTemperament, // TODO from view property
            .tonic = getCurrentKey(),
            .scaleType = getCurrentScaleType(),
            .chipChoice = selectedChip.get(),
            .chipClock = clockFrequencyMhz.get() * MHz,
            .a4Frequency = a4Frequency.get()
        };

        tuningSystem = makeBuiltinTuning(options);

        if (tuningSystem) {
            // Apply tuning defaults when changing tuning table or initializing
            selectedTonic = options.tonic;
            selectedScale = options.scaleType;
            selectedChip = options.chipChoice;
            clockFrequencyMhz = options.chipClock / MHz; // Convert Hz to MHz
            a4Frequency = options.a4Frequency;
        }
    }

    // Helper methods
    // Helper method for finding best matching clock preset

    int findBestMatchingClockPreset(double frequency) {
        int bestMatch = static_cast<int>(ChipClockEnum::Custom); // Default to Custom
        double bestDiff = std::numeric_limits<double>::max();

        for (int i = 0; i < static_cast<int>(ChipClockEnum::Custom); ++i) {
            double presetFreq = ChipClockEnum::clockValues[i];
            double diff = std::abs(frequency - presetFreq);
            if (diff < bestDiff && diff < 1000.0) { // Within 1kHz tolerance
                bestMatch = i;
                bestDiff = diff;
            }
        }

        return bestMatch;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningViewModel)
};

} // namespace MoTool
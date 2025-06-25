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

using namespace uZX;

struct TuningNoteName {
    int noteNumber;        // 0-based note number in 12-semitone system, ie C is 0, C# is 1, etc.
    bool isInScale;        // Whether this note is part of the scale
    String name;           // Note name (e.g., "C", "A#")
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
    DECLARE_ID(a4Frequency)
    DECLARE_ID(clockFrequency)

    #undef DECLARE_ID
}

class TuningViewModel : public ChangeBroadcaster, private Value::Listener {
public:
    // Any UI component can now listen to tuning system changes by implementing ChangeListener
    // and calling viewModel.addChangeListener(this) in their constructor
    TuningViewModel()
        : transientState(IDs::TUNINGVIEWSTATE)
        , tuningTableIndex(transientState, IDs::tuningTable, nullptr, 0)
        , chipClockIndex(transientState, IDs::chipClock, nullptr, static_cast<int>(ChipClockEnum::ZX_Spectrum_1_77_MHz))
        , keyIndex(transientState, IDs::key, nullptr, 1)
        , scaleIndex(transientState, IDs::scale, nullptr, 1)
        , a4Frequency(transientState, IDs::a4Frequency, nullptr, 440.0)
        , clockFrequencyMhz(transientState, IDs::clockFrequency, nullptr, 1.7734) // MHz
        // value objects, for listeners and bindings
        , tuningTableIndexValue(tuningTableIndex.getPropertyAsValue())
        , chipClockIndexValue(chipClockIndex.getPropertyAsValue())
        , keyIndexValue(keyIndex.getPropertyAsValue())
        , scaleIndexValue(scaleIndex.getPropertyAsValue())
        , a4FrequencyValue(a4Frequency.getPropertyAsValue())
        , clockFrequencyValue(clockFrequencyMhz.getPropertyAsValue())
        // objects
        , currentScale(Scale::ScaleType::IonianOrMajor)
        , chipCapabilities {16, Range<int>(1, 4096)}
    {
        // Set up Value listeners for bidirectional sync
        tuningTableIndexValue.addListener(this);
        chipClockIndexValue.addListener(this);
        keyIndexValue.addListener(this);
        scaleIndexValue.addListener(this);
        a4FrequencyValue.addListener(this);
        clockFrequencyValue.addListener(this);

        initTuningSystem();
    }

    std::vector<TuningNoteName> getColumnNoteNames() const {
        std::vector<TuningNoteName> noteNames;
        noteNames.reserve(12);

        // Get the scale intervals - update cache if needed
        auto currentScaleType = getCurrentScaleType();
        if (currentScale.getType() != currentScaleType) {
            currentScale = Scale(currentScaleType);
        }
        auto scaleIntervals = currentScale.getIntervals();
        const auto scaleChromDegrees = currentScale.getChromaticDegrees();
        auto refTuning = tuningSystem ? tuningSystem->getReferenceTuning() : nullptr;

        // Build the result
        const auto keyIdx = static_cast<int>(getCurrentKey());
        for (int i = 0; i < 12; ++i) {
            const int semitonesFromKey = (i - keyIdx + 12) % 12;
            noteNames.emplace_back(
                i,
                currentScale.isIntervalInScale(semitonesFromKey),
                // scaleNotes.count(i) > 0,  // isInScale
                String::fromUTF8(chromaticNoteNames[(size_t) i].data()),
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

    String getScaleName() const {
        return getKeyName(getCurrentKey()) + " " + Scale(getCurrentScaleType()).getName();
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

    // Scale and Scale::Key selection methods
    Scale::ScaleType getCurrentScaleType() const {
        auto scaleType = static_cast<Scale::ScaleType>(scaleIndex.get() - 1);
        if (currentScale.getType() != scaleType) {
            currentScale = Scale(scaleType); // Update cache only when needed
        }
        return scaleType;
    }

    void setCurrentScale(Scale::ScaleType scaleType) {
        scaleIndex = static_cast<int>(scaleType) + 1;
        currentScale = Scale(scaleType); // Update cache
        updateTuningSystem();
        sendChangeMessage();
    }

    Scale::Key getCurrentKey() const {
        return static_cast<Scale::Key>(keyIndex.get());
    }

    void setCurrentKey(Scale::Key key) {
        keyIndex = static_cast<int>(key);
        updateTuningSystem();
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
            names.add(getKeyName(static_cast<Scale::Key>(i)));
        }
        return names;
    }

    static String getKeyName(Scale::Key key) {
        return String::fromUTF8(chromaticNoteNames[static_cast<size_t>(key)].data());
    }

    // Tuning table selection methods
    StringArray getTuningTableNames() const {
        StringArray names;
        BuiltinTuningType::forEach([&](auto&& tableType) {
            names.add(BuiltinTuningType(tableType).getLongLabel().data());
        });
        return names;
    }

    int getCurrentTuningTableIndex0() const {
        return tuningTableIndex.get();
    }

    void setCurrentTuningTableIndex0(int index) {
        if (index != tuningTableIndex.get() && index >= 0 && index < getTuningTableNames().size()) {
            tuningTableIndex = index;
            recreateTuningSystem(true); // Reset to tuning defaults when changing tuning table
            sendChangeMessage();
        }
    }

    StringArray getChipClockLabels() const {
        StringArray labels;
        ChipClockChoice::forEach([&](auto&& clockType) {
            labels.add(ChipClockChoice(clockType).getLongLabel().data());
        });
        return labels;
    }

    int getCurrentChipClockIndex() const {
        return chipClockIndex.get();
    }

    ChipClockChoice getCurrentChipClock() const {
        return ChipClockChoice(static_cast<ChipClockEnum::Enum>(chipClockIndex.get()));
    }

    void setChipClockChoice(int index) {
        if (index >= 0 && index < static_cast<int>(ChipClockChoice::size())) {
            chipClockIndex = index;
            updateParameters();
        }
    }

    double getA4Frequency() const {
        return a4Frequency.get();
    }

    void setA4Frequency(double frequency) {
        if (frequency >= 220.0 && frequency <= 880.0) {
            a4Frequency = frequency;
            updateTuningSystem();
            sendChangeMessage();
        }
    }

    void setClockFrequencyHz(double frequency) {
        if (frequency >= 1.0 * MHz && frequency <= 2.0 * MHz) {
            clockFrequencyMhz = frequency / MHz; // Store as MHz
            updateTuningSystem();
            sendChangeMessage();
        }
    }

    bool isCustomClockEnabled() const {
        return chipClockIndex.get() == static_cast<int>(ChipClockEnum::Custom);
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
        filename += " A4=" + String(getA4Frequency(), 0);

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

    // Transient view state
    ValueTree transientState;

    // CachedValue objects - single source of truth for all persisted types
    CachedValue<int> tuningTableIndex;
    CachedValue<int> chipClockIndex;
    CachedValue<int> keyIndex;
    CachedValue<int> scaleIndex;            // 1-based index for UI convenience, 0 is invalid
    CachedValue<double> a4Frequency;        // Hz - authoritative source
    CachedValue<double> clockFrequencyMhz;  // MHz - authoritative source

public:
    // Value objects for UI binding (public for direct access)
    Value tuningTableIndexValue;
    Value chipClockIndexValue;
    Value keyIndexValue;
    Value scaleIndexValue;                  // 1-based index for UI convenience, 0 is invalid
    Value a4FrequencyValue;
    Value clockFrequencyValue;

private:
    // Cached objects derived from values (performance optimization) - only for complex conversions
    mutable Scale currentScale;            // Scale object cache
    ChipCapabilities chipCapabilities; // Chip capabilities for the tuning system
    std::unique_ptr<TuningSystem> tuningSystem;

    // TODO move to Scale class
    static inline constexpr std::array<std::string_view, 12> chromaticScaleDegreeNames = {
        "1", "♭2", "2", "♭3", "3", "4", "♭5", "5", "♭6", "6", "♭7", "7"
    };

    static inline constexpr std::array<std::string_view, 12> chromaticNoteNames = {
        "C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"
    };



    // Value::Listener implementation for bidirectional sync
    void valueChanged(Value& value) override {
        if (value.refersToSameSourceAs(scaleIndexValue)) {
            auto newScale = static_cast<Scale::ScaleType>(static_cast<int>(value.getValue()));
            if (newScale != currentScale.getType()) {
                currentScale = Scale(newScale); // Update Scale object cache
                updateTuningSystem();
                sendChangeMessage();
            }
        }
        else if (value.refersToSameSourceAs(keyIndexValue)) {
            // Key changed from UI - no cache needed, CachedValue is authoritative
            updateTuningSystem();
            sendChangeMessage();
        }
        else if (value.refersToSameSourceAs(a4FrequencyValue)) {
            double newFreq = value.getValue();
            if (newFreq >= 220.0 && newFreq <= 880.0) {
                updateTuningSystem();
                sendChangeMessage();
            }
        }
        else if (value.refersToSameSourceAs(clockFrequencyValue)) {
            double newFreqMHz = value.getValue();
            if (newFreqMHz >= 1.0 && newFreqMHz <= 2.0) {
                updateTuningSystem();
                sendChangeMessage();
            }
        }
        else if (value.refersToSameSourceAs(chipClockIndexValue)) {
            int newIndex = static_cast<int>(value.getValue());
            if (newIndex >= 0 && newIndex < static_cast<int>(ChipClockChoice::size())) {
                updateParameters();
            }
        }
        else if (value.refersToSameSourceAs(tuningTableIndexValue)) {
            int newIndex = value.getValue();
            if (newIndex != tuningTableIndex.get()) {
                setCurrentTuningTableIndex0(newIndex);
            }
        }
    }

    void initTuningSystem() {
        // TODO scale and root note
        chipCapabilities.divider = 16;
        tuningTableIndex = 0; // Equal Temperament
        recreateTuningSystem(true); // Apply defaults during initialization
    }

    void recreateTuningSystem(bool applyDefaults = false) {
        auto tuningType = static_cast<BuiltinTuningType>(tuningTableIndex.get());

        TuningOptions options {
            .tableType = tuningType,
            .capabilities = chipCapabilities,
            .temperamentType = TemperamentType::EqualTemperament, // TODO from view property
            .tonic = getCurrentKey(),
            .scaleType = getCurrentScaleType(),
            .chipChoice = getCurrentChipClock(),
            .chipClock = clockFrequencyMhz.get() * MHz,
            .a4Frequency = getA4Frequency()
        };

        tuningSystem = makeBuiltinTuning(options);

        if (tuningSystem && applyDefaults) {
            // Apply tuning defaults when changing tuning table or initializing
            keyIndex = static_cast<int>(options.tonic);
            scaleIndex = static_cast<int>(options.scaleType) + 1;
            currentScale = Scale(options.scaleType);

            // Update CachedValue objects to match the new defaults
            chipClockIndex = static_cast<int>(options.chipChoice.value);
            clockFrequencyMhz = options.chipClock / MHz; // Convert Hz to MHz
            a4Frequency = options.a4Frequency;
        }
    }

    void updateTuningSystem() {
        tuningSystem->setA4Frequency(getA4Frequency());
        tuningSystem->setClockFrequency(clockFrequencyMhz.get() * MHz);
        tuningSystem->setTonic(getCurrentKey());
        // TODO set reference tuning
        // tuningSystem->setReferenceTuning(...)
    }

    void updateParameters() {
        // Get current chip clock index
        int index = chipClockIndex.get();

        if (index != ChipClockEnum::Custom) {
            // Update clock frequency when preset is selected
            clockFrequencyMhz = ChipClockEnum::clockValues[index] / MHz; // Update CachedValue (MHz)
        }

        // Always update tuning system when parameters change
        recreateTuningSystem(false); // Don't override user's current settings

        // Notify all registered listeners that the tuning system has changed
        sendChangeMessage();
    }

    // Helper methods
    // Helper method for finding best matching clock preset
    // (old setParameterWithValidation method removed as Values are now authoritative)

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

}

namespace juce {

using namespace MoTool::uZX;

template<>
struct VariantConverter<MoTool::ChipClockChoice> {
    static MoTool::ChipClockChoice fromVar(const var& v) {
        return MoTool::ChipClockChoice(v.toString().toStdString());
    }

    static var toVar(MoTool::ChipClockChoice choice) {
        const std::string_view label = choice.getLabel();
        return String {label.data(), label.size()};
    }
};

} // namespace juce
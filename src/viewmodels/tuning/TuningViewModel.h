#pragma once

#include <JuceHeader.h>

#include "../../controllers/Parameters.h"

#include "../../models/tuning/TemperamentSystem.h"
#include "../../models/tuning/TuningSystemBase.h"
#include "../../models/tuning/TuningRegistry.h"
#include "../../models/tuning/Scales.h"

#include "../../plugins/uZX/aychip/aychip.h"
#include "../../util/convert.h"
#include "../../utils/StringLiterals.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>

using namespace juce;
namespace te = tracktion;

namespace MoTool {

enum class NoteGridHeadingType {
    Tuning,
    Degrees,
    Notes
};

struct EnvShapeSimpleEnum {
    enum Enum {
        Triangle,
        Sawtooth,
    };

    static inline constexpr std::string_view longLabels[] {
        "Triangle",
        "Sawtooth",
    };

    static inline constexpr std::string_view shortLabels[] {
        "Tri",
        "Saw"
    };
};

using EnvShapeChoice = Util::EnumChoice<EnvShapeSimpleEnum>;


struct ModulationEnum {
    enum Enum {
        Unison,
        OctaveDown,
        OctaveUp,
        FifthDown,
        FifthUp,
        ForthDown,
        ForthUp
    };

    static inline constexpr std::string_view longLabels[] {
        "0 Unison",
        "-12 Octave Down",
        "+12 Octave Up",
        "-7 Fifth Down",
        "+7 Fifth Up",
        "-5 Forth Down",
        "+5 Forth Up"
    };

    static inline constexpr std::string_view shortLabels[] {
        "0",
        "-12",
        "+12",
        "-7",
        "+7",
        "-5",
        "+5"
    };

    static inline constexpr int semitones[] {
        0,
        -12,
        +12,
        -7,
        +7,
        -5,
        +5
    };
};

using EnvIntervalChoice = Util::EnumChoice<ModulationEnum>;


struct TuningNoteName {
    int noteNumber;        // 0-based note number in 12-semitone system, ie C is 0, C# is 1, etc.
    bool isInScale;        // Whether this note is part of the scale
    String name;           // Note pitch class name (e.g., "C", "A#")
    ScaleDegree degree;    // Scale degree representation
    String tuning;         // Tuning representation
    bool isRootNote;

    String getHeadingText(NoteGridHeadingType headingType) const {
        switch (headingType) {
            case NoteGridHeadingType::Tuning:
                return tuning;
            case NoteGridHeadingType::Degrees:
                return degree.toString();
            case NoteGridHeadingType::Notes:
                return name;
        }
        return String();
    }
};

struct TuningNote {
    int midiNote;          // can be greater than MIDI note range (0-127) for presentation purposes
    bool isInScale;        // Whether this note is part of the scale
    bool isDefined;        // If this note period is defined in generated or custom table
    double offtune;        // Offtune in cents (positive or negative)
    double frequency;      // Calculated frequency in Hz
    int period;            // Chip period value (for AY-3-8910, etc.)
    int envPeriod;          //Chip envelope period value
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

        String periodInfo;
        if (envPeriod == -1) {  // period is env
            periodInfo = String::formatted("Env period: %d", period);
        } else {
            periodInfo = String::formatted("Period: %d\nEnv: %d %s",
                                period, envPeriod,
                                isSafeForEnvelope() ? "(in sync)" : "(out of sync)"
                            );
        }

        return name + String::formatted(": MIDI %s\nTracker note: %s\n%s\nFrequency: %s\n%s\nOfftune: %+.1f cents",
                                midiInfo.toUTF8(),
                                trackerInfo.toUTF8(),
                                periodInfo.toUTF8(),
                                freqInfo.toUTF8(),
                                hearableInfo.toUTF8(),
                                offtune
                            );
    }
};

namespace IDs {
    #define DECLARE_ID(name)  const Identifier name(#name);
    DECLARE_ID(TUNINGVIEWSTATE)
    DECLARE_ID(tuningSystem)
    DECLARE_ID(tuningTable)
    DECLARE_ID(chipClock)
    DECLARE_ID(key)
    DECLARE_ID(scale)
    DECLARE_ID(a4Freq)
    DECLARE_ID(clockFreq)
    DECLARE_ID(playChords)
    DECLARE_ID(playTone)
    DECLARE_ID(playEnvelope)
    DECLARE_ID(envelopeShape)
    DECLARE_ID(envelopeMode)
    DECLARE_ID(retriggerTone)

    #undef DECLARE_ID
}

static constexpr inline const auto TuningSystemTypeLabels =
    truncateArray<TuningSystemType::CustomRational>(TuningSystemType::getLongLabels());

class TuningViewModel : public ChangeBroadcaster,
                        private Value::Listener
{
public:
    // Any UI component can now listen to tuning system changes by implementing ChangeListener
    // and calling viewModel.addChangeListener(this) in their constructor

    class SelectionParams : public ParamsBase<SelectionParams> {
    public:
        using ParamsBase<SelectionParams>::ParamsBase;

        template<typename Visitor>
        void visit(Visitor&& visitor) {
            visitor(clockFrequencyMhz);
            visitor(tonic);
            visitor(scaleType);
            visitor(a4Frequency);
            visitor(tuningTable);
            visitor(tuningType);
            visitor(playChords);
            visitor(playTone);
            visitor(playEnvelope);
            visitor(retriggerTone);
            visitor(envelopeShape);
            visitor(envInterval);
        }

        ParameterValue<double> clockFrequencyMhz      {{IDs::clockFreq.toString(),    IDs::clockFreq,      "Clock",            "Chip clock frequency",      1.7734,                              {0.8, 2.0}, "MHz"}};

        ParameterValue<Scale::Tonic> tonic            {{IDs::key.toString(),          IDs::key,            "Key",              "Selected key tonic",        Scale::Tonic::C,                     Scale::Tonic::getLongLabels()}};
        ParameterValue<Scale::ScaleType> scaleType    {{IDs::scale.toString(),        IDs::scale,          "Scale",            "Selected scale type",       Scale::ScaleType::IonianOrMajor,     Scale::ScaleType::getLongLabels()}};
        ParameterValue<double> a4Frequency            {{IDs::a4Freq.toString(),       IDs::a4Freq,         "A4",               "Concert A frequency",       440.0,                               NormalisableRange<double>(220.0, 880.0, 0.1), "Hz"}};
        ParameterValue<BuiltinTuningType> tuningTable {{IDs::tuningTable.toString(),  IDs::tuningTable,    "Tuning Table",     "Selected tuning table",     BuiltinTuningType::EqualTemperament, BuiltinTuningType::getLongLabels()}};
        ParameterValue<TuningSystemType> tuningType   {{IDs::tuningSystem.toString(), IDs::tuningSystem,   "Reference tuning", "Reference tuning system",   TuningSystemType::EqualTemperament,  TuningSystemTypeLabels }};

        ParameterValue<bool> playChords               {{IDs::playChords.toString(),    IDs::playChords,    "Play chords",      "Play chords",               false}};
        ParameterValue<bool> playTone                 {{IDs::playTone.toString(),      IDs::playTone,      "Play tone",        "Play fundamental tone",     true}};
        ParameterValue<bool> playEnvelope             {{IDs::playEnvelope.toString(),  IDs::playEnvelope,  "Play envelope",    "Play envelope modulation",  false}};
        ParameterValue<bool> retriggerTone            {{IDs::retriggerTone.toString(), IDs::retriggerTone, "Retrigger tone",   "Retrigger tone",            false}};
        ParameterValue<EnvShapeChoice> envelopeShape  {{IDs::envelopeShape.toString(), IDs::envelopeShape, "Envelope shape",   "Envelope oscillator shape", EnvShapeChoice(EnvShapeSimpleEnum::Triangle), EnvShapeChoice::getLongLabels()}};
        ParameterValue<EnvIntervalChoice> envInterval {{IDs::envelopeMode.toString(),  IDs::envelopeMode,  "Modulation mode",  "Modulation interval",       EnvIntervalChoice(ModulationEnum::Unison), EnvIntervalChoice::getLongLabels()}};
    };

    TuningViewModel(te::Edit& ed)
        : transientState(IDs::TUNINGVIEWSTATE)
        , edit(ed)
        , undoManager(edit.getUndoManager())
        // objects
        , currentScale(Scale::ScaleType::IonianOrMajor)
                                                                 // no undo for this control
    {
        // Set up Value listeners for bidirectional sync
        selectedParams.referTo(transientState, &undoManager);
        selectedParams.addListener(this);
        currentScale = Scale(selectedParams.scaleType.getStoredValue());

        recreateTuningSystem();
    }

    ~TuningViewModel() override {
        selectedParams.removeListener(this);
    }

    te::Edit& getEdit() const {
        return edit;
    }

    std::vector<TuningNoteName> getColumnNoteNames() const {
        std::vector<TuningNoteName> noteNames;
        noteNames.reserve(12);

        const auto& scale = currentScale;
        auto scaleIntervals = scale.getIntervals();
        const auto scaleChromDegrees = scale.getChromaticDegrees();
        auto refTuning = tuningSystem ? tuningSystem->getReferenceTuning() : nullptr;

        // Build the result
        const auto keyIdx = static_cast<int>(getCurrentTonic());
        for (int i = 0; i < 12; ++i) {
            const int semitonesFromKey = (i - keyIdx + 12) % 12;
            noteNames.emplace_back(
                i,
                scale.isIntervalInScale(semitonesFromKey),
                // scaleNotes.count(i) > 0,  // isInScale
                Scale::getTonicName(Scale::Tonic(i)),
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
        return Range<int>(0, 10);
    }

    int getNumRows() const {
        return getOctaveRange().getLength();
    }

    std::shared_ptr<TuningSystem> getTuningSystem() const {
        return tuningSystem;
    }

    // Octave -1 is subcontroctave
    std::vector<TuningNote> getOctaveNotes(int octave) const {
        std::vector<TuningNote> notes;
        notes.reserve((size_t) getNumColumns());
        auto mode = getCurrentPeriodMode();
        for (auto noteName : getColumnNoteNames()) {
            TuningNote note;
            note.midiNote = (octave + 1) * 12 + noteName.noteNumber; // MIDI note number
            note.isInScale = noteName.isInScale;
            if (!tuningSystem) {
                note.frequency = 0.0; // Default frequency
                note.period = 0;
                note.offtune = 0.0; // Default detune if no tuning system is set
            } else {
                if (mode == TuningSystem::Tone) {
                    note.period = tuningSystem->midiNoteToPeriod(note.midiNote, TuningSystem::Tone); // Calculate period for the mode
                    note.envPeriod = tuningSystem->midiNoteToPeriod(note.midiNote, TuningSystem::Envelope); // Calculate period for the mode
                } else {
                    note.period = tuningSystem->midiNoteToPeriod(note.midiNote, TuningSystem::Envelope); // Calculate period for the mode
                    note.envPeriod = -1;   // no env period, for tooltip purposes
                }

                note.offtune = tuningSystem->getOfftune(note.midiNote, mode); // Get detune from tuning system
                note.frequency = tuningSystem->periodToFrequency(note.period, mode); // Calculate frequency from period
            }
            // note.detune = 0.0; // Default detune, can be adjusted later

            note.name = noteName.name + String(octave); // Note name with octave
            notes.push_back(note);
        }
        return notes;
    }

    // // get values in the range -0.5...0.5 representing offtunes of all periods around this note
    // std::vector<double> getTicksAroundNote(const TuningNote& note) const {
    //     jassert(tuningSystem != nullptr);
    //     std::vector<double> ticks;
    //     auto lowerNote = (double) note.midiNote - 0.5f;
    //     auto mode = getCurrentPeriodMode();
    //     DBG("Current mode: " << static_cast<size_t>(mode));
    //     // TODO use chip capabilities to calc periods
    //     auto upperPeriod = tuningSystem->midiNoteToPeriod(lowerNote, mode);
    //     auto actualLowerNote = tuningSystem->periodToMidiNote(upperPeriod, mode);
    //     if (actualLowerNote > (double) note.midiNote + 0.5f) {
    //         return ticks; // No ticks available
    //     }
    //     if (actualLowerNote < (double) note.midiNote - 0.5f) {
    //         --upperPeriod;
    //     }
    //     auto upperNote = (double) note.midiNote + 0.5f;
    //     auto lowerPeriod = tuningSystem->midiNoteToPeriod(upperNote, mode);

    //     auto actualUpperNote = tuningSystem->periodToMidiNote(lowerPeriod, mode);
    //     if (actualUpperNote < (double) note.midiNote - 0.5f) {
    //         return ticks; // No ticks available
    //     }
    //     if (actualUpperNote > (double) note.midiNote + 0.5f) {
    //         ++lowerPeriod;
    //     }
    //     auto ticksNum = upperPeriod - lowerPeriod + 1;
    //     if (ticksNum <= 0) {
    //         return ticks; // No ticks available
    //     }
    //     // find suitable step for power of 10
    //     auto step = std::log2(ticksNum);
    //     if (step < 0.0) {
    //         step = std::pow(10.0, std::ceil(step));
    //     } else {
    //         step = std::pow(10.0, std::floor(step));
    //     }
    //     // // DBG("Ticks around note " << note.name
    //     //     << ": lowerPeriod = " << lowerPeriod
    //     //     << ", upperPeriod = " << upperPeriod
    //     //     << ", note = " << note.midiNote
    //     //     << ", actualLowerNote = " << actualLowerNote
    //     //     << ", actualUpperNote = " << actualUpperNote
    //     //     << ", ticksNum = " << ticksNum
    //     //     << ", step = " << step);
    //     ticks.reserve(static_cast<size_t>(ticksNum));
    //     int intStep = static_cast<int>(step);
    //     for (int p = upperPeriod; p >= lowerPeriod; p -= intStep) {
    //         auto n = tuningSystem->periodToMidiNote(p, mode);
    //         ticks.push_back(n - note.midiNote);
    //         // // DBG("Tick for period " << p << ": note = " << n << ", offtune = " << (n - note.midiNote));
    //     }
    //     return ticks;
    // }

    // used in tests
    String getScaleName() const {
        return Scale::getTonicName(getCurrentTonic()) + " " + Scale(getCurrentScaleType()).getName();
    }

    // String getTuningTypeName() const {
    //     return tuningSystem ? String(std::string(tuningSystem->getType().getLabel())) : "Unknown tuning system type";
    // }

    String getTuningDescription() const {
        return tuningSystem ? tuningSystem->getDescription() : "Default tuning";
    }

    std::vector<int> getScaleNotes(int octave, bool includeOctave = false) const {
        const auto& scale = currentScale;
        std::vector<int> result;
        result.reserve(scale.getIntervals().size() + (includeOctave ? 1 : 0));
        for (const auto& interval : scale.getIntervals()) {
            int noteNumber = static_cast<int>(getCurrentTonic()) + interval + (octave + 1) * 12;
            result.push_back(noteNumber);
        }
        if (includeOctave) {
            result.push_back(static_cast<int>(getCurrentTonic()) + (octave + 2) * 12);
        }
        return result;
    }

    std::vector<int> getScaleNotesFrom(int midiNote, size_t numNotes = 5) const {
        const auto& scale = currentScale;
        std::vector<int> result;
        result.reserve(numNotes);
        // let midiNote = D4, root = E
        // (midiNote % 12) = 2, root = 4
        // startInterval = 10
        // rootMidiNote = E3
        int root = static_cast<int>(getCurrentTonic());
        int startInterval = ((midiNote % 12) - root + 12) % 12;
        int rootMidiNote = midiNote - startInterval;

        auto scaleIntervals = scale.getIntervalsForOctaves(2);
        // find startInterval in scale
        auto it = std::find(scaleIntervals.begin(), scaleIntervals.end(), startInterval);
        if (it == scaleIntervals.end()) {
            // If the start interval is not found, return an empty vector
            return result;
        }
        // iterate from the found interval
        for (size_t i = 0; i < numNotes && it != scaleIntervals.end(); ++i, ++it) {
            int noteNumber = rootMidiNote + *it;
            if (noteNumber < 0 || noteNumber > 127) {
                // Skip notes outside MIDI range
                continue;
            }
            result.push_back(noteNumber);
        }
        return result;
    }

    void setCurrentScaleType(Scale::ScaleType scaleType) {
        selectedParams.scaleType.setStoredValue(scaleType);
        updateCurrentScale();
    }

    void updateCurrentScale() {
        if (currentScale.getType() != selectedParams.scaleType.getStoredValue()) {
            currentScale = Scale(selectedParams.scaleType.getStoredValue()); // Update cache only when needed
            sendChangeMessage();
        }
    }

    Scale::ScaleType getCurrentScaleType() const {
        return selectedParams.scaleType.getStoredValue();
    }

    const Scale& getCurrentScale() {
        updateCurrentScale();
        return currentScale;
    }

    Scale::Tonic getCurrentTonic() const {
        return selectedParams.tonic.getStoredValue();
    }

    void setCurrentTonic(Scale::Tonic tonic) {
        selectedParams.tonic.setStoredValue(tonic);
        // TODO double update after assigning to keyIndex1?
        tuningSystem->setRoot(tonic); // Update tuning system tonic
        sendChangeMessage();
    }

    // used in tests only
    static StringArray getScaleTypeNames() {
        StringArray names;
        for (auto scaleType : Scale::getAllScaleTypes()) {
            if (scaleType != Scale::ScaleType::User) {
                names.add(Scale::getNameForType(scaleType));
            }
        }
        return names;
    }

    static std::vector<NoteGridHeadingType> getHeaderTypes() {
        return {
            NoteGridHeadingType::Tuning,
            NoteGridHeadingType::Degrees,
            NoteGridHeadingType::Notes
        };
    }

    // Tuning table selection methods
    StringArray getTuningTableNames() const {
        return toStringArray(BuiltinTuningType::getLongLabels());
    }

    // used in tests only
    void setA4Frequency(double frequency) {
        // DBG("setA4Frequency: " << frequency);
        if (frequency >= 220.0 && frequency <= 880.0) {
            selectedParams.a4Frequency.setStoredValue(frequency);
        }
    }

    bool isToneEnabled() const {
        return selectedParams.playTone.getStoredValue();
    }

    bool isEnvelopeEnabled() const {
        return selectedParams.playEnvelope.getStoredValue();
    }

    bool isEnvelopePeriodsShown() const {
        // Envelope periods are shown only if envelope is enabled and playTone is not
        return selectedParams.playEnvelope.getStoredValue();
    }

    bool isModulationEnabled() const {
        // Envelope modulation is enabled when both tone and envelope are enabled
        return selectedParams.playTone.getStoredValue() && selectedParams.playEnvelope.getStoredValue();
    }

    int getEnvelopeInterval() const {
        // Get the modulation semitones based on the current modulation mode
        if (!isToneEnabled() || !isEnvelopeEnabled()) {
            return 0; // No modulation if tone or envelope is not enabled
        }
        return ModulationEnum::semitones[static_cast<size_t>(selectedParams.envInterval.getStoredValue())];
    }

    TuningSystem::PeriodMode getCurrentPeriodMode() const {
        return isEnvelopePeriodsShown() ? TuningSystem::Envelope : TuningSystem::Tone;
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
                asciiNoteName = asciiNoteName.replace("♯"_u, "#");
                asciiNoteName = asciiNoteName.replace("♭"_u, "b");
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
        filename += " " + String(selectedParams.clockFrequencyMhz.getStoredValue(), 2).replace(".", "_") + "MHz";

        // Add A4 frequency
        filename += " A4=" + String(selectedParams.a4Frequency.getStoredValue(), 0);

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

    EnvShape getEnvelopeShape() const {
        switch (selectedParams.envelopeShape.getStoredValue()) {
            case EnvShapeSimpleEnum::Triangle:
                return EnvShape::UP_DOWN_E;
            case EnvShapeSimpleEnum::Sawtooth:
                return EnvShape::UP_UP_C;
            default:
                return EnvShape::UP_DOWN_E; // Default to Triangle if unknown
        }
    }

    TuningSystemType getTuningType() const {
        jassert(tuningSystem != nullptr);
        return tuningSystem->getReferenceTuning()->getType();
    }

private:
    // Value::Listener implementation for bidirectional sync
    void valueChanged(Value& value) override {
        if (value.refersToSameSourceAs(selectedParams.scaleType.getPropertyAsValue())) {
            selectedParams.scaleType.forceUpdateOfCachedValue();
            updateCurrentScale();
        } else if (value.refersToSameSourceAs(selectedParams.tonic.getPropertyAsValue())) {
            // DBG("Root changed from valueChanged");
            selectedParams.tonic.forceUpdateOfCachedValue();
            tuningSystem->setRoot(getCurrentTonic());
            // DBG("Root changed from valueChanged sendChangeMessage");
            sendChangeMessage();
        } else if (value.refersToSameSourceAs(selectedParams.a4Frequency.getPropertyAsValue())) {
            selectedParams.a4Frequency.forceUpdateOfCachedValue();
            double newFreq = selectedParams.a4Frequency.getStoredValue();
            if (newFreq >= 220.0 && newFreq <= 880.0) {
                tuningSystem->setA4Frequency(newFreq);
                // DBG("A4 frequency changed from valueChanged sendChangeMessage");
                sendChangeMessage();
            }
        } else if (value.refersToSameSourceAs(selectedParams.clockFrequencyMhz.getPropertyAsValue())) {
            selectedParams.clockFrequencyMhz.forceUpdateOfCachedValue();
            double newFreqMHz = selectedParams.clockFrequencyMhz.getStoredValue();
            tuningSystem->setClockFrequency(newFreqMHz * MHz);
            // DBG("Clock frequency changed from valueChanged sendChangeMessage");
            sendChangeMessage();
        } else if (value.refersToSameSourceAs(selectedParams.tuningType.getPropertyAsValue())) {
            selectedParams.tuningType.forceUpdateOfCachedValue();
            updateReferenceTuning();
            sendChangeMessage();
        } else if (value.refersToSameSourceAs(selectedParams.tuningTable.getPropertyAsValue())) {
            selectedParams.tuningTable.forceUpdateOfCachedValue();
            recreateTuningSystem();
            sendChangeMessage();
        } else if (value.refersToSameSourceAs(selectedParams.playChords.getPropertyAsValue())) {
            selectedParams.playChords.forceUpdateOfCachedValue();
            // DBG("playChords changed from valueChanged");
            updatePlayState();
            // sendChangeMessage();
        } else if (value.refersToSameSourceAs(selectedParams.playTone.getPropertyAsValue())) {
            selectedParams.playTone.forceUpdateOfCachedValue();
            // DBG("playTone changed from valueChanged");
            updatePlayState();
            // sendChangeMessage();
        } else if (value.refersToSameSourceAs(selectedParams.playEnvelope.getPropertyAsValue())) {
            selectedParams.playEnvelope.forceUpdateOfCachedValue();
            // DBG("playEnvelope changed from valueChanged");
            updatePlayState();
        } else if (value.refersToSameSourceAs(selectedParams.envelopeShape.getPropertyAsValue())) {
            selectedParams.envelopeShape.forceUpdateOfCachedValue();
            sendChangeMessage();
        } else if (value.refersToSameSourceAs(selectedParams.envInterval.getPropertyAsValue())) {
            selectedParams.envInterval.forceUpdateOfCachedValue();
            sendChangeMessage();
        } else if (value.refersToSameSourceAs(selectedParams.retriggerTone.getPropertyAsValue())) {
            selectedParams.retriggerTone.forceUpdateOfCachedValue();
            sendChangeMessage();
        }
    }

    void recreateTuningSystem() {
        // DBG("Recreating tuning system with index: " << tuningTableIndex0.get());
        auto builtinTable = selectedParams.tuningTable.getStoredValue();

        TuningOptions options {
            .builtinTable = builtinTable,
            .tonic = getCurrentTonic(),
            .scaleType = getCurrentScaleType(),
            .chipClockFreq = selectedParams.clockFrequencyMhz.getStoredValue() * MHz,
            .a4Frequency = selectedParams.a4Frequency.getStoredValue()
        };

        tuningSystem = makeBuiltinTuning(options);

        if (tuningSystem) {
            // Apply tuning defaults when changing tuning table or initializing
            selectedParams.tuningType.setStoredValue(tuningSystem->getReferenceTuning()->getType());
            selectedParams.tonic.setStoredValue(options.tonic);
            selectedParams.scaleType.setStoredValue(options.scaleType);
            selectedParams.clockFrequencyMhz.setStoredValue(options.chipClockFreq / MHz);
            selectedParams.a4Frequency.setStoredValue(options.a4Frequency);
        }
    }

    void updateReferenceTuning() {
        if (tuningSystem) {
            tuningSystem->setReferenceTuning(makeReferenceTuningSystem(
                selectedParams.tuningType.getStoredValue(),
                getCurrentTonic(),
                selectedParams.a4Frequency.getStoredValue()
        ));
        }
    }

    void updatePlayState() {
        if (selectedParams.playChords.getStoredValue() && !selectedParams.playTone.getStoredValue()) {
            selectedParams.playTone.setStoredValue(true);
        } else if (!selectedParams.playTone.getStoredValue() && !selectedParams.playEnvelope.getStoredValue()) {
            // If both playTone and playEnvelope are false, enable playTone
            selectedParams.playTone.setStoredValue(true);
        }
        sendChangeMessage(); // Notify listeners about the state change
    }

    // Helper methods
    // Helper method for finding best matching clock preset

    // int findBestMatchingClockPreset(double frequency) {
    //     int bestMatch = static_cast<int>(ChipClockEnum::Custom); // Default to Custom
    //     double bestDiff = std::numeric_limits<double>::max();

    //     for (int i = 0; i < static_cast<int>(ChipClockEnum::Custom); ++i) {
    //         double presetFreq = ChipClockEnum::clockValues[i];
    //         double diff = std::abs(frequency - presetFreq);
    //         if (diff < bestDiff && diff < 1000.0) { // Within 1kHz tolerance
    //             bestMatch = i;
    //             bestDiff = diff;
    //         }
    //     }

    //     return bestMatch;
    // }
    //-------------------------------------------------------------------------
private:
    // Transient view state
    ValueTree transientState;
    te::Edit& edit;
    UndoManager& undoManager; // Undo manager reference for ParamAttachments

    // Cached objects derived from values (performance optimization) - only for complex conversions
    mutable Scale currentScale;            // Scale object cache
    std::shared_ptr<TuningSystem> tuningSystem;

public:

    SelectionParams selectedParams;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningViewModel)
};

} // namespace MoTool


namespace juce {

using namespace MoTool;
using namespace MoTool::Util;

template <>
struct VariantConverter<EnvShapeChoice> : public EnumVariantConverter<EnvShapeChoice> {};

template <>
struct VariantConverter<EnvIntervalChoice> : public EnumVariantConverter<EnvIntervalChoice> {};


}  // namespace juce

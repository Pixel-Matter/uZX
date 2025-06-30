#pragma once

#include <JuceHeader.h>
#include <memory>

#include "../../util/enumchoice.h"
#include "TemperamentSystem.h"

namespace MoTool {

struct TuningTypeEnum {
    enum Enum : size_t {
        AutoTuning,
        CustomTable,
    };

    static inline constexpr std::string_view longLabels[] {
        "Auto tuning",
        "Custom tuning table",
    };
};

using TuningType = MoTool::Util::EnumChoice<TuningTypeEnum>;

struct ChipCapabilities {
    // double clockFrequency; // Chip clock frequency in Hz
    int divider;           // Divider value for the chip (e.g., 16 for AY-3-8910)
    Range<int> registerRange; // Range of tone register supported by the chip (e.g., 1-4095 inclusive)
};

class TuningSystem {
public:
    TuningSystem(const ChipCapabilities& capabilities, double chipClock, std::unique_ptr<TemperamentSystem> refTuning)
        : chip(capabilities)
        , clockFrequency(chipClock)
        , referenceTuning(std::move(refTuning))
    {
        jassert(referenceTuning != nullptr);
    }

    virtual ~TuningSystem() = default;
    virtual String getDescription() const = 0;
    inline String getTypeName() const {
        return getType().getLongLabel().data();
    }
    virtual TuningType getType() const = 0;
    virtual int midiNoteToPeriod(double midiNote) const = 0;
    virtual double periodToMidiNote(int period) const = 0;
    virtual bool isDefined(int midiNote) const = 0;

    inline TemperamentSystem* getReferenceTuning() const {
        return referenceTuning.get();
    }
    inline void setReferenceTuning(std::unique_ptr<TemperamentSystem> refTuning) {
        this->referenceTuning = std::move(refTuning);
    }
    // Default chip-based period/frequency conversion
    double periodToFrequency(int period) const;
    int frequencyToPeriod(double frequency) const;

    // Core conversion functions
    // midiNote is double because we want slides and pitch bends
    double midiNoteToFrequency(double midiNote) const;
    double frequencyToMidiNote(double frequency) const;
    double getOfftune(double midiNote) const;

    // Setters
    void setA4Frequency(double frequency);
    double getA4Frequency() const;
    void setClockFrequency(double frequency);
    double getClockFrequency() const;
    void setRoot(Scale::Key newRoot);
    Scale::Key getRoot() const;

    // Serialization
    // virtual juce::ValueTree getState() const = 0;
    // virtual void setState(const juce::ValueTree& state) = 0;
protected:
    // TODO use CachedValues refTo-ed to a state value in a tree
    const ChipCapabilities& chip;
    double clockFrequency;
    std::unique_ptr<TemperamentSystem> referenceTuning;

    double getReferenceFrequency(int midiNote) const;
    double getReferenceFrequency(double midiNote) const;
};

}
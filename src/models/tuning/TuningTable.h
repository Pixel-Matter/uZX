#pragma once

#include <JuceHeader.h>

#include "TuningSystemBase.h"


namespace MoTool {


class TuningTable final : public TuningSystem {
public:
    TuningTable(
        const ChipCapabilities& caps,
        double chipClock,
        std::unique_ptr<TemperamentSystem> refTuning,
        const std::map<int, int>& periodTable,
        const String& customName
    );

    // Constructor for ProTracker-style sequential period tables
    TuningTable(
        const ChipCapabilities& caps,
        double chipClock,
        std::unique_ptr<TemperamentSystem> refTuning,
        int startingMidiNote,
        const std::vector<int>& periods,
        const String& customName
    );

    // Constructor with initializer list for convenience
    TuningTable(
        const ChipCapabilities& caps,
        double chipClock,
        std::unique_ptr<TemperamentSystem> refTuning,
        int startingMidiNote,
        std::initializer_list<int> periods,
        const String& customName
    );

    String getName() const override;
    TuningType getType() const override;
    int midiNoteToPeriod(double midiNote) const override;
    double periodToMidiNote(int period) const override;
    bool isDefined(int midiNote) const override;

    // Additional methods specific to TuningTable
    const std::map<int, int>& getPeriodTable() const;
    void updatePeriodTable(const std::map<int, int>& newTable);
    Range<int> getDefinedNoteRange() const;

private:
    std::map<int, int> periodTable_; // MIDI note -> chip period
    String customName_;
    int minDefinedNote_;
    int maxDefinedNote_;

    int findClosestNoteByPeriod(int period) const;
};

}
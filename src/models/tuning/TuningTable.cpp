#include "TuningTable.h"

namespace MoTool {

TuningTable::TuningTable(
    double chipClock,
    std::unique_ptr<ReferenceTuningSystem> refTuning,
    const std::map<int, int>& periodTable,
    const String& customName
)
    : TuningSystem(chipClock, std::move(refTuning))
    , periodTable_(periodTable)
    , customName_(customName)
{
    // Find the range of defined notes
    if (!periodTable_.empty()) {
        minDefinedNote_ = periodTable_.begin()->first;
        maxDefinedNote_ = periodTable_.rbegin()->first;
    } else {
        minDefinedNote_ = 0; // Default to middle C
        maxDefinedNote_ = 0;
    }
}

TuningTable::TuningTable(
    double chipClock,
    std::unique_ptr<ReferenceTuningSystem> refTuning,
    int startingMidiNote,
    const std::vector<int>& periods,
    const String& customName
)
    : TuningSystem(chipClock, std::move(refTuning))
    , customName_(customName)
{
    // Build period table from sequential array starting at startingMidiNote
    for (size_t i = 0; i < periods.size(); ++i) {
        periodTable_[startingMidiNote + static_cast<int>(i)] = periods[i];
    }

    // Find the range of defined notes
    if (!periodTable_.empty()) {
        minDefinedNote_ = periodTable_.begin()->first;
        maxDefinedNote_ = periodTable_.rbegin()->first;
    } else {
        minDefinedNote_ = startingMidiNote;
        maxDefinedNote_ = startingMidiNote;
    }
}

TuningTable::TuningTable(
    double chipClock,
    std::unique_ptr<ReferenceTuningSystem> refTuning,
    int startingMidiNote,
    std::initializer_list<int> periods,
    const String& customName
)
    : TuningTable(chipClock, std::move(refTuning), startingMidiNote, std::vector<int>(periods), customName)
{}

String TuningTable::getDescription() const {
    return customName_ + ", defined notes "
        + getMidiNoteName(minDefinedNote_) + "-" + getMidiNoteName(maxDefinedNote_)
        + String::formatted(", A4 = %.2fHz", getA4Frequency());
}

TuningType TuningTable::getType() const {
    return TuningType::CustomTable;
}

int TuningTable::midiNoteToPeriodRaw(double midiNote) const {
    int note = static_cast<int>(std::round(midiNote));

    // If exact note is in table, return it
    auto it = periodTable_.find(note);
    if (it != periodTable_.end()) {
        return it->second;
    }

    int result;
    // Handle notes outside the defined range
    if (note < minDefinedNote_) {
        // Extrapolate downward using octave relationship
        int octaveShift = (minDefinedNote_ - note + 11) / 12;
        int baseNote = note + octaveShift * 12;
        if (auto baseIt = periodTable_.find(baseNote); baseIt != periodTable_.end()) {
            result = baseIt->second * (1 << octaveShift); // Double period for each octave down
        } else {
            result = periodTable_.begin()->second * 2; // Fallback
        }
        return result;
    }

    if (note > maxDefinedNote_) {
        // Extrapolate upward using octave relationship
        int octaveShift = (note - maxDefinedNote_ + 11) / 12;
        int baseNote = note - octaveShift * 12;
        if (auto baseIt = periodTable_.find(baseNote); baseIt != periodTable_.end()) {
            result = std::max(1, baseIt->second / (1 << octaveShift)); // Halve period for each octave up
        } else {
            result = std::max(1, periodTable_.rbegin()->second / 2); // Fallback
        }
        return result;
    }

    // Interpolate between nearest defined notes
    auto upper = periodTable_.upper_bound(note);
    if (upper == periodTable_.end()) {
        return periodTable_.rbegin()->second;
    }
    if (upper == periodTable_.begin()) {
        return upper->second;
    }

    auto lower = std::prev(upper);

    // Linear interpolation in logarithmic space (geometric interpolation)
    double ratio = static_cast<double>(note - lower->first) / (upper->first - lower->first);
    double logPeriod = std::log(static_cast<double>(lower->second)) * (1.0 - ratio)
                       + std::log(static_cast<double>(upper->second)) * ratio;

    result = static_cast<int>(std::round(std::exp(logPeriod)));
    return result;
}

int TuningTable::midiNoteToPeriod(double midiNote, PeriodMode mode) const {
    constexpr int baseDivider = getPeriodMode(PeriodMode::Tone).divider;
    auto period = midiNoteToPeriodRaw(midiNote);
    auto pmode = getPeriodMode(mode);
    auto addDivider = pmode.divider / baseDivider;
    // divide rounded to addDivider
    period = (period + addDivider / 2) / addDivider;
    return jlimit(pmode.registerRange.getStart(), pmode.registerRange.getEnd() - 1, period);
}

double TuningTable::periodToMidiNote(int period, PeriodMode mode) const {
    if (periodTable_.empty()) {
        return 60.0; // Default to middle C
    }

    int closestNote = findClosestNoteByPeriod(period);

    // Calculate more precise note using frequency relationship
    double actualFreq = periodToFrequency(period, mode);
    double targetFreq = periodToFrequency(periodTable_.at(closestNote), mode);
    double noteOffset = 12.0 * std::log2(actualFreq / targetFreq);

    return closestNote + noteOffset;
}

bool TuningTable::isDefined(int midiNote) const {
    // Check if note is directly in the period table
    if (periodTable_.find(midiNote) != periodTable_.end()) {
        return true;
    }
    return false;  // only defined notes are those in the period table, not interpolated ones
}

const std::map<int, int>& TuningTable::getPeriodTable() const {
    return periodTable_;
}

void TuningTable::updatePeriodTable(const std::map<int, int>& newTable) {
    periodTable_ = newTable;
    if (!periodTable_.empty()) {
        minDefinedNote_ = periodTable_.begin()->first;
        maxDefinedNote_ = periodTable_.rbegin()->first;
    }
}

Range<int> TuningTable::getDefinedNoteRange() const {
    return Range<int>(minDefinedNote_, maxDefinedNote_ + 1);
}

int TuningTable::findClosestNoteByPeriod(int period) const {
    if (periodTable_.empty()) {
        return 60; // Default to middle C
    }

    // Binary search for closest period using lower_bound
    // Since periods decrease as notes get higher, we need to search by period value
    int closestNote = periodTable_.begin()->first;
    int closestPeriodDiff = std::abs(periodTable_.begin()->second - period);

    // Find the first note with period <= target period
    auto it = std::lower_bound(periodTable_.begin(), periodTable_.end(), period,
        [](const auto& pair, int targetPeriod) {
            return pair.second > targetPeriod; // Note: periods decrease with higher notes
        });

    // Check the found position and its neighbors
    if (it != periodTable_.end()) {
        int diff = std::abs(it->second - period);
        if (diff < closestPeriodDiff) {
            closestPeriodDiff = diff;
            closestNote = it->first;
        }
    }

    // Check the previous element if it exists
    if (it != periodTable_.begin()) {
        auto prev = std::prev(it);
        int diff = std::abs(prev->second - period);
        if (diff < closestPeriodDiff) {
            closestPeriodDiff = diff;
            closestNote = prev->first;
        }
    }

    return closestNote;
}

}
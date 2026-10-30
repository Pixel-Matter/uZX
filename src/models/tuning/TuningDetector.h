#pragma once

#include <JuceHeader.h>

#include <array>
#include <map>
#include <optional>
#include <vector>

#include "../PsgParameter.h"
#include "Scales.h"
#include "TuningRegistry.h"

namespace MoTool {

//==============================================================================
/** Period -> MIDI note for every note a tuning defines. Trackers write these exact
    register values, so a note either is or is not on the grid: no cents tolerance. */
using PeriodGrid = std::map<int, int>;

PeriodGrid makePeriodGrid(const TuningSystem& tuning);
PeriodGrid makeEqualTemperamentGrid(double a4Frequency, double chipClock);

//==============================================================================
/** Frame-weighted counts of the audible pitches of a clip, built from the same
    visitFrameNotes() rules the Notes view paints. Envelope pitches are kept apart:
    a tracker rounds them to tone / 16, so they only match a grid within half an
    envelope step. */
struct PitchHistogram
{
    std::map<int, int> tone;      // tone period -> frames
    std::map<int, int> envelope;  // 16 * envelope period -> frames

    void addFrame(const PsgParamFrameData& frame);
    int getTotal() const noexcept;
    bool isEmpty() const noexcept { return tone.empty() && envelope.empty(); }

    /** Share of note-frames on the grid: tone exact, envelope within ±envelopeTolerance. */
    float getShareOnGrid(const PeriodGrid& grid) const;

    static constexpr int envelopeTolerance = 8;  // half an envelope step, in tone-period units
};

//==============================================================================
struct DetectedKey
{
    Scale::Tonic tonic;
    bool minor = false;
    float correlation = 0.0f;

    String getName() const;  // "E minor"
    Scale::ScaleType getScaleType() const;
};

/** The 24 major/minor keys ranked by Krumhansl-Kessler profile correlation, best first. */
std::vector<DetectedKey> rankKeys(const std::array<float, 12>& pitchClasses);

/** Normalised weight per pitch class; off-grid periods go to their nearest grid note. */
std::array<float, 12> makePitchClassHistogram(const PitchHistogram& hist, const PeriodGrid& grid);

/** Pitch classes at sub-semitone resolution: every period is placed at its fractional
    note on the grid (grid notes sit exactly on bin multiples of binsPerSemitone, whether
    or not the table is octave-periodic), folded to one octave, and smeared over its
    quantisation band so high notes show as wide low bumps. Bin 0 is C; the result is
    normalised to a peak of 1. */
std::vector<float> makeFoldedPitchHistogram(const PitchHistogram& hist, const PeriodGrid& grid, int binsPerSemitone);

//==============================================================================
struct TuningCandidate
{
    BuiltinTuningType table;
    float share = 0.0f;
};

struct EqualTemperamentFit
{
    double a4Frequency = 440.0;  // at the ZX clock
    float share = 0.0f;
};

struct TuningDetection
{
    /** Built-in tables best first. Grids that share over 90% of their periods are one
        candidate, named by the later registry entry (PT #3 absorbs PT #0). */
    std::vector<TuningCandidate> tables;
    /** The best equal-temperament grid over one semitone of A4 around 440 Hz. */
    EqualTemperamentFit equalTemperament;
    /** True when the fit beat the best table by more than tieGap. */
    bool usesEqualTemperament = false;

    PeriodGrid grid;     // the chosen grid
    float share = 0.0f;  // of the chosen grid
    bool tableTie = false;

    std::array<float, 12> pitchClasses{};
    std::vector<DetectedKey> keys;  // best first
    bool keyTie = false;

    String getTableLabel() const;  // "PT #1" or "12TET 436 Hz"
    const DetectedKey& getKey() const { return keys.front(); }

    static constexpr float tieGap = 0.05f;
};

TuningDetection detectTuning(const PitchHistogram& hist);

/** The grid of a built-in table (cached: the registry builds them from constants). */
const PeriodGrid& getBuiltinPeriodGrid(BuiltinTuningType table);

String getEqualTemperamentLabel(double a4Frequency);

}  // namespace MoTool

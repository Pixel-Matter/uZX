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

/** The clock tunings are fitted at. Tracker tables are period lists and need no clock; a
    fitted tuning is reported at this clock unless another common clock brings its A4 to
    440 Hz (see TuningFit). */
inline constexpr double tuningGridClock = uZX::ChipClockEnum::clockValues[uZX::ChipClockEnum::ZX_Spectrum_1_77_MHz];

/** Clocks a fitted tuning can be expressed at, most likely first. Detection guesses only
    the ZX scene's clocks, and a draw goes to the earlier one. The Atari ST and Amstrad are
    chosen by hand: a ZX table two semitones low (PT #1) is 440 Hz at the ST's 2 MHz, and
    Amstrad's 1 MHz is the same periods an octave down. */
struct TuningClock
{
    double hz;
    const char* name;
    bool guessed;
};
/** "Pentagon" for a listed clock, else its frequency: "1.7897 MHz". */
String getClockName(double chipClock);

inline constexpr TuningClock tuningClocks[] {
    {tuningGridClock, "ZX Spectrum", true},
    {uZX::ChipClockEnum::clockValues[uZX::ChipClockEnum::Pentagon_1_75_MHz], "Pentagon", true},
    {uZX::ChipClockEnum::clockValues[uZX::ChipClockEnum::Atari_ST_2_MHz], "Atari ST", false},
    {uZX::ChipClockEnum::clockValues[uZX::ChipClockEnum::Amstrad_1_MHz], "Amstrad", false},
};

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

    /** Collects frames in flat per-period counters and makes the histogram once at the
        end: a map update per note is far slower over tens of thousands of frames. */
    class Builder
    {
    public:
        /** Adds a register state that sounds for `frames` machine frames. */
        void addFrame(const PsgParamFrameData& frame, int frames = 1);
        PitchHistogram build() const;

    private:
        // Register periods are 16-bit; the envelope pitch is filtered to the tone range.
        std::vector<int> tone_ = std::vector<int>(1 << 16), envelope_ = std::vector<int>(1 << 16);
    };

    int getTotal() const noexcept;
    bool isEmpty() const noexcept { return tone.empty() && envelope.empty(); }
    /** Adds another histogram's frames, as for clips played together. */
    PitchHistogram& operator+=(const PitchHistogram& other);

    /** Share of note-frames on the grid: tone exact, envelope within ±envelopeTolerance.
        Envelope pitches whose window is a semitone or wider match every grid, so they are
        left out of the share entirely. */
    float getShareOnGrid(const PeriodGrid& grid) const;

    /** Same, for a grid given as its periods in ascending order. */
    float getShareOnPeriods(const std::vector<int>& ascendingPeriods) const;

    static constexpr int envelopeTolerance = 8;  // half an envelope step, in tone-period units

    /** True when an envelope pitch at this tone-equivalent period can tell grids apart. */
    static bool isEnvelopeDiscriminating(int period) noexcept;
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

/** True when the two best keys are too close to call (see TuningDetection::tieGap). */
bool isKeyTie(const std::vector<DetectedKey>& rankedKeys);

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

/** A computed tuning fitted to a clip: equal temperament or a just intonation built on a
    tonic, at an A4 and a chip clock. Periods only fix the pitch relative to the clock (A4 =
    440 Hz at the Pentagon clock is 446 Hz at the ZX clock), so a fit can be expressed at any
    clock with the same share; detection names the guessed clock at which A4 is within a few
    cents of 440 Hz, else the ZX clock. */
struct TuningFit
{
    TuningSystemType system = TuningSystemType::EqualTemperament;
    Scale::Tonic tonic = Scale::Tonic::C;  // a just intonation's tonic; C for 12TET
    double a4Frequency = 440.0;            // at chipClock
    double chipClock = tuningGridClock;
    float share = 0.0f;
};

/** The systems detection fits, simplest first: a just intonation has to beat 12TET. */
inline constexpr TuningSystemEnum::Enum fittedSystems[] {
    TuningSystemEnum::EqualTemperament, TuningSystemEnum::Just5Limit, TuningSystemEnum::Just5LimitT45_64,
    TuningSystemEnum::Pythagorean};

PeriodGrid makeFitGrid(const TuningFit& fit);

/** The same periods at another clock. They sound at frequencies scaled by the clock ratio,
    and the tonic is named after the nearest equal-temperament note at A4 = 440 Hz, so the
    names move with the clock: a ZX 12TET at 436 Hz is an Atari ST one at 438 Hz, every note
    named two semitones higher. */
TuningFit atClock(const TuningFit& fit, double chipClock);

/** "12TET 436 Hz", or "5LJI E 440 Hz Pentagon" for a just intonation at another clock. */
String getFitLabel(const TuningFit& fit);

struct TuningDetection
{
    /** False for a clip with no audible pitches: nothing below is meaningful then. */
    bool hasNotes = false;
    /** The histogram the detection was made from; the picker draws it. */
    PitchHistogram histogram;

    /** Tracker tables best first; the computed presets (12TET, just intonations) are left
        out. Grids that share over 90% of their periods are one candidate, named by the
        later registry entry (PT #3 absorbs PT #0). */
    std::vector<TuningCandidate> tables;
    /** One fit per fitted system, best first; 12TET leads unless a just intonation beats
        it by more than tieGap. Never empty. */
    std::vector<TuningFit> fits;
    /** True when the first fit beat the best table by more than tieGap. */
    bool usesFit = false;

    PeriodGrid grid;     // the chosen grid
    float share = 0.0f;  // of the chosen grid
    bool tableTie = false;

    std::array<float, 12> pitchClasses{};
    std::vector<DetectedKey> keys;  // best first
    bool keyTie = false;

    String getTableLabel() const;  // "PT #1" or getFitLabel(getFit())
    const DetectedKey& getKey() const { return keys.front(); }
    const TuningFit& getFit() const { return fits.front(); }

    static constexpr float tieGap = 0.05f;
};

TuningDetection detectTuning(const PitchHistogram& hist);

/** The grid of a built-in table (cached: the registry builds them from constants). */
const PeriodGrid& getBuiltinPeriodGrid(BuiltinTuningType table);

/** True for the lists trackers play (PT, VT); false for the presets the registry computes
    (12TET, just intonations), which detection leaves out. */
bool isTrackerTable(BuiltinTuningType table);

}  // namespace MoTool

#pragma once

#include "../midi/midi.h"
#include "../utils/tools.h"
#include "../utils/just_intonation.h"

#include <utility>


namespace uZX::Tuning {

using namespace uZX::Midi;


// Maps MIDI notes to frequencies and periods for PSG chips clock counters
using FrequencyTable = std::array<double, N_MIDI_NOTES>;
using PeriodTable = std::array<int, N_MIDI_NOTES>;


template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}


class Tuner {
public:
    auto setClock(double clock) noexcept -> void {
        clock_ = clock;
    }

    virtual ~Tuner() = default;
    // constexpr virtual auto getFreq(float pitch) noexcept -> double = 0;
    constexpr virtual auto getPeriod(float pitch, int divider, int maxPeriod) noexcept -> int = 0;
    // TODO tables are useless for detunes and pitch wheel (unless somehow interpolated)
//    constexpr virtual auto getFreqTable() noexcept -> FrequencyTable = 0;
//    constexpr virtual auto getPeriodTable(double clock, int divider, int maxPeriod) noexcept -> PeriodTable = 0;

protected:
    double baseFreq_ = 440.0;
    int baseNote_ = 69;
    double clock_;
};


class EqualTemperamentTuner : public Tuner {
public:
    auto getPeriod(float pitch, int divider, int maxPeriod) noexcept -> int override {
        assert(clock_ > 0 && "Clock can't be zero");
        // TODO optimize to use linear interpolation between freq table entries
        auto freq = noteToFreq(pitch, baseFreq_, baseNote_);
        // std::cerr << "Freq " << freq << std::endl;
        return std::min(freqToPeriod(freq, clock_, divider), maxPeriod);
    }

    // TODO tables are useless for detunes and pitch wheel (unless somehow interpolated)
    // auto getFreqTable() noexcept -> FrequencyTable override {
    //     FrequencyTable result {};
    //     for (int note = 0; note < N_MIDI_NOTES; ++note) {
    //         result[note] = noteToFreq(note, baseFreq_, baseNote_);
    //     }
    //     return result;
    // }

    // TODO tables are useless for detunes and pitch wheel (unless somehow interpolated)
    // auto getPeriodTable(double clock, int divider, int maxPeriod) noexcept -> PeriodTable override {
    //     PeriodTable result {};
    //     for (int note = 0; note < N_MIDI_NOTES; ++note) {
    //         const auto freq = noteToFreq(note, baseFreq_, baseNote_);
    //         const auto period = std::min(freqToPeriod(freq, clock_, divider), maxPeriod);
    //         result[note] = period;
    //         // std::cerr << note << ", " << freq << ", " << period << std::endl;
    //         // TODO round to make fifths more pure, but how to do so?
    //         // Hyerarchical tuning? 5th, 3rd, 2nd, 4th, 6th, 7th, semitones
    //     }
    //     return result;
    // }
};


class JustIntonationTuner : public Tuner {
private:
    auto calcTableBaseNote(int divider) const -> int {
        const auto octBasePeriod = table_[0];
        const auto octBaseFreq = periodToFreq(octBasePeriod, clock_, divider);
        const auto note = freqToNote(octBaseFreq, baseFreq_, baseNote_);
        // std::cerr << "octBasePeriod " << octBasePeriod << std::endl
        //           << "octBaseFreq " << octBaseFreq << std::endl
        //           << "note " << note << std::endl
        //           << "round note " << std::round(note) << std::endl
        //           << "octBaseFreq check " << noteToFreq(note, baseFreq_, baseNote_) << std::endl;
        return std::round(note);
    }

public:
    using TableMaker = std::function<const Just::OctavePeriodsTable&()>;

    JustIntonationTuner(TableMaker&& tableMaker)
        : table_ {tableMaker()}
    {}

    const static constexpr auto table = Just::getEPhrygianPeriods();

    auto getPeriodFromTable(int note, int divider, int maxPeriod) noexcept -> float {
        // std::cerr << "Note " << note << std::endl;
        const int tableBaseNote = calcTableBaseNote(divider);
        float period = table_[std::abs((note + 120 - tableBaseNote) % 12)];
        int octave = (note + 120 - tableBaseNote) / 12 - 10;  // we need rounding down during the division

        // std::cerr << "tableBaseNote " << tableBaseNote << std::endl
        //           << "in-octave period " << period << std::endl
        //           << "octave " << octave << std::endl;

        if (octave < 0) {
            for (int i = 0; i < -octave; ++i) {
                period *= 2;
            }
        } else {
            for (int i = 0; i < octave; ++i) {
                period /= 2;
            }
        }
        return period;
    }

    auto getPeriod(float pitch, int divider, int maxPeriod) noexcept -> int override {
        assert(clock_ > 0 && "Clock can't be zero");
        int note = std::round(pitch);
        const float detune = pitch - note;
        float period = getPeriodFromTable(note, divider, maxPeriod);
        if (detune != 0.0) {
            const float period2 = getPeriodFromTable(note + sgn(detune), divider, maxPeriod);
            period = period * (1.0 - std::abs(detune)) + period2 * std::abs(detune);
        }
        return std::min(static_cast<int>(std::round(period)), maxPeriod);
    }

private:
    Just::OctavePeriodsTable table_;
};


struct TuningEnum {
    enum Enum {
        EQUAL_TTEMPERAMENT,
        JUST_INTONATION_7_LIMIT,
        JUST_INTONATION_E_PHRYGIAN,
    };
    static inline constexpr std::string_view labels[] {
        "Equal temperament",
        "Just intonation (7Limit)",
        "Just intonation (EPhrygian)",
    };
};
using TuningType = EnumChoice<TuningEnum>;


namespace {

static inline std::array<std::shared_ptr<Tuner>, TuningType::size()> TUNERS {
    std::make_shared<EqualTemperamentTuner>(),
    std::make_shared<JustIntonationTuner>(&Just::get7LimitPeriods),
    std::make_shared<JustIntonationTuner>(&Just::getEPhrygianPeriods),
};

}

inline constexpr auto getTunings() noexcept {
    return TuningType::getLabels();
}

inline auto getTuner(TuningType tuning) noexcept -> Tuner* {
    return TUNERS[static_cast<size_t>(tuning)].get();
}



}  // namespace uZX

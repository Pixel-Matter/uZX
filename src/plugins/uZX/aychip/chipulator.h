#pragma once

#include "../midi/midi.h"
#include "../midi/effect.h"
#include "../instrument/oscillator.h"
#include "aychip.h"
#include "freq_tables.h"
#include <functional>

namespace uZX {

using namespace Midi;
using namespace Tuning;

// Gets MIDI messages from channels
// Knows avaliable chips and their voices
class Chipulator: public Midi::Effect<Chipulator> {
    using Base = Midi::Effect<Chipulator>;

public:
    Chipulator(Chip::AYInterface& chip, TuningType tunerType)
        : Base {}
        , chip_ {chip}
        , tuner_ {getTuner(tunerType)}
    {
        Base::reset();
        setMidiInChan(13);
        setMidiOutChan(13);
    }

    void reset() {
        const auto clock = chip_.getClock();
        tuner_->setClock(clock);
        // noteTable_ = tuner_->getPeriodTable(clock, 16, 4095);
        // envTable_ = tuner_->getPeriodTable(clock, 16 * 16, 65535);
    }

    auto onSetMidiOutChan() noexcept -> void {
    }

    auto setTuning(TuningType t) noexcept -> void {
        tuner_ = getTuner(t);
        tuner_->setClock(chip_.getClock());
    }

    int getNotePeriod(float tone) const {
        return tuner_->getPeriod(tone, 16, 4095);
    }

    int getEnvPeriod(float tone) const {
        return tuner_->getPeriod(tone, 16 * 16, 65535);
    }

    constexpr inline auto toIntVolume(float volume) -> int {
        return static_cast<int>(std::round(15.0 * volume));
    }

    // TODO There is also retrigger events on channels 1 - 4

    auto renderNextStep(MidiStream& out, int sample) -> void {
        sendToChip();
    }

    auto renderAudioBlock(float* leftOut, float* rightOut, int numSamples) -> void {
        chip_.processBlock(leftOut, rightOut, numSamples);
    }

    auto getAyEnvShape(const ControllerChange& c) const noexcept -> Chip::AYInterface::EnvShape {
        using EnvShape = Chip::AYInterface::EnvShape;
        static constexpr std::array<Chip::AYInterface::EnvShape, Oscillator::Shape::size()> shapeToEnv {
            EnvShape::UP_HOLD_TOP_D,  // SQUARE  TODO what to do?
            EnvShape::UP_UP_C,        // SAW_UP
            EnvShape::DOWN_DOWN_8,    // SAW_DOWN
            EnvShape::UP_DOWN_E,      // TRIANGLE_UP
            EnvShape::DOWN_UP_A,      // TRIANGLE_DOWN
            EnvShape::UP_HOLD_TOP_D,  // NOISE   TODO what to do?
        };
        auto shape = c.value * Oscillator::Shape::size() / 128;
//        std::cerr << "Value: " << c.value
//                  << ", Osc shape idx: " << static_cast<double>(c.value) * Oscillator::Shape::size() / 128.0
//                  << " (" << shape
//                  << "), Env shape: " << shapeToEnv[shape]
//                  << " (" << int(shapeToEnv[shape]) << ")"
//                  << std::endl;
        return shapeToEnv[shape];
    }

    auto getAyMixTone(const ChannelState& state) const noexcept -> bool {
        return state.getController(CC_TONE_OFF) < 64;
    }

    auto getAyMixNoise(const ChannelState& state) const noexcept -> bool {
        return state.getController(CC_NOISE_ON) >= 64;
    }

    auto getNoiseFreq(const ChannelState& state) const noexcept -> int {
        return state.getController(CC_NOISE_TONE);
    }

    auto getAyMixEnv(const ChannelState& state) const noexcept -> bool {
        return state.getController(CC_ENV_ON) >= 64;
    }

    auto setNoteOn(int c, int noteNum) noexcept -> void {
        auto& chanState = getChannelState(getMidiInChan() + c);
        auto& note = chanState.getNote(noteNum);
        auto tone = chanState.getNoteTone(noteNum);
        const auto period = getNotePeriod(tone);
        // const auto toneOn = true;
        const auto toneOn = getAyMixTone(chanState);
        const auto noiseOn = getAyMixNoise(chanState);
        // const auto envOn = true;
        const auto envOn = getAyMixEnv(chanState);
        std::cerr << "Chip " << c << "-" << noteNum << "(" << tone << "/" << note
                  << "), p=" << period
                  << ", tne=" << toneOn << ", " << noiseOn << ", " << envOn << ", " << std::endl;
        // TODO If env == on we do not need to get volume, it's always max
        auto volume = toIntVolume(note.velocity);
        chip_.setTonePeriod(c, period);
        chip_.setVolume    (c, volume);
        chip_.setToneOn    (c, toneOn);
        chip_.setNoiseOn   (c, noiseOn);
        chip_.setEnvelopeOn(c, envOn);
    }

    auto setNoteOff(int c) noexcept -> void {
        chip_.setEnvelopeOn(c, false);
        chip_.setNoiseOn   (c, false);
        chip_.setVolume(c, 0);
        chip_.setToneOn    (c, false);
    }

    auto setEnvNoteOn(const ChannelState& chanState, int noteNum) noexcept -> void {
        auto& note = chanState.getNote(noteNum);
        auto tone = chanState.getNoteTone(noteNum);
        const auto period = getEnvPeriod(tone);
        std::cerr << "Chip env -" << noteNum << "(" << tone << "/" << note
                    << "), p=" << period
                    << std::endl;
        // TODO not here, change env period as soon as chanState changes
        chip_.setEnvelopePeriod(period);
    }

private:
    auto sendToChip() -> void {
        for (int c = 0; c < 3; c++) {
            auto& chanState = getChannelState(getMidiInChan() + c);
            auto events = chanState.diffAndApplyTo(prevStates_[c], c, 0);
            chanState = prevStates_[c];

            // TODO We can refactor this behavior to parent class Effect
            if (!events.empty()) {
                for (const auto& [sample, event] : events) {
                    event.visit(overloaded {
                        [this, c, sample=sample](const ControllerChange& m) {
                            std::cerr << "Channel " << c << ": " << sample << ", " << m << std::endl;
                            if (m.controller == CC_ENV_SHAPE) {
                                const auto shape = getAyEnvShape(m);
                                std::cerr << ", shape " << shape;
                                chip_.setEnvelopeShape(shape);
                                // TODO set modulation
                            }
                            std::cerr << std::endl;
                        },
                        [](const auto& other) {}
                    });
                }
                std::cerr << "==========================" << std::endl;
            }

            auto notes = chanState.getNotes();
            if (notes.size() > 0) {
                setNoteOn(c, notes.front());
            } else {
                setNoteOff(c);
            }
        }
        // Fake env, TODO remove this
        auto& modState = getChannelState(getMidiInChan());
        // auto& modState = getChannelState(getMidiInChan() + 3);
        auto notes = modState.getNotes();

        chip_.setNoisePeriod(getNoiseFreq(modState) / 4);  // 0..127 --> 0...31

        if (notes.size() > 0) {
            setEnvNoteOn(modState, notes.front());
        }
    }

    // TODO AYChipState inside of AYInterface
    Chip::AYInterface& chip_;
    Tuning::Tuner* tuner_;
    std::array<ChannelState, 4> prevStates_;
};

}  // namespace uZX

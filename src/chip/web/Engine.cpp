#include "Engine.h"

#include "../ChipAllocator.h"
#include "../NoteFrameClock.h"
#include "../PsgRegisterWriter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>

extern "C" {
#include "../../../third_party/ayumi/ayumi.h"
}

namespace MoTool::uZX {

namespace {

constexpr std::uint32_t MAX_AUDIO_SAMPLES = 128;
constexpr std::size_t MAX_TRACE_SNAPSHOTS = 32;
constexpr double CHIP_CLOCK = 1773400.0;
constexpr double OUTPUT_GAIN = 0.25;
constexpr std::array CHANNEL_PANS{0.15, 0.5, 0.85};
constexpr std::uint32_t MAX_FRAME_RATE = 1000;
constexpr float MAX_FRAMES = 1.0e6f;

struct Gate
{
    std::uint32_t id;
    std::uint32_t instrument;
    float pitch;
    float velocity;
    int slot;
    NoteSound sound;
    std::size_t age = 0;  ///< frames sounded so far
    std::optional<std::size_t> releasedAt{};  ///< the age at which the gate closed
};

bool isFinite(const NoteSound& sound) noexcept {
    std::array<float, sizeof(NoteSound) / sizeof(float)> fields;
    std::memcpy(fields.data(), &sound, sizeof(sound));
    return std::all_of(fields.begin(), fields.end(), [](float field) { return std::isfinite(field); });
}

bool isOn(float value) noexcept {
    return value >= 0.5f;
}

/** A time in frames. Anything that is not a positive time is 0. */
float duration(float frames) noexcept {
    return frames > 0.0f ? std::min(frames, MAX_FRAMES) : 0.0f;
}

/** How many steps of a table are in use. */
std::size_t stepsOf(float count, std::size_t capacity) noexcept {
    return static_cast<std::size_t>(std::clamp(count, 0.0f, static_cast<float>(capacity)));
}

/** A table's value at a frame: its last value holds. `steps` is not 0. */
template <std::size_t Capacity>
float tableAt(const std::array<float, Capacity>& table, std::size_t steps, float age) noexcept {
    return table[std::min(static_cast<std::size_t>(age), steps - 1)];
}

/** The level while the gate is open: the level table, or attack, decay and sustain. */
float heldLevel(const NoteSound& sound, float age) noexcept {
    if (const auto steps = stepsOf(sound.levelSteps, sound.levels.size()))
        return std::clamp(tableAt(sound.levels, steps, age), 0.0f, 1.0f);
    const auto attack = duration(sound.attack);
    const auto decay = duration(sound.decay);
    const auto sustain = std::clamp(sound.sustain, 0.0f, 1.0f);
    if (age < attack)
        return age / attack;
    if (age < attack + decay)
        return 1.0f - (1.0f - sustain) * (age - attack) / decay;
    return sustain;
}

float levelOf(const Gate& gate) noexcept {
    const auto age = static_cast<float>(gate.age);
    if (!gate.releasedAt)
        return heldLevel(gate.sound, age);
    const auto releasedAt = static_cast<float>(*gate.releasedAt);
    const auto release = duration(gate.sound.release);
    // Without a release the gate sounds only if it closed before its first frame, and then at the held level.
    const auto remaining = release > 0.0f ? std::max(0.0f, 1.0f - (age - releasedAt) / release) : 1.0f;
    return heldLevel(gate.sound, releasedAt) * remaining;
}

/** The first frame always sounds, so a gate shorter than a frame is heard. */
bool hasEnded(const Gate& gate) noexcept {
    if (gate.age == 0)
        return false;
    const auto age = static_cast<float>(gate.age);
    const auto& sound = gate.sound;
    if (gate.releasedAt)
        return age - static_cast<float>(*gate.releasedAt) >= duration(sound.release);
    // A note that has fallen to silence for good ends, and frees its channel.
    if (const auto steps = stepsOf(sound.levelSteps, sound.levels.size()))
        return sound.levels[steps - 1] <= 0.0f && gate.age >= steps - 1;
    return sound.sustain <= 0.0f && age >= duration(sound.attack) + duration(sound.decay);
}

/** Ornament, pitch envelope and vibrato, in semitones from the note's own pitch. */
float pitchOffsetOf(const Gate& gate) noexcept {
    const auto& sound = gate.sound;
    const auto age = static_cast<float>(gate.age);
    auto offset = 0.0f;

    if (const auto steps = stepsOf(sound.ornamentLength, sound.ornament.size())) {
        const auto step = std::max(1.0f, duration(sound.ornamentStep));
        offset += sound.ornament[static_cast<std::size_t>(age / step) % steps];
    }

    const auto attack = duration(sound.pitchAttack);
    const auto decay = duration(sound.pitchDecay);
    if (age < attack)
        offset += sound.pitchStart + (sound.pitchPeak - sound.pitchStart) * age / attack;
    else if (age < attack + decay)
        offset += sound.pitchPeak * (1.0f - (age - attack) / decay);

    const auto delay = duration(sound.vibratoDelay);
    if (sound.vibratoRate > 0.0f && age >= delay) {
        constexpr auto turn = 6.28318530717958647692f;
        offset += sound.vibratoDepth * std::sin(turn * sound.vibratoRate * (age - delay));
    }
    return std::isfinite(offset) ? offset : 0.0f;
}

/** The pitch on the MIDI note scale at which the chip's tone has this period. */
float pitchOfTonePeriod(float period) noexcept {
    const auto frequency = CHIP_CLOCK / 16.0 / std::clamp(static_cast<double>(period), 1.0, 4095.0);
    return static_cast<float>(69.0 + 12.0 * std::log2(frequency / 440.0));
}

/** The note's pitch before ornament, pitch envelope and vibrato: this frame of the tone table, or the note's own. */
float basePitchOf(const Gate& gate) noexcept {
    const auto& sound = gate.sound;
    const auto steps = stepsOf(sound.toneSteps, sound.tones.size());
    if (steps == 0)
        return gate.pitch;
    const auto tone = tableAt(sound.tones, steps, static_cast<float>(gate.age));
    return isOn(sound.tonesArePeriods) ? pitchOfTonePeriod(tone) : tone;
}

int periodFor(double pitch, PsgPeriodMode mode) noexcept {
    constexpr std::array divisors{16.0, 16.0 * 16, 16.0 * 32};
    const auto frequency = 440.0 * std::pow(2.0, (pitch - 69.0) / 12.0);
    const auto period = CHIP_CLOCK / divisors[static_cast<std::size_t>(mode)] / frequency;
    return mode == PsgPeriodMode::Tone
        ? static_cast<int>(std::clamp(std::round(period), 1.0, 4095.0))
        : static_cast<int>(std::clamp(std::round(period), 0.0, 65534.0));
}

/** Each gate retains its slot for its lifetime and the voices are built in note-on order. */
class Engine
{
public:
    bool init(std::uint32_t sampleRate, int isYm, std::uint32_t frameRate) noexcept {
        if (sampleRate < 8000 || sampleRate > 384000 || (isYm != 0 && isYm != 1)
            || frameRate < 1 || frameRate > MAX_FRAME_RATE)
            return false;
        sampleRate_ = sampleRate;
        isYm_ = isYm;
        frameRate_ = frameRate;
        reset();
        return true;
    }

    void reset() noexcept {
        gateCount_ = 0;
        occupied_.fill(false);
        allocator_ = {};
        allocator_.setChannelPans(CHANNEL_PANS);
        writer_.reset();
        clock_ = NoteFrameClock{sampleRate_, frameRate_};
        nextSound = {};
        sample_ = 0;
        traceCount = 0;
        droppedNoteCount = 0;
        left.fill(0.0f);
        right.fill(0.0f);
        if (sampleRate_ == 0)
            return;
        ayumi_configure(&chip_, isYm_, CHIP_CLOCK, static_cast<int>(sampleRate_));
        ayumi_set_noise(&chip_, 0);
        ayumi_set_envelope_shape(&chip_, 0);
        for (int channel = 0; channel < ChipAllocator::numToneChannels; ++channel) {
            ayumi_set_pan(&chip_, channel, CHANNEL_PANS[static_cast<std::size_t>(channel)], 1);
            ayumi_set_mixer(&chip_, channel, 1, 1, 0);
        }
    }

    bool noteOn(std::uint32_t id, std::uint32_t instrument, float pitch, float velocity) noexcept {
        const auto sound = std::exchange(nextSound, {});
        if (id == 0 || !isFinite(sound) || !std::isfinite(pitch) || pitch < 0.0f || pitch > 127.0f
            || !std::isfinite(velocity) || velocity < 0.0f || velocity > 1.0f
            || sampleRate_ == 0 || gateCount_ == gates_.size() || findGate(id) != gates_.data() + gateCount_) {
            ++droppedNoteCount;
            return false;
        }

        const auto freeSlot = std::find(occupied_.begin(), occupied_.end(), false);
        *freeSlot = true;
        const auto slot = static_cast<int>(freeSlot - occupied_.begin());
        gates_[gateCount_++] = {id, instrument, pitch, velocity, slot, sound};
        return true;
    }

    void noteOff(std::uint32_t id) noexcept {
        const auto gate = findGate(id);
        if (gate != gates_.data() + gateCount_ && !gate->releasedAt)
            gate->releasedAt = gate->age;
    }

    void render(std::uint32_t count) noexcept {
        count = std::min(count, MAX_AUDIO_SAMPLES);
        if (sampleRate_ == 0)
            return;

        std::uint32_t offset = 0;
        clock_.advance(count, [&](std::uint64_t tick) {
            const auto tickOffset = static_cast<std::uint32_t>(tick - sample_);
            renderAudio(offset, tickOffset);
            renderTick(tick);
            offset = tickOffset;
        });
        renderAudio(offset, count);
        sample_ += count;
    }

    NoteSound nextSound{};
    std::array<float, MAX_AUDIO_SAMPLES> left{};
    std::array<float, MAX_AUDIO_SAMPLES> right{};
    std::array<TraceSnapshot, MAX_TRACE_SNAPSHOTS> trace{};
    std::size_t traceCount = 0;
    std::uint32_t droppedNoteCount = 0;

private:
    Gate* findGate(std::uint32_t id) noexcept {
        return std::find_if(gates_.data(), gates_.data() + gateCount_, [id](const Gate& gate) { return gate.id == id; });
    }

    void renderAudio(std::uint32_t begin, std::uint32_t end) noexcept {
        for (auto sample = begin; sample < end; ++sample) {
            ayumi_process(&chip_);
            ayumi_remove_dc(&chip_);
            double l;
            double r;
            ayumi_get_stereo_output(&chip_, &l, &r);
            left[sample] = static_cast<float>(l * OUTPUT_GAIN);
            right[sample] = static_cast<float>(r * OUTPUT_GAIN);
        }
    }

    void renderTick(std::uint64_t tick) noexcept {
        const auto end = std::remove_if(gates_.data(), gates_.data() + gateCount_, [&](const Gate& gate) {
            if (hasEnded(gate)) {
                occupied_[static_cast<std::size_t>(gate.slot)] = false;
                return true;
            }
            return false;
        });
        gateCount_ = static_cast<std::size_t>(end - gates_.data());

        NoteFrame frame;
        TraceSnapshot snapshot{static_cast<std::uint32_t>(tick), static_cast<std::uint32_t>(tick >> 32)};
        for (std::size_t i = 0; i < gateCount_; ++i) {
            auto& gate = gates_[i];
            auto voice = makeVoice(gate);
            frame.add(voice);
            snapshot.voices[i] = {gate.id, gate.instrument, static_cast<float>(voice.pitch), voice.level};
            ++gate.age;
        }
        snapshot.count = static_cast<std::uint32_t>(gateCount_);

        const auto voices = allocator_.allocate(frame);
        const auto owner = [&](const NoteVoice* voice) {
            return voice == nullptr ? 0u : snapshot.voices[static_cast<std::size_t>(voice - frame.begin())].id;
        };
        for (std::size_t i = 0; i < voices.channels.size(); ++i)
            snapshot.channels[i] = owner(voices.channels[i]);
        snapshot.noise = owner(voices.noise);
        snapshot.envelope = owner(voices.envelope);

        const auto patch = renderPsg(voices, periodFor);
        snapshot.retrigger = patch.retriggerEnvelope;
        apply(writer_.write(patch));

        if (traceCount == trace.size()) {
            std::move(trace.begin() + 1, trace.end(), trace.begin());
            --traceCount;
            trace.front().gapBefore = 1;
        }
        trace[traceCount++] = snapshot;
    }

    static NoteVoice makeVoice(const Gate& gate) noexcept {
        const auto& sound = gate.sound;
        NoteVoice voice;
        voice.channel = gate.slot + 1;
        voice.key = static_cast<std::int32_t>(gate.id);
        voice.pitch = std::clamp(basePitchOf(gate) + pitchOffsetOf(gate), 0.0f, 127.0f);
        voice.level = gate.velocity * levelOf(gate);
        voice.attack = gate.age == 0;
        voice.priority = static_cast<int>(std::clamp(sound.priority, 0.0f, 127.0f));
        if (sound.pan >= 0.0f)
            voice.pan = std::min(sound.pan, 1.0f);

        const auto noiseHold = duration(sound.noiseHold);
        voice.timbre.tone = isOn(sound.tone);
        voice.timbre.noise = isOn(sound.noise) && (noiseHold <= 0.0f || static_cast<float>(gate.age) < noiseHold);
        voice.timbre.noisePeriod = static_cast<int>(std::clamp(sound.noisePeriod, 0.0f, 31.0f));
        voice.timbre.envelope = isOn(sound.envelope);
        voice.timbre.drivesEnvelope = isOn(sound.drivesEnvelope);
        voice.timbre.envelopeShape = static_cast<EnvelopeShapeEnum::Enum>(std::clamp(sound.envelopeShape, 0.0f, 7.0f));
        voice.restartEnvelope = voice.attack && voice.timbre.drivesEnvelope && isOn(sound.restartEnvelope);
        return voice;
    }

    void apply(const PsgRegisterFrame& registers) noexcept {
        for (int i = 0; i < ChipAllocator::numToneChannels; ++i) {
            const auto channel = static_cast<std::size_t>(i);
            if (registers.hasTonePeriodSet(channel))
                ayumi_set_tone(&chip_, i, registers.getTonePeriod(channel));
        }
        if (registers.hasNoisePeriodSet())
            ayumi_set_noise(&chip_, registers.getNoisePeriod());
        for (int i = 0; i < ChipAllocator::numToneChannels; ++i) {
            const auto channel = static_cast<std::size_t>(i);
            if (registers.hasMixerSet() || registers.hasVolumeOrEnvModSet(channel))
                ayumi_set_mixer(&chip_, i, !registers.getToneOn(channel), !registers.getNoiseOn(channel),
                                registers.getEnvMod(channel));
            if (registers.hasVolumeOrEnvModSet(channel))
                ayumi_set_volume(&chip_, i, registers.getVolume(channel));
        }
        if (registers.hasEnvelopePeriodSet())
            ayumi_set_envelope(&chip_, registers.getEnvelopePeriod());
        if (registers.hasEnvelopeShapeSet())
            ayumi_set_envelope_shape(&chip_, registers.getEnvelopeShape());
    }

    std::uint32_t sampleRate_ = 0;
    int isYm_ = 0;
    std::uint32_t frameRate_ = 50;
    std::uint64_t sample_ = 0;
    NoteFrameClock clock_{0};
    std::array<Gate, NoteFrame::maxVoices> gates_{};
    std::array<bool, NoteFrame::maxVoices> occupied_{};
    std::size_t gateCount_ = 0;
    ChipAllocator allocator_;
    PsgRegisterWriter writer_;
    ayumi chip_{};
};

Engine engine;

}  // namespace

}  // namespace MoTool::uZX

using MoTool::uZX::engine;

extern "C" {

int uzx_init(std::uint32_t sampleRate, int isYm, std::uint32_t frameRate) noexcept {
    return engine.init(sampleRate, isYm, frameRate);
}

void uzx_reset() noexcept {
    engine.reset();
}

MoTool::uZX::NoteSound* uzx_note_sound() noexcept {
    return &engine.nextSound;
}

std::uint32_t uzx_sound_length() noexcept {
    return static_cast<std::uint32_t>(sizeof(MoTool::uZX::NoteSound) / sizeof(float));
}

int uzx_note_on(std::uint32_t id, std::uint32_t instrument, float pitch, float velocity) noexcept {
    return engine.noteOn(id, instrument, pitch, velocity);
}

void uzx_note_off(std::uint32_t id) noexcept {
    engine.noteOff(id);
}

void uzx_render(std::uint32_t count) noexcept {
    engine.render(count);
}

const float* uzx_audio_left() noexcept {
    return engine.left.data();
}

const float* uzx_audio_right() noexcept {
    return engine.right.data();
}

const MoTool::uZX::TraceSnapshot* uzx_trace_data() noexcept {
    return engine.trace.data();
}

std::uint32_t uzx_trace_count() noexcept {
    return static_cast<std::uint32_t>(engine.traceCount);
}

void uzx_trace_clear() noexcept {
    engine.traceCount = 0;
}

std::uint32_t uzx_dropped_note_count() noexcept {
    return engine.droppedNoteCount;
}

}  // extern "C"

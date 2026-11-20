#include "Engine.h"

#include "../ChipAllocator.h"
#include "../NoteFrameClock.h"
#include "../PsgRegisterWriter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

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
constexpr std::array PLUCK_LEVELS{1.0f, 0.74f, 0.54f, 0.38f, 0.26f, 0.16f, 0.09f};

enum class Instrument : std::uint32_t
{
    Bass,
    Pluck,
};

struct Gate
{
    std::uint32_t id;
    Instrument instrument;
    float pitch;
    float velocity;
    int slot;
    std::size_t age = 0;
    bool released = false;
};

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
    bool init(std::uint32_t sampleRate, int isYm) noexcept {
        if (sampleRate < 8000 || sampleRate > 384000 || (isYm != 0 && isYm != 1))
            return false;
        sampleRate_ = sampleRate;
        isYm_ = isYm;
        reset();
        return true;
    }

    void reset() noexcept {
        gateCount_ = 0;
        occupied_.fill(false);
        allocator_ = {};
        allocator_.setChannelPans(CHANNEL_PANS);
        writer_.reset();
        clock_ = NoteFrameClock{sampleRate_};
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
        if (id == 0 || instrument > static_cast<std::uint32_t>(Instrument::Pluck)
            || !std::isfinite(pitch) || pitch < 0.0f || pitch > 127.0f
            || !std::isfinite(velocity) || velocity < 0.0f || velocity > 1.0f
            || sampleRate_ == 0 || gateCount_ == gates_.size() || findGate(id) != gates_.data() + gateCount_) {
            ++droppedNoteCount;
            return false;
        }

        const auto freeSlot = std::find(occupied_.begin(), occupied_.end(), false);
        *freeSlot = true;
        const auto slot = static_cast<int>(freeSlot - occupied_.begin());
        gates_[gateCount_++] = {id, static_cast<Instrument>(instrument), pitch, velocity, slot};
        return true;
    }

    void noteOff(std::uint32_t id) noexcept {
        const auto gate = findGate(id);
        if (gate != gates_.data() + gateCount_)
            gate->released = true;
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
            // A gate that started and ended between the same two ticks still gets its first frame.
            if ((gate.released && gate.age > 0)
                || (gate.instrument == Instrument::Pluck && gate.age >= PLUCK_LEVELS.size())) {
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
            snapshot.voices[i] = {gate.id, static_cast<std::uint32_t>(gate.instrument), gate.pitch, voice.level};
            if (gate.age < PLUCK_LEVELS.size())
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
        NoteVoice voice;
        voice.channel = gate.slot + 1;
        voice.key = static_cast<std::int32_t>(gate.id);
        voice.pitch = gate.pitch;
        voice.level = gate.velocity;
        voice.attack = gate.age == 0;
        if (gate.instrument == Instrument::Bass) {
            voice.priority = 2;
        } else {
            voice.priority = 1;
            voice.level *= PLUCK_LEVELS[gate.age];
            voice.timbre.noise = gate.age < 2;
            voice.timbre.noisePeriod = 5;
            // Use the envelope just on the initial noise transient. The following decay uses the voice level.
            voice.timbre.envelope = voice.attack && gate.velocity >= 0.5f;
            voice.timbre.drivesEnvelope = voice.timbre.envelope;
            voice.timbre.envelopeShape = EnvelopeShapeEnum::Decay;
            voice.restartEnvelope = voice.timbre.drivesEnvelope;
        }
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

int uzx_init(std::uint32_t sampleRate, int isYm) noexcept {
    return engine.init(sampleRate, isYm);
}

void uzx_reset() noexcept {
    engine.reset();
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

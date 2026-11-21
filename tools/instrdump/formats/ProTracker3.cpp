#include "../Formats.h"
#include "../Tracks.h"

#include "formats/chiptune/aym/protracker3.h"
#include "module/players/aym/vortex.h"

namespace MoTool::InstrDump {

namespace {

namespace PT3 = Formats::Chiptune::ProTracker3;

/** As Module::Vortex's player plays them (module/players/aym/vortex.cpp, SynthesizeChannel). */
Sample toSample(unsigned slot, const PT3::Sample& source) {
    Sample sample;
    sample.slot = slot;
    for (const auto& line : source.Lines) {
        SampleStep step;
        step.vol = static_cast<int>(line.Level);
        step.volSlide = line.VolumeSlideAddon;
        step.tone = !line.ToneMask;
        step.toneOfs = line.ToneOffset;
        step.toneAcc = line.KeepToneOffset;
        step.noise = !line.NoiseMask;
        step.env = !line.EnvMask;
        // one field: the noise offset while the noise sounds, else the envelope's
        if (step.noise) {
            step.noiseOfs = line.NoiseOrEnvelopeOffset;
            step.noiseAcc = line.KeepNoiseOrEnvelopeOffset;
        } else {
            step.envOfs = line.NoiseOrEnvelopeOffset;
            step.envAcc = line.KeepNoiseOrEnvelopeOffset;
        }
        sample.steps.push_back(step);
    }
    // A loop at the size is a line that does not exist, which the player reads as a silent one for good.
    if (source.GetLoop() < sample.steps.size()) {
        sample.loop = source.GetLoop();
    } else {
        sample.stops = true;
    }
    return sample;
}

Ornament toOrnament(unsigned slot, const PT3::Ornament& source) {
    Ornament ornament;
    ornament.slot = slot;
    for (const auto offset : source.Lines) {
        ornament.steps.push_back({.noteOfs = offset});
    }
    // A loop at the size is a line that does not exist, which the player reads as 0 for good.
    ornament.loop = source.GetLoop();
    if (ornament.loop >= ornament.steps.size()) {
        ornament.loop = static_cast<unsigned>(ornament.steps.size());
        ornament.steps.push_back({});
    }
    return ornament;
}

class Builder : public PT3::Builder {
public:
    explicit Builder(Module& module) : module_(module), meta_(module) {}

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override { return meta_; }
    void SetVersion(uint_t version) override { version_ = version; }
    void SetNoteTable(PT3::NoteTable table) override {
        module_.noteTable = ::Module::Vortex::GetFreqTable(static_cast<::Module::Vortex::NoteTable>(table), version_);
    }
    void SetMode(uint_t mode) override { turboPatternsOffset_ = mode; }
    void SetInitialTempo(uint_t tempo) override { tracks_.setInitialTempo(tempo); }
    void SetSample(uint_t index, PT3::Sample sample) override { module_.samples.push_back(toSample(index, sample)); }
    void SetOrnament(uint_t index, PT3::Ornament ornament) override {
        module_.ornaments.push_back(toOrnament(index, ornament));
    }
    void SetPositions(PT3::Positions positions) override {
        for (const auto pattern : positions.Lines) {
            order_.push_back({.pattern = pattern});
        }
    }

    Formats::Chiptune::PatternBuilder& StartPattern(uint_t index) override { return tracks_.startPattern(index); }
    void StartChannel(uint_t index) override { tracks_.startChannel(index); }
    void SetRest() override { tracks_.rest(); }
    void SetNote(uint_t note) override { tracks_.note(note); }
    void SetSample(uint_t sample) override { tracks_.sample(sample); }
    void SetOrnament(uint_t ornament) override { tracks_.ornament(ornament); }
    void SetVolume(uint_t) override {}
    void SetGlissade(uint_t, int_t) override {}
    void SetNoteGliss(uint_t, int_t, uint_t) override {}
    void SetSampleOffset(uint_t) override {}
    void SetOrnamentOffset(uint_t) override {}
    void SetVibrate(uint_t, uint_t) override {}
    void SetEnvelopeSlide(uint_t, int_t) override {}
    void SetEnvelope(uint_t type, uint_t) override { tracks_.envelope(static_cast<int>(type)); }
    void SetNoEnvelope() override { tracks_.envelope(-1); }
    void SetNoiseBase(uint_t) override {}

    void finish() {
        tracks_.play(order_, PT3::DEFAULT_SAMPLE, PT3::DEFAULT_ORNAMENT, module_.usage);
        // A Turbo Sound PT3 holds the second chip's patterns in the same module, mirrored by number.
        if (turboPatternsOffset_) {
            auto mirrored = order_;
            for (auto& position : mirrored) {
                position.pattern = turboPatternsOffset_ - 1 - position.pattern;
            }
            tracks_.play(mirrored, PT3::DEFAULT_SAMPLE, PT3::DEFAULT_ORNAMENT, module_.usage);
        }
    }

private:
    Module& module_;
    Meta meta_;
    Tracks tracks_;
    std::vector<Tracks::Position> order_;
    unsigned version_ = 6;
    unsigned turboPatternsOffset_ = 0;
};

bool read(PT3::Parser parse, const Binary::Container& data, Module& module) {
    Builder builder(module);
    if (!parse(data, builder)) {
        return false;
    }
    builder.finish();
    return true;
}

}  // namespace

bool readProTracker3(const Binary::Container& data, Module& module) {
    return read(&PT3::Parse, data, module);
}

bool readVortexTracker2(const Binary::Container& data, Module& module) {
    return read(&PT3::VortexTracker2::Parse, data, module);
}

}  // namespace MoTool::InstrDump

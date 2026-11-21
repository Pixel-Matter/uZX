#include "../Formats.h"
#include "../Tracks.h"

#include "formats/chiptune/aym/soundtrackerpro.h"
#include "module/players/aym/freq_tables.h"

namespace MoTool::InstrDump {

namespace {

namespace STP = Formats::Chiptune::SoundTrackerPro;

/** As Module::SoundTrackerPro's player plays them (module/players/aym/soundtrackerpro.cpp, SynthesizeChannel). */
Sample toSample(unsigned slot, const STP::Sample& source) {
    Sample sample;
    sample.slot = slot;
    auto lines = source.Lines;
    if (lines.empty()) {
        lines.emplace_back();  // the player keeps playing a default line of an empty sample
        sample.loop = 0;
    } else if (source.GetLoop() < lines.size()) {
        sample.loop = source.GetLoop();
    } else {
        sample.stops = true;  // a loop at or past the end switches the channel off
    }
    for (const auto& line : lines) {
        SampleStep step;
        step.vol = static_cast<int>(line.Level);  // the player also subtracts the pattern's volume
        step.tone = !line.ToneMask;
        step.toneOfs = line.Vibrato;
        step.noise = !line.NoiseMask;
        step.noiseOfs = static_cast<int>(line.Noise);
        step.noiseAbs = true;
        step.env = line.EnvelopeMask;  // set, it lets the channel's envelope drive the volume
        sample.steps.push_back(step);
    }
    return sample;
}

Ornament toOrnament(unsigned slot, const STP::Ornament& source) {
    Ornament ornament;
    ornament.slot = slot;
    for (const auto offset : source.Lines) {
        ornament.steps.push_back({.noteOfs = offset});
    }
    ornament.loop = source.GetLoop();
    if (ornament.loop >= ornament.steps.size()) {
        // the player then keeps the position past the end, which reads as no offset
        ornament.loop = static_cast<unsigned>(ornament.steps.size());
        ornament.steps.push_back({});
    }
    return ornament;
}

class Builder : public STP::Builder {
public:
    explicit Builder(Module& module) : module_(module), meta_(module) {
        module_.noteTable = std::string(::Module::AYM::TABLE_SOUNDTRACKER_PRO);
    }

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override { return meta_; }
    void SetInitialTempo(uint_t tempo) override { tracks_.setInitialTempo(tempo); }
    void SetSample(uint_t index, STP::Sample sample) override { module_.samples.push_back(toSample(index, sample)); }
    void SetOrnament(uint_t index, STP::Ornament ornament) override {
        module_.ornaments.push_back(toOrnament(index, ornament));
    }
    void SetPositions(STP::Positions positions) override {
        for (const auto& position : positions.Lines) {
            order_.push_back({.pattern = position.PatternIndex, .transposition = position.Transposition});
        }
    }

    Formats::Chiptune::PatternBuilder& StartPattern(uint_t index) override { return tracks_.startPattern(index); }
    void StartChannel(uint_t index) override { tracks_.startChannel(index); }
    void SetRest() override { tracks_.rest(); }
    void SetNote(uint_t note) override { tracks_.note(note); }
    void SetSample(uint_t sample) override { tracks_.sample(sample); }
    void SetOrnament(uint_t ornament) override { tracks_.ornament(ornament); }
    // The player ignores the ornament while the envelope is on, and the parser pairs every envelope with ornament 0.
    void SetEnvelope(uint_t type, uint_t period) override {
        tracks_.envelopePeriod(period);
        // type 0 keeps the shape that was set before
        if (type) {
            tracks_.envelope(static_cast<int>(type));
        } else {
            tracks_.envelopeOn();
        }
    }
    void SetNoEnvelope() override { tracks_.envelope(-1); }
    void SetGliss(uint_t) override {}
    void SetVolume(uint_t) override {}

    void finish() { tracks_.play(order_, STP::DEFAULT_SAMPLE, STP::DEFAULT_ORNAMENT, module_.usage); }

private:
    Module& module_;
    Meta meta_;
    Tracks tracks_;
    std::vector<Tracks::Position> order_;
};

}  // namespace

bool readSoundTrackerPro(const Binary::Container& data, Module& module) {
    Builder builder(module);
    if (!STP::ParseCompiled(data, builder)) {
        return false;
    }
    builder.finish();
    return true;
}

}  // namespace MoTool::InstrDump

#include "../Formats.h"
#include "../Tracks.h"

#include "formats/chiptune/aym/protracker2.h"
#include "module/players/aym/freq_tables.h"

namespace MoTool::InstrDump {

namespace {

namespace PT2 = Formats::Chiptune::ProTracker2;

/** As Module::ProTracker2's player plays them (module/players/aym/protracker2.cpp, SynthesizeChannel). */
Sample toSample(unsigned slot, const PT2::Sample& source) {
    Sample sample;
    sample.slot = slot;
    for (const auto& line : source.Lines) {
        SampleStep step;
        step.vol = static_cast<int>(line.Level);  // at the pattern's full volume, which is the initial one
        step.tone = !line.ToneMask;
        step.toneOfs = line.Vibrato;  // added to the period for this frame only
        step.noise = !line.NoiseMask;
        if (step.noise) {
            step.noiseOfs = static_cast<int>(line.Noise);  // the pattern's noise addon is added to it
        }
        step.env = true;  // the envelope is the channel's: the player enables it on every frame
        sample.steps.push_back(step);
    }
    // Past its last line a sample goes on from its loop; a loop equal to the size is a line that
    // does not exist, which the player reads as a silent one for good.
    if (source.GetLoop() < sample.steps.size()) {
        sample.loop = source.GetLoop();
    } else {
        sample.stops = true;
    }
    return sample;
}

Ornament toOrnament(unsigned slot, const PT2::Ornament& source) {
    Ornament ornament;
    ornament.slot = slot;
    for (const auto offset : source.Lines) {
        ornament.steps.push_back({.noteOfs = offset});
    }
    // A loop equal to the size is a line that does not exist, which the player reads as 0 for good.
    ornament.loop = source.GetLoop();
    if (ornament.loop >= ornament.steps.size()) {
        ornament.loop = static_cast<unsigned>(ornament.steps.size());
        ornament.steps.push_back({});
    }
    return ornament;
}

class Builder : public PT2::Builder {
public:
    explicit Builder(Module& module) : module_(module), meta_(module) {
        module_.noteTable = std::string(::Module::AYM::TABLE_PROTRACKER2);
    }

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override { return meta_; }
    void SetInitialTempo(uint_t tempo) override { tracks_.setInitialTempo(tempo); }
    void SetSample(uint_t index, PT2::Sample sample) override { module_.samples.push_back(toSample(index, sample)); }
    void SetOrnament(uint_t index, PT2::Ornament ornament) override {
        module_.ornaments.push_back(toOrnament(index, ornament));
    }
    void SetPositions(PT2::Positions positions) override {
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
    void SetVolume(uint_t volume) override { tracks_.volume(volume); }
    void SetGlissade(int_t) override {}
    void SetNoteGliss(int_t, uint_t) override {}
    void SetNoGliss() override {}
    void SetEnvelope(uint_t type, uint_t period) override {
        tracks_.envelope(static_cast<int>(type));
        tracks_.envelopePeriod(period);
    }
    void SetNoEnvelope() override { tracks_.envelope(-1); }
    void SetNoiseAddon(int_t value) override { tracks_.noise(value); }

    void finish() { tracks_.play(order_, PT2::DEFAULT_SAMPLE, PT2::DEFAULT_ORNAMENT, module_.usage); }

private:
    Module& module_;
    Meta meta_;
    Tracks tracks_;
    std::vector<Tracks::Position> order_;
};

}  // namespace

bool readProTracker2(const Binary::Container& data, Module& module) {
    Builder builder(module);
    if (!PT2::Parse(data, builder)) {
        return false;
    }
    builder.finish();
    return true;
}

}  // namespace MoTool::InstrDump

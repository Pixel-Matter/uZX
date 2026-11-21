#include "../Formats.h"
#include "../Tracks.h"

#include "formats/chiptune/aym/protracker1.h"
#include "module/players/aym/freq_tables.h"

#include <algorithm>

namespace MoTool::InstrDump {

namespace {

namespace PT1 = Formats::Chiptune::ProTracker1;

// The player starts every channel on sample 0 and ornament 0.
constexpr unsigned defaultSample = 0;
constexpr unsigned defaultOrnament = 0;

/** As Module::ProTracker1's player plays them (module/players/aym/protracker1.cpp, SynthesizeChannel). */
Sample toSample(unsigned slot, const PT1::Sample& source) {
    Sample sample;
    sample.slot = slot;
    for (const auto& line : source.Lines) {
        SampleStep step;
        step.vol = static_cast<int>(line.Level);  // at the pattern's full volume, which is the initial one
        step.tone = !line.ToneMask;
        step.toneOfs = line.Vibrato;  // added to the period for this frame only
        step.noise = !line.NoiseMask;
        if (step.noise) {
            step.noiseOfs = static_cast<int>(line.Noise);  // the noise period itself: there is no noise base
            step.noiseAbs = true;
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

/** The player reads an ornament by the position in the sample, not by a position of its own, and
    an ornament has no loop: it is 0 past its last line. Here it runs by its own frames from the
    note, so it differs once the sample loops.
*/
Ornament toOrnament(unsigned slot, const PT1::Ornament& source) {
    Ornament ornament;
    ornament.slot = slot;
    auto end = source.Lines.end();
    while (end != source.Lines.begin() && end[-1] == 0) {
        --end;
    }
    for (auto line = source.Lines.begin(); line != end; ++line) {
        ornament.steps.push_back({.noteOfs = *line});
    }
    ornament.loop = static_cast<unsigned>(ornament.steps.size());
    ornament.steps.push_back({});
    return ornament;
}

class Builder : public PT1::Builder {
public:
    explicit Builder(Module& module) : module_(module), meta_(module) {
        module_.noteTable = std::string(::Module::AYM::TABLE_PROTRACKER3_ST);
    }

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override { return meta_; }
    void SetInitialTempo(uint_t tempo) override { tracks_.setInitialTempo(tempo); }
    void SetSample(uint_t index, PT1::Sample sample) override { module_.samples.push_back(toSample(index, sample)); }
    void SetOrnament(uint_t index, PT1::Ornament ornament) override {
        module_.ornaments.push_back(toOrnament(index, ornament));
    }
    void SetPositions(PT1::Positions positions) override {
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
    void SetEnvelope(uint_t type, uint_t period) override {
        tracks_.envelope(static_cast<int>(type));
        tracks_.envelopePeriod(period);
    }
    void SetNoEnvelope() override { tracks_.envelope(-1); }

    void finish() { tracks_.play(order_, defaultSample, defaultOrnament, module_.usage); }

private:
    Module& module_;
    Meta meta_;
    Tracks tracks_;
    std::vector<Tracks::Position> order_;
};

}  // namespace

bool readProTracker1(const Binary::Container& data, Module& module) {
    Builder builder(module);
    if (!PT1::Parse(data, builder)) {
        return false;
    }
    builder.finish();
    return true;
}

}  // namespace MoTool::InstrDump

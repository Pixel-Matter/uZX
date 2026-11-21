#include "formats/chiptune/aym/globaltracker.h"

#include "../Formats.h"
#include "../Tracks.h"
#include "module/players/aym/freq_tables.h"

#include <algorithm>

namespace MoTool::InstrDump {

namespace {

namespace GT = Formats::Chiptune::GlobalTracker;

/** As Module::GlobalTracker's player plays them (module/players/aym/globaltracker.cpp, SynthesizeChannel). */
SampleStep toStep(const GT::SampleLine& line) {
    SampleStep step;
    step.vol = static_cast<int>(line.Level & 15);  // the level register takes the low nibble of the 8-bit level
    step.tone = !line.ToneMask;
    step.toneOfs = static_cast<int16_t>(line.Vibrato);  // added to the period modulo 4096
    step.noise = !line.NoiseMask;
    step.noiseOfs = static_cast<int>(line.Noise);  // the noise period, OR-ed over the channels
    step.noiseAbs = true;
    step.env = line.EnvelopeMask;  // unlike the other masks, a set bit lets the envelope sound
    return step;
}

Sample toSample(unsigned slot, const GT::Sample& source) {
    Sample sample;
    sample.slot = slot;
    for (const auto& line : source.Lines) {
        sample.steps.push_back(toStep(line));
    }
    sample.loop = source.GetLoop();
    // Past the last line the player reads a default line: silent level, but with the envelope bit set.
    if (sample.loop >= sample.steps.size()) {
        sample.loop = static_cast<unsigned>(sample.steps.size());
        sample.steps.push_back(toStep({}));
    }
    return sample;
}

Ornament toOrnament(unsigned slot, const GT::Ornament& source) {
    Ornament ornament;
    ornament.slot = slot;
    for (const auto offset : source.Lines) {
        ornament.steps.push_back({.noteOfs = offset});
    }
    ornament.loop = source.GetLoop();
    // Past the last line the player reads a zero offset.
    if (ornament.loop >= ornament.steps.size()) {
        ornament.loop = static_cast<unsigned>(ornament.steps.size());
        ornament.steps.push_back({});
    }
    return ornament;
}

class Builder : public GT::Builder
{
public:
    explicit Builder(Module& module) : module_(module), meta_(module) {
        module_.noteTable = std::string(::Module::AYM::TABLE_PROTRACKER3_ST);
    }

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override { return meta_; }
    void SetInitialTempo(uint_t tempo) override { tracks_.setInitialTempo(tempo); }
    void SetSample(uint_t index, GT::Sample sample) override { module_.samples.push_back(toSample(index, sample)); }
    void SetOrnament(uint_t index, GT::Ornament ornament) override {
        module_.ornaments.push_back(toOrnament(index, ornament));
    }
    void SetPositions(GT::Positions positions) override {
        for (const auto pattern : positions.Lines) {
            order_.push_back({.pattern = pattern});
        }
    }

    Formats::Chiptune::PatternBuilder& StartPattern(uint_t index) override { return tracks_.startPattern(index); }
    void StartChannel(uint_t index) override { tracks_.startChannel(index); }
    void SetRest() override { tracks_.rest(); }
    void SetNote(uint_t note) override { tracks_.note(note); }
    void SetSample(uint_t sample) override { tracks_.sample(sample); }
    // Newer versions turn the envelope off with an ornament: the parser reports that as SetNoEnvelope.
    void SetOrnament(uint_t ornament) override { tracks_.ornament(ornament); }
    void SetEnvelope(uint_t type, uint_t period) override {
        tracks_.envelope(static_cast<int>(type));
        tracks_.envelopePeriod(period);
    }
    void SetNoEnvelope() override { tracks_.envelope(-1); }
    // the pattern's volume is what the player takes off every level
    void SetVolume(uint_t attenuation) override { tracks_.volume(15 - std::min<uint_t>(attenuation, 15)); }

    void finish() {
        tracks_.play(order_, 0, 0, module_.usage);
    }

private:
    Module& module_;
    Meta meta_;
    Tracks tracks_;
    std::vector<Tracks::Position> order_;
};

}  // namespace

bool readGlobalTracker(const Binary::Container& data, Module& module) {
    Builder builder(module);
    if (!GT::Parse(data, builder)) {
        return false;
    }
    builder.finish();
    return true;
}

}  // namespace MoTool::InstrDump

#include "formats/chiptune/aym/fasttracker.h"

#include <algorithm>

#include "../Formats.h"
#include "../Tracks.h"
#include "module/players/aym/freq_tables.h"

namespace MoTool::InstrDump {

namespace {

namespace FT = Formats::Chiptune::FastTracker;

/** As Module::FastTracker's player plays them (module/players/aym/fasttracker.cpp, SynthesizeChannel). */
Sample toSample(unsigned slot, const FT::Sample& source) {
    Sample sample;
    sample.slot = slot;
    for (const auto& line : source.Lines) {
        SampleStep step;
        step.vol = static_cast<int>(line.Level);
        step.volSlide = line.VolSlide;
        step.tone = !line.ToneMask;
        step.toneOfs = static_cast<int>(line.Tone);
        step.toneAcc = line.AccumulateTone;
        step.noise = !line.NoiseMask;
        step.noiseOfs = static_cast<int>(line.Noise);  // added to the pattern's noise base and the ornament's
        step.noiseAcc = line.AccumulateNoise;
        step.env = line.EnableEnvelope;
        step.envOfs = -line.EnvelopeAddon;  // the player subtracts it from the envelope period
        step.envAcc = line.AccumulateEnvelope;
        sample.steps.push_back(step);
    }
    // a loop at the end of the sample leaves no line to play: the channel is silenced
    sample.stops = source.GetLoop() >= source.Lines.size();
    sample.loop = sample.stops ? 0 : source.GetLoop();
    return sample;
}

Ornament toOrnament(unsigned slot, const FT::Ornament& source) {
    Ornament ornament;
    ornament.slot = slot;
    for (const auto& line : source.Lines) {
        ornament.steps.push_back({.noteOfs = line.NoteAddon,
                                  .noteAcc = line.KeepNoteAddon,
                                  .noiseOfs = line.NoiseAddon,
                                  .noiseAcc = line.KeepNoiseAddon});
    }
    // the player dereferences no line when the loop is at the end (undefined behaviour): replay the last one
    ornament.loop = std::min<unsigned>(source.GetLoop(), ornament.steps.empty() ? 0 : ornament.steps.size() - 1);
    return ornament;
}

class Builder : public FT::Builder
{
public:
    explicit Builder(Module& module) : module_(module), meta_(module) {}

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override { return meta_; }
    void SetNoteTable(FT::NoteTable table) override {
        switch (table) {
            case FT::NoteTable::SOUNDTRACKER:
                module_.noteTable = std::string(::Module::AYM::TABLE_SOUNDTRACKER);
                break;
            case FT::NoteTable::FASTTRACKER:
                module_.noteTable = std::string(::Module::AYM::TABLE_FASTTRACKER);
                break;
            default:
                module_.noteTable = std::string(::Module::AYM::TABLE_PROTRACKER2);
                break;
        }
    }
    void SetInitialTempo(uint_t tempo) override { tracks_.setInitialTempo(tempo); }
    void SetSample(uint_t index, FT::Sample sample) override { module_.samples.push_back(toSample(index, sample)); }
    void SetOrnament(uint_t index, FT::Ornament ornament) override {
        module_.ornaments.push_back(toOrnament(index, ornament));
    }
    void SetPositions(FT::Positions positions) override {
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
    void SetVolume(uint_t) override {}
    void SetEnvelope(uint_t type, uint_t) override { tracks_.envelope(static_cast<int>(type)); }
    void SetNoEnvelope() override { tracks_.envelope(-1); }
    void SetNoise(uint_t) override {}
    void SetRetrig() override {}
    void SetSlide(int_t) override {}
    void SetNoteSlide(uint_t) override {}

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

bool readFastTracker(const Binary::Container& data, Module& module) {
    Builder builder(module);
    if (!FT::Parse(data, builder)) {
        return false;
    }
    builder.finish();
    return true;
}

}  // namespace MoTool::InstrDump

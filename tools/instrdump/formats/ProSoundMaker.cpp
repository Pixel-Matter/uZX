#include "../Formats.h"
#include "../Tracks.h"

#include "formats/chiptune/aym/prosoundmaker.h"
#include "module/players/aym/freq_tables.h"

#include <optional>
#include <utility>
#include <vector>

namespace MoTool::InstrDump {

namespace {

namespace PSM = Formats::Chiptune::ProSoundMaker;

constexpr unsigned defaultSample = 0;    // what the player starts every channel on
constexpr unsigned defaultOrnament = 0;
constexpr unsigned noOrnament = 32;      // a number no ornament has: the player adds nothing to the note

SampleStep toSampleStep(const PSM::SampleLine& line) {
    SampleStep step;
    step.vol = static_cast<int>(line.Level);
    step.tone = !line.ToneMask;
    // the player adds Gliss to the slide before the frame, and keeps the slide until the next note
    step.toneOfs = line.Gliss;
    step.toneAcc = true;
    step.noise = !line.NoiseMask;
    step.noiseOfs = static_cast<int>(line.Noise);
    step.noiseAbs = true;
    // With the envelope on, the player sets the envelope bit only while the line's level plus the pattern
    // volume reach 15, and else plays that level one louder: the model has no such rule.
    step.env = true;
    return step;
}

/** As Module::ProSoundMaker's player plays it (module/players/aym/prosoundmaker.cpp, SynthesizeChannel). */
Sample toSample(unsigned slot, const PSM::Sample& source) {
    Sample sample;
    sample.slot = slot;
    for (const auto& line : source.Lines) {
        sample.steps.push_back(toSampleStep(line));
    }
    const auto size = static_cast<unsigned>(source.Lines.size());
    const auto loop = source.GetLoop();
    const auto volumeDelta = static_cast<int>(source.VolumeDeltaValue);
    if (loop >= size) {
        sample.stops = true;
    } else if (volumeDelta == 0) {
        sample.loop = loop;
    } else {
        // The volume changes by the delta after the first pass and then after every VolumeDeltaPeriod-th loop,
        // before the step the loop restarts on: unrolled to the loop played that many times.
        sample.loop = size;
        for (unsigned pass = 0; pass != source.VolumeDeltaPeriod; ++pass) {
            for (unsigned pos = loop; pos != size; ++pos) {
                sample.steps.push_back(toSampleStep(source.Lines[pos]));
            }
        }
        sample.steps[size].volSlide = volumeDelta;
    }
    return sample;
}

/** After its last line an ornament without a loop adds nothing more. */
Ornament toOrnament(unsigned slot, const PSM::Ornament& source) {
    Ornament ornament;
    ornament.slot = slot;
    for (const auto offset : source.Lines) {
        ornament.steps.push_back({.noteOfs = offset});
    }
    const auto size = static_cast<unsigned>(source.Lines.size());
    if (source.GetLoop() < size) {
        ornament.loop = source.GetLoop();
    } else if (size != 0 && source.Lines.back() == 0) {
        ornament.loop = size - 1;
    } else {
        ornament.loop = size;
        ornament.steps.push_back({});
    }
    return ornament;
}

/** What a channel's row holds, kept until the row ends to apply it in the player's order (GetNewChannelState),
    which is not the order of the module's bytes.
*/
struct Cell {
    enum class Command { Envelope, NoOrnament, EnvelopeReinit };

    std::optional<bool> enabled;
    std::optional<unsigned> note;
    std::optional<unsigned> sample;
    std::optional<unsigned> ornament;
    std::vector<std::pair<Command, int>> commands;
};

class Builder : public PSM::Builder, public Formats::Chiptune::PatternBuilder {
public:
    explicit Builder(Module& module) : module_(module), meta_(module) {
        module_.noteTable = std::string(::Module::AYM::TABLE_PROSOUNDMAKER);
    }

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override { return meta_; }
    void SetSample(uint_t index, PSM::Sample sample) override { module_.samples.push_back(toSample(index, sample)); }
    void SetOrnament(uint_t index, PSM::Ornament ornament) override {
        module_.ornaments.push_back(toOrnament(index, ornament));
    }
    void SetPositions(PSM::Positions positions) override {
        for (const auto& position : positions.Lines) {
            order_.push_back({.pattern = position.PatternIndex, .transposition = position.Transposition});
        }
    }

    Formats::Chiptune::PatternBuilder& StartPattern(uint_t index) override {
        flush();
        tracks_.startPattern(index);
        return *this;
    }
    void Finish(uint_t size) override {
        flush();
        tracks_.Finish(size);
    }
    void StartLine(uint_t index) override {
        flush();
        tracks_.StartLine(index);
    }
    void SetTempo(uint_t tempo) override { tracks_.SetTempo(tempo); }

    void StartChannel(uint_t index) override {
        flush();
        tracks_.startChannel(index);
        cell_.emplace();
    }
    void SetRest() override { cell().enabled = false; }
    void SetNote(uint_t note) override {
        cell().enabled = true;
        cell().note = note;
    }
    void SetSample(uint_t sample) override { cell().sample = sample; }
    void SetOrnament(uint_t ornament) override { cell().ornament = ornament; }
    void SetVolume(uint_t) override {}
    void DisableOrnament() override { cell().commands.emplace_back(Cell::Command::NoOrnament, 0); }
    void SetEnvelopeReinit(bool) override { cell().commands.emplace_back(Cell::Command::EnvelopeReinit, 0); }
    void SetEnvelopeTone(uint_t type, uint_t) override {
        cell().commands.emplace_back(Cell::Command::Envelope, static_cast<int>(type));
    }
    void SetEnvelopeNote(uint_t type, uint_t) override {
        cell().commands.emplace_back(Cell::Command::Envelope, static_cast<int>(type));
    }

    void finish() {
        flush();
        tracks_.play(order_, defaultSample, defaultOrnament, module_.usage);
    }

private:
    Cell& cell() {
        if (!cell_) {
            cell_.emplace();
        }
        return *cell_;
    }

    /** The couplings of the player: setting an ornament turns the envelope off, and while the envelope is on
        the ornament adds nothing, nor does it again once the envelope is off, as only an ornament command does that.
    */
    void flush() {
        if (!cell_) {
            return;
        }
        const auto cell = std::move(*cell_);
        cell_.reset();
        if (cell.sample) {
            tracks_.sample(*cell.sample);
        }
        if (cell.ornament) {
            tracks_.ornament(*cell.ornament);
            tracks_.envelope(-1);
        }
        for (const auto& [command, value] : cell.commands) {
            switch (command) {
                case Cell::Command::Envelope:
                    tracks_.envelope(value);
                    tracks_.ornament(noOrnament);
                    break;
                case Cell::Command::NoOrnament:
                    tracks_.ornament(noOrnament);
                    tracks_.envelope(-1);
                    break;
                case Cell::Command::EnvelopeReinit:
                    // the player swaps the ornament for an empty one
                    tracks_.ornament(noOrnament);
                    break;
            }
        }
        if (cell.note) {
            tracks_.note(*cell.note);
        }
        if (cell.enabled == false) {
            tracks_.rest();
        }
    }

    Module& module_;
    Meta meta_;
    Tracks tracks_;
    std::vector<Tracks::Position> order_;
    std::optional<Cell> cell_;
};

}  // namespace

bool readProSoundMaker(const Binary::Container& data, Module& module) {
    Builder builder(module);
    if (!PSM::ParseCompiled(data, builder)) {
        return false;
    }
    builder.finish();
    return true;
}

}  // namespace MoTool::InstrDump

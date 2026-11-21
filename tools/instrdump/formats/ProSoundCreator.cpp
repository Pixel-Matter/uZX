#include "../Formats.h"
#include "../Tracks.h"

#include "formats/chiptune/aym/prosoundcreator.h"
#include "module/players/aym/freq_tables.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace MoTool::InstrDump {

namespace {

namespace PSC = Formats::Chiptune::ProSoundCreator;

/** As Module::ProSoundCreator's player plays it (module/players/aym/prosoundcreator.cpp, SynthesizeChannel). */
SampleStep toStep(const PSC::SampleLine& line) {
    SampleStep step;
    step.vol = static_cast<int>(line.Level);
    // inexact: the player adds the slide to the channel's volume, which scales the sample's level, not to the level
    step.volSlide = line.VolumeDelta;
    step.tone = !line.ToneMask;
    // the player keeps the tone offset in 16 bits
    step.toneOfs = static_cast<int16_t>(line.Tone);
    step.toneAcc = true;
    step.noise = !line.NoiseMask;
    step.env = line.EnableEnvelope;
    // inexact: on a channel whose envelope is off, the player adds this line's adding to the noise instead
    if (line.EnableEnvelope && line.NoiseMask) {
        step.envOfs = line.Adding;
        step.envAcc = true;
    } else {
        step.noiseOfs = line.Adding;
        step.noiseAcc = true;
    }
    return step;
}

OrnamentStep toStep(const PSC::OrnamentLine& line) {
    return {.noteOfs = line.NoteAddon, .noteAcc = true, .noiseOfs = line.NoiseAddon, .noiseAcc = true};
}

/** The player remembers the last loop-begin line it played and goes back to it from a loop-end line; past the
    last line the object has ended. Steps are taken as the player walks the lines, up to a state it was in before.
*/
template <class Step, class Object>
Sequence<Step> toSequence(unsigned slot, const Object& source) {
    Sequence<Step> sequence;
    sequence.slot = slot;
    std::map<std::pair<unsigned, unsigned>, unsigned> seen;
    unsigned position = 0;
    unsigned loopPosition = 0;
    while (position < source.GetSize()) {
        const auto [found, isNew] = seen.try_emplace({position, loopPosition}, sequence.steps.size());
        if (!isNew) {
            sequence.loop = found->second;
            return sequence;
        }
        const auto& line = source.GetLine(position);
        sequence.steps.push_back(toStep(line));
        if (line.LoopBegin) {
            loopPosition = position;
        }
        position = line.LoopEnd ? loopPosition : position + 1;
    }
    sequence.stops = true;
    return sequence;
}

Sample toSample(unsigned slot, const PSC::Sample& source) {
    return toSequence<SampleStep>(slot, source);
}

Ornament toOrnament(unsigned slot, const PSC::Ornament& source) {
    auto ornament = toSequence<OrnamentStep>(slot, source);
    // an ornament that ends leaves the note and the noise where it took them, while the sample goes on
    if (ornament.stops) {
        ornament.stops = false;
        ornament.loop = static_cast<unsigned>(ornament.steps.size());
        ornament.steps.push_back({.noteOfs = 0, .noteAcc = true, .noiseOfs = 0, .noiseAcc = true});
    }
    return ornament;
}

/** What a pattern's cell sets on its channel, in any order the module stores it. */
struct Cell {
    unsigned line = 0;
    int channel = 0;
    std::optional<unsigned> note;
    bool rest = false;
    std::optional<unsigned> sample;
    std::optional<unsigned> ornament;
    bool noOrnament = false;
    std::optional<bool> envelope;
    std::optional<int> envelopeShape;
    std::optional<unsigned> tempo;
};

struct Pattern {
    unsigned size = 0;
    std::vector<Cell> cells;
};

class Patterns : public Formats::Chiptune::PatternBuilder {
public:
    Formats::Chiptune::PatternBuilder& start(unsigned index) {
        pattern_ = &patterns_[index];
        *pattern_ = {};
        line_ = 0;
        return *this;
    }

    void startChannel(unsigned index) {
        if (pattern_) {
            add(static_cast<int>(index));
        }
    }

    Cell& cell() {
        if (!pattern_) {
            return stub_;
        }
        if (pattern_->cells.empty() || pattern_->cells.back().line != line_) {
            add(-1);
        }
        return pattern_->cells.back();
    }

    void Finish(uint_t size) override {
        if (pattern_) {
            pattern_->size = size;
        }
    }
    void StartLine(uint_t index) override { line_ = index; }
    void SetTempo(uint_t tempo) override { cell().tempo = tempo; }

    const Pattern* find(unsigned index) const {
        const auto found = patterns_.find(index);
        return found == patterns_.end() ? nullptr : &found->second;
    }

private:
    void add(int channel) {
        Cell cell;
        cell.line = line_;
        cell.channel = channel;
        pattern_->cells.push_back(cell);
    }

    std::map<unsigned, Pattern> patterns_;
    Pattern* pattern_ = nullptr;
    unsigned line_ = 0;
    Cell stub_;
};

class Builder : public PSC::Builder {
public:
    explicit Builder(Module& module) : module_(module), meta_(module) {
        module_.noteTable = std::string(::Module::AYM::TABLE_ASM);
    }

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override { return meta_; }
    void SetInitialTempo(uint_t tempo) override { tracks_.setInitialTempo(tempo); }
    void SetSample(uint_t index, PSC::Sample sample) override { module_.samples.push_back(toSample(index, sample)); }
    void SetOrnament(uint_t index, PSC::Ornament ornament) override {
        module_.ornaments.push_back(toOrnament(index, ornament));
    }
    void SetPositions(PSC::Positions positions) override { order_ = std::move(positions.Lines); }

    Formats::Chiptune::PatternBuilder& StartPattern(uint_t index) override { return patterns_.start(index); }
    void StartChannel(uint_t index) override { patterns_.startChannel(index); }
    void SetRest() override {
        patterns_.cell().rest = true;
        patterns_.cell().note.reset();
    }
    void SetNote(uint_t note) override {
        patterns_.cell().note = note;
        patterns_.cell().rest = false;
    }
    void SetSample(uint_t sample) override { patterns_.cell().sample = sample; }
    void SetOrnament(uint_t ornament) override { patterns_.cell().ornament = ornament; }
    void SetVolume(uint_t) override {}
    // the player takes an envelope command without a shape or a period as the one that turns the envelope on
    void SetEnvelope(uint_t type, uint_t tone) override {
        if (type || tone) {
            patterns_.cell().envelopeShape = static_cast<int>(type);
        } else {
            patterns_.cell().envelope = true;
        }
    }
    void SetEnvelope() override { patterns_.cell().envelope = true; }
    void SetNoEnvelope() override { patterns_.cell().envelope = false; }
    void SetNoiseBase(uint_t) override {}
    void SetBreakSample() override {}
    void SetBreakOrnament() override {}
    void SetNoOrnament() override { patterns_.cell().noOrnament = true; }
    void SetGliss(uint_t) override {}
    void SetSlide(int_t) override {}
    void SetVolumeSlide(uint_t, int_t) override {}

    /** The channels' envelopes and ornaments depend on the cells played before, so the positions are replayed in
        order as the player does, each into a pattern of its own.
    */
    void finish() {
        std::vector<Tracks::Position> played;
        for (unsigned position = 0; position != order_.size(); ++position) {
            if (const auto* pattern = patterns_.find(order_[position])) {
                replay(position, *pattern);
                played.push_back({.pattern = position});
            }
        }
        tracks_.play(played, 0, 0, module_.usage);
    }

private:
    struct Channel {
        bool envelope = false;
        unsigned ornament = 0;
        bool noOrnament = false;
    };

    void replay(unsigned index, const Pattern& pattern) {
        tracks_.startPattern(index);
        for (const auto& cell : pattern.cells) {
            tracks_.StartLine(cell.line);
            if (cell.tempo) {
                tracks_.SetTempo(*cell.tempo);
            }
            if (cell.channel < 0 || cell.channel >= Tracks::channels) {
                continue;
            }
            auto& channel = channels_[static_cast<size_t>(cell.channel)];
            tracks_.startChannel(static_cast<unsigned>(cell.channel));
            // a note restarts the ornament, then the cell's sample and ornament take over what plays, then its
            // commands apply
            if (cell.note) {
                channel.noOrnament = false;
                tracks_.note(*cell.note);
            } else if (cell.rest) {
                tracks_.rest();
            }
            if (cell.sample) {
                tracks_.sample(*cell.sample);
            }
            if (cell.ornament) {
                channel.ornament = *cell.ornament;
            }
            // inexact: the player turns the ornament off until the next note; the dump plays ornament 0 instead
            if (cell.noOrnament) {
                channel.noOrnament = true;
            }
            if (cell.note || cell.ornament || cell.noOrnament) {
                tracks_.ornament(channel.noOrnament ? 0 : channel.ornament);
            }
            // the envelope's shape is the chip's: a channel that has it on plays the new one
            if (cell.envelopeShape) {
                envelopeShape_ = *cell.envelopeShape;
                for (int other = 0; other != Tracks::channels; ++other) {
                    if (channels_[static_cast<size_t>(other)].envelope) {
                        tracks_.startChannel(static_cast<unsigned>(other));
                        tracks_.envelope(envelopeShape_);
                        break;
                    }
                }
                tracks_.startChannel(static_cast<unsigned>(cell.channel));
            }
            if (cell.envelope) {
                channel.envelope = *cell.envelope;
                tracks_.envelope(channel.envelope ? envelopeShape_ : -1);
            }
        }
        tracks_.Finish(pattern.size);
    }

    Module& module_;
    Meta meta_;
    Tracks tracks_;
    Patterns patterns_;
    std::vector<unsigned> order_;
    std::array<Channel, Tracks::channels> channels_;
    int envelopeShape_ = 0;
};

}  // namespace

bool readProSoundCreator(const Binary::Container& data, Module& module) {
    Builder builder(module);
    if (!PSC::Parse(data, builder)) {
        return false;
    }
    builder.finish();
    return true;
}

}  // namespace MoTool::InstrDump

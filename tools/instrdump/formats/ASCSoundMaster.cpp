#include "../Formats.h"
#include "../Tracks.h"

#include "formats/chiptune/aym/ascsoundmaster.h"
#include "module/players/aym/freq_tables.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <vector>

namespace MoTool::InstrDump {

namespace {

namespace ASC = Formats::Chiptune::ASCSoundMaster;

/** As Module::ASCSoundMaster's player plays it (module/players/aym/ascsoundmaster.cpp, SynthesizeChannel). */
SampleStep toStep(const ASC::SampleLine& line) {
    SampleStep step;
    step.vol = static_cast<int>(line.Level);
    step.volSlide = line.VolSlide;
    step.tone = !line.ToneMask;
    step.toneOfs = line.ToneDeviation;
    step.toneAcc = true;
    step.noise = !line.NoiseMask;
    step.env = line.EnableEnvelope;
    // the adding moves the envelope period on a line that masks the noise and uses the envelope, else the noise
    if (line.NoiseMask && line.EnableEnvelope) {
        step.envOfs = line.Adding;
        step.envAcc = true;
    } else {
        step.noiseOfs = line.Adding;
        step.noiseAcc = true;
    }
    return step;
}

OrnamentStep toStep(const ASC::OrnamentLine& line) {
    return {.noteOfs = line.NoteAddon, .noteAcc = true, .noiseOfs = line.NoiseAddon, .noiseAcc = true};
}

/** The player plays the lines up to the loop limit, then goes back to the loop, even when the loop lies beyond
    the limit: that line then repeats on its own. The lines past the limit sound only after a break-sample command.
*/
template <class Step, class Object>
Sequence<Step> toSequence(unsigned slot, const Object& source) {
    Sequence<Step> sequence;
    sequence.slot = slot;
    const auto limit = source.GetLoopLimit();
    for (unsigned position = 0; position <= limit; ++position) {
        sequence.steps.push_back(toStep(source.GetLine(position)));
    }
    if (source.GetLoop() <= limit) {
        sequence.loop = source.GetLoop();
    } else {
        sequence.steps.push_back(toStep(source.GetLine(source.GetLoop())));
        sequence.loop = limit + 1;
    }
    return sequence;
}

/** What a pattern's cell sets on its channel, in any order the module stores it. */
struct Cell {
    unsigned line = 0;
    int channel = 0;
    std::optional<unsigned> note;
    bool rest = false;
    std::optional<unsigned> sample;
    std::optional<unsigned> ornament;
    std::optional<bool> envelope;
    std::optional<int> envelopeShape;
    bool continueSample = false;
    bool continueOrnament = false;
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
    void StartLine(uint_t index) override {
        line_ = index;
        // the parser does not finish a pattern that fills all its lines: it ends at its last one, as in the player
        if (pattern_) {
            pattern_->size = std::max(pattern_->size, index + 1);
        }
    }
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

class Builder : public ASC::Builder {
public:
    explicit Builder(Module& module) : module_(module), meta_(module) {
        module_.noteTable = std::string(::Module::AYM::TABLE_ASM);
    }

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override { return meta_; }
    void SetInitialTempo(uint_t tempo) override { tracks_.setInitialTempo(tempo); }
    void SetSample(uint_t index, ASC::Sample sample) override {
        module_.samples.push_back(toSequence<SampleStep>(index, sample));
    }
    void SetOrnament(uint_t index, ASC::Ornament ornament) override {
        module_.ornaments.push_back(toSequence<OrnamentStep>(index, ornament));
    }
    void SetPositions(ASC::Positions positions) override { order_ = std::move(positions.Lines); }

    Formats::Chiptune::PatternBuilder& StartPattern(uint_t index) override { return patterns_.start(index); }
    void StartChannel(uint_t index) override { patterns_.startChannel(index); }
    void SetRest() override { patterns_.cell().rest = true; }
    void SetNote(uint_t note) override { patterns_.cell().note = note; }
    void SetSample(uint_t sample) override { patterns_.cell().sample = sample; }
    void SetOrnament(uint_t ornament) override { patterns_.cell().ornament = ornament; }
    void SetVolume(uint_t) override {}
    void SetEnvelopeType(uint_t type) override { patterns_.cell().envelopeShape = static_cast<int>(type); }
    void SetEnvelopeTone(uint_t) override {}
    void SetEnvelope() override { patterns_.cell().envelope = true; }
    void SetNoEnvelope() override { patterns_.cell().envelope = false; }
    void SetNoise(uint_t) override {}
    void SetContinueSample() override { patterns_.cell().continueSample = true; }
    void SetContinueOrnament() override { patterns_.cell().continueOrnament = true; }
    void SetGlissade(int_t) override {}
    void SetSlide(int_t) override {}
    void SetNoteSlide(int_t, bool) override {}
    void SetVolumeSlide(uint_t, int_t) override {}
    void SetBreakSample() override {}

    /** The channels' envelopes and their continued samples depend on the cells played before, so the positions
        are replayed in order as the player does, each into a pattern of its own.
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
        unsigned sample = 0;
        unsigned ornament = 0;
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
            if (cell.sample) {
                channel.sample = *cell.sample;
            }
            if (cell.ornament) {
                channel.ornament = *cell.ornament;
            }
            // a sample or ornament chosen in the pattern starts with the next note that does not continue the
            // current one
            if (cell.note) {
                if (!cell.continueSample) {
                    tracks_.sample(channel.sample);
                }
                if (!cell.continueOrnament) {
                    tracks_.ornament(channel.ornament);
                }
                tracks_.note(*cell.note);
            } else if (cell.rest) {
                tracks_.rest();
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

bool read(ASC::Parser parse, const Binary::Container& data, Module& module) {
    Builder builder(module);
    if (!parse(data, builder)) {
        return false;
    }
    builder.finish();
    return true;
}

}  // namespace

bool readASCSoundMaster0(const Binary::Container& data, Module& module) {
    return read(&ASC::Ver0::Parse, data, module);
}

bool readASCSoundMaster1(const Binary::Container& data, Module& module) {
    return read(&ASC::Ver1::Parse, data, module);
}

}  // namespace MoTool::InstrDump

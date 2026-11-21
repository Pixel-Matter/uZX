#include "../Formats.h"
#include "../Tracks.h"

#include "formats/chiptune/aym/sqtracker.h"
#include "module/players/aym/freq_tables.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace MoTool::InstrDump {

namespace {

namespace SQT = Formats::Chiptune::SQTracker;

constexpr unsigned lineCount = 32;  // of every sample and ornament
constexpr int lastNote = 95;        // of the note table

/** Where the player (module/players/aym/sqtracker.cpp, SynthesizeChannel) goes after the 32 lines of a sample
    or an ornament: Loop..LoopLimit-1 over and over, or silence when Loop is 32.
*/
struct LoopRange {
    unsigned loop = lineCount;
    unsigned limit = lineCount;

    auto operator<=>(const LoopRange&) const = default;
};

template <class Line>
LoopRange loopRange(const Formats::Chiptune::LinesObjectWithLoopLimit<Line>& source) {
    return {source.GetLoop(), source.GetLoopLimit()};
}

template <class Step, class Line, class ToStep>
Sequence<Step> toSequence(unsigned slot, const std::vector<Line>& lines, LoopRange range, ToStep toStep) {
    const auto line = [&](unsigned pos) { return pos < lines.size() ? lines[pos] : Line{}; };
    Sequence<Step> sequence;
    sequence.slot = slot;
    for (unsigned pos = 0; pos != lineCount; ++pos) {
        sequence.steps.push_back(toStep(line(pos)));
    }
    if (range.loop >= lineCount) {
        sequence.stops = true;
    } else if (range.limit == lineCount) {
        sequence.loop = range.loop;
    } else if (range.limit > range.loop) {
        // the loop range ends before the last line: unrolled once
        sequence.loop = lineCount;
        for (unsigned pos = range.loop; pos != range.limit; ++pos) {
            sequence.steps.push_back(toStep(line(pos)));
        }
    } else {
        // an empty loop range never ends: the player plays Loop..31 once more, then empty lines for good
        for (unsigned pos = range.loop; pos != lineCount; ++pos) {
            sequence.steps.push_back(toStep(line(pos)));
        }
        sequence.loop = static_cast<unsigned>(sequence.steps.size());
        sequence.steps.push_back(toStep(Line{}));
    }
    return sequence;
}

SampleStep toSampleStep(const SQT::SampleLine& line) {
    SampleStep step;
    step.vol = static_cast<int>(line.Level);
    step.tone = line.EnableTone;
    step.toneOfs = line.ToneDeviation;
    step.noise = line.EnableNoise;
    step.noiseOfs = static_cast<int>(line.Noise);
    step.noiseAbs = true;
    // the envelope sounds only on lines of level 0
    step.env = line.Level == 0;
    return step;
}

OrnamentStep toOrnamentStep(int_t offset) {
    return {.noteOfs = offset};
}

/** What one row of a pattern holds for its channel, as zxtune's player reads it. */
struct Cell {
    std::optional<bool> enabled;
    std::optional<unsigned> note;
    std::optional<unsigned> sample;
    std::optional<unsigned> ornament;
    std::optional<int> envelope;
    std::optional<unsigned> envelopePeriod;
    unsigned tempo = 0;
    unsigned tempoAddon = 0;
};

struct Pattern {
    unsigned size = 0;
    std::map<unsigned, Cell> cells;
};

/** A position names a pattern per channel, each with its own transposition: every position gets a pattern of
    its own in Tracks, into which the three channels' rows are played as the player plays them.
*/
class Builder : public SQT::Builder, public Formats::Chiptune::PatternBuilder {
public:
    explicit Builder(Module& module) : module_(module), meta_(module) {
        module_.noteTable = std::string(::Module::AYM::TABLE_SQTRACKER);
    }

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override { return meta_; }
    void SetSample(uint_t index, SQT::Sample sample) override {
        module_.samples.push_back(toSequence<SampleStep>(index, sample.Lines, loopRange(sample), &toSampleStep));
        sampleLoops_[index] = loopRange(sample);
    }
    void SetOrnament(uint_t index, SQT::Ornament ornament) override {
        ornaments_[index] = {std::move(ornament.Lines), loopRange(ornament)};
    }
    void SetPositions(SQT::Positions positions) override { positions_ = std::move(positions.Lines); }

    Formats::Chiptune::PatternBuilder& StartPattern(uint_t index) override {
        pattern_ = &patterns_[index];
        *pattern_ = {};
        line_ = 0;
        return *this;
    }
    void Finish(uint_t size) override { pattern_->size = size; }
    void StartLine(uint_t index) override { line_ = index; }
    void SetTempo(uint_t tempo) override { cell().tempo = tempo; }

    void SetTempoAddon(uint_t add) override { cell().tempoAddon += add; }
    void SetRest() override { cell().enabled = false; }
    void SetNote(uint_t note) override { cell().note = note; }
    void SetSample(uint_t sample) override {
        cell().enabled = true;
        cell().sample = sample;
    }
    void SetOrnament(uint_t ornament) override { cell().ornament = ornament; }
    void SetEnvelope(uint_t type, uint_t period) override {
        cell().envelope = static_cast<int>(type);
        cell().envelopePeriod = period;
    }
    void SetGlissade(int_t) override {}
    void SetAttenuation(uint_t) override {}
    void SetAttenuationAddon(int_t) override {}
    void SetGlobalAttenuation(uint_t) override {}
    void SetGlobalAttenuationAddon(int_t) override {}

    void finish() {
        std::vector<Tracks::Position> order;
        for (unsigned index = 0; index != positions_.size(); ++index) {
            playPosition(index, positions_[index]);
            order.push_back({.pattern = index});
        }
        // Channels start with no sample: nothing sounds before one is set. Ornament 0 is none.
        tracks_.play(order, 0, 0, module_.usage);
        for (const auto& [slot, ornament] : ornaments_) {
            module_.ornaments.push_back(toOrnament(slot, ornament.first, ornament.second));
        }
    }

private:
    struct Channel {
        bool active = false;  // the player has a sample on it
        unsigned note = 0;
        int transposition = 0;
        std::optional<unsigned> framesLeft;  // until a sample that stops does, when it does
    };

    Cell& cell() { return pattern_->cells[line_]; }

    /** Without a loop of its own (Loop 32) an ornament goes on over the loop range of the sample it plays with,
        which the sample setting it restarts with it: that range when all of its samples share one.
    */
    Ornament toOrnament(unsigned slot, const std::vector<int_t>& lines, LoopRange range) const {
        if (range.loop < lineCount) {
            return toSequence<OrnamentStep>(slot, lines, range, &toOrnamentStep);
        }
        std::set<LoopRange> sampleRanges;
        for (const auto& [key, count] : module_.usage) {
            const auto sample = sampleLoops_.find(key.sample);
            if (key.ornament == slot && sample != sampleLoops_.end() && sample->second.loop < lineCount) {
                sampleRanges.insert(sample->second);
            }
        }
        if (sampleRanges.size() == 1) {
            return toSequence<OrnamentStep>(slot, lines, *sampleRanges.begin(), &toOrnamentStep);
        }
        // played only with samples that stop, it stops with them
        auto ornament = toSequence<OrnamentStep>(slot, lines, range, &toOrnamentStep);
        if (!sampleRanges.empty()) {
            // samples of different loop ranges: the model has one loop per ornament, it holds its last line
            ornament.stops = false;
            ornament.loop = lineCount - 1;
        }
        return ornament;
    }

    const Cell* findCell(unsigned pattern, unsigned line) const {
        const auto found = patterns_.find(pattern);
        if (found == patterns_.end()) {
            return nullptr;
        }
        const auto cell = found->second.cells.find(line);
        return cell == found->second.cells.end() ? nullptr : &cell->second;
    }

    unsigned transposed(const Channel& channel) const {
        return static_cast<unsigned>(std::clamp(static_cast<int>(channel.note) + channel.transposition, 0, lastNote));
    }

    /** As ModuleData::ConvertPattern and DataRenderer::GetNewChannelState do, channel C first. */
    void playPosition(unsigned index, const SQT::PositionEntry& position) {
        tracks_.startPattern(index);
        const auto found = patterns_.find(position.Channels[2].Pattern);
        const unsigned size = found == patterns_.end() ? 0 : found->second.size;
        for (unsigned chan = 0; chan != Tracks::channels; ++chan) {
            channels_[chan].transposition = position.Channels[chan].Transposition;
        }
        unsigned tempo = position.Tempo;
        std::optional<unsigned> lastTempo;
        for (unsigned line = 0; line != size; ++line) {
            tracks_.StartLine(line);
            for (unsigned chan = Tracks::channels; chan-- != 0;) {
                auto& channel = channels_[chan];
                tracks_.startChannel(chan);
                if (channel.active && channel.framesLeft == 0u) {
                    channel.active = false;
                    tracks_.rest();
                }
                const auto& attributes = position.Channels[chan];
                if (const auto* cell = findCell(attributes.Pattern, line)) {
                    if (attributes.EnabledEffects && cell->tempo) {
                        tempo = cell->tempo;
                    }
                    playCell(*cell, channel);
                    if (attributes.EnabledEffects && cell->tempoAddon) {
                        tempo = (tempo + cell->tempoAddon) & 31;
                    }
                }
            }
            const unsigned frames = tempo ? tempo : 32;
            if (lastTempo != frames) {
                tracks_.SetTempo(frames);
                lastTempo = frames;
            }
            for (auto& channel : channels_) {
                if (channel.framesLeft) {
                    channel.framesLeft = *channel.framesLeft - std::min(*channel.framesLeft, frames);
                }
            }
        }
        tracks_.Finish(size);
    }

    void playCell(const Cell& cell, Channel& channel) {
        if (cell.enabled == false) {
            channel.active = false;
            tracks_.rest();
        }
        if (cell.note) {
            channel.note = *cell.note;
        }
        // the envelope command's period is the chip's, set with its shape
        if (cell.envelopePeriod) {
            tracks_.envelopePeriod(*cell.envelopePeriod);
        }
        if (cell.sample) {
            // a sample restarts the channel: no ornament, no envelope, unless the row sets them
            channel.active = true;
            const auto range = sampleLoops_.find(*cell.sample);
            const bool stops = range != sampleLoops_.end() && range->second.loop >= lineCount;
            channel.framesLeft = stops ? std::optional(lineCount) : std::nullopt;
            tracks_.note(transposed(channel));
            tracks_.sample(*cell.sample);
            tracks_.ornament(cell.ornament.value_or(0));
            tracks_.envelope(cell.envelope.value_or(-1));
            return;
        }
        // a note without a sample changes the pitch of the sample playing, and does not start a silent channel
        if (cell.note && channel.active) {
            tracks_.note(transposed(channel));
        }
        if (cell.ornament) {
            tracks_.ornament(*cell.ornament);
        }
        if (cell.envelope) {
            tracks_.envelope(*cell.envelope);
        }
    }

    Module& module_;
    Meta meta_;
    Tracks tracks_;
    std::vector<SQT::PositionEntry> positions_;
    std::map<unsigned, Pattern> patterns_;
    std::map<unsigned, LoopRange> sampleLoops_;
    std::map<unsigned, std::pair<std::vector<int_t>, LoopRange>> ornaments_;
    Pattern* pattern_ = nullptr;
    unsigned line_ = 0;
    std::array<Channel, Tracks::channels> channels_;
};

}  // namespace

bool readSQTracker(const Binary::Container& data, Module& module) {
    Builder builder(module);
    if (!SQT::ParseCompiled(data, builder)) {
        return false;
    }
    builder.finish();
    return true;
}

}  // namespace MoTool::InstrDump

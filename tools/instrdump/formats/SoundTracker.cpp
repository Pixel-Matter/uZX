#include "../Formats.h"
#include "../Tracks.h"

#include "formats/chiptune/aym/soundtracker.h"
#include "module/players/aym/freq_tables.h"

#include <algorithm>

namespace MoTool::InstrDump {

namespace {

namespace ST = Formats::Chiptune::SoundTracker;

constexpr unsigned sampleSize = 32;

/** The player's position in a sample (module/players/aym/soundtracker.cpp, StateCursor). */
struct Cursor {
    int countDown = static_cast<int>(sampleSize);  // frames left, -1 when the channel is silent
    unsigned position = 0;                         // the line after the one being played

    auto operator<=>(const Cursor&) const = default;

    void next(const ST::Sample& sample) {
        --countDown;
        position = (position + 1) & 0x1f;
        if (countDown == 0) {
            if (const auto loop = sample.GetLoop()) {
                position = loop & 0x1f;
                countDown = static_cast<int>(sample.GetLoopLimit() - loop + 1);
            } else {
                countDown = -1;
            }
        }
    }
};

/** The player counts a note's 32 frames down and takes a repeat from the sample's loop and limit,
    which can go round several times before it ends or repeats for good: the steps are the lines in
    the order the frames play them, up to where that sequence repeats. The 32nd frame does not play
    line 31: it plays the line before the loop position, or silences the channel.
*/
Sample toSample(unsigned slot, const ST::Sample& source) {
    Sample sample;
    sample.slot = slot;
    std::vector<Cursor> seen;
    Cursor cursor;
    for (;;) {
        cursor.next(source);
        if (cursor.countDown < 0) {
            sample.stops = true;
            break;
        }
        const auto repeat = std::find(seen.begin(), seen.end(), cursor);
        if (repeat != seen.end()) {
            sample.loop = static_cast<unsigned>(repeat - seen.begin());
            break;
        }
        seen.push_back(cursor);

        const auto& line = source.GetLine((cursor.position - 1) & 0x1f);
        SampleStep step;
        step.vol = static_cast<int>(line.Level);
        // the field is named after the envelope, but it is the tone's mask
        step.tone = !line.EnvelopeMask;
        step.toneOfs = line.Effect;  // the player adds it to the tone period
        step.noise = !line.NoiseMask;
        step.noiseOfs = static_cast<int>(line.Noise);
        step.noiseAbs = true;
        step.env = true;  // a channel's envelope covers the whole sample
        sample.steps.push_back(step);
    }
    return sample;
}

/** The player reads an ornament at the sample's line, not at a position of its own: the model's
    steps follow the frames, which is the same until the sample repeats a part of itself.
*/
Ornament toOrnament(unsigned slot, const ST::Ornament& source) {
    Ornament ornament;
    ornament.slot = slot;
    for (unsigned line = 0; line != sampleSize; ++line) {
        ornament.steps.push_back({.noteOfs = source.GetLine(line)});
    }
    return ornament;
}

class Builder : public ST::Builder {
public:
    explicit Builder(Module& module) : module_(module), meta_(module) {
        module_.noteTable = std::string(::Module::AYM::TABLE_SOUNDTRACKER);
    }

    Formats::Chiptune::MetaBuilder& GetMetaBuilder() override { return meta_; }
    void SetInitialTempo(uint_t tempo) override { tracks_.setInitialTempo(tempo); }
    void SetSample(uint_t index, ST::Sample sample) override { module_.samples.push_back(toSample(index, sample)); }
    void SetOrnament(uint_t index, ST::Ornament ornament) override {
        module_.ornaments.push_back(toOrnament(index, ornament));
    }
    void SetPositions(ST::Positions positions) override {
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
    // The parsers pair these with the ornament commands the format ties to them.
    void SetEnvelope(uint_t type, uint_t period) override {
        tracks_.envelope(static_cast<int>(type));
        tracks_.envelopePeriod(period);
    }
    void SetNoEnvelope() override { tracks_.envelope(-1); }

    void finish() { tracks_.play(order_, 0, 0, module_.usage); }

private:
    Module& module_;
    Meta meta_;
    Tracks tracks_;
    std::vector<Tracks::Position> order_;
};

bool read(ST::Parser parse, const Binary::Container& data, Module& module) {
    Builder builder(module);
    if (!parse(data, builder)) {
        return false;
    }
    builder.finish();
    return true;
}

}  // namespace

bool readSoundTracker(const Binary::Container& data, Module& module) {
    return read(&ST::Ver1::Parse, data, module);
}

bool readSoundTrackerCompiled(const Binary::Container& data, Module& module) {
    return read(&ST::Ver1::ParseCompiled, data, module);
}

bool readSoundTracker3(const Binary::Container& data, Module& module) {
    return read(&ST::Ver3::Parse, data, module);
}

}  // namespace MoTool::InstrDump

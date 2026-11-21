#pragma once

#include "Model.h"

#include "formats/chiptune/common/builder_meta.h"
#include "formats/chiptune/common/builder_pattern.h"

#include <map>
#include <vector>

namespace MoTool::InstrDump {

//==============================================================================
/** The patterns of a module as a format's parser reports them, played afterwards in the order of
    its positions to count what each channel sounds. A format's adapter forwards its Builder calls
    here; the couplings a tracker has between them (an ornament that turns the envelope off, ...)
    are the adapter's to apply, as that format's player does.
*/
class Tracks : public Formats::Chiptune::PatternBuilder {
public:
    static constexpr int channels = 3;

    struct Position {
        unsigned pattern = 0;
        int transposition = 0;  // semitones added to every note of the pattern
    };

    void setInitialTempo(unsigned tempo);

    Formats::Chiptune::PatternBuilder& startPattern(unsigned index);
    void startChannel(unsigned index);
    void note(unsigned note);
    void rest();
    void sample(unsigned slot);
    void ornament(unsigned slot);
    void envelope(int shape);  // turns the channel's envelope on with this shape, which is the chip's; -1 turns it off
    void envelopeOn();         // turns it on with the shape the chip has
    void envelopePeriod(unsigned period);  // the chip's envelope period from here on, whichever channel sets it
    void noise(int base);      // what the patterns add to the noise period of the channel's sample from here on
    void volume(unsigned level);  // the channel's volume from here on, 15 is full, which a channel starts at

    void Finish(uint_t size) override;
    void StartLine(uint_t index) override;
    void SetTempo(uint_t tempo) override;

    /** Plays the positions once, each channel starting on the default sample and ornament with
        the envelope off, and adds what sounds to `usage`. A position whose pattern is missing
        is skipped.
    */
    void play(const std::vector<Position>& order, unsigned defaultSample, unsigned defaultOrnament, Usage& usage) const;

private:
    enum class Kind { Note, Rest, Sample, Ornament, Envelope, EnvelopeOn, EnvelopePeriod, Noise, Volume, Tempo };

    struct Event {
        unsigned line = 0;
        int channel = 0;
        Kind kind = Kind::Note;
        int value = 0;
    };

    struct Pattern {
        unsigned size = 0;
        std::vector<Event> events;  // in line order
    };

    void add(Kind kind, int value);

    std::map<unsigned, Pattern> patterns_;
    Pattern* pattern_ = nullptr;
    unsigned line_ = 0;
    int channel_ = 0;
    unsigned initialTempo_ = 1;
};

//==============================================================================
/** Takes the texts a parser reports into a Module, as UTF-8. */
class Meta : public Formats::Chiptune::MetaBuilder {
public:
    explicit Meta(Module& module) : module_(module) {}

    void SetProgram(StringView program) override;
    void SetTitle(StringView title) override;
    void SetAuthor(StringView author) override;
    void SetStrings(const Strings::Array&) override {}
    void SetComment(StringView) override {}

private:
    Module& module_;
};

}  // namespace MoTool::InstrDump

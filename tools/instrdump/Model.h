#pragma once

#include <compare>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace MoTool::InstrDump {

//==============================================================================
/** What a sample does to its channel in one frame, in terms every AY tracker's sample fits.

    An offset with its `Acc` flag clear is added to the base for this frame only. With the flag set
    it is also kept: the frame plays `accumulator + offset`, and that sum becomes the accumulator
    the next frames start from. A format whose offsets always accumulate sets the flag on every
    step; one whose offsets never do leaves it clear.
*/
struct SampleStep {
    int vol = 0;             // 0..15
    int volSlide = 0;        // added to an accumulator that is added to vol, each time the step plays
    bool tone = false;       // the tone generator sounds
    int toneOfs = 0;         // added to the note's tone period, in chip period units
    bool toneAcc = false;
    bool noise = false;      // the noise generator sounds
    int noiseOfs = 0;        // the noise period, or what is added to the pattern's noise base
    bool noiseAcc = false;
    bool noiseAbs = false;   // noiseOfs is the noise period itself: the format has no noise base
    bool env = false;        // the hardware envelope drives the volume, when the pattern turns it on
    int envOfs = 0;          // added to the envelope period
    bool envAcc = false;
};

/** What an ornament does to its channel in one frame. */
struct OrnamentStep {
    int noteOfs = 0;         // semitones added to the note
    bool noteAcc = false;
    int noiseOfs = 0;        // added to the noise period
    bool noiseAcc = false;
};

/** A sample or an ornament: a step per frame. After the last step it goes on from `loop`,
    or the channel falls silent when it `stops`. Steps that can never play are left out.
*/
template <class Step>
struct Sequence {
    unsigned slot = 0;       // its number in the module
    std::vector<Step> steps;
    unsigned loop = 0;
    bool stops = false;
};

using Sample = Sequence<SampleStep>;
using Ornament = Sequence<OrnamentStep>;

//==============================================================================
/** How a module's patterns sound a sample: the rows it plays with one ornament, envelope, noise
    base and note. */
struct UsageKey {
    unsigned sample = 0;
    unsigned ornament = 0;
    int envelope = -1;       // the envelope shape (AY register 13), -1 when the envelope is off
    int noise = 0;           // what the pattern adds to the sample's noise period: its noise base
    unsigned note = 0;       // the note's number in the tracker's table, 0 is its lowest

    auto operator<=>(const UsageKey&) const = default;
};

struct UsageCount {
    unsigned notes = 0;      // times the note was struck
    unsigned rows = 0;       // rows it sounded for
    unsigned frames = 0;     // the same in frames, by the tempo of each row
};

using Usage = std::map<UsageKey, UsageCount>;

//==============================================================================
/** The instruments of one module and how its patterns use them. */
struct Module {
    std::string format;      // zxtune's name of the format
    std::string title;       // UTF-8
    std::string author;
    std::string program;     // the tracker that saved it, when the module says
    std::string noteTable;   // the tracker's note table
    std::vector<Sample> samples;
    std::vector<Ornament> ornaments;
    Usage usage;
};

/** Writes {"modules": [...]}: a Turbo Sound file has two modules, any other has one. */
void writeJson(std::ostream& out, const std::vector<Module>& modules);

}  // namespace MoTool::InstrDump

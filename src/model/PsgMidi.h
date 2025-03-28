#pragma once

#include <JuceHeader.h>

#include "../formats/psg/PsgFile.h"
#include "../formats/psg/PsgData.h"

namespace te = tracktion;

namespace MoTool {

//==============================================================================

void loadMidiListStateFrom(const te::Edit& edit, ValueTree &seqState, const uZX::PsgFile &psgFile);

juce::MidiMessageSequence createPsgPlaybackMidiSequence(const te::MidiList& list, const te::MidiClip& clip, te::MidiList::TimeBase timeBase, bool generateMPE);

class PsgMidiCCSequenceReader {
public:
    struct MaybeRegPair {
        int reg = -1;
        uint8_t value = 0;

        bool isValid() const noexcept { return reg >= 0; }
        operator bool() const noexcept { return isValid(); }
    };

    MaybeRegPair read(const te::MidiMessageWithSource& m);

private:
    uZX::PsgRegsAYFrame registers = {};
};

}  // namespace MoTool
#pragma once

#include <JuceHeader.h>
#include "../formats/psg/psg_file.h"

namespace te = tracktion;

namespace MoTool {

namespace IDs {
    #define DECLARE_ID(name)  const juce::Identifier name(#name);
    DECLARE_ID(PSGCLIP)
    #undef DECLARE_ID
}  // namespace IDs

class PsgClip : public te::MidiClip {
public:
    using Ptr = juce::ReferenceCountedObjectPtr<PsgClip>;

    // PsgClip(te::Edit& edit, const juce::ValueTree& v)
    //     : te::MidiClip(edit, v)
    // {
    // }

    // ~PsgClip() override
    // {
    //     notifyListenersOfDeletion();
    // }

    // juce::String getSelectableDescription() override
    // {
    //     return TRANS("PSG Clip") + " - " + getName();
    // }

    // bool isMidiClip() const override { return false; }
    // bool isPsgClip() const { return true; }

    // void loadPsgFile(const juce::File& file)
    // {
    //     // Load PSG file and convert to MIDI events
    //     uZX::PsgFile psgFile(file);

    //     // Clear existing MIDI sequence
    //     getMidiList().clear();

    //     // TODO: Convert PSG data to MIDI events
    //     // This would involve parsing the PSG file and creating
    //     // appropriate MIDI messages for the AY chip plugin

    //     // For now, this is just a placeholder
    //     setName(file.getFileNameWithoutExtension());
    //     setCurrentSourceFile(file);
    // }
};

}  // namespace MoTool
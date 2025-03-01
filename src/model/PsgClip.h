#pragma once

#include <JuceHeader.h>
#include "../formats/psg/psg_file.h"
#include "tracktion_engine/tracktion_engine.h"

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

    PsgClip(const juce::ValueTree& v, te::EditItemID id, te::ClipOwner& parent_)
        : te::MidiClip(v, id, parent_)
    {}

    void initialise() override {
        DBG("PsgClip::initialise()");
        te::MidiClip::initialise();
    }

    static Ptr insertTo(te::ClipOwner& owner, const String& name, uZX::PsgFile& psgFile, te::ClipPosition position) {
        return {};
    }

    juce::String getSelectableDescription() override {
        return TRANS("PSG Clip") + " - " + getName();
    }

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
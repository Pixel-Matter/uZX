#pragma once

#include <JuceHeader.h>
#include "../formats/psg/psg_file.h"
#include "CustomClip.h"

namespace te = tracktion;

namespace MoTool {


class PsgClip : public te::MidiClip, public CustomClip {
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
        auto* clip = dynamic_cast<PsgClip*>(CustomClip::insertClipWithState(owner, {}, name, CustomClip::Type::psg, position,
                                            te::DeleteExistingClips::no, false));
        return clip;
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

    //     // TODO: Convert PSG data to MIDI list
    //     // This would involve parsing the PSG file and creating appropriate MIDI

    //     // For now, this is just a placeholder
    //     setName(file.getFileNameWithoutExtension());
    //     setCurrentSourceFile(file);
    // }
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgClip)
};

}  // namespace MoTool
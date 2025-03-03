#pragma once

#include <JuceHeader.h>
#include "../formats/psg/PsgFile.h"
#include "CustomClip.h"
#include "juce_core/system/juce_PlatformDefs.h"

namespace te = tracktion;

namespace MoTool {


class PsgClip : public te::MidiClip, public CustomClip {
public:
    using Ptr = juce::ReferenceCountedObjectPtr<PsgClip>;

    PsgClip(const juce::ValueTree& v, te::EditItemID id, te::ClipOwner& parent_)
        : te::MidiClip(v, id, parent_)
    {}

    void initialise() override {
        // DBG("PsgClip::initialise()");
        te::MidiClip::initialise();
    }

    static Ptr insertTo(
        te::ClipOwner& owner,
        uZX::PsgFile& psgFile,
        te::ClipPosition position,
        UndoManager* undoManager = nullptr
    ) {
        auto* clip = dynamic_cast<PsgClip*>(CustomClip::insertClipWithState(
            owner,
            /*stateToUse=*/ {},
            psgFile.getFile().getFileNameWithoutExtension(),
            CustomClip::Type::psg,
            position,
            te::DeleteExistingClips::no,
            false
        ));
        jassert(clip != nullptr);
        clip->loadFromFile(psgFile, undoManager);
        return clip;
    }

    juce::String getSelectableDescription() override {
        return TRANS("PSG Clip") + " - " + getName();
    }

private:
    void loadFromFile(uZX::PsgFile& psgFile, UndoManager* undoManager = nullptr) {
        // Load PSG file and convert to MIDI events
        getSequence().clear(undoManager);
        psgFile.ensureRead();
        auto& seq = getSequence();
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgClip)
};

}  // namespace MoTool
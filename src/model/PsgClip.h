#pragma once

#include <JuceHeader.h>
#include "../formats/psg/PsgFile.h"
#include "CustomClip.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "tracktion_core/utilities/tracktion_Time.h"
#include "tracktion_core/utilities/tracktion_TimeRange.h"
#include "tracktion_engine/tracktion_engine.h"

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
        te::ClipPosition position
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
        clip->getUndoManager()->beginNewTransaction();
        clip->loadFromFile(psgFile);
        return clip;
    }

    juce::String getSelectableDescription() override {
        return "PSG AY data clip - " + getName();
    }

private:
    inline ValueTree createRegValueTree(te::BeatPosition start, double dur, int reg, double val) {
        return te::createValueTree(
            te::IDs::NOTE,
            te::IDs::p, reg,
            te::IDs::b, start.inBeats(),
            te::IDs::l, dur,
            te::IDs::v, val);
        // or
        // auto v = te::MidiControllerEvent::createControllerEvent(startBeat, 20 + j, reg);
    }

    void loadFromFile(uZX::PsgFile& psgFile) {
        // Load PSG file and convert to MidiList data
        te::MidiList& seq = getSequence();
        seq.clear(getUndoManager());
        psgFile.ensureRead();
        // auto& edit = getEdit();
        const double frameRate = 50; // TODO get from edit timeOptions
        auto& data = psgFile.getData();
        for (size_t i = 0; i < data.frames.size(); i++) {
            auto& frame = data.frames[i];
            auto timeSec = psgFile.frameNumToSeconds(i, frameRate);
            auto startBeat = getContentBeatAtTime(te::TimePosition::fromSeconds(timeSec));
            // DBG("Frame " << i << " time=" << timeBeat);
            for (size_t j = 0; j < frame.registers.size(); j++) {
                if (frame.mask[j]) {
                    // DBG("Register " << j << " = " << reg);
                    auto reg = frame.registers[j];
                    auto v = createRegValueTree(startBeat, 0.01, 20 + j, reg);
                    // auto v = juce::createNoteValueTree(j, startBeat, 1.0/50, 127, 0);
                    seq.state.addChild(v, -1, getUndoManager());
                    // Too slow to call addControllerEvent
                    // seq.addControllerEvent(timeBeat, 20 + j, reg, undoManager);
                }
            }
        }
        changed();
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgClip)
};

}  // namespace MoTool
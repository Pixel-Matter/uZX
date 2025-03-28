#pragma once

#include <JuceHeader.h>
#include <_stdio.h>

#include "CustomClip.h"
#include "PsgMidi.h"
#include "../formats/psg/PsgFile.h"
#include "../plugins/uZX/aychip/AYPlugin.h"

namespace te = tracktion;

namespace MoTool {

class PsgClip : public te::MidiClip, public CustomClip {
public:
    using Ptr = juce::ReferenceCountedObjectPtr<PsgClip>;

    PsgClip(const juce::ValueTree& v, te::EditItemID id, te::ClipOwner& parent_)
        : te::MidiClip(v, id, parent_)
    {}

    void initialise() override {
        te::MidiClip::initialise();
        if (getColour() == getDefaultColour()) {
            auto track = getTrack();
            // TODO make clip colors use themed palette
            float hue = (track->getIndexInEditTrackList() % 18) * 1.0f / 18.0f + 0.3f;
            setColour(getDefaultColour().withHue(hue));
        }
        // Not sure we should have clip plugins yet
        // ensureHasAYPlugin();
    }

    String getSelectableDescription() override {
        return "PSG AY data clip - " + getName();
    }

    Colour getDefaultColour() const override {
        return Colours::blue;
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
            te::DeleteExistingClips::yes,
            false
        ));
        jassert(clip != nullptr);
        clip->getUndoManager()->beginNewTransaction();
        clip->loadFromFile(psgFile);
        return clip;
    }

private:
    // TODO
    // 1. Dialog for interpreting PSG file
    // 2. BackgroundJob for loading PSG file
    void loadFromFile(uZX::PsgFile &psgFile) {
        psgFile.ensureRead();
        auto *um = getUndoManager();

        getSequence().clear(um);
        // Fastest midi inport
        // 1. construct MidiList state detached from everything,
        // 2. remove old sequence from the state
        // 3. add the new sequence tree directly to the clips state in one operation
        auto seqState = getSequence().state.createCopy();
        double timeElapsed;
        {
            juce::ScopedTimeMeasurement measurement(timeElapsed);
            loadMidiListStateFrom(edit, seqState, psgFile);
            state.removeChild(state.getChildWithName(te::IDs::SEQUENCE), um);
            state.addChild(seqState, -1, um);
        }
        DBG("PSG clip constructed in " << timeElapsed << "s");
        changed();
        scaleVerticallyToFit();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgClip)
};

}  // namespace MoTool
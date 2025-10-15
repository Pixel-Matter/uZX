
#pragma once

#include <JuceHeader.h>

#include "PsgClip.h"
#include "PsgMidi.h"


namespace te = tracktion;

namespace MoTool {

class ExtEngineBehaviour : public te::EngineBehaviour {
public:
    ExtEngineBehaviour() = default;

    bool autoInitialiseDeviceManager() override {
        return true;
    }

    bool shouldOpenAudioInputByDefault() override {
        return false;
    }

    te::Clip::Ptr createCustomClipForState(
        const juce::ValueTree& v, te::EditItemID id, te::ClipOwner& targetParent
    ) override {
        // DBG("createCustomClipForState");
        auto type = v.getType();

        // Check for our custom clip types
        if (type == MoTool::IDs::PSGCLIP)
            return new PsgClip(v, id, targetParent);

        // Add more custom clip types as needed

        return {};
    }

    bool isCustomClipType(const juce::Identifier& identifier) override {
        // DBG("isCustomClipType " << identifier.toString());
        return identifier == MoTool::IDs::PSGCLIP;
    }

    MidiMessageSequence createPlaybackMidiSequence(const te::MidiList& list, te::MidiClip& clip, te::MidiList::TimeBase tb, bool generateMPE) override {
        // DBG("ExtEngineBehaviour::createPlaybackMidiSequence for clip " << clip.getName());
        if (auto psgClip = dynamic_cast<PsgClip*>(&clip)) {
            return psgClip->getPsg().exportToPlaybackMidiSequence(*psgClip, tb);
            // return createPsgPlaybackMidiSequence(list, clip, tb);
        }
        auto seq = te::MidiList::createDefaultPlaybackMidiSequence(list, clip, tb, generateMPE);
        // for (auto& msg : seq) {
        //     DBG(msg->message.getDescription());
        // }
        return seq;
    }

    bool shouldPlayMidiGuideNotes() override {
        // For tuning preview
        return true;
    }

};

}  // namespace MoTool
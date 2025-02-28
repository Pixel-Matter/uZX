
#pragma once

#include <JuceHeader.h>

#include "PsgClip.h"


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
        if (type == IDs::PSGCLIP)
            return new PsgClip(v, id, targetParent);

        // Add more custom clip types as needed

        return {};
    }

    bool isCustomClipType(const juce::Identifier& identifier) override {
        // DBG("isCustomClipType " << identifier.toString());
        return identifier == IDs::PSGCLIP;
    }
};

}  // namespace MoTool
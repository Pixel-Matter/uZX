#pragma once

#include <JuceHeader.h>

#include "PsgClip.h"


namespace te = tracktion;

namespace MoTool {

namespace IDs {
    #define DECLARE_ID(name)  const juce::Identifier name(#name);

    DECLARE_ID(PSGTRACK)
    #undef DECLARE_ID
}  // namespace IDs


class PsgTrack : public te::AudioTrack {
public:
    PsgTrack() = delete;
    PsgTrack(te::Edit& ed, const juce::ValueTree& v)
        : te::AudioTrack(ed, v)
    {}

    ~PsgTrack() override {
        notifyListenersOfDeletion();
    }

    juce::String getSelectableDescription() override { return TRANS("PSG Track"); }

    bool canContainPlugin ([[maybe_unused]] te::Plugin* plugin) const override {
        // Allow same plugins as AudioTrack for now
        return true;
    }

    // Override any AudioTrack methods that need customization for PSG

    PsgClip::Ptr insertPsgClip(const juce::String& name, const juce::File& sourceFile, te::ClipPosition position) {
        // // Create a PsgClip
        // auto v = te::MidiClip::createNewMidiClipValueTree(juce::Uuid().toString(), name, position);
        // v.setType(IDs::PSGCLIP); // Change the type to our custom PSG clip type

        // if (auto clip = dynamic_cast<PsgClip*>(edit.insertClipWithState(*this, v, &edit.getUndoManager()).get())) {
        //     // Load PSG file data
        //     clip->loadPsgFile(sourceFile);

        //     return clip;
        // }
        return {};
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgTrack)
};

}  // namespace MoTool
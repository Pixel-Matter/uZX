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
    // bool canContainPlugin (Plugin*) const override  { // TODO implement this }

    PsgClip::Ptr insertPsgClip(const juce::String& name, const juce::File& sourceFile, te::ClipPosition position) {
        return {};
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgTrack)
};

}  // namespace MoTool
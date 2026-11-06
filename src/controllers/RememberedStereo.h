#pragma once

#include <JuceHeader.h>

namespace MoTool {

namespace uZX { class AYChipPlugin; }

/** The Player's stereo is an app setting, as its master volume is: the layout and width last
    chosen on any AY chip of the edit are saved, and apply() gives them to the chip of a PSG
    file opened next. Loading an edit changes nothing, so a saved one keeps its own.
    Create one per edit. */
class RememberedStereo : private juce::ValueTree::Listener {
public:
    explicit RememberedStereo(tracktion::Edit& e) : edit_{e} {
        edit_.state.addListener(this);
    }

    ~RememberedStereo() override {
        edit_.state.removeListener(this);
    }

    static void apply(uZX::AYChipPlugin& chip);

    static constexpr const char* layoutKey = "stereoLayout";
    static constexpr const char* widthKey = "stereoWidth";

private:
    tracktion::Edit& edit_;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& prop) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RememberedStereo)
};

}  // namespace MoTool

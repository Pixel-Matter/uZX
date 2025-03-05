#pragma once

#include <JuceHeader.h>

#include "../../../formats/psg/PsgData.h"
#include "aychip.h"


namespace te = tracktion;

namespace MoTool::uZX {

class AYChipPlugin : public te::Plugin,
                     private AsyncUpdater {
public:
    AYChipPlugin (te::PluginCreationInfo);
    ~AYChipPlugin() override;

    //==============================================================================
    static const char* getPluginName()                  { return "AY Chip"; }
    static const char* xmlTypeName;

    String getName() const override               { return "AY Chip"; }
    String getPluginType() override               { return xmlTypeName; }
    String getShortName (int) override            { return "AY"; }
    String getSelectableDescription() override    { return "AY Chip plugin based on Ayumi emulator"; }
    bool isSynth() override                       { return true; }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return jmin (numInputChannels, 2); }
    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const te::PluginRenderContext&) override;

    //==============================================================================
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return false; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    void restorePluginStateFromValueTree (const ValueTree&) override;

private:
    //==============================================================================

    Colour colour;
    CriticalSection lock;

    PsgRegsAYFrame registers;
    std::unique_ptr<AYInterface> chip;

    void valueTreeChanged() override;
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AYChipPlugin)
};

} // namespace MoTool::uZX

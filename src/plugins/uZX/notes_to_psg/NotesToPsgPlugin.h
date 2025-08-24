#pragma once

#include <JuceHeader.h>

#include "NotesToPsgMapper.h"

#include "../midi_effects/MidiEffect.h"
#include "../../../models/tuning/TuningSystemBase.h"
#include "../../../controllers/ParamAttachments.h"

namespace te = tracktion;

namespace MoTool {

namespace IDs {
    #define DECLARE_ID(name)  const juce::Identifier name(#name);
    DECLARE_ID(midiBase)
    DECLARE_ID(midiChans)
    #undef DECLARE_ID
}

namespace uZX {


class NotesToPsgPlugin : public MidiFxPluginBase<NotesToPsgMapper>
{
public:
    using Ptr = ReferenceCountedObjectPtr<NotesToPsgPlugin>;
    NotesToPsgPlugin(te::PluginCreationInfo);
    ~NotesToPsgPlugin() override;

    //==============================================================================
    static const char* getPluginName() { return "μZX MIDI to PSG"; }
    static const char* xmlTypeName;

    String getName() const override { return String::fromUTF8(getPluginName()); }
    String getPluginType() override { return xmlTypeName; }
    String getShortName(int) override { return "MIDI-PSG"; }
    String getSelectableDescription() override { return "Converts MIDI notes to PSG MIDI CC messages"; }

    void initialise(const te::PluginInitialisationInfo&) override;
    void deinitialise() override;
    void midiPanic() override;
    void reset() override;

    //==============================================================================
    void restorePluginStateFromValueTree(const ValueTree&) override;
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;

    struct Params {
        RangedParamAttachment<int> baseMidiChannelValue;
        RangedParamAttachment<int> numChannelsValue;

        Params(te::Plugin& p)
            : baseMidiChannelValue(p.state, p.getUndoManager())
            , numChannelsValue(p.state, p.getUndoManager())
        {
            initialise();
        }

        void initialise();
        void restoreFromTree(const juce::ValueTree& v);
    };

    // specific for MidiToPsg methods
    void setTuningSystem(TuningSystem* tuningSystem);

    Params staticParams {*this};

private:
    //==============================================================================
    // uZX::NotesToPsgMapper transformer;
    TuningSystem* currentTuningSystem = nullptr;

    void valueTreeChanged() override;
    void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override;
    void updateConverterParams();
    void processMidiMessageWithSource(const te::MidiMessageWithSource& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotesToPsgPlugin)
};

} // namespace MoTool::uZX

} // namespace MoTool
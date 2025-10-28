#pragma once

#include <JuceHeader.h>

#include "NotesToPsgMapper.h"

#include "../midi_effects/MidiEffect.h"
#include "../../../models/tuning/TuningSystemBase.h"
#include "../../../controllers/Parameters.h"

namespace te = tracktion;

namespace MoTool::uZX {

namespace IDs {
    #define DECLARE_ID(name)  inline const juce::Identifier name(#name);
    DECLARE_ID(midiBase)
    DECLARE_ID(midiChans)
    #undef DECLARE_ID
}


class NotesToPsgPlugin : public MidiFxPluginBase<NotesToPsgMapper>
{
public:
    using Ptr = ReferenceCountedObjectPtr<NotesToPsgPlugin>;
    NotesToPsgPlugin(te::PluginCreationInfo);
    ~NotesToPsgPlugin() override;

    //==============================================================================
    static const char* getPluginName() { return "μZX MIDI to PSG"; }
    static const char* xmlTypeName;

    String getVendor() override { return "PixelMatter"; }
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

    class StaticParams : public ParamsBase<StaticParams> {
    public:
        using ParamsBase<StaticParams>::ParamsBase;

        ParameterValue<int> baseMidiChannel {{"midiBase", IDs::midiBase, "MIDI", "MIDI channel range", 1, {1, 16 - 3, 1}}};
        // TODO Tuning system parameter, choice

        template<typename Visitor>
        void visit(Visitor&& visitor) {
            visitor(baseMidiChannel);
        }
    };

    // specific for MidiToPsg methods
    // TODO make this static parameter
    void setTuningSystem(TuningSystem* tuningSystem);

    StaticParams staticParams;

private:
    //==============================================================================
    NotesToPsgMapper transformer;
    TuningSystem* currentTuningSystem = nullptr;

    void valueTreeChanged() override;
    void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override;
    void updateParams();
    void processMidiMessageWithSource(const te::MidiMessageWithSource& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotesToPsgPlugin)
};

} // namespace MoTool::uZX
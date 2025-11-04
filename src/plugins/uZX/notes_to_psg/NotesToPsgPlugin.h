#pragma once

#include <JuceHeader.h>
#include <memory>

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
    DECLARE_ID(tuningTable)
    DECLARE_ID(chipClock)
    DECLARE_ID(scaleType)
    #undef DECLARE_ID
}


class NotesToPsgPlugin : public MidiFxPluginBase<NotesToPsgMapper>
                       , public ChangeBroadcaster
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
        using TunType = BuiltinTuningType;

        ParameterValue<int>     baseMidiChannel {{"midiBase",    IDs::midiBase,    "MIDI",          "MIDI channel range",
                                                  1, {1, 16 - 3, 1}}};
        ParameterValue<TunType> tuningTable     {{"tuningTable", IDs::tuningTable, "Tuning preset", "Selected tuning table",
                                                  TunType::EqualTemperament, TunType::getLabels()}};
        // ParameterValue<Scale::ScaleType> scaleType {{"scaleType", IDs::scaleType, "Scale", "Selected scale type",
        //                                              Scale::ScaleType::IonianOrMajor, Scale::ScaleType::getLongLabels()}};
        // ParameterValue<double>  chipClock       {{"clock",       IDs::chipClock,   "Clock",         "Clock frequncy",
        //                                           1.7734, {0.894887, 2.0, 0.01}, "MHz"}};

        template<typename Visitor>
        void visit(Visitor&& visitor) {
            visitor(baseMidiChannel);
            visitor(tuningTable);
            // visitor(scaleType);
            // visitor(chipClock);
        }
    };

    // specific for MidiToPsg methods
    TuningSystem& getTuningSystem() const;
    void setTuningSystem(std::shared_ptr<TuningSystem> tuningSystem);

    // Scale::ScaleType getScaleType() const;
    // void setScaleType(Scale::ScaleType scaleType);

    StaticParams staticParams;

private:
    //==============================================================================
    NotesToPsgMapper transformer;

    void valueTreeChanged() override;
    void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override;
    void recreateTuningSystem();
    void updateParams();
    void processMidiMessageWithSource(const te::MidiMessageWithSource& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotesToPsgPlugin)
};

} // namespace MoTool::uZX
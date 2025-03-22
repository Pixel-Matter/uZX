#pragma once

#include <JuceHeader.h>

#include "../../../formats/psg/PsgData.h"
#include "aychip.h"

#include <atomic>
#include <optional>

namespace te = tracktion;

namespace MoTool {

namespace IDs {
    #define DECLARE_ID(name)  const juce::Identifier name(#name);
    DECLARE_ID(chip)
    DECLARE_ID(clock)
    DECLARE_ID(layout)
    DECLARE_ID(pan)
    #undef DECLARE_ID
}  // namespace IDs

namespace uZX {

constexpr double MHz = 1000000.0;

template <typename Type>
struct ParamAttachment {
    using type = Type;

    ParamAttachment(te::Plugin& p)
        : plugin(p)
    {}

    // TODO implement
    // class Listener {
    //     virtual void valueChanged() = 0;
    // };
    // TODO implement
    // void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override;

    void referTo(const Identifier& id, const String& n, const NormalisableRange<Type>& r, const Type& def, const String& u) {
        name = n;
        units = u;
        range = r;
        value.referTo(plugin.state, id, plugin.getUndoManager(), def);
    }

    inline operator CachedValue<Type>&() noexcept { return value; }

    // for CachedValue-like transparent access
    inline operator Type() const noexcept         { return value.get(); }

    inline Type get() const noexcept              { return value.get(); }

    inline const Type& operator*() const noexcept        { return *value; }

    template <typename OtherType>
    inline bool operator== (const OtherType& other) const { return value == other; }

    template <typename OtherType>
    inline bool operator!= (const OtherType& other) const   { return ! operator== (other); }

    inline Type getDefault() const                          { return value.getDefault(); }

    inline ParamAttachment& operator= (const Type& newValue) {
        value = newValue;
        return *this;
    }

    // ======================================================================================
    te::Plugin& plugin;
    String name;
    String units;
    CachedValue<Type> value;
    NormalisableRange<Type> range;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamAttachment)
};

class AYChipPlugin : public te::Plugin,
                     private AsyncUpdater {
public:
    AYChipPlugin (te::PluginCreationInfo);
    ~AYChipPlugin() override;

    enum ChannelsLayout {
        ABC,
        ACB,
        BAC
    };

    //==============================================================================
    static const char* getPluginName()                  { return "AY Chip"; }
    static const char* xmlTypeName;

    String getName() const override               { return "AY Chip"; }
    String getPluginType() override               { return xmlTypeName; }
    String getShortName (int) override            { return "AY"; }
    String getSelectableDescription() override    { return "AY Chip plugin based on Ayumi emulator"; }
    bool isSynth() override                       { return true; }

    int getNumOutputChannelsGivenInputs(int numInputChannels) override { return jmin (numInputChannels, 2); }
    void initialise(const te::PluginInitialisationInfo&) override;
    void initialiseAY();
    void deinitialise() override;
    void applyToBuffer(const te::PluginRenderContext&) noexcept override;
    void midiPanic() override;
    void reset() override;

    //==============================================================================
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return false; }
    bool producesAudioWhenNoAudioInput() override       { return false; }
    void restorePluginStateFromValueTree (const ValueTree&) override;
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;

    struct Params {
        ParamAttachment<AYInterface::ChipType> chipTypeValue;
        ParamAttachment<double> clockValue;
        // ParamAttachment<ChannelsLayout> channelsLayoutValue;
        // ParamAttachment<double> panWidthValue;
    };

    // TODO make it nice
    Params staticParams {
        .chipTypeValue = {*this},
        .clockValue    = {*this}
    };

    juce::CachedValue<AYInterface::ChipType> chipTypeValue;

private:
    //==============================================================================

    Colour colour;
    CriticalSection lock;

    PsgRegsAYFrame registers;
    std::unique_ptr<AYInterface> chip;

    double timeFromReset;

    struct PendingChanges {
        std::unique_ptr<AYInterface> chip;
    };

    PendingChanges pendingChanges;
    std::atomic<bool> isProcessing {false};

    void valueTreeChanged() override;
    void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override;
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AYChipPlugin)
};

} // namespace MoTool::uZX

} // namespace MoTool
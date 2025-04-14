#pragma once

#include <JuceHeader.h>

#include "../../../formats/psg/PsgData.h"
#include "../../../model/PsgMidi.h"
#include "aychip.h"

#include <atomic>
#include <array>

namespace te = tracktion;

namespace MoTool {

namespace IDs {
    #define DECLARE_ID(name)  const juce::Identifier name(#name);
    DECLARE_ID(chip)
    DECLARE_ID(clock)
    DECLARE_ID(layout)
    DECLARE_ID(stereo)
    DECLARE_ID(noDC)
    DECLARE_ID(midi)
    #undef DECLARE_ID
}  // namespace IDs

namespace uZX {

constexpr double MHz = 1000000.0;


template <size_t N>
static StringArray toStringArray(const std::array<std::string_view, N>& ch) {
    StringArray result;
    for (const auto& s : ch) {
        result.add(String(s.data(), s.size()));
    }
    return result;
}

template <typename Type>
struct ParamAttachment {
    using type = Type;

    ParamAttachment(te::Plugin& p)
        : plugin(p)
    {}

    void referTo(const Identifier& id, const String& n, const Type& def, const String& u) {
        name = n;
        units = u;
        value.referTo(plugin.state, id, plugin.getUndoManager(), def);
    }

    void referTo(const Identifier& id, const String& n, const NormalisableRange<Type>& r, const Type& def, const String& u) {
        referTo(id, n, def, u);
        range = r;
    }

    void referTo(const Identifier& id, const String& n, const StringArray& ch, const Type& def, const String& u) {
        referTo(id, n, def, u);
        choices.clear();
        for (int i = 0; i < ch.size(); ++i) {
            choices.push_back({static_cast<Type>(i), ch[i]});
        }
    }

    template <size_t N>
    void referTo(const Identifier& id, const String& n, const std::array<std::string_view, N>& ch, const Type& def, const String& u) {
        referTo(id, n, toStringArray(ch), def, u);
    }

    void referTo(const Identifier& id, const String& n, const std::vector<std::pair<Type, String>>& ch, const Type& def, const String& u) {
        referTo(id, n, def, u);
        choices = ch;
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

    inline Value getPropertyAsValue() {
        return value.getPropertyAsValue();
    }

    const std::vector<std::pair<Type, String>>& getChoices() const {
        return choices;
    }

    // ======================================================================================
    te::Plugin& plugin;
    String name;
    String units;
    CachedValue<Type> value;
    NormalisableRange<Type> range;
    std::vector<std::pair<Type, String>> choices;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamAttachment)
};

class AYChipPlugin : public te::Plugin {
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

    int getNumOutputChannelsGivenInputs(int numInputChannels) override { return jmin (numInputChannels, 2); }
    void initialise(const te::PluginInitialisationInfo&) override;
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
        ParamAttachment<AYInterface::ChannelsLayout> channelsLayoutValue;
        ParamAttachment<double> stereoWidthValue;
        ParamAttachment<bool> removeDCValue;
        ParamAttachment<int> baseMidiChannelValue;

        Params(te::Plugin& p)
            : chipTypeValue(p)
            , clockValue(p)
            , channelsLayoutValue(p)
            , stereoWidthValue(p)
            , removeDCValue(p)
            , baseMidiChannelValue(p)
        {
            initialise();
        }

        void initialise();
        void restoreFromTree(const juce::ValueTree& v);
    };

    Params staticParams {*this};

private:
    //==============================================================================

    Colour colour;
    CriticalSection lock;

    PsgParamsMidiReader midiParamsCCReader;
    PsgRegsMidiSequenceReader midiCCReader;
    PsgRegsFrame registersFrame;
    std::unique_ptr<AYInterface> chip;

    double timeFromReset;

    struct PendingChanges {
        std::unique_ptr<AYInterface> chip;
    };

    PendingChanges pendingChanges;
    std::atomic<bool> isProcessing {false};

    void valueTreeChanged() override;
    void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override;
    void updateChip(const PsgParamFrameData& params);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AYChipPlugin)
};

} // namespace MoTool::uZX

} // namespace MoTool
#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <ostream>

#include "../midi_effects/MidiEffect.h"


namespace MoTool::uZX {

namespace IDs {
    inline const juce::Identifier logTag("logTag");
}

class MidiLoggerEffect {
public:
    MidiLoggerEffect();

    void setOutputStream(std::ostream& stream) noexcept;
    std::ostream* getOutputStream() const noexcept;
    void setTag(const juce::String& newTag);

    void operator()(MidiBufferContext& context);

private:
    std::atomic<std::ostream*> outputStream { nullptr };
    juce::SpinLock tagLock;
    juce::String tag;
};


class MidiLoggerPlugin : public MidiFxPluginBase<MidiLoggerEffect> {
public:
    using Ptr = ReferenceCountedObjectPtr<MidiLoggerPlugin>;

    MidiLoggerPlugin(tracktion::PluginCreationInfo);
    ~MidiLoggerPlugin() override = default;

    static const char* getPluginName() { return "μZX MIDI Logger"; }
    static const char* xmlTypeName;

    String getVendor() override { return "PixelMatter"; }
    String getName() const override { return String::fromUTF8(getPluginName()); }
    String getPluginType() override { return xmlTypeName; }
    String getShortName(int) override { return "MLog"; }
    String getSelectableDescription() override { return "Prints incoming MIDI messages to stdout"; }

    void initialise(const tracktion::PluginInitialisationInfo&) override {}
    void deinitialise() override {}
    void reset() override {}
    void midiPanic() override {}

    void setOutputStream(std::ostream& stream) noexcept;
    void setLogTag(const juce::String& tag);
    juce::String getLogTag() const;

    void restorePluginStateFromValueTree(const juce::ValueTree&) override;

private:
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;

    MidiLoggerEffect logger;
    juce::CachedValue<juce::String> logTag;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLoggerPlugin)
};

}  // namespace MoTool::uZX

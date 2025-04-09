#include "AYPlugin.h"
#include "AYPluginEditor.h"

namespace te = tracktion;

namespace MoTool::uZX {


//==============================================================================
void AYChipPlugin::Params::initialise() {
    clockValue          .referTo(IDs::clock,  "Clock frequncy",    {0.894887, 2.0, 0.01},  1.7734, "MHz");
    chipTypeValue       .referTo(IDs::chip,   "Chip type",         AYInterface::ChipType::getLabels(),       AYInterface::TypeEnum::AY,    {});
    channelsLayoutValue .referTo(IDs::layout, "Channels layout",   AYInterface::ChannelsLayout::getLabels(), AYInterface::LayoutEnum::ACB, {});
    stereoWidthValue    .referTo(IDs::stereo, "Stereo width",      {0.0, 1.0, 0.01},       0.5,    {});
    removeDCValue       .referTo(IDs::noDC,   "Remove DC",                                 true,   {});
    baseMidiChannelValue.referTo(IDs::midi,   "Base MIDI channel", {1, 15 - 4, 1},         1,      {});
}


void AYChipPlugin::Params::restoreFromTree(const juce::ValueTree& v) {
    te::copyPropertiesToCachedValues(v,
        chipTypeValue.value,
        clockValue.value,
        channelsLayoutValue.value,
        stereoWidthValue.value,
        removeDCValue.value,
        baseMidiChannelValue.value
    );
}

//==============================================================================
AYChipPlugin::AYChipPlugin(te::PluginCreationInfo info)
    : te::Plugin(info)
    , midiParamsCCReader(1) // FIXME refer to plugin midi channel
{
}

AYChipPlugin::~AYChipPlugin() {
    notifyListenersOfDeletion();
}

const char* AYChipPlugin::xmlTypeName = "aychip";

void AYChipPlugin::valueTreeChanged() {
    te::Plugin::valueTreeChanged();
}

void AYChipPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    // TODO staticParams.isParamProperty(id);
    if (v == state) {
        if (id == IDs::clock || id == IDs::chip) {
            reset();
        } else if (id == IDs::stereo || id == IDs::layout) {
            if (chip != nullptr) {
                const ScopedLock sl(lock);
                chip->setLayoutAndStereoWidth(staticParams.channelsLayoutValue, staticParams.stereoWidthValue);
            }
        } else if (id == IDs::noDC) {
            // no need to do anything
            // DBG("removeDC = " << (staticParams.removeDCValue ? "true" : "false"));
        }
        propertiesChanged();
    }
    Plugin::valueTreePropertyChanged(v, id);
}

void AYChipPlugin::initialise(const te::PluginInitialisationInfo&) {
    reset();
}

void AYChipPlugin::deinitialise() {
    chip = nullptr;
}

void AYChipPlugin::midiPanic() {
    reset();
}

void AYChipPlugin::reset() {
    const ScopedLock sl(lock);
    if (chip == nullptr) {
        chip = std::make_unique<AyumiEmulator>(sampleRate, staticParams.clockValue * MHz, staticParams.chipTypeValue);
    } else {
        chip->reset(static_cast<int>(sampleRate), staticParams.clockValue * MHz, staticParams.chipTypeValue);
    }
    chip->setMasterVolume(0.5f);
    chip->setLayoutAndStereoWidth(staticParams.channelsLayoutValue, staticParams.stereoWidthValue);
    timeFromReset = 0.0;
    midiParamsCCReader.reset();
    midiCCReader.reset();
}

void AYChipPlugin::applyToBuffer(const te::PluginRenderContext& fc) noexcept {
    if (chip == nullptr || fc.destBuffer == nullptr || fc.bufferForMidiMessages == nullptr
        || !(fc.isPlaying || fc.isScrubbing || fc.isRendering)
    ) {
        return;
    }

    SCOPED_REALTIME_CHECK
    const ScopedLock sl(lock);

    te::clearChannels(*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

    // Process PSG regiser events, no midi notes on this low level
    int currentSample = 0;
    for (auto& m : *fc.bufferForMidiMessages) {
        // process up to this event
        const int timeSample = roundToInt(m.getTimeStamp() * sampleRate);
        if (timeSample - currentSample > 0) {
            chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample), fc.destBuffer->getWritePointer(1, currentSample),
                               static_cast<size_t>(timeSample - currentSample), staticParams.removeDCValue);
            currentSample = timeSample;
        }
        if (auto regpair = midiCCReader.read(m)) {
            chip->setRegister(static_cast<size_t>(regpair.reg), regpair.value);
        }

    }
    // process to the end of the block
    if (currentSample < fc.destBuffer->getNumSamples()) {
        chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample), fc.destBuffer->getWritePointer(1, currentSample),
                           static_cast<size_t>(fc.bufferNumSamples - currentSample),
                           staticParams.removeDCValue
                        );
    }
    timeFromReset += (double) fc.destBuffer->getNumSamples() / sampleRate;
    // DBG("timeFromReset = " << timeFromReset);
}

void AYChipPlugin::restorePluginStateFromValueTree(const juce::ValueTree& v) {
    staticParams.restoreFromTree(v);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

std::unique_ptr<te::Plugin::EditorComponent> AYChipPlugin::createEditor() {
    return std::make_unique<AYPluginEditor>(*this);
}

//==============================================================================

template <typename ChoiceType>
static ChoiceType choiceFromVar(const var& v) {
    // look for the first match from getLabels
    for (int i = 0; i <= static_cast<int>(ChoiceType::size()); ++i) {
        auto ct = ChoiceType::getLabelFor(static_cast<size_t>(i));
        if (v.toString().toStdString() == ct)
            return static_cast<ChoiceType>(i);
    }
    return static_cast<ChoiceType>(0);
}

template <typename ChoiceType>
static var choicetoVar(ChoiceType c) {
    const std::string_view label = c.getLabel();
    return String {label.data(), label.size()};
}

} // namespace MoTool::uZX


namespace juce {

using namespace MoTool::uZX;

template<>
struct VariantConverter<AYInterface::ChipType> {
    static AYInterface::ChipType fromVar(const var& v) {
        return choiceFromVar<AYInterface::ChipType>(v);
    }

    static var toVar(AYInterface::ChipType ct) {
        return choicetoVar(ct);
    }
};

template<>
struct VariantConverter<AYInterface::ChannelsLayout> {
    static AYInterface::ChannelsLayout fromVar(const var& v) {
        return choiceFromVar<AYInterface::ChannelsLayout>(v);
    }

    static var toVar(AYInterface::ChannelsLayout layout) {
        return choicetoVar(layout);
    }
};

} // namespace juce

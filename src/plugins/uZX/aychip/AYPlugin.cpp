#include "AYPlugin.h"
#include "AYPluginEditor.h"

namespace te = tracktion;

namespace MoTool::uZX {


//==============================================================================
void AYChipPlugin::Params::initialise() {
    clockValue          .referTo(IDs::clock,  "Clock frequncy",    {0.894887, 2.0, 0.01},  1.7734, "MHz");
    chipTypeValue       .referTo(IDs::chip,   "Chip type",         ChipType::getLabels(),       ChipType::AY,    {});
    channelsLayoutValue .referTo(IDs::layout, "Channels layout",   ChannelsLayout::getLabels(), ChannelsLayout::ACB, {});
    stereoWidthValue    .referTo(IDs::stereo, "Stereo width",      {0.0, 1.0, 0.01},       0.5,    {});
    removeDCValue       .referTo(IDs::noDC,   "Remove DC",                                 true,   {});
    baseMidiChannelValue.referTo(IDs::midi,   "Base MIDI channel", {1, 15 - 4, 1},         1,      {});
}


void AYChipPlugin::Params::restoreFromTree(const juce::ValueTree& v) {
    te::copyPropertiesToCachedValues(v,
        chipTypeValue.cachedValue,
        clockValue.cachedValue,
        channelsLayoutValue.cachedValue,
        stereoWidthValue.cachedValue,
        removeDCValue.cachedValue,
        baseMidiChannelValue.cachedValue
    );
}

//==============================================================================
AYChipPlugin::AYChipPlugin(te::PluginCreationInfo info)
    : te::Plugin(info)
    , staticParams(*this)
    , midiParamsReader(staticParams.baseMidiChannelValue)
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
        } else if (id == IDs::midi) {
            midiParamsReader.setBaseChannel(staticParams.baseMidiChannelValue);
        }
        // no need to do anything
        // else if (id == IDs::noDC) {
        //     // DBG("removeDC = " << (staticParams.removeDCValue ? "true" : "false"));
        // }
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
    midiParamsReader.reset();
    midiRegsReader.reset();
    registersFrame = {};
}

void AYChipPlugin::updateRegistersFromMidiParams() noexcept {
    const auto& params = midiParamsReader.getParams();
    registersFrame.clear();
    params.updateRegisters(registersFrame);
}

void AYChipPlugin::updateRegistersFromMidiRegs() noexcept {
    registersFrame = midiRegsReader.getRegisters();
    midiRegsReader.clear();
}

void AYChipPlugin::updateChip() noexcept {
    if (midiReaderMode == MidiReaderMode::Params) {
        updateRegistersFromMidiParams();
    } else if (midiReaderMode == MidiReaderMode::Regs) {
        updateRegistersFromMidiRegs();
    }

    for (size_t i = 0; i < registersFrame.size(); ++i) {
        if (registersFrame.isSet(i)) {
            chip->setRegister(i, registersFrame.getRaw(i));
        }
    }
}

void AYChipPlugin::readMidi(const te::MidiMessageWithSource& m) noexcept {
    if (midiReaderMode == MidiReaderMode::Params) {
        midiParamsReader.read(m);
    } else if (midiReaderMode == MidiReaderMode::Regs) {
        midiRegsReader.read(m);
    }
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
    // DBG("======== applyToBuffer");
    // Process PSG regiser events, no midi notes on this low level
    int currentSample = 0;
    for (auto& m : *fc.bufferForMidiMessages) {
        const int timeSample = roundToInt(m.getTimeStamp() * sampleRate);
        if (timeSample > currentSample) {
            // DBG("---------- " << m.getTimeStamp());
            updateChip();
            // DBG("---------- processing " << double(currentSample) / sampleRate << "-" << m.getTimeStamp());
            chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample),
                               fc.destBuffer->getWritePointer(1, currentSample),
                               static_cast<size_t>(timeSample - currentSample),
                               staticParams.removeDCValue);
            currentSample = timeSample;
        }
        readMidi(m);
    }
    // process to the end of the block
    updateChip();
    if (currentSample < fc.destBuffer->getNumSamples()) {
        chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample),
                           fc.destBuffer->getWritePointer(1, currentSample),
                           static_cast<size_t>(fc.bufferNumSamples - currentSample),
                           staticParams.removeDCValue);
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

} // namespace MoTool::uZX


using namespace MoTool::uZX;
using namespace MoTool::Util;

template <>
struct juce::VariantConverter<ChipType> : public EnumVariantConverter<ChipType> {};

template <>
struct juce::VariantConverter<ChannelsLayout> : public EnumVariantConverter<ChannelsLayout> {};

template <>
struct juce::VariantConverter<ChipClockChoice> : public EnumVariantConverter<ChipClockChoice> {};

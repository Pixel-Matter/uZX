#include "MidiToPsgPlugin.h"

namespace MoTool::uZX {

const char* MidiToPsgPlugin::xmlTypeName = "midiToPsg";

MidiToPsgPlugin::MidiToPsgPlugin(te::PluginCreationInfo info)
    : te::Plugin(info)
    , converter_(1, 3) // Default: channels 1-3
{
}

MidiToPsgPlugin::~MidiToPsgPlugin() {
}

void MidiToPsgPlugin::initialise(const te::PluginInitialisationInfo&) {
    updateConverterParams();
}

void MidiToPsgPlugin::deinitialise() {
}

void MidiToPsgPlugin::applyToBuffer(const te::PluginRenderContext& rc) noexcept {
    // Process MIDI input
    if (rc.bufferForMidiMessages != nullptr) {
        for (auto& m : *rc.bufferForMidiMessages) {
            DBG("in midi message " << m.getDescription());
            processMidiMessageWithSource(m);
        }

        // Get output messages from converter and add to buffer
        auto outputMessages = converter_.getOutputMessages();
        for (const auto& msg : outputMessages) {
            DBG("out midi message " << msg.getDescription());
            rc.bufferForMidiMessages->addMidiMessage(msg, 0);
        }
    }

    // This plugin is MIDI-only, no audio processing needed
}

void MidiToPsgPlugin::midiPanic() {
    converter_.clearOutput();
}

void MidiToPsgPlugin::reset() {
    converter_.clearOutput();
}

void MidiToPsgPlugin::restorePluginStateFromValueTree(const ValueTree& v) {
    staticParams.restoreFromTree(v);
    updateConverterParams();
}

std::unique_ptr<te::Plugin::EditorComponent> MidiToPsgPlugin::createEditor() {
    // Return nullptr for now - no GUI needed for basic functionality
    return nullptr;
}

void MidiToPsgPlugin::Params::initialise() {
    baseMidiChannelValue.referTo(IDs::midiToPsgBaseChannel, "Base MIDI channel", {1, 16, 1}, 1);
    numChannelsValue.referTo(IDs::midiToPsgNumChannels, "Number of channels", {1, 4, 1}, 3);
}

void MidiToPsgPlugin::Params::restoreFromTree(const juce::ValueTree& v) {
    te::copyPropertiesToCachedValues(v,
        baseMidiChannelValue.cachedValue,
        numChannelsValue.cachedValue
    );
}

void MidiToPsgPlugin::valueTreeChanged() {
    updateConverterParams();
}

void MidiToPsgPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    juce::ignoreUnused(v);

    if (id == IDs::midiToPsgBaseChannel || id == IDs::midiToPsgNumChannels) {
        updateConverterParams();
    }
}

void MidiToPsgPlugin::updateConverterParams() {
    converter_.setBaseChannel(staticParams.baseMidiChannelValue.get());
    converter_.setNumChannels(staticParams.numChannelsValue.get());
}

void MidiToPsgPlugin::setTuningSystem(TuningSystem* tuningSystem) {
    currentTuningSystem_ = tuningSystem;
    converter_.setTuningSystem(currentTuningSystem_);
}

void MidiToPsgPlugin::processMidiMessageWithSource(const te::MidiMessageWithSource& msg) {
    if (msg.isNoteOn()) {
        converter_.noteOn(msg.getChannel(), msg.getNoteNumber(), msg.getVelocity());
    }
    else if (msg.isNoteOff()) {
        converter_.noteOff(msg.getChannel(), msg.getNoteNumber());
    }
    else if (msg.isAftertouch()) {
        converter_.aftertouch(msg.getChannel(), msg.getAfterTouchValue());
    }
    else if (msg.isController()) {
        converter_.controlChange(msg.getChannel(), msg.getControllerNumber(), msg.getControllerValue());
    }
}

} // namespace MoTool::uZX
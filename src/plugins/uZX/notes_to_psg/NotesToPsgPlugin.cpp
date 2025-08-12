#include "NotesToPsgPlugin.h"

namespace MoTool::uZX {

const char* NotesToPsgPlugin::xmlTypeName = "midiToPsg";

NotesToPsgPlugin::NotesToPsgPlugin(te::PluginCreationInfo info)
    : te::Plugin(info)
    , transformer(1, 4) // Default: channels 1-4
{
}

NotesToPsgPlugin::~NotesToPsgPlugin() {
}

void NotesToPsgPlugin::initialise(const te::PluginInitialisationInfo&) {
    updateConverterParams();
}

void NotesToPsgPlugin::deinitialise() {
}

void NotesToPsgPlugin::applyToBuffer(const te::PluginRenderContext& rc) noexcept {
    // Process MIDI input
    if (rc.bufferForMidiMessages != nullptr) {
        for (auto& m : *rc.bufferForMidiMessages) {
            // DBG("in midi message " << m.getDescription());
            processMidiMessageWithSource(m);
        }

        // Get output messages from converter and add to buffer
        auto outputMessages = transformer.getOutputMessages();
        for (const auto& msg : outputMessages) {
            // DBG("out midi message " << msg.getDescription());
            rc.bufferForMidiMessages->addMidiMessage(msg, 0);
        }
    }

    // This plugin is MIDI-only, no audio processing needed
}

void NotesToPsgPlugin::midiPanic() {
    reset();
}

void NotesToPsgPlugin::reset() {
    transformer.clearOutput();
    transformer.initPSG();
}

void NotesToPsgPlugin::restorePluginStateFromValueTree(const ValueTree& v) {
    staticParams.restoreFromTree(v);
    updateConverterParams();
}

std::unique_ptr<te::Plugin::EditorComponent> NotesToPsgPlugin::createEditor() {
    // Return nullptr for now - no GUI needed for basic functionality
    return nullptr;
}

void NotesToPsgPlugin::Params::initialise() {
    baseMidiChannelValue.referTo(IDs::midiBase, "Base MIDI channel", {1, 16, 1}, 1);
    numChannelsValue.referTo(IDs::midiChans, "Number of channels", {1, 4, 1}, 4);
}

void NotesToPsgPlugin::Params::restoreFromTree(const juce::ValueTree& v) {
    te::copyPropertiesToCachedValues(v,
        baseMidiChannelValue.cachedValue,
        numChannelsValue.cachedValue
    );
}

void NotesToPsgPlugin::valueTreeChanged() {
    updateConverterParams();
}

void NotesToPsgPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    juce::ignoreUnused(v);

    if (id == IDs::midiBase || id == IDs::midiChans) {
        updateConverterParams();
    }
}

void NotesToPsgPlugin::updateConverterParams() {
    transformer.setBaseChannel(staticParams.baseMidiChannelValue.get());
    transformer.setNumChannels(staticParams.numChannelsValue.get());
    reset();
}

void NotesToPsgPlugin::setTuningSystem(TuningSystem* tuningSystem) {
    currentTuningSystem = tuningSystem;
    transformer.setTuningSystem(currentTuningSystem);
}

void NotesToPsgPlugin::processMidiMessageWithSource(const te::MidiMessageWithSource& msg) {
    // DBG("Processing MIDI message: " << msg.getDescription());
    // converter_.debugChannelStates();
    if (msg.isNoteOn()) {
        transformer.noteOn(msg.getChannel(), msg.getNoteNumber(), msg.getVelocity());
    } else if (msg.isNoteOff()) {
        transformer.noteOff(msg.getChannel(), msg.getNoteNumber());
    } else if (msg.isAftertouch()) {
        transformer.aftertouch(msg.getChannel(), msg.getAfterTouchValue());
    } else if (msg.isController()) {
        transformer.controlChange(msg.getChannel(), msg.getControllerNumber(), msg.getControllerValue());
    }
}

} // namespace MoTool::uZX
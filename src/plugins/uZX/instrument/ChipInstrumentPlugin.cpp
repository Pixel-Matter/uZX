#include "ChipInstrumentPlugin.h"

namespace MoTool::uZX {

namespace te = tracktion;

const char* ChipInstrumentPlugin::xmlTypeName = "uzxtrmnt";

ChipInstrumentPlugin::ChipInstrumentPlugin(te::PluginCreationInfo info) : Plugin(info) {
    levelMeasurer.addClient(*this);

    // TODO add all params from instrument
    // auto um = getUndoManager();
    // paramValue.referTo(state, paramDef.Id, um, paramDef.defaultValue)
    // addedParam = addParam(paramDef, paramValue);
    // addedParam->attachToCurrentValue(paramValue);

    valueTreePropertyChanged(state, te::IDs::voiceMode);
    valueTreePropertyChanged(state, te::IDs::mpe);

    // TODO
    // instrument.loadModMatrix();
}

ChipInstrumentPlugin::~ChipInstrumentPlugin() {
    notifyListenersOfDeletion();

    // TODO detach all instrument parameters
    // param.detachFromCurrentValue()
}

bool ChipInstrumentPlugin::hasNameForMidiBank(int /* num */, juce::String& /* name */) {
    return false;
}

bool ChipInstrumentPlugin::hasNameForMidiProgram(int /*programNum*/, int /*bank*/, juce::String& /* name */) {
    return false;
}

bool ChipInstrumentPlugin::hasNameForMidiNoteNumber(int /*note*/, int /*midiChannel*/, juce::String& /*name*/) {
    // TODO implement for drum pads or for microtonal scales
    return false;
}

te::AutomatableParameter* ChipInstrumentPlugin::addParam(const String& paramID,
                                                         const String& name,
                                                         NormalisableRange<float> valueRange,
                                                         String label) {
    auto p = Plugin::addParam(paramID, name, valueRange);

    if (label.isNotEmpty())
        paramLabels[paramID] = label;

    return p;
}

float ChipInstrumentPlugin::getLevel(int channel) {
    auto& peak = levels[channel];

    auto elapsedMilliseconds = std::max(0, int(Time::getApproximateMillisecondCounter() - peak.time) - 50);
    float currentLevel = peak.dB - (48.0f * (float) elapsedMilliseconds / 1000.0f);

    auto latest = getAndClearAudioLevel(channel);

    if (latest.dB > currentLevel) {
        peak = latest;
        return jlimit(-100.0f, 0.0f, peak.dB);
    }

    return jlimit(-100.0f, 0.0f, currentLevel);
}

void ChipInstrumentPlugin::valueTreeChanged() {
    Plugin::valueTreeChanged();
}

void ChipInstrumentPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& i) {
    Plugin::valueTreePropertyChanged(v, i);

    // TODO delegate to instrument's valueTreePropertyChanged (so it should have
    // state reference)
}

void ChipInstrumentPlugin::valueTreeChildAdded(ValueTree& v, ValueTree& c) {
    Plugin::valueTreeChildAdded(v, c);

    // TODO delegate to instrument's valueTreeChildAdded
}

void ChipInstrumentPlugin::valueTreeChildRemoved(ValueTree& v, ValueTree& c, int i) {
    Plugin::valueTreeChildRemoved(v, c, i);

    // TODO delegate to instrument's valueTreeChildRemoved
}

void ChipInstrumentPlugin::flushPluginStateToValueTree() {
    ScopedValueSetter<bool> svs(flushingState, true);

    // TODO delegate
    // instrument.flushStateToValueTree(getUndoManager());
    Plugin::flushPluginStateToValueTree();  // Add any parameter values that are
                                            // being modified
}

void ChipInstrumentPlugin::initialise(const te::PluginInitialisationInfo& /*info*/) {
    // setCurrentPlaybackSampleRate(info.sampleRate);

    instrument.reset();
}

void ChipInstrumentPlugin::deinitialise() {}

//==============================================================================
void ChipInstrumentPlugin::reset() {
    instrument.reset();
}

void ChipInstrumentPlugin::midiPanic() {
    instrument.reset();
}

void ChipInstrumentPlugin::applyToBuffer(const te::PluginRenderContext& fc) {
    if (fc.bufferForMidiMessages == nullptr) {
        return;
    }

    SCOPED_REALTIME_CHECK

    // find the tempo
    currentPos.set(fc.editTime.getStart());
    instrument.setCurrentTempo(float(currentPos.getTempo()));

    ScopedLock sl(instrument.getVoiceLock());
    // Handle all notes off first
    if (fc.bufferForMidiMessages->isAllNotesOff) {
        instrument.turnOffAllVoices(true);
    }
    // TODO iterate buffer by blocks designated by midi message timestamps
    // TODO see 4OSC plugin and MPESynth

    // te::MidiMessageArray midiOut;
    // instrument.applyToBuffer(&fc.bufferForMidiMessages, midiOut);  // also update modulation phases and update params
    // // TODO copy midi to output buffer
}

//==============================================================================
void ChipInstrumentPlugin::restorePluginStateFromValueTree(const ValueTree& v) {
    instrument.restoreStateFromValueTree(v);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

// Array<float> ChipInstrumentPlugin::getLiveModulationPositions(te::AutomatableParameter::Ptr param) {
//     // return instrument.getLiveModulationPositions(param);
// }

// bool ChipInstrumentPlugin::isModulated(te::AutomatableParameter::Ptr param) {
//     // return instrument.isModulated(param);
// }

// Array<ChipInstrumentPlugin::ModSource> ChipInstrumentPlugin::getModulationSources(te::AutomatableParameter::Ptr param) {
//     // return instrument.getModulationSources(param);
// }

// float ChipInstrumentPlugin::getModulationDepth(ChipInstrumentPlugin::ModSource src,
//                                                te::AutomatableParameter::Ptr param) {
//     // return instrument.getModulationDepth(src, param);
// }

// void ChipInstrumentPlugin::setModulationDepth(ChipInstrumentPlugin::ModSource src,
//                                               te::AutomatableParameter::Ptr param,
//                                               float depth) {
//     // instrument.setModulationDepth(src, param, depth);
// }

// void ChipInstrumentPlugin::clearModulation(ModSource src, te::AutomatableParameter::Ptr param) {
//     // instrument.clearModulation(src, param);
// }

// float ChipInstrumentPlugin::paramValue(te::AutomatableParameter::Ptr param) {
//     jassert(param != nullptr);
//     if (param == nullptr)
//         return 0.0f;

//     auto smoothItr = smoothers.find(param.get());
//     if (smoothItr == smoothers.end())
//         return param->getCurrentValue();

//     float val = param->getCurrentNormalisedValue();
//     smoothItr->second.setValue(val);
//     return
//     param->valueRange.convertFrom0to1(smoothItr->second.getCurrentValue());
// }

}  // namespace MoTool::uZX
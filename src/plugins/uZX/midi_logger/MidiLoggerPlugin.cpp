#include "MidiLoggerPlugin.h"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace MoTool::uZX {

//==============================================================================
MidiLoggerEffect::MidiLoggerEffect() {
    outputStream.store(&std::cout, std::memory_order_release);
}

void MidiLoggerEffect::setOutputStream(std::ostream& stream) noexcept {
    outputStream.store(&stream, std::memory_order_release);
}

std::ostream* MidiLoggerEffect::getOutputStream() const noexcept {
    return outputStream.load(std::memory_order_acquire);
}

void MidiLoggerEffect::setTag(const juce::String& newTag) {
    const juce::SpinLock::ScopedLockType sl(tagLock);
    tag = newTag;
}

void MidiLoggerEffect::operator()(MidiBufferContext& context) {
    if (auto* stream = getOutputStream()) {
        juce::String currentTag;
        {
            const juce::SpinLock::ScopedLockType sl(tagLock);
            currentTag = tag;
        }

        for (const auto& midi : context.buffer) {
            std::ostringstream line;

            if (currentTag.isNotEmpty()) {
                line << "[" << currentTag.toStdString() << "] ";
            }

            line << "[ch " << midi.getChannel() << " @ "
                 << std::fixed << std::setprecision(6)
                 << (context.playPosition.inSeconds() + midi.getTimeStamp())
                 << "s] "
                 << midi.getDescription().toStdString();

            (*stream) << line.str() << '\n';
        }

        stream->flush();
    }
}

//==============================================================================
const char* MidiLoggerPlugin::xmlTypeName = "uzxmidilogger";

MidiLoggerPlugin::MidiLoggerPlugin(tracktion::PluginCreationInfo info)
    : MidiFxPluginBase<MidiLoggerEffect>(info, logger) {
    logTag.referTo(state, IDs::logTag, getUndoManager(), {});
    midiEffect.setTag(logTag.get());
}

void MidiLoggerPlugin::setOutputStream(std::ostream& stream) noexcept {
    midiEffect.setOutputStream(stream);
}

void MidiLoggerPlugin::setLogTag(const juce::String& tagValue) {
    logTag = tagValue;
    midiEffect.setTag(tagValue);
}

juce::String MidiLoggerPlugin::getLogTag() const {
    return logTag.get();
}

void MidiLoggerPlugin::restorePluginStateFromValueTree(const juce::ValueTree& v) {
    PluginBase::restorePluginStateFromValueTree(v);
    logTag.forceUpdateOfCachedValue();
    midiEffect.setTag(logTag.get());
}

void MidiLoggerPlugin::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& id) {
    PluginBase::valueTreePropertyChanged(v, id);

    if (v == state && id == IDs::logTag) {
        logTag.forceUpdateOfCachedValue();
        midiEffect.setTag(logTag.get());
    }
}

}  // namespace MoTool::uZX

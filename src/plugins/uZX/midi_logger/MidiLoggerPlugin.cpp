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

void MidiLoggerEffect::operator()(MidiBufferContext& context) {
    if (auto* stream = getOutputStream()) {
        for (const auto& midi : context.buffer) {
            std::ostringstream line;
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
}

void MidiLoggerPlugin::setOutputStream(std::ostream& stream) noexcept {
    midiEffect.setOutputStream(stream);
}

}  // namespace MoTool::uZX


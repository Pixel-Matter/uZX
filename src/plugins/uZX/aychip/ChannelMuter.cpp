#include "ChannelMuter.h"
#include "aychip.h"

namespace MoTool::uZX {

void ChannelMuter::setupLinkedToggleBehavior() {
    // Listen to channel parameter changes to implement linked behavior
    channelA.addListener(this);
    channelB.addListener(this);
    channelC.addListener(this);
}

ChannelMuter::~ChannelMuter() {
    // Clean up listeners
    channelA.removeListener(this);
    channelB.removeListener(this);
    channelC.removeListener(this);
}

void ChannelMuter::applyToRegsFrame(PsgRegsFrame& regs) const noexcept {
    for (size_t chan = 0; chan < 3; ++chan) {
        if (!getChannelEfecctivelyEnabled(chan)) {
            // When channel is disabled or all TNE are off, set volume to 0
            // to prevent audible clicks (tone-off sets signal high)
            regs.setVolumeAndEnvMod(chan, 0, false);
        } else {
            // Channel is enabled, but check individual effects
            if (!getToneEnabled(chan)) {
                regs.setToneOn(chan, false);
            }
            if (!getNoiseEnabled(chan)) {
                regs.setNoiseOn(chan, false);
            }
            if (!getEnvelopeEnabled(chan)) {
                regs.setEnvMod(chan, false);
            }
        }
    }
}

void ChannelMuter::applyToChip(AYInterface& chip) const noexcept {
    for (size_t chan = 0; chan < 3; ++chan) {
        const bool channelEnabled = getChannelEfecctivelyEnabled(chan);
        const bool toneEnabled = getToneEnabled(chan);
        const bool noiseEnabled = getNoiseEnabled(chan);
        const bool envelopeEnabled = getEnvelopeEnabled(chan);

        chip.applyChannelMute(static_cast<int>(chan), toneEnabled, noiseEnabled, envelopeEnabled, channelEnabled);
    }
}

void ChannelMuter::valueChanged(Value&) {
}

bool ChannelMuter::getChannelEnabled(size_t chan) const noexcept {
    switch (chan) {
        case 0: return channelA.getLiveValue();
        case 1: return channelB.getLiveValue();
        case 2: return channelC.getLiveValue();
        default: return false;
    }
}

bool ChannelMuter::getChannelEfecctivelyEnabled(size_t c) const noexcept {
    return getChannelEnabled(c) && (getToneEnabled(c) || getNoiseEnabled(c) || getEnvelopeEnabled(c));
}

bool ChannelMuter::getToneEnabled(size_t chan) const noexcept {
    switch (chan) {
        case 0: return toneA.getLiveValue();
        case 1: return toneB.getLiveValue();
        case 2: return toneC.getLiveValue();
        default: return false;
    }
}

bool ChannelMuter::getNoiseEnabled(size_t chan) const noexcept {
    switch (chan) {
        case 0: return noiseA.getLiveValue();
        case 1: return noiseB.getLiveValue();
        case 2: return noiseC.getLiveValue();
        default: return false;
    }
}

bool ChannelMuter::getEnvelopeEnabled(size_t chan) const noexcept {
    switch (chan) {
        case 0: return envelopeA.getLiveValue();
        case 1: return envelopeB.getLiveValue();
        case 2: return envelopeC.getLiveValue();
        default: return false;
    }
}

} // namespace MoTool::uZX

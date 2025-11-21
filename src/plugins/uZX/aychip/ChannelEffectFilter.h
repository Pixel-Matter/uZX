#pragma once

#include "../../../formats/psg/PsgData.h"

namespace MoTool::uZX {

//==============================================================================
/**
 * Filters PSG register data to enable/disable channels and effects
 *
 * This class sits between the MIDI parameter reader and the chip registers,
 * allowing selective muting of channels and their individual effects (tone,
 * noise, envelope).
 */
class ChannelEffectFilter {
public:
    ChannelEffectFilter() = default;

    /**
     * Apply channel and effect filters to the register frame
     *
     * @param regs The register frame to filter
     */
    void apply(PsgRegsFrame& regs) const noexcept {
        for (size_t chan = 0; chan < 3; ++chan) {
            if (!channelEnabled_[chan]) {
                // Disable entire channel by disabling all effects
                // Note: No need to set volume to 0 - when both tone and noise
                // are disabled via mixer, the channel is silent regardless of volume
                regs.setToneOn(chan, false);
                regs.setNoiseOn(chan, false);
                regs.setEnvMod(chan, false);
            } else {
                // Channel is enabled, but check individual effects
                if (!toneEnabled_[chan]) {
                    regs.setToneOn(chan, false);
                }
                if (!noiseEnabled_[chan]) {
                    regs.setNoiseOn(chan, false);
                }
                if (!envelopeEnabled_[chan]) {
                    regs.setEnvMod(chan, false);
                }
            }
        }
    }

    // Channel enable/disable
    void setChannelEnabled(size_t chan, bool enabled) noexcept {
        if (chan < 3) {
            channelEnabled_[chan] = enabled;
        }
    }

    bool isChannelEnabled(size_t chan) const noexcept {
        return chan < 3 ? channelEnabled_[chan] : false;
    }

    // Tone enable/disable
    void setToneEnabled(size_t chan, bool enabled) noexcept {
        if (chan < 3) {
            toneEnabled_[chan] = enabled;
        }
    }

    bool isToneEnabled(size_t chan) const noexcept {
        return chan < 3 ? toneEnabled_[chan] : false;
    }

    // Noise enable/disable
    void setNoiseEnabled(size_t chan, bool enabled) noexcept {
        if (chan < 3) {
            noiseEnabled_[chan] = enabled;
        }
    }

    bool isNoiseEnabled(size_t chan) const noexcept {
        return chan < 3 ? noiseEnabled_[chan] : false;
    }

    // Envelope enable/disable
    void setEnvelopeEnabled(size_t chan, bool enabled) noexcept {
        if (chan < 3) {
            envelopeEnabled_[chan] = enabled;
        }
    }

    bool isEnvelopeEnabled(size_t chan) const noexcept {
        return chan < 3 ? envelopeEnabled_[chan] : false;
    }

private:
    std::array<bool, 3> channelEnabled_  = {true, true, true};
    std::array<bool, 3> toneEnabled_     = {true, true, true};
    std::array<bool, 3> noiseEnabled_    = {true, true, true};
    std::array<bool, 3> envelopeEnabled_ = {true, true, true};
};

} // namespace MoTool::uZX

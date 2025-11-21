#pragma once

#include "../../../formats/psg/PsgData.h"
#include "../../../controllers/Parameters.h"

namespace MoTool::uZX {

namespace IDs {
    #define DECLARE_ID(name)  inline const juce::Identifier name(#name);
    DECLARE_ID(channelA)
    DECLARE_ID(channelB)
    DECLARE_ID(channelC)
    DECLARE_ID(toneA)
    DECLARE_ID(toneB)
    DECLARE_ID(toneC)
    DECLARE_ID(noiseA)
    DECLARE_ID(noiseB)
    DECLARE_ID(noiseC)
    DECLARE_ID(envelopeA)
    DECLARE_ID(envelopeB)
    DECLARE_ID(envelopeC)
    #undef DECLARE_ID
}  // namespace IDs

//==============================================================================
/**
 * Filters PSG register data to enable/disable channels and effects
 *
 * This class sits between the MIDI parameter reader and the chip registers,
 * allowing selective muting of channels and their individual effects (tone,
 * noise, envelope).
 *
 * Implements linked toggle behavior: when a channel is disabled, all its
 * effects (tone, noise, envelope) are automatically disabled as well.
 */
class ChannelMuter : public ParamsBase<ChannelMuter>,
                     private Value::Listener {
public:
    using ParamsBase<ChannelMuter>::ParamsBase;

    template<typename Visitor>
    void visit(Visitor&& visitor) {
        visitor(channelA);
        visitor(channelB);
        visitor(channelC);
        visitor(toneA);
        visitor(toneB);
        visitor(toneC);
        visitor(noiseA);
        visitor(noiseB);
        visitor(noiseC);
        visitor(envelopeA);
        visitor(envelopeB);
        visitor(envelopeC);
    }

    // Channel enables
    ParameterValue<bool> channelA   {{"channelA", IDs::channelA, "A", "Channel A enable", true}};
    ParameterValue<bool> channelB   {{"channelB", IDs::channelB, "B", "Channel B enable", true}};
    ParameterValue<bool> channelC   {{"channelC", IDs::channelC, "C", "Channel C enable", true}};

    // Tone enables
    ParameterValue<bool> toneA      {{"toneA", IDs::toneA, "T", "Tone A enable", true}};
    ParameterValue<bool> toneB      {{"toneB", IDs::toneB, "T", "Tone B enable", true}};
    ParameterValue<bool> toneC      {{"toneC", IDs::toneC, "T", "Tone C enable", true}};

    // Noise enables
    ParameterValue<bool> noiseA     {{"noiseA", IDs::noiseA, "N", "Noise A enable", true}};
    ParameterValue<bool> noiseB     {{"noiseB", IDs::noiseB, "N", "Noise B enable", true}};
    ParameterValue<bool> noiseC     {{"noiseC", IDs::noiseC, "N", "Noise C enable", true}};

    // Envelope enables
    ParameterValue<bool> envelopeA  {{"envelopeA", IDs::envelopeA, "E", "Envelope A enable", true}};
    ParameterValue<bool> envelopeB  {{"envelopeB", IDs::envelopeB, "E", "Envelope B enable", true}};
    ParameterValue<bool> envelopeC  {{"envelopeC", IDs::envelopeC, "E", "Envelope C enable", true}};

    void setupLinkedToggleBehavior() {
        // Listen to channel parameter changes to implement linked behavior
        channelA.addListener(this);
        channelB.addListener(this);
        channelC.addListener(this);
    }

    ~ChannelMuter() override {
        // Clean up listeners
        channelA.removeListener(this);
        channelB.removeListener(this);
        channelC.removeListener(this);
    }

    /**
     * Apply channel and effect filters to the register frame
     *
     * @param regs The register frame to filter
     */
    void apply(PsgRegsFrame& regs) const noexcept {
        for (size_t chan = 0; chan < 3; ++chan) {
            bool channelEnabled = getChannelEnabled(chan);

            if (!channelEnabled) {
                regs.setToneOn(chan, false);
                regs.setNoiseOn(chan, false);
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

private:
    // Value::Listener - implement linked toggle behavior
    void valueChanged(Value& value) override {
        // Find which channel changed and update its TNE buttons
        // if (value.refersToSameSourceAs(channelA.getPropertyAsValue())) {
        //     bool enabled = channelA.getStoredValue();
        //     toneA.setStoredValue(enabled);
        //     noiseA.setStoredValue(enabled);
        //     envelopeA.setStoredValue(enabled);
        // }
        // else if (value.refersToSameSourceAs(channelB.getPropertyAsValue())) {
        //     bool enabled = channelB.getStoredValue();
        //     toneB.setStoredValue(enabled);
        //     noiseB.setStoredValue(enabled);
        //     envelopeB.setStoredValue(enabled);
        // }
        // else if (value.refersToSameSourceAs(channelC.getPropertyAsValue())) {
        //     bool enabled = channelC.getStoredValue();
        //     toneC.setStoredValue(enabled);
        //     noiseC.setStoredValue(enabled);
        //     envelopeC.setStoredValue(enabled);
        // }
    }

    bool getChannelEnabled(size_t chan) const noexcept {
        switch (chan) {
            case 0: return channelA.getLiveValue();
            case 1: return channelB.getLiveValue();
            case 2: return channelC.getLiveValue();
            default: return false;
        }
    }

    bool getToneEnabled(size_t chan) const noexcept {
        switch (chan) {
            case 0: return toneA.getLiveValue();
            case 1: return toneB.getLiveValue();
            case 2: return toneC.getLiveValue();
            default: return false;
        }
    }

    bool getNoiseEnabled(size_t chan) const noexcept {
        switch (chan) {
            case 0: return noiseA.getLiveValue();
            case 1: return noiseB.getLiveValue();
            case 2: return noiseC.getLiveValue();
            default: return false;
        }
    }

    bool getEnvelopeEnabled(size_t chan) const noexcept {
        switch (chan) {
            case 0: return envelopeA.getLiveValue();
            case 1: return envelopeB.getLiveValue();
            case 2: return envelopeC.getLiveValue();
            default: return false;
        }
    }
};

} // namespace MoTool::uZX

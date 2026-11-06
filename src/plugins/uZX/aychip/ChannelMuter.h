#pragma once

#include <JuceHeader.h>

#include <atomic>

#include "../../../formats/psg/PsgData.h"
#include "../../../controllers/Parameters.h"

namespace MoTool::uZX {

namespace IDs {
    #define DECLARE_ID(name)  inline const Identifier name(#name);
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
    DECLARE_ID(soloA)
    DECLARE_ID(soloB)
    DECLARE_ID(soloC)
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
 *
 * Solo is apart from mute: while any channel is soloed, the others are silent
 * and keep their toggles; a soloed channel plays even when muted.
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

    // Not visited: solo is not automatable
    ParameterValue<bool> soloA      {{"soloA", IDs::soloA, "S", "Channel A solo", false}};
    ParameterValue<bool> soloB      {{"soloB", IDs::soloB, "S", "Channel B solo", false}};
    ParameterValue<bool> soloC      {{"soloC", IDs::soloC, "S", "Channel C solo", false}};

    /** Refers the visited parameters to `v` with `um`, and the solos without it. */
    void referTo(ValueTree& v, UndoManager* um);

    void setupLinkedToggleBehavior();
    ~ChannelMuter() override;

    /** The on/off toggle of channel A, B or C (0, 1, 2). */
    ParameterValue<bool>& getChannelParam(size_t chan) noexcept;
    /** The solo of channel A, B or C (0, 1, 2). */
    ParameterValue<bool>& getSoloParam(size_t chan) noexcept;
    bool hasSolo() const;

    /**
     * Apply channel and effect filters to the register frame
     *
     * @param regs The register frame to filter
     */
    void applyToRegsFrame(PsgRegsFrame& regs) const noexcept;

    /**
     * Apply channel and effect filters directly to the chip registers
     *
     * @param chip The chip to apply filters to
     */
    void applyToChip(class AYInterface& chip) const noexcept;

private:
    void valueChanged(Value&) override;
    void updateSoloMask();
    bool getChannelEnabled(size_t chan) const noexcept;
    bool getChannelEfecctivelyEnabled(size_t c) const noexcept;
    bool getToneEnabled(size_t chan) const noexcept;
    bool getNoiseEnabled(size_t chan) const noexcept;
    bool getEnvelopeEnabled(size_t chan) const noexcept;

    std::atomic<uint8_t> soloMask_ {0};  // bit per soloed channel, for the audio thread
};

} // namespace MoTool::uZX

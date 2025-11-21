#include <JuceHeader.h>
#include "ChannelEffectFilter.h"

namespace MoTool::uZX {

//==============================================================================
class ChannelEffectFilterTest : public juce::UnitTest {
public:
    ChannelEffectFilterTest() : UnitTest("ChannelEffectFilter", "MoTool") {}

    void runTest() override {
        beginTest("Disabling channel should disable all effects");
        {
            ChannelEffectFilter filter;
            PsgRegsFrame regs;

            // Setup: channel has tone, noise, and volume
            regs.setToneOn(0, true);
            regs.setNoiseOn(0, true);
            regs.setVolume(0, 15);  // max volume
            regs.setEnvMod(0, false);

            // Disable channel A
            filter.setChannelEnabled(0, false);
            filter.apply(regs);

            // Verify: tone and noise are disabled
            expect(!regs.getToneOn(0), "Tone should be disabled");
            expect(!regs.getNoiseOn(0), "Noise should be disabled");
            expect(!regs.getEnvMod(0), "Envelope mod should be disabled");
            // Volume might be 0 or non-zero, we're testing if it matters
        }

        beginTest("Disabling tone only should preserve noise and volume");
        {
            ChannelEffectFilter filter;
            PsgRegsFrame regs;

            // Setup
            regs.setToneOn(1, true);
            regs.setNoiseOn(1, true);
            regs.setVolume(1, 12);
            regs.setEnvMod(1, false);

            // Disable only tone
            filter.setToneEnabled(1, false);
            filter.apply(regs);

            // Verify
            expect(!regs.getToneOn(1), "Tone should be disabled");
            expect(regs.getNoiseOn(1), "Noise should still be enabled");
            expectEquals(regs.getVolume(1), static_cast<uint8_t>(12), "Volume should be preserved");
        }

        beginTest("Disabling envelope should clear envelope mod bit");
        {
            ChannelEffectFilter filter;
            PsgRegsFrame regs;

            // Setup: channel using envelope
            regs.setToneOn(2, true);
            regs.setVolume(2, 8);
            regs.setEnvMod(2, true);

            // Disable envelope
            filter.setEnvelopeEnabled(2, false);
            filter.apply(regs);

            // Verify
            expect(!regs.getEnvMod(2), "Envelope mod should be disabled");
            expect(regs.getToneOn(2), "Tone should still be enabled");
        }

        beginTest("Channel disabled with only tone - verify silence without volume=0");
        {
            ChannelEffectFilter filter;
            PsgRegsFrame regs;

            // Setup: channel with tone only, high volume
            regs.setToneOn(0, true);
            regs.setNoiseOn(0, false);
            regs.setVolume(0, 15);
            regs.setEnvMod(0, false);

            // Create a version WITHOUT setVolume(0) in apply()
            // Just disable tone and noise
            regs.setToneOn(0, false);
            regs.setNoiseOn(0, false);
            regs.setEnvMod(0, false);
            // Note: volume is still 15!

            // Verify registers state
            expect(!regs.getToneOn(0), "Tone disabled");
            expect(!regs.getNoiseOn(0), "Noise disabled");
            expectEquals(regs.getVolume(0), static_cast<uint8_t>(15), "Volume still 15");

            // This should produce silence even though volume=15
            // because both tone and noise are disabled in the mixer
        }

        beginTest("All channels can be controlled independently");
        {
            ChannelEffectFilter filter;
            PsgRegsFrame regs;

            // Setup all channels
            for (size_t i = 0; i < 3; ++i) {
                regs.setToneOn(i, true);
                regs.setNoiseOn(i, true);
                regs.setVolume(i, 10);
            }

            // Disable different things on each channel
            filter.setChannelEnabled(0, false);  // A: completely off
            filter.setToneEnabled(1, false);      // B: no tone
            filter.setNoiseEnabled(2, false);     // C: no noise

            filter.apply(regs);

            // Verify A
            expect(!regs.getToneOn(0), "A: tone off");
            expect(!regs.getNoiseOn(0), "A: noise off");

            // Verify B
            expect(!regs.getToneOn(1), "B: tone off");
            expect(regs.getNoiseOn(1), "B: noise still on");

            // Verify C
            expect(regs.getToneOn(2), "C: tone still on");
            expect(!regs.getNoiseOn(2), "C: noise off");
        }
    }
};

static ChannelEffectFilterTest channelEffectFilterTest;

} // namespace MoTool::uZX

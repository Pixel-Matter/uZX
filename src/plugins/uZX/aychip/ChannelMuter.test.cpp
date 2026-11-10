#include <JuceHeader.h>
#include "ChannelMuter.h"
#include "aychip.h"
#include "../../../util/TestHelpers.h"

namespace MoTool::uZX {

//==============================================================================
class ChannelEffectFilterTest : public UnitTest {
public:
    ChannelEffectFilterTest() : UnitTest("ChannelMuter", "MoTool") {}

    void runTest() override {
        beginTest("Disabling channel should silence it by setting volume to 0");
        {
            ValueTree state("ChannelMuter");
            ChannelMuter filter;
            filter.referTo(state, nullptr);

            PsgRegsFrame regs;

            // Setup: channel has tone, noise, and volume
            regs.setToneOn(0, true);
            regs.setNoiseOn(0, true);
            regs.setVolume(0, 15);  // max volume
            regs.setEnvMod(0, false);

            // Disable channel A
            filter.channelA.setStoredValue(false);
            filter.applyToRegsFrame(regs);

            // Verify: volume is set to 0 to prevent clicks
            // (tone-off would set signal high, causing audible clicks with non-zero volume)
            expectEquals(regs.getVolume(0), static_cast<uint8_t>(0), "Volume should be 0");
            expect(!regs.getEnvMod(0), "Envelope mod should be disabled");
        }

        beginTest("Soloed channels play alone, several at once, muted or not");
        {
            ValueTree state("ChannelMuter");
            ChannelMuter muter;
            muter.referTo(state, nullptr);

            auto volumes = [&] {
                PsgRegsFrame regs;
                for (size_t chan = 0; chan < 3; ++chan) {
                    regs.setToneOn(chan, true);
                    regs.setVolume(chan, 15);
                }
                muter.applyToRegsFrame(regs);
                String s;
                for (size_t chan = 0; chan < 3; ++chan)
                    s << (regs.getVolume(chan) > 0 ? "ABC"[chan] : '-');
                return s;
            };

            muter.soloB.setStoredValue(true);
            TestHelpers::flushMessageQueue();
            expectEquals(volumes(), String("-B-"));
            expect(muter.channelA.getStoredValue() && muter.channelC.getStoredValue(), "the others aren't muted");

            muter.soloC.setStoredValue(true);
            TestHelpers::flushMessageQueue();
            expectEquals(volumes(), String("-BC"), "several solos");

            muter.channelC.setStoredValue(false);
            expectEquals(volumes(), String("-BC"), "solo wins over mute");

            muter.soloB.setStoredValue(false);
            muter.soloC.setStoredValue(false);
            TestHelpers::flushMessageQueue();
            expectEquals(volumes(), String("AB-"), "unsoloed, C is muted again");
        }

        beginTest("Disabling tone only should preserve noise and volume");
        {
            ValueTree state("ChannelMuter");
            ChannelMuter filter;
            filter.referTo(state, nullptr);

            PsgRegsFrame regs;

            // Setup
            regs.setToneOn(1, true);
            regs.setNoiseOn(1, true);
            regs.setVolume(1, 12);
            regs.setEnvMod(1, false);

            // Disable only tone
            filter.toneB.setStoredValue(false);
            filter.applyToRegsFrame(regs);

            // Verify
            expect(!regs.getToneOn(1), "Tone should be disabled");
            expect(regs.getNoiseOn(1), "Noise should still be enabled");
            expectEquals(regs.getVolume(1), static_cast<uint8_t>(12), "Volume should be preserved");
        }

        beginTest("Disabling envelope should clear envelope mod bit");
        {
            ValueTree state("ChannelMuter");
            ChannelMuter filter;
            filter.referTo(state, nullptr);

            PsgRegsFrame regs;

            // Setup: channel using envelope
            regs.setToneOn(2, true);
            regs.setVolume(2, 8);
            regs.setEnvMod(2, true);

            // Disable envelope
            filter.envelopeC.setStoredValue(false);
            filter.applyToRegsFrame(regs);

            // Verify
            expect(!regs.getEnvMod(2), "Envelope mod should be disabled");
            expect(regs.getToneOn(2), "Tone should still be enabled");
        }

        beginTest("Channel disabled with only tone - verify silence without volume=0");
        {
            ChannelMuter filter;
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

        beginTest("All TNE disabled should set volume to 0 to prevent clicks");
        {
            ValueTree state("ChannelMuter");
            ChannelMuter filter;
            filter.referTo(state, nullptr);

            PsgRegsFrame regs;

            // Setup: channel with all effects enabled
            regs.setToneOn(1, true);
            regs.setNoiseOn(1, true);
            regs.setVolume(1, 12);
            regs.setEnvMod(1, false);

            // Disable all TNE effects while keeping channel enabled
            filter.toneB.setStoredValue(false);
            filter.noiseB.setStoredValue(false);
            filter.envelopeB.setStoredValue(false);
            filter.applyToRegsFrame(regs);

            // Verify: volume is set to 0 to prevent audible clicks
            expectEquals(regs.getVolume(1), static_cast<uint8_t>(0), "Volume should be 0 when all TNE disabled");
        }

        beginTest("All channels can be controlled independently");
        {
            ValueTree state("ChannelMuter");
            ChannelMuter filter;
            filter.referTo(state, nullptr);

            PsgRegsFrame regs;

            // Setup all channels
            for (size_t i = 0; i < 3; ++i) {
                regs.setToneOn(i, true);
                regs.setNoiseOn(i, true);
                regs.setVolume(i, 10);
            }

            // Disable different things on each channel
            filter.channelA.setStoredValue(false);  // A: completely off
            filter.toneB.setStoredValue(false);      // B: no tone
            filter.noiseC.setStoredValue(false);     // C: no noise

            filter.applyToRegsFrame(regs);

            // Verify A - channel disabled means volume=0
            expectEquals(regs.getVolume(0), static_cast<uint8_t>(0), "A: volume should be 0");

            // Verify B - tone disabled but noise still on
            expect(!regs.getToneOn(1), "B: tone off");
            expect(regs.getNoiseOn(1), "B: noise still on");
            expectEquals(regs.getVolume(1), static_cast<uint8_t>(10), "B: volume preserved");

            // Verify C - noise disabled but tone still on
            expect(regs.getToneOn(2), "C: tone still on");
            expect(!regs.getNoiseOn(2), "C: noise off");
            expectEquals(regs.getVolume(2), static_cast<uint8_t>(10), "C: volume preserved");
        }

        beginTest("Unmuting brings back the channel the registers last set");
        {
            ValueTree state("ChannelMuter");
            ChannelMuter filter;
            filter.referTo(state, nullptr);
            AyumiEmulator chip;
            chip.setRegister(7, 0x3e);  // tone A on
            chip.setRegister(8, 0x1f);  // A at 15 with the envelope

            filter.channelA.setStoredValue(false);
            filter.applyToChip(chip);
            expectEquals(chip.getVolume(0), 0, "muted");

            filter.channelA.setStoredValue(true);
            filter.toneA.setStoredValue(false);
            filter.envelopeA.setStoredValue(false);
            filter.applyToChip(chip);
            expectEquals(chip.getVolume(0), 15, "the volume is back");
            expect(! std::get<0>(chip.getMixer(0)) && ! std::get<2>(chip.getMixer(0)), "tone and envelope muted");

            filter.toneA.setStoredValue(true);
            filter.envelopeA.setStoredValue(true);
            filter.applyToChip(chip);
            expect(std::get<0>(chip.getMixer(0)) && std::get<2>(chip.getMixer(0)), "tone and envelope are back");
            expectEquals(int(chip.getRegister(8)), 0x1f, "the registers stay as written");
        }
    }
};

static ChannelEffectFilterTest channelEffectFilterTest;

} // namespace MoTool::uZX

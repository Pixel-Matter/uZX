#include <JuceHeader.h>

#include <cmath>

#include "../formats/psg/PsgData.h"
#include "PsgClip.h"
#include "PsgSoundingStates.h"

namespace MoTool {

/** PsgSoundingStates: what the state of each stretch of a clip holds. */
class PsgSoundingStatesTest : public UnitTest
{
public:
    PsgSoundingStatesTest() : UnitTest("PsgSoundingStates", "MoTool") {}

    void runTest() override {
        using R = uZX::PsgRegType;
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto insert = [&edit](uZX::PsgData data) {
            const auto length = te::TimeDuration::fromSeconds(data.getLengthSeconds());
            return PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data, {{te::TimePosition(), length}, {}}, "t");
        };

        // A at period 400 in full, B an octave above at volume 8 with noise; held for three frames, then A
        // goes on the envelope with its tone off
        const uZX::PsgData data {
            {{{R::Mixer, 0x28}, {R::VolumeA, 15}, {R::VolumeB, 8}, {R::NoisePeriod, 7},
              {R::TonePeriodFineA, 400 & 0xff}, {R::TonePeriodCoarseA, 400 >> 8}, {R::TonePeriodFineB, 200}},
             {},
             {},
             {{R::Mixer, 0x29}, {R::VolumeA, 0x10}, {R::EnvelopePeriodFine, 50}, {R::EnvelopeShape, 0x0C}}},
            {50.0, 1}
        };
        auto clip = insert(data);
        const auto& frames = clip->getPsg().getFrames();
        PsgSoundingStates states;
        int storedFrame = -1;
        clip->forEachSoundingState(TimerSpan::toTheEnd, [&] (int from, int to, const PsgParamFrameData& frameData) {
            const bool startsFrame = storedFrame + 1 < frames.size() && frames[storedFrame + 1]->getFrameIndex() == from;
            if (startsFrame)
                ++storedFrame;
            states.add(from, to, storedFrame, startsFrame, frameData, clip->getTimers());
        });

        beginTest("A state lasts until the next stored frame, the last to the end");
        expectEquals(static_cast<int>(states.getStates().size()), 2);
        const auto& held = states.getStates()[0];
        const auto& enveloped = states.getStates()[1];
        expectEquals(held.from, 0);
        expectEquals(held.to, 3);
        expectEquals(held.storedFrame, 0);
        expectEquals(enveloped.from, 3);
        expectEquals(enveloped.to, TimerSpan::toTheEnd);
        expectEquals(enveloped.storedFrame, 1);

        beginTest("Notes come highest pitch first, with how loud they sound");
        {
            const auto notes = states.getNotes(held);
            expectEquals(static_cast<int>(notes.size()), 2);
            expectEquals(static_cast<int>(notes[0].channelMask), 2, "B is the higher");
            expectEquals(static_cast<int>(notes[1].channelMask), 1);
            expect(notes[0].pitch > notes[1].pitch);
            expectWithinAbsoluteError(notes[0].level, std::exp2(-7.0f / 3.0f), 1.0e-6f);
            expectWithinAbsoluteError(notes[1].level, 1.0f, 1.0e-6f);
            expectEquals(static_cast<int>(held.toneChannels), 3);
            expectEquals(static_cast<int>(held.pitchChannels), 3);
        }

        beginTest("Noise is as loud as its channel");
        expectEquals(static_cast<int>(held.noisePeriod), 7);
        expectWithinAbsoluteError(held.noiseLevels[0], 0.0f, 1.0e-6f);
        expectWithinAbsoluteError(held.noiseLevels[1], std::exp2(-7.0f / 3.0f), 1.0e-6f);
        expectWithinAbsoluteError(held.noiseLevel, held.noiseLevels[1], 1.0e-6f);

        beginTest("The envelope's pitch belongs to the one channel on it");
        {
            expect(! held.envelopeRestarts);
            expectEquals(static_cast<int>(held.envelopeChannel), -1);
            expect(enveloped.envelopeRestarts, "the frame writes the shape");
            expectEquals(static_cast<int>(enveloped.envelopeShape), 0x0C);
            expectEquals(static_cast<int>(enveloped.envelopeChannel), 0);
            expectEquals(static_cast<int>(enveloped.lowEnvelopeChannels), 0);
            const auto notes = states.getNotes(enveloped);
            expectEquals(static_cast<int>(notes.size()), 2);
            expect(notes[1].isEnvelopePitch && notes[1].channelMask == 1, "period 50 sounds as tone period 800");
            expectEquals(static_cast<int>(enveloped.toneChannels), 2);
            expectEquals(static_cast<int>(enveloped.pitchChannels), 3);
        }
    }
};

static PsgSoundingStatesTest psgSoundingStatesTest;

}  // namespace MoTool

#include "TempoInspector.h"

#include <JuceHeader.h>

#include "../../formats/psg/PsgData.h"
#include "../../formats/psg/PsgFile.h"
#include "../../models/PsgClip.h"
#include "../../models/PsgFrameRetimer.h"

namespace MoTool::Tests {

class TempoInspectorTests : public juce::UnitTest
{
public:
    TempoInspectorTests() : UnitTest("TempoInspector", "MoTool") {}

    /** A note on channel A every `speed` frames; one held note when speed is 0. */
    static uZX::PsgData makeTune(int numFrames, int speed) {
        uZX::PsgData data;
        data.options = {50.0, 1};
        data.frames.resize((size_t) numFrames);
        data.frames[0].setTonePeriod(0, 200);
        data.frames[0].setVolume(0, 15);
        for (int t = speed; speed > 0 && t + 2 < numFrames; t += speed) {
            data.frames[(size_t) t].setTonePeriod(0, (uint16_t) (200 + (t / speed % 4) * 20));
            data.frames[(size_t) t].setVolume(0, 15);
            data.frames[(size_t) t + 2].setVolume(0, 0);
        }
        return data;
    }

    void runTest() override {
        auto& engine = *tracktion::Engine::getEngines()[0];

        beginTest("The inspector draws a tune with a tempo, one without rows, and one too short to repeat");
        for (auto tune : {makeTune(3000, 6), makeTune(3000, 0), makeTune(100, 6)}) {
            auto edit = tracktion::Edit::createSingleTrackEdit(engine);
            const auto length = tracktion::TimeDuration::fromSeconds(tune.getLengthSeconds());
            auto clip = PsgClip::insertTo(
                *tracktion::getAudioTracks(*edit)[0], tune, {{tracktion::TimePosition(), length}, {}}, "tune");
            auto inspector = createTempoInspector(*clip);
            for (auto size : {inspector->getLocalBounds(), juce::Rectangle<int>(120, 90)}) {
                inspector->setBounds(size);
                const auto image = inspector->createComponentSnapshot(inspector->getLocalBounds());
                expect(image.isValid() && image.getWidth() >= size.getWidth());
            }
        }

        beginTest("Set as project tempo makes the cursor's beat the project's, and keeps when the frames sound");
        {
            auto edit = tracktion::Edit::createSingleTrackEdit(engine);
            auto tune = makeTune(3000, 6);
            const auto length = tracktion::TimeDuration::fromSeconds(tune.getLengthSeconds());
            auto clip = PsgClip::insertTo(
                *tracktion::getAudioTracks(*edit)[0], tune, {{tracktion::TimePosition(), length}, {}}, "tune");
            auto& tempo = edit->tempoSequence.getTempoAt(tracktion::TimePosition());
            PsgTiming::setTempoBpmRetimingFrames(*edit, tempo, 90.0, /*preserveAbsoluteFrameTimes=*/true);
            const auto fps = clip->getPsg().getEffectiveFps(*clip);

            auto inspector = createTempoInspector(*clip);  // the cursor opens on the detected beat: 4 rows of 6 frames
            juce::TextButton* apply = nullptr;
            for (auto* child : inspector->getChildren())
                if (auto* button = dynamic_cast<juce::TextButton*>(child);
                    button != nullptr && button->getButtonText().startsWith("Set as"))
                    apply = button;
            expect(apply != nullptr);
            if (apply != nullptr)
                apply->onClick();
            expectWithinAbsoluteError(tempo.getBpm(), 125.0, 0.01);
            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), fps, 1e-6);
        }

        beginTest("The inspector's metronome is not left on in the project");
        {
            auto edit = tracktion::Edit::createSingleTrackEdit(engine);
            auto tune = makeTune(3000, 6);
            const auto length = tracktion::TimeDuration::fromSeconds(tune.getLengthSeconds());
            auto clip = PsgClip::insertTo(
                *tracktion::getAudioTracks(*edit)[0], tune, {{tracktion::TimePosition(), length}, {}}, "tune");
            {
                auto inspector = createTempoInspector(*clip);
                edit->clickTrackEnabled = true;  // as the inspector's Metronome switch does
            }
            expect(!edit->clickTrackEnabled.get());
        }

        runSnapshots();
    }

    /** Development aid, run only when UZX_TEMPO_INSPECT names a folder of PSG files: the inspector of
        each, as it opens, is written next to it as <name>.inspector.png. */
    void runSnapshots() {
        const auto dirName = juce::SystemStats::getEnvironmentVariable("UZX_TEMPO_INSPECT", {});
        if (dirName.isEmpty())
            return;
        beginTest("Inspector snapshots from UZX_TEMPO_INSPECT");
        for (const auto& file : juce::File(dirName).findChildFiles(juce::File::findFiles, false, "*.psg")) {
            uZX::PsgFile psg(file);
            psg.ensureRead();
            auto edit = tracktion::Edit::createSingleTrackEdit(*tracktion::Engine::getEngines()[0]);
            const auto length = tracktion::TimeDuration::fromSeconds(psg.getData().getLengthSeconds());
            auto clip = PsgClip::insertTo(*tracktion::getAudioTracks(*edit)[0],
                                          psg.getData(),
                                          {{tracktion::TimePosition(), length}, {}},
                                          file.getFileNameWithoutExtension());
            auto inspector = createTempoInspector(*clip);
            const auto out = file.withFileExtension("inspector.png");
            out.deleteFile();
            juce::FileOutputStream stream(out);
            expect(stream.openedOk()
                   && juce::PNGImageFormat().writeImageToStream(
                       inspector->createComponentSnapshot(inspector->getLocalBounds(), true, 2.0f), stream));
        }
    }
};

static TempoInspectorTests tempoInspectorTests;

}  // namespace MoTool::Tests

#include <JuceHeader.h>

#include "PsgParamEditorComponent.h"
#include "../../formats/psg/PsgData.h"

namespace MoTool::Tests {

class PsgParamEditorTests : public juce::UnitTest {
public:
    PsgParamEditorTests() : UnitTest("PsgParamEditor", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        uZX::PsgData data {{{{uZX::PsgRegType::VolumeA, 12}}}, {50.0, 1}};
        auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                      {{te::TimePosition(), te::TimeDuration::fromSeconds(1.0)}, {}}, "clip");
        te::SelectionManager selection(engine);
        EditViewState view(*edit, selection);
        view.zoom.setViewWidthPx(400);
        view.zoom.setRange({te::TimePosition(), te::TimeDuration::fromSeconds(1.0)});
        TimelineGrid grid(view);
        PsgParamEditorComponent editor(view, grid);
        editor.setSize(400, 100);
        editor.setCurrentParam(PsgParamType::VolumeA);
        const auto settle = [] { MessageManager::getInstance()->runDispatchLoopUntil(50); };

        beginTest("Without a PSG clip there is nothing to hit");
        expect(! editor.hasClip());
        expect(! editor.hitTest(0, 20));

        beginTest("Shows the selected PSG clip and keeps it while a track is selected");
        selection.selectOnly(clip.get());
        settle();
        expect(editor.hasClip());
        expect(editor.hitTest(0, 20), "volume 12 of 15 is drawn at y 20");
        selection.selectOnly(te::getAudioTracks(*edit)[0]);
        settle();
        expect(editor.hasClip());

        beginTest("A click on the line selects the clip");
        editor.mouseDown({Desktop::getInstance().getMainMouseSource(), {0.0f, 20.0f}, ModifierKeys::leftButtonModifier,
                          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &editor, &editor, Time::getCurrentTime(), {0.0f, 20.0f},
                          Time::getCurrentTime(), 1, false});
        expect(selection.isSelected(clip.get()));

        beginTest("The envelope's rows mark each restart, the same shape again too");
        {
            // The shape is written at 0.25 s and again, the same, at 0.5 s: the line stays flat there
            std::vector<uZX::PsgRegsFrame> frames(50);
            frames[0] = {{uZX::PsgRegType::VolumeA, 12}};
            frames[12] = {{uZX::PsgRegType::EnvelopeShape, 8}};
            frames[25] = {{uZX::PsgRegType::EnvelopeShape, 8}};
            uZX::PsgData tune {std::move(frames), {50.0, 1}};
            auto restarting = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], tune,
                                                {{te::TimePosition::fromSeconds(2.0), te::TimeDuration::fromSeconds(1.0)}, {}}, "restarts");
            view.zoom.setRange({te::TimePosition::fromSeconds(2.0), te::TimeDuration::fromSeconds(1.0)});
            selection.selectOnly(restarting.get());
            settle();
            const auto pixelBelowLine = [&](PsgParamType row, int x) {
                editor.setCurrentParam(row);
                // A finished snapshot: on Windows an image gets what is drawn only when its Graphics ends
                return editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f).getPixelAt(x, 95);
            };
            // Against another row of the same clip, which has the same grid and no marks
            const auto isMarked = [&](int x) {
                return pixelBelowLine(PsgParamType::EnvelopeShape, x) != pixelBelowLine(PsgParamType::VolumeA, x);
            };
            expect(isMarked(200), "the restart at 0.5 s");
            expect(isMarked(96), "the first write at 0.24 s");
            expect(! isMarked(300), "nothing at 0.75 s");
            expect(pixelBelowLine(PsgParamType::RetriggerEnvelope, 200) == pixelBelowLine(PsgParamType::EnvelopeShape, 200),
                   "the retrigger row has them too");
            restarting->removeFromParent();
            editor.setCurrentParam(PsgParamType::VolumeA);
            selection.selectOnly(clip.get());
            settle();
        }

        beginTest("Forgets a deleted clip instead of reading its frames");
        clip->removeFromParent();
        clip = nullptr;
        settle();
        expect(! editor.hasClip());
        expect(! editor.hitTest(0, 20));

        beginTest("A value changed before the view holds level until the next change");
        uZX::PsgData steps {std::vector<uZX::PsgRegsFrame>(50), {50.0, 1}};
        // Every frame sets something, so the frames between the volume changes stay in the clip
        for (size_t i = 0; i < steps.frames.size(); ++i)
            steps.frames[i] = {{uZX::PsgRegType::VolumeB, static_cast<uint8_t>(i % 16)}};
        steps.frames[0] = {{uZX::PsgRegType::VolumeA, 15}};
        steps.frames[1] = {{uZX::PsgRegType::VolumeA, 0}};
        steps.frames[49] = {{uZX::PsgRegType::VolumeA, 15}};
        clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], steps,
                                 {{te::TimePosition(), te::TimeDuration::fromSeconds(1.0)}, {}}, "steps");
        selection.selectOnly(clip.get());
        // The view starts after the drop to 0 at frame 1
        view.zoom.setRange({te::TimePosition::fromSeconds(0.1), te::TimePosition::fromSeconds(1.0)});
        settle();
        const auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
        bool level = true;
        for (int y = 2; y < 90; ++y)
            level = level && image.getPixelAt(200, y) == image.getPixelAt(200, 2);
        expect(level, "no line crosses the middle of the view above the 0 level");
    }
};

static PsgParamEditorTests psgParamEditorTests;

}  // namespace MoTool::Tests

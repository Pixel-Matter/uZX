#include "VideoRenderJob.h"

#include <JuceHeader.h>

#include "../../models/PsgClip.h"
#include "../../util/Helpers.h"

namespace MoTool::Tests {

/** Renders a real tune through the ffmpeg found on this machine; skipped where there is none. */
class VideoRenderJobTest : public juce::UnitTest
{
public:
    VideoRenderJobTest() : UnitTest("VideoRenderJob", "MoTool") {}

    void runTest() override {
        beginTest("A tune renders to a video at the output size, as long as the tune and its tail, with sound");

        const auto ffmpeg = Video::findFfmpeg({});
        const auto psg = juce::File(UZX_FIXTURES_DIR).getChildFile("psg/04_Igoval - ZX-SOS (1998).psg");
        if (!ffmpeg || !ffmpeg->has("libx264") || !psg.existsAsFile()) {
            logMessage("no ffmpeg with libx264, or no fixture: skipping");
            return;
        }

        auto& engine = *tracktion::Engine::getEngines()[0];
        auto edit = tracktion::Edit::createSingleTrackEdit(engine);
        auto clips = Helpers::importPsgFiles(*edit, {psg}, {}, tracktion::getAudioTracks(*edit)[0]);
        expectEquals(clips.size(), 1);
        if (clips.isEmpty())
            return;
        clips[0]->setLength(tracktion::TimeDuration::fromSeconds(2.0), true);

        Video::Settings settings;
        settings.renderHeight = 720;
        settings.scale = 2;
        settings.fps = 25;
        settings.zoom = Video::Settings::Zoom::beats;
        juce::TemporaryFile dest{".mp4"};
        {
            VideoRenderJob job(*edit, settings, ffmpeg->program, dest.getFile());
            expect(job.getError().isEmpty(), job.getError());
            engine.getUIBehaviour().runTaskWithProgressBar(job);
            expect(job.hasWritten(), job.getError());
        }

        // ffmpeg describes its input, then complains there is no output
        juce::ChildProcess probe;
        expect(probe.start(juce::StringArray{
            ffmpeg->program.getFullPathName(), "-hide_banner", "-i", dest.getFile().getFullPathName()}));
        const auto info = probe.readAllProcessOutput();
        expect(info.contains("Duration: 00:00:05.00"), info);
        expect(info.contains("2560x1440"), info);
        expect(info.contains("25 fps"), info);
        expect(info.contains("Audio: aac") && info.contains("48000 Hz") && info.contains("stereo"), info);

        beginTest("A cancelled render leaves the file it would replace as it was, and nothing beside it");
        {
            dest.getFile().replaceWithText("keep");
            {
                VideoRenderJob job(*edit, settings, ffmpeg->program, dest.getFile());
                for (int i = 0; i < 40; ++i)  // the warm-up, then a few frames: on this thread, as it draws here
                    job.runJob();
                job.signalJobShouldExit();
                expect(job.runJob() == juce::ThreadPoolJob::jobHasFinished);
                expect(!job.hasWritten() && job.getError().isEmpty(), job.getError());
            }
            expectEquals(dest.getFile().loadFileAsString(), juce::String("keep"));
            const auto siblings = dest.getFile().getParentDirectory().findChildFiles(
                juce::File::findFiles, false, dest.getFile().getFileNameWithoutExtension() + "_temp*");
            expect(siblings.isEmpty(), "left beside it: " + siblings.getFirst().getFullPathName());
        }
    }
};

static VideoRenderJobTest videoRenderJobTest;

}  // namespace MoTool::Tests

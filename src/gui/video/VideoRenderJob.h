#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>
#include <vector>

#include "../../controllers/EditState.h"
#include "../../formats/video/VideoExport.h"
#include "../../util/PipedProcess.h"

namespace MoTool {

class PlayerDocumentComponent;

//==============================================================================
/**
    Renders an edit to a video file. Plays the edit offline one video frame of sound at a time and draws
    each frame as the Player's Viz mode shows it, at the zoom on screen; ffmpeg encodes the frames as they
    come, then adds the sound. With loops, the sound of each played section (see Video::playedSections)
    renders on its own, and the sections are joined before ffmpeg adds them. Create and delete on the
    message thread; run with UIBehaviour::runTaskWithProgressBar.
*/
class VideoRenderJob : public tracktion::ThreadPoolJobWithProgress,
                       private juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver
{
public:
    VideoRenderJob(tracktion::Edit&, const Video::Settings&, const juce::File& ffmpeg, const juce::File& dest);
    ~VideoRenderJob() override;

    JobStatus runJob() override;
    float getCurrentTaskProgress() override;
    bool canCancel() const override { return true; }

    /** Once finished: true if the video was written. */
    bool hasWritten() const { return stage == Stage::done; }
    /** Once finished: why it failed; empty if it was written or cancelled. */
    juce::String getError() const { return error; }

private:
    enum class Stage
    {
        frames,
        encoding,
        muxing,
        done
    };

    /** Moves on to the next stage, named in the progress dialog. */
    void setStage(Stage, const juce::String& name);
    /** On the message thread: starts rendering the current section's sound, into a file of its own. */
    bool startSection();
    /** Writes the sections' sound one after another into the sound file. */
    bool joinSections();
    void renderFrames();
    void finishEncoding();
    void finishMuxing();
    bool drawFrame(tracktion::TimePosition playhead);
    juce::StringArray ffmpegCommand(const juce::StringArray& args) const;
    juce::String ffmpegErrors() const;

    // The sound written so far, block by block
    void reset(int, double, juce::int64) override {}
    void addBlock(juce::int64 sampleNumberInSource, const juce::AudioBuffer<float>&, int, int numSamples) override;

    tracktion::Edit& edit;
    const Video::Settings settings;
    const juce::File ffmpeg;
    const juce::Array<juce::Range<double>> sections;
    const int width, height, totalFrames;

    juce::TemporaryFile video{".mp4"}, sound{".wav"}, log{".log"};
    juce::OwnedArray<juce::TemporaryFile> sectionSounds;
    juce::TemporaryFile output;  // beside the destination, which it replaces only once the video is complete

    tracktion::SelectionManager selection;
    EditViewState view;
    std::unique_ptr<PlayerDocumentComponent> document;
    juce::Image frame;
    std::vector<juce::uint8> pixels;

    std::unique_ptr<tracktion::Edit::ScopedRenderStatus> renderStatus;
    tracktion::Renderer::Parameters renderParams;  // each section's, but for its time and file
    std::unique_ptr<tracktion::Renderer::RenderTask> audioRender;
    int section = 0;
    juce::int64 samplesBeforeSection = 0;
    PipedProcess encoder, muxer;  // after the temporary files: they stop before the files go

    Stage stage = Stage::frames;
    std::atomic<int> framesDone{0};
    std::atomic<bool> finished{false};
    juce::int64 samplesRendered = 0;
    juce::String error;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoRenderJob)
};

}  // namespace MoTool

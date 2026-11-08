#include "VideoRenderJob.h"

#include <cmath>
#include <cstring>

#include "../../plugins/uZX/scope/ScopePluginUI.h"
#include "../main/PlayerDocument.h"

namespace MoTool {

namespace {
// Off screen there is no display frame to pull the scopes' sound: each video frame does
void refreshScopes(juce::Component& component) {
    if (auto* scope = dynamic_cast<uZX::WaveformDisplay*>(&component))
        scope->refresh();
    for (auto* child : component.getChildren())
        refreshScopes(*child);
}
}  // namespace

VideoRenderJob::VideoRenderJob(tracktion::Edit& e,
                               const Video::Settings& s,
                               const juce::File& program,
                               const juce::File& dest)
    : ThreadPoolJobWithProgress("Rendering video"), edit(e), settings(s), ffmpeg(program),
      width(Video::Settings::widthFor(s.renderHeight)), height(s.renderHeight),
      totalFrames(juce::jmax(1, (int) std::ceil((e.getLength().inSeconds() + Video::Settings::tailSeconds) * s.fps))),
      output(dest), selection(e.engine), view(e, selection, true) {
    // The Player's Viz mode, on its own copy of the view settings: the view on screen stays where it is
    view.showHeaders = false;
    view.showFooters = true;
    view.showClipHeaders = false;
    view.vizMode = true;
    document = std::make_unique<PlayerDocumentComponent>(edit, view);
    document->setSize(width, height);
    frame = juce::Image(juce::Image::ARGB, width, height, true);
    pixels.resize((size_t) width * (size_t) height * 4);

    edit.getTransport().stop(false, false);
    renderStatus = std::make_unique<tracktion::Edit::ScopedRenderStatus>(edit, true);
    tracktion::Renderer::turnOffAllPlugins(edit);

    tracktion::Renderer::Parameters r(edit);
    r.tracksToDo = tracktion::toBitSet(tracktion::getAllTracks(edit));
    r.destFile = sound.getFile();
    r.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
    r.bitDepth = 24;
    r.ditheringEnabled = true;
    r.blockSizeForAudio = settings.samplesPerFrame();  // a block a frame: each frame's scopes draw its own sound
    r.sampleRateForAudio = Video::Settings::sampleRate;
    r.time = tracktion::TimeRange(tracktion::TimePosition(),
                                  tracktion::TimeDuration::fromSeconds(totalFrames / (double) settings.fps));
    r.canRenderInMono = false;
    r.checkNodesForAudio = false;
    r.useMasterPlugins = false;  // the master volume is the listener's, not part of the tune
    audioRender = tracktion::render_utils::createRenderTask(r, getJobName(), nullptr, this);

    if (audioRender == nullptr)
        error = "There is nothing to render.";
    else if (!encoder.start(ffmpegCommand(Video::encodeArgs(settings, video.getFile())), log.getFile()))
        error = "Could not start ffmpeg at " + ffmpeg.getFullPathName() + ".";
}

VideoRenderJob::~VideoRenderJob() {
    audioRender.reset();  // its graph goes on the message thread, which this is
    tracktion::Renderer::turnOffAllPlugins(edit);
    renderStatus.reset();  // playback comes back, and the plugins with it at the device's rate
}

juce::ThreadPoolJob::JobStatus VideoRenderJob::runJob() {
    if (!shouldExit() && error.isEmpty()) {
        switch (stage) {
            case Stage::frames:
                renderFrames();
                break;
            case Stage::encoding:
                finishEncoding();
                break;
            case Stage::muxing:
                finishMuxing();
                break;
            case Stage::done:
                break;
        }
    }

    if (stage != Stage::done && error.isEmpty() && !shouldExit())
        return jobNeedsRunningAgain;

    finished = true;
    return jobHasFinished;
}

float VideoRenderJob::getCurrentTaskProgress() {
    return finished ? 1.0f : 0.95f * (float) framesDone.load() / (float) totalFrames;
}

void VideoRenderJob::renderFrames() {
    const bool soundDone = audioRender->runJob() == jobHasFinished;  // a frame of sound, or of the warm-up before it
    if (soundDone && audioRender->errorMessage.isNotEmpty()) {
        error = audioRender->errorMessage;
        return;
    }

    // A frame is drawn once its sound is: the playhead at the frame's start, the scopes drawing the sound
    // heard while the frame is shown
    const auto samplesPerFrame = (juce::int64) settings.samplesPerFrame();
    while (framesDone < totalFrames && (soundDone || samplesRendered >= (framesDone + 1) * samplesPerFrame)) {
        if (!drawFrame(tracktion::TimePosition::fromSeconds(framesDone / (double) settings.fps))) {
            error = "Rendering was interrupted.";
            return;
        }
        if (!encoder.write(pixels.data(), pixels.size())) {
            error = "ffmpeg stopped encoding the video." + ffmpegErrors();
            return;
        }
        ++framesDone;
    }

    if (soundDone) {
        encoder.closeInput();
        stage = Stage::encoding;
    }
}

void VideoRenderJob::finishEncoding() {
    if (encoder.isRunning()) {
        juce::Thread::sleep(20);
        return;
    }

    if (encoder.getExitCode() != 0)
        error = "ffmpeg could not encode the video." + ffmpegErrors();
    else if (!muxer.start(ffmpegCommand(Video::muxArgs(settings, video.getFile(), sound.getFile(), output.getFile())),
                          log.getFile()))
        error = "Could not start ffmpeg at " + ffmpeg.getFullPathName() + ".";
    else
        stage = Stage::muxing;
}

void VideoRenderJob::finishMuxing() {
    if (muxer.isRunning()) {
        juce::Thread::sleep(20);
        return;
    }

    if (muxer.getExitCode() != 0)
        error = "ffmpeg could not add the sound to the video." + ffmpegErrors();
    else if (!output.overwriteTargetFileWithTemporary())
        error = "Could not write " + output.getTargetFile().getFullPathName() + ".";
    else
        stage = Stage::done;
}

bool VideoRenderJob::drawFrame(tracktion::TimePosition playhead) {
    return juce::MessageManager::callSync([this, playhead] {
        view.zoom.showOfflineFrame(playhead);
        refreshScopes(*document);
        {
            juce::Graphics g(frame);
            g.fillAll(document->getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
            document->paintEntireComponent(g, true);
        }

        const juce::Image::BitmapData data(frame, juce::Image::BitmapData::readOnly);
        const auto rowBytes = (size_t) width * 4;  // BGRA in memory, as ffmpeg reads it
        for (int y = 0; y < height; ++y)
            std::memcpy(pixels.data() + (size_t) y * rowBytes, data.getLinePointer(y), rowBytes);
    });
}

void VideoRenderJob::addBlock(juce::int64 sampleNumberInSource, const juce::AudioBuffer<float>&, int, int numSamples) {
    samplesRendered = sampleNumberInSource + numSamples;
}

juce::StringArray VideoRenderJob::ffmpegCommand(const juce::StringArray& args) const {
    juce::StringArray command{ffmpeg.getFullPathName()};
    command.addArray(args);
    return command;
}

juce::String VideoRenderJob::ffmpegErrors() const {
    auto lines = juce::StringArray::fromLines(log.getFile().loadFileAsString().trim());
    lines.removeRange(0, lines.size() - 8);  // the last few say what went wrong
    return lines.isEmpty() ? juce::String() : "\n\n" + lines.joinIntoString("\n");
}

}  // namespace MoTool

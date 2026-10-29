#include "EditState.h"
#include "../util/TestHelpers.h"

namespace MoTool {

class TrackViewStateTest : public UnitTest
{
public:
    TrackViewStateTest() : UnitTest("TrackViewState", "MoTool") {}

    void runTest() override {
        TrackViewState view(ValueTree(te::IDs::TRACK), nullptr);
        Component container;
        Component row;
        container.setSize(800, 1000);
        container.addChildComponent(row);
        row.setBounds(0, 100, 800, 160);

        auto resizeHeight = [&](int height) {
            view.getConstrainer().setBoundsForComponent(&row, row.getBounds().withHeight(height),
                                                       false, false, true, false);
        };

        beginTest("Track can grow beyond 600 pixels within its container");
        resizeHeight(700);
        expectEquals(row.getHeight(), 700);

        beginTest("Bottom-edge drag stops at the container bottom without moving the top");
        resizeHeight(2000);
        expectEquals(row.getY(), 100);
        expectEquals(row.getBottom(), container.getHeight());

        beginTest("Resize limit follows container size changes");
        container.setSize(800, 1200);
        resizeHeight(2000);
        expectEquals(row.getHeight(), 1100);

        beginTest("Scrolled rows keep their top position and bottom edge visible");
        row.setTopLeftPosition(0, -200);
        resizeHeight(2000);
        expectEquals(row.getY(), -200);
        expectEquals(row.getBottom(), container.getHeight());

        beginTest("Minimum track height remains enforced");
        resizeHeight(1);
        expectEquals(row.getHeight(), 56);
    }
};

static TrackViewStateTest trackViewStateTest;

//==============================================================================
/** Plays through a hosted device that reports 0.25 s of output latency and checks the
    playhead follows what is heard, not what the transport has already rendered. */
class PlayheadPositionTest : public UnitTest
{
public:
    PlayheadPositionTest() : UnitTest("PlayheadPosition", "MoTool") {}

    void runTest() override {
        using namespace tracktion::literals;
        auto& engine = *te::Engine::getEngines()[0];
        ScopedOutputLatency deviceLatency {engine, 11025};

        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto& transport = edit->getTransport();
        ZoomViewState zoom {*edit};
        const auto playhead = [&] { return zoom.getPlayheadPosition().inSeconds(); };
        const auto heard = [&] { return transport.getPosition().inSeconds() - 0.25; };

        // The transport takes playhead updates at 50 Hz, and ignores them for 200 ms after a seek.
        const auto play = [&](int numBlocks) {
            TestHelpers::processBlocks(engine, numBlocks);
            MessageManager::getInstance()->runDispatchLoopUntil(60);
        };
        const auto seek = [&](te::TimePosition t) {
            transport.setPosition(t);
            TestHelpers::processBlocks(engine, 1);  // the audio thread takes the jump
            MessageManager::getInstance()->runDispatchLoopUntil(250);
        };

        beginTest("Stopped: the playhead is at the transport position");
        expectWithinAbsoluteError(engine.getDeviceManager().getOutputLatencySeconds(), 0.25, 1.0e-6,
                                  "precondition: the device reports its output latency");
        transport.setPosition(1_tp);
        expectEquals(playhead(), 1.0);

        beginTest("Playback start: the playhead waits at the start until it is heard");
        transport.play(false);
        MessageManager::getInstance()->runDispatchLoopUntil(250);
        play(10);
        expectGreaterThan(transport.getPosition().inSeconds(), 1.1);
        expectEquals(playhead(), 1.0);

        beginTest("Playing: the playhead trails the transport by the output latency");
        play(40);
        expectWithinAbsoluteError(playhead(), heard(), 1.0e-9);

        beginTest("Seek while playing: the playhead lands on the target and waits there");
        transport.setPosition(5_tp);
        expectEquals(playhead(), 5.0);
        seek(5_tp);
        play(5);
        expectGreaterThan(transport.getPosition().inSeconds(), 5.0);
        expectEquals(playhead(), 5.0);
        play(40);
        expectWithinAbsoluteError(playhead(), heard(), 1.0e-9);

        beginTest("Stop: the playhead returns to the transport position and is redrawn");
        PosListener listener;
        zoom.addListener(&listener);
        transport.stop(false, false);
        TestHelpers::flushMessageQueue();
        expectGreaterThan(listener.calls, 0);
        expectEquals(playhead(), transport.getPosition().inSeconds());
        zoom.removeListener(&listener);

        beginTest("Loop wrap: the playhead restarts from the loop start, not from where play began");
        transport.setLoopRange({1_tp, 2_tp});
        transport.looping = true;
        seek(1.5_tp);
        transport.play(false);
        MessageManager::getInstance()->runDispatchLoopUntil(250);
        play(40);  // just before the loop end
        play(10);  // wrapped
        expectLessThan(transport.getPosition().inSeconds(), 1.5);
        expectGreaterOrEqual(playhead(), 1.0);
        expectLessThan(playhead(), 1.5);

        transport.stop(false, false);
        transport.looping = false;
    }

private:
    /** Swaps the test engine's hosted audio device for one that reports (and adds) output latency. */
    struct ScopedOutputLatency
    {
        ScopedOutputLatency(te::Engine& e, int numSamples) : engine(e) { reset(numSamples); }
        ~ScopedOutputLatency() { reset(0); }

        void reset(int numSamples) {
            auto& dm = engine.getDeviceManager();
            dm.deviceManager.closeAudioDevice();
            dm.removeHostedAudioDeviceInterface();  // a re-initialised device would keep its delay line
            te::HostedAudioDeviceInterface::Parameters params;
            params.outputLatencyNumSamples = numSamples;
            auto& audioIO = dm.getHostedAudioDeviceInterface();
            audioIO.initialise(params);
            audioIO.prepareToPlay(params.sampleRate, params.blockSize);
            dm.dispatchPendingUpdates();
            TestHelpers::flushMessageQueue();  // the device manager reads the latency asynchronously
        }

        te::Engine& engine;
    };

    struct PosListener : ZoomViewState::Listener
    {
        int calls = 0;
        void zoomChanged() override {}
        void zoomOrPosChanged() override { ++calls; }
    };
};

static PlayheadPositionTest playheadPositionTest;

}  // namespace MoTool

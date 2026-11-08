#pragma once

#include <JuceHeader.h>

namespace MoTool::TestHelpers {

/** Flush all pending async messages on the message thread.
    Each round posts a sentinel via callAsync and spins the dispatch loop until it arrives,
    so everything queued before it has been processed. Messages those post in turn (an async
    track build posting an async clip build) need further rounds: Windows dispatches only
    the batch queued at the time, where macOS delivers the newcomers as well. */
inline void flushMessageQueue(int rounds = 4)
{
    for (int round = 0; round < rounds; ++round) {
        bool flushed = false;
        juce::MessageManager::callAsync([&flushed] { flushed = true; });

        for (int i = 0; i < 200 && !flushed; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    }
}

/** Runs numBlocks 512-sample blocks through the test engine's hosted audio device. */
inline void processBlocks(tracktion::Engine& engine, int numBlocks)
{
    auto& audioIO = engine.getDeviceManager().getHostedAudioDeviceInterface();
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    for (int i = 0; i < numBlocks; ++i) {
        buffer.clear();
        audioIO.processBlock(buffer, midi);
    }
}

}  // namespace MoTool::TestHelpers

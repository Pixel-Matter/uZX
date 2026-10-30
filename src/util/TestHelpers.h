#pragma once

#include <JuceHeader.h>

namespace MoTool::TestHelpers {

/** Flush all pending async messages on the message thread.
    Posts a sentinel via callAsync and spins the dispatch loop until it arrives,
    guaranteeing all previously-queued async messages have been processed. */
inline void flushMessageQueue()
{
    bool flushed = false;
    juce::MessageManager::callAsync([&flushed] { flushed = true; });

    for (int i = 0; i < 200 && !flushed; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
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

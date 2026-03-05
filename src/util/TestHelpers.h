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

}  // namespace MoTool::TestHelpers

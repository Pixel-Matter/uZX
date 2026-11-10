#include "ScopeBuffer.h"

#include <JuceHeader.h>

#include <latch>
#include <thread>
#include <vector>

namespace MoTool::uZX {

class ScopeBufferTest : public juce::UnitTest
{
public:
    ScopeBufferTest() : UnitTest("ScopeBuffer", "MoTool") {}

    void runTest() override {
        const int capacity = ScopeBuffer::size();

        beginTest("Full history wraps with the sample, sync and amplitude lanes aligned");
        {
            ScopeBuffer buffer;
            std::vector<float> input(static_cast<size_t>(capacity + 17));
            std::vector<float> output(static_cast<size_t>(capacity)), sync(output.size()), amplitude(output.size());
            for (size_t i = 0; i < input.size(); ++i)
                input[i] = static_cast<float>(i);

            buffer.pushSamples(input.data(), capacity, input.data(), input.data());
            buffer.pushSamples(input.data() + capacity, 17, input.data() + capacity, input.data() + capacity);
            expectEquals(buffer.copyTo(output.data(), capacity + 1, sync.data(), amplitude.data()), capacity);
            expect(std::equal(output.begin(), output.end(), input.begin() + 17));
            expect(output == sync && output == amplitude);

            beginTest("A reset discards the history and the next short block is padded with silence");
            buffer.requestClear();
            expectEquals(buffer.copyTo(output.data(), capacity, sync.data(), amplitude.data()), capacity);
            expect(
                std::all_of(output.begin(), output.end(), [](float value) { return juce::exactlyEqual(value, 0.0f); }));
            expect(output == sync && output == amplitude);

            const float value = 42.0f;
            buffer.pushSamples(&value, 1);
            expectEquals(buffer.copyTo(output.data(), 2, sync.data(), amplitude.data()), 2);
            expect(juce::exactlyEqual(output[0], 0.0f) && juce::exactlyEqual(output[1], value));
            expect(juce::exactlyEqual(sync[1], 0.0f) && juce::exactlyEqual(amplitude[1], 0.0f));
        }

        beginTest("Concurrent full-ring reads return a coherent snapshot or skip the frame");
        {
            constexpr int blockSize = 127;
            constexpr int blocks = 2048;
            ScopeBuffer buffer;
            std::vector<float> input(static_cast<size_t>(capacity + blockSize * blocks));
            std::vector<float> inverted(input.size());
            std::vector<float> output(static_cast<size_t>(capacity)), sync(output.size()), amplitude(output.size());
            for (size_t i = 0; i < input.size(); ++i) {
                input[i] = static_cast<float>(i);
                inverted[i] = -input[i];
            }

            buffer.pushSamples(input.data(), capacity, inverted.data(), input.data());
            std::latch start{2};
            std::jthread producer([&] {
                start.arrive_and_wait();
                for (int i = 0; i < blocks; ++i) {
                    const auto offset = capacity + i * blockSize;
                    buffer.pushSamples(
                        input.data() + offset, blockSize, inverted.data() + offset, input.data() + offset);
                }
            });

            const auto coherent = [&] {
                for (size_t i = 0; i < output.size(); ++i) {
                    if (!juce::exactlyEqual(output[i], output[0] + static_cast<float>(i))
                        || !juce::exactlyEqual(sync[i], -output[i]) || !juce::exactlyEqual(amplitude[i], output[i]))
                        return false;
                }
                return true;
            };
            start.arrive_and_wait();
            bool snapshotsCoherent = true;
            for (int i = 0; i < 256; ++i) {
                const int copied = buffer.copyTo(output.data(), capacity, sync.data(), amplitude.data());
                if (copied != 0 && (copied != capacity || !coherent())) {
                    snapshotsCoherent = false;
                    break;
                }
            }
            producer.join();

            expect(snapshotsCoherent);
            expectEquals(buffer.copyTo(output.data(), capacity, sync.data(), amplitude.data()), capacity);
            expect(coherent());
            expect(juce::exactlyEqual(output.back(), input.back()));
        }
    }
};

static ScopeBufferTest scopeBufferTest;

}  // namespace MoTool::uZX

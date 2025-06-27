#pragma once

#include <JuceHeader.h>
#include "../../models/tuning/TuningSystemBase.h"

#include <array>
#include <vector>
#include <optional>

namespace MoTool::uZX {

/**
 * Core converter class that processes MIDI note events and generates PSG MIDI CC messages.
 * This class is designed to be testable and reusable.
 */
class MidiToPsgConverter {
public:
    struct ChannelState {
        std::optional<int> currentNote;
        int velocity = 0;
        int aftertouch = 0;
        int lastVolume = -1;  // Track last emitted volume to avoid redundant updates
        bool toneOn = false;

        void clear() {
            currentNote.reset();
            velocity = 0;
            aftertouch = 0;
            lastVolume = -1;
            toneOn = false;
        }
    };

    explicit MidiToPsgConverter(int baseChannel = 1, int numChannels = 3);

    // Configuration
    void setTuningSystem(const TuningSystem* tuning) { tuningSystem_ = tuning; }
    void setBaseChannel(int channel) { baseChannel_ = channel; }
    void setNumChannels(int channels) { numChannels_ = juce::jlimit(1, 4, channels); }

    // MIDI input processing
    void noteOn(int channel, int note, int velocity);
    void noteOff(int channel, int note);
    void aftertouch(int channel, int aftertouch);
    void controlChange(int channel, int controller, int value);

    // Output retrieval
    std::vector<juce::MidiMessage> getOutputMessages();
    void clearOutput() { outputBuffer_.clear(); }

    // State access for testing
    const ChannelState& getChannelState(int channel) const;

private:
    int baseChannel_ = 1;
    int numChannels_ = 3;
    std::array<ChannelState, 4> channels_;
    const TuningSystem* tuningSystem_ = nullptr;
    std::vector<juce::MidiMessage> outputBuffer_;

    // Helper methods
    bool isChannelInRange(int channel) const;
    ChannelState& getChannelState(int channel);
    void emitVolumeCC(int channel, int volume);
    void emitPeriodCC(int channel, int period);
    void emitToneSwitchCC(int channel, bool on);
    void emitPassthroughCC(int channel, int controller, int value);
    int velocityAndAftertouchToVolume(int velocity, int aftertouch) const;
    void updateChannelOutput(int channel);
};

} // namespace MoTool::uZX
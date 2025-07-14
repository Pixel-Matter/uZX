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
class MidiToPsgTransformer {
public:
    struct ChannelState {
        std::optional<int> currentNote;
        int velocity = 0;
        int aftertouch = 0;
        int lastVolume = -1;  // Track last emitted volume to avoid redundant updates
        // bool toneOn = false;
        // bool noiseOn = false;
        // bool envOn = true;

        void clear() {
            currentNote.reset();
            velocity = 0;
            aftertouch = 0;
            lastVolume = -1;
            // do not clear modulation switches, they should stay on until explicitly turned off
        }
    };

    explicit MidiToPsgTransformer(int baseChannel = 1, int numChannels = 3);

    // Configuration
    void setTuningSystem(const TuningSystem* tuning) { tuningSystem_ = tuning; }
    void setBaseChannel(int channel) { baseChannel_ = channel; }
    void setNumChannels(int channels) { numChannels_ = juce::jlimit(1, 4, channels); }

    // MIDI input processing
    void initPSG();
    void noteOn(int channel, int note, int velocity);
    void noteOff(int channel, int note);
    void allNotesOff(int channel);
    void aftertouch(int channel, int aftertouch);
    void controlChange(int channel, int controller, int value);

    // Output retrieval
    std::vector<juce::MidiMessage> getOutputMessages();
    void clearOutput() { outputBuffer_.clear(); }

    // State access for testing
    const ChannelState& getChannelState(int channel) const;
    void debugChannelStates() const {
        for (size_t i = 0; i < static_cast<size_t>(numChannels_); ++i) {
            DBG("Channel " << (static_cast<size_t>(baseChannel_) + i) << ": "
                << "Note: " << (channels_[i].currentNote.has_value() ? std::to_string(channels_[i].currentNote.value()) : "none")
                << ", Velocity: " << channels_[i].velocity
                << ", Aftertouch: " << channels_[i].aftertouch
                // << ", Tone On: " << (channels_[i].toneOn ? "Yes" : "No")
                // << ", Noise On: " << (channels_[i].noiseOn ? "Yes" : "No")
                // << ", Env On: " << (channels_[i].envOn ? "Yes" : "No")
            );
        }
    }

private:
    int baseChannel_ = 1;
    int numChannels_ = 4;
    std::array<ChannelState, 4> channels_;
    const TuningSystem* tuningSystem_ = nullptr;
    std::vector<juce::MidiMessage> outputBuffer_;

    // Helper methods
    bool isChannelInRange(int channel) const;
    ChannelState& getChannelState(int channel);
    void emitToneSwitchCC(int channel, bool on);
    void emitNoiseSwitchCC(int channel, bool on);
    void emitEnvSwitchCC(int channel, bool on);
    void emitVolumeCC(int channel, int volume);
    void emitPeriodCC(int channel, int period);
    void emitCC(int channel, int controller, int value);
    int velocityAndAftertouchToVolume(int velocity, int aftertouch) const;
    void updateNoteVolume(int channel);
};

} // namespace MoTool::uZX
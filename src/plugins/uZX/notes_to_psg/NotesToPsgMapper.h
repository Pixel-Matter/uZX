#pragma once

#include <JuceHeader.h>
#include "../../../models/tuning/TuningSystemBase.h"
#include "../../../models/tuning/TuningRegistry.h"
#include "../../../models/PsgMidi.h"
#include "../midi_effects/MidiEffect.h"

#include <array>
#include <vector>
#include <optional>

namespace MoTool::uZX {

using namespace MoTool;

/**
 * Core converter class that processes MIDI note events and generates PSG MIDI CC messages.
 * This class is designed to be testable and reusable.
 */
class NotesToPsgMapper {
public:
    struct ChannelVoice {
    private:
        tracktion::MidiMessageArray& midiBuffer_;
    public:
        int midiChannel;
        bool isEnvChannel = false;

        std::optional<int> initialNote;
        float actualNote = -1.0f; // float for bends
        int velocity;         // 0-127
        int aftertouchValue;  // 0-127
        int chipVolume = -1;  // 0-15, Track last emitted volume to avoid redundant updates
        bool toneOn = false;
        bool noiseOn = false;
        bool envOn = true;
        tracktion::MPESourceID mpeSourceId_ = 0;

        ChannelVoice(tracktion::MidiMessageArray& buffer, int chan, bool isEnv = false);

        void reset();
        bool isActive() const;
        int getEffectiveChipVolume();

        void noteOn(int note, int vel, const TuningSystem& tuning);
        void noteOff(int note);
        void aftertouch(int note, int value);
        void controllerChange(MidiCCType controller, int value);

        void emitControllerChange(MidiCCType controller, int value);
        void emitVolume(int volume);
        void emitPeriod(int period);
        void emitToneSwitch(bool on);
        void emitNoiseSwitch(bool on);
        void emitEnvSwitch(bool on);
        void updateVolume();

        void debug() const;
    };

    NotesToPsgMapper();
    ~NotesToPsgMapper();

    // explicit NotesToPsgMapper(int baseChannel = 1, int numChannels = 3);

    // Configuration
    void setTuningSystem(const TuningSystem* tuning) { tuningSystem_ = tuning; }
    void setBaseChannel(int channel);
    void reset();

    // Output retrieval
    tracktion::MidiMessageArray takeOutputMessages();
    void clearOutput() { midiBuffer_.clear(); }

    void handleMidiMessage(const tracktion::MidiMessageWithSource& msg);

    // MIDI fx processor callback
    void operator()(MidiBufferContext& c);

    // State access for testing
    const ChannelVoice& getChannelVoice(int channel) const;
    void debugChannelVoices() const {
        for (size_t i = 0; i < static_cast<size_t>(numChannels_); ++i) {
            DBG("Channel " << (static_cast<size_t>(baseChannel_) + i) << ": "
                << "Note: " << (voices_[i].initialNote.has_value() ? std::to_string(voices_[i].initialNote.value()) : "none")
                << ", Velocity: " << voices_[i].velocity
                << ", Aftertouch: " << voices_[i].aftertouchValue
                // << ", Tone On: " << (channels_[i].toneOn ? "Yes" : "No")
                // << ", Noise On: " << (channels_[i].noiseOn ? "Yes" : "No")
                // << ", Env On: " << (channels_[i].envOn ? "Yes" : "No")
            );
        }
    }

    friend struct ChannelVoice;

private:
    MPEInstrument mpeInstrument_;

    int baseChannel_ = 1;
    tracktion::MPESourceID mpeSourceId_ = 0;
    inline static constexpr int numChannels_ = 4;
    std::array<ChannelVoice, numChannels_> voices_;
    const TuningSystem* tuningSystem_ = nullptr;
    tracktion::MidiMessageArray midiBuffer_;
    bool passthruOutsideChannels_ = true;
    bool passthruUnprocessedMIDI_ = false;
    std::unique_ptr<TuningSystem> defaultTuningSystem_ = makeBuiltinTuning(BuiltinTuningType::EqualTemperament);

    // Helper methods
    bool isChannelInRange(int channel) const;
    const ChannelVoice& getVoice(int channel) const;
    ChannelVoice& getVoice(int channel);
};

} // namespace MoTool::uZX
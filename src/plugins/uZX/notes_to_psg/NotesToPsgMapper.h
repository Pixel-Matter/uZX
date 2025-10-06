#pragma once

#include <JuceHeader.h>
#include "../../../models/tuning/TuningSystemBase.h"
#include "../../../models/tuning/TuningRegistry.h"
#include "../../../models/PsgMidi.h"
#include "../midi_effects/MidiEffect.h"

#include <array>
#include <vector>
#include <optional>

namespace MoTool::Tests {
class MidiToPsgConverterTests;
}

namespace MoTool::uZX {

using namespace MoTool;

/**
 * Core converter class that processes MIDI note events and generates PSG MIDI CC messages.
 * This class is designed to be testable and reusable.
 */
class NotesToPsgMapper : private MPEInstrument::Listener {
public:
    struct ChannelState {
        int period = 0;       // Period value for tone/noise
        int volume = 0;       // Volume level 0-15
        bool toneOn = false;
        bool noiseOn = true;
        bool envOn = false;   // Envelope is off by default
        EnvShape envShape = EnvShape::DOWN_HOLD_BOTTOM_0;
    };

    struct ChannelVoice {
    private:
        tracktion::MidiMessageArray& midiBuffer_;
        const TuningSystem* tuning_ = nullptr;
    public:
        int midiChannel;
        bool isEnvChannel = false;
        tracktion::MPESourceID mpeSourceId_ = 0;

        MPENote mpeNote;
        ChannelState state;
        ChannelState lastState;

        ChannelVoice(tracktion::MidiMessageArray& buffer, int chan, bool isEnv = false);

        void reset();
        void setTuningSystem(const TuningSystem* tuning) { tuning_ = tuning; }

        bool isActive() const;
        int getEffectiveChipVolume() const;
        int getEffectiveChipPeriod() const;

        void noteOn(MPENote newNote);
        void noteOff(MPENote offNote);
        void aftertouch(MPENote changedNote);
        void controllerChange(MidiCCType controller, int value);
        void pitchbendChange(MPENote changedNote);

        void emitControllerChange(MidiCCType controller, int value);
        void emitVolume(int volume);
        void emitPeriod(int period);
        void emitToneSwitch(bool on);
        void emitNoiseSwitch(bool on);
        void emitEnvSwitch(bool on);

        //==============================================================================
        // Rendering
        void updatePeriod();
        void updateVolume();
        void updateModulation();
        void render();

        void debug() const;
    };

    NotesToPsgMapper();
    ~NotesToPsgMapper() override;

    // explicit NotesToPsgMapper(int baseChannel = 1, int numChannels = 3);

    // Configuration
    void setTuningSystem(const TuningSystem* tuning);
    void setBaseChannel(int channel);
    void reset();

    // MIDI processing
    tracktion::MidiMessageArray takeOutputMessages();
    void clearOutput() { midiBuffer_.clear(); }
    void handleMidiMessage(const tracktion::MidiMessageWithSource& msg);

    // MIDI fx processor callback
    void operator()(MidiBufferContext& c);

    // For debugging
    void debugChannelVoices() const;

    friend struct ChannelVoice;
    friend class MoTool::Tests::MidiToPsgConverterTests;

private:
    void setPassthruOutsideChannels(bool enable) { passthruOutsideChannels_ = enable; }
    void setPassthruUnprocessedMIDI(bool enable) { passthruUnprocessedMIDI_ = enable; }

    // Helper methods
    bool isChannelInRange(int channel) const;
    const ChannelVoice& getVoice(int channel) const;
    ChannelVoice& getVoice(int channel);

    [[nodiscard]] te::MidiMessageArray renderVoices();

    //==============================================================================
    // MPEInstrument::Listener callbacks (connect MPE events to voice management)

    /** Called when a new MPE note is added. */
    void noteAdded(MPENote newNote) override;

    /** Called when an MPE note is released. */
    void noteReleased(MPENote finishedNote) override;

    /** Called when an MPE note's pressure changes. */
    void notePressureChanged(MPENote changedNote) override;

    /** Called when an MPE note's pitchbend changes. */
    void notePitchbendChanged(MPENote changedNote) override;

    /** Called when an MPE note's timbre changes. */
    void noteTimbreChanged(MPENote changedNote) override;

    // Do not need key state changes for now
    // /** Called when an MPE note's key state changes. */
    // void noteKeyStateChanged(MPENote changedNote) override;

    //==============================================================================

    MPEInstrument mpeInstrument_;

    int baseChannel_ = 1;
    tracktion::MPESourceID mpeSourceId_ = 0;
    inline static constexpr int numChannels_ = 4;
    std::array<ChannelVoice, numChannels_> voices_;
    tracktion::MidiMessageArray midiBuffer_;
    bool passthruOutsideChannels_ = true;
    bool passthruUnprocessedMIDI_ = false;

    const TuningSystem* tuningSystem_ = nullptr;
    std::unique_ptr<TuningSystem> defaultTuningSystem_ = makeBuiltinTuning(BuiltinTuningType::EqualTemperament);

};

} // namespace MoTool::uZX

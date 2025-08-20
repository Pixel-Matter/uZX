#pragma once

#include <JuceHeader.h>

namespace MoTool::uZX {

//==============================================================================
/**
    Manages MPE instrument state and MIDI processing.
    Pure MPE/MIDI logic with no audio rendering concerns.

    This class handles:
    - MPE zone layout management
    - MIDI event processing and routing
    - Legacy mode support
    - MPE tracking mode configuration
    - Event listener management

    @see MPEEffect, VoiceManager
*/
class MPEInstrumentManager {
public:
    //==============================================================================
    /** Constructor using default MPE instrument. */
    MPEInstrumentManager();

    /** Constructor using custom MPE instrument. */
    explicit MPEInstrumentManager(MPEInstrument& customInstrument);

    //==============================================================================
    // MPE Configuration

    /** Returns the current MPE zone layout. */
    MPEZoneLayout getZoneLayout() const noexcept;

    /** Sets a new MPE zone layout. */
    void setZoneLayout(MPEZoneLayout newLayout);

    /** Enables legacy mode with specified parameters. */
    void enableLegacyMode(int pitchbendRange = 2, Range<int> channelRange = Range<int>(1, 17));

    /** Returns true if legacy mode is enabled. */
    bool isLegacyModeEnabled() const noexcept;

    //==============================================================================
    // Legacy Mode Configuration

    /** Returns the legacy mode channel range. */
    Range<int> getLegacyModeChannelRange() const noexcept;

    /** Sets the legacy mode channel range. */
    void setLegacyModeChannelRange(Range<int> channelRange);

    /** Returns the legacy mode pitchbend range in semitones. */
    int getLegacyModePitchbendRange() const noexcept;

    /** Sets the legacy mode pitchbend range in semitones. */
    void setLegacyModePitchbendRange(int pitchbendRange);

    //==============================================================================
    // MPE Tracking Modes

    /** Sets the pressure tracking mode. */
    void setPressureTrackingMode(MPEInstrument::TrackingMode mode);

    /** Sets the pitchbend tracking mode. */
    void setPitchbendTrackingMode(MPEInstrument::TrackingMode mode);

    /** Sets the timbre tracking mode. */
    void setTimbreTrackingMode(MPEInstrument::TrackingMode mode);

    //==============================================================================
    // MIDI Processing

    /** Processes a single MIDI event. */
    void handleMidiEvent(const MidiMessage& message);

    /** Processes MIDI events within a sample block with subdivision. */
    void processEventsInBlock(const MidiBuffer& inputMidi, int startSample, int numSamples,
                             std::function<void(const MidiMessage&, int)> eventCallback);

    //==============================================================================
    // Rendering Subdivision Control

    /** Sets the minimum rendering subdivision size. */
    void setMinimumRenderingSubdivisionSize(int numSamples, bool shouldBeStrict = false) noexcept;

    /** Gets the minimum rendering subdivision size. */
    int getMinimumRenderingSubdivisionSize() const noexcept { return minimumSubBlockSize; }

    /** Returns whether subdivision is strict. */
    bool isRenderingSubdivisionStrict() const noexcept { return subBlockSubdivisionIsStrict; }

    //==============================================================================
    // Observer Pattern for MPE Events

    /** Adds an MPE event listener. */
    void addListener(MPEInstrument::Listener* listener);

    /** Removes an MPE event listener. */
    void removeListener(MPEInstrument::Listener* listener);

    //==============================================================================
    // Utility

    /** Releases all currently playing notes. */
    void releaseAllNotes();

private:
    //==============================================================================
    MPEInstrument& mpeInstrument;
    MPEInstrument defaultInstrument{MPEZone(MPEZone::Type::lower, 15)};

    CriticalSection noteStateLock;
    int minimumSubBlockSize = 32;
    bool subBlockSubdivisionIsStrict = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MPEInstrumentManager)
};

}  // namespace MoTool::uZX
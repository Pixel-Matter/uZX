#include "MPEInstrumentManager.h"

namespace MoTool::uZX {

MPEInstrumentManager::MPEInstrumentManager() : mpeInstrument(defaultInstrument) {
}

MPEInstrumentManager::MPEInstrumentManager(MPEInstrument& customInstrument) 
    : mpeInstrument(customInstrument) {
}

//==============================================================================
MPEZoneLayout MPEInstrumentManager::getZoneLayout() const noexcept {
    return mpeInstrument.getZoneLayout();
}

void MPEInstrumentManager::setZoneLayout(MPEZoneLayout newLayout) {
    mpeInstrument.setZoneLayout(newLayout);
}

//==============================================================================
void MPEInstrumentManager::enableLegacyMode(int pitchbendRange, Range<int> channelRange) {
    mpeInstrument.enableLegacyMode(pitchbendRange, channelRange);
}

bool MPEInstrumentManager::isLegacyModeEnabled() const noexcept {
    return mpeInstrument.isLegacyModeEnabled();
}

Range<int> MPEInstrumentManager::getLegacyModeChannelRange() const noexcept {
    return mpeInstrument.getLegacyModeChannelRange();
}

void MPEInstrumentManager::setLegacyModeChannelRange(Range<int> channelRange) {
    mpeInstrument.setLegacyModeChannelRange(channelRange);
}

int MPEInstrumentManager::getLegacyModePitchbendRange() const noexcept {
    return mpeInstrument.getLegacyModePitchbendRange();
}

void MPEInstrumentManager::setLegacyModePitchbendRange(int pitchbendRange) {
    mpeInstrument.setLegacyModePitchbendRange(pitchbendRange);
}

//==============================================================================
void MPEInstrumentManager::setPressureTrackingMode(MPEInstrument::TrackingMode mode) {
    mpeInstrument.setPressureTrackingMode(mode);
}

void MPEInstrumentManager::setPitchbendTrackingMode(MPEInstrument::TrackingMode mode) {
    mpeInstrument.setPitchbendTrackingMode(mode);
}

void MPEInstrumentManager::setTimbreTrackingMode(MPEInstrument::TrackingMode mode) {
    mpeInstrument.setTimbreTrackingMode(mode);
}

//==============================================================================
void MPEInstrumentManager::handleMidiEvent(const MidiMessage& message) {
    mpeInstrument.processNextMidiEvent(message);
}

void MPEInstrumentManager::processEventsInBlock(const MidiBuffer& inputMidi, int startSample, int numSamples,
                                               std::function<void(const MidiMessage&, int)> eventCallback) {
    const ScopedLock sl(noteStateLock);
    
    auto prevSample = startSample;
    const auto endSample = startSample + numSamples;
    
    for (auto it = inputMidi.findNextSamplePosition(startSample); it != inputMidi.cend(); ++it) {
        const auto metadata = *it;
        
        if (metadata.samplePosition >= endSample)
            break;
            
        const auto smallBlockAllowed = (prevSample == startSample && !subBlockSubdivisionIsStrict);
        const auto thisBlockSize = smallBlockAllowed ? 1 : minimumSubBlockSize;
        
        if (metadata.samplePosition >= prevSample + thisBlockSize) {
            prevSample = metadata.samplePosition;
        }
        
        // Process MIDI event and notify callback
        handleMidiEvent(metadata.getMessage());
        if (eventCallback) {
            eventCallback(metadata.getMessage(), metadata.samplePosition);
        }
    }
}

//==============================================================================
void MPEInstrumentManager::setMinimumRenderingSubdivisionSize(int numSamples, bool shouldBeStrict) noexcept {
    jassert(numSamples > 0);
    minimumSubBlockSize = numSamples;
    subBlockSubdivisionIsStrict = shouldBeStrict;
}

//==============================================================================
void MPEInstrumentManager::addListener(MPEInstrument::Listener* listener) {
    mpeInstrument.addListener(listener);
}

void MPEInstrumentManager::removeListener(MPEInstrument::Listener* listener) {
    mpeInstrument.removeListener(listener);
}

//==============================================================================
void MPEInstrumentManager::releaseAllNotes() {
    mpeInstrument.releaseAllNotes();
}

}  // namespace MoTool::uZX
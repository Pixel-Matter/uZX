#pragma once

#include <JuceHeader.h>
#include <functional>

namespace MoTool::uZX {

//==============================================================================
/**
    Manages voice allocation, deallocation, and stealing algorithms.
    Pure voice management with no MPE-specific logic.
    
    This class handles:
    - Voice collection management (add/remove/clear)
    - Voice allocation and stealing algorithms
    - Voice rendering coordination
    - Play rate management for voices
    
    @see MPEEffect, MPEInstrumentManager
*/
template <class Voice>
class VoiceManager {
public:
    //==============================================================================
    VoiceManager() = default;
    ~VoiceManager() = default;
    
    //==============================================================================
    // Voice Collection Management
    
    /** Adds a new voice to the manager. */
    void addVoice(Voice* newVoice);
    
    /** Removes all voices. */
    void clearVoices();
    
    /** Removes a voice by index. */
    void removeVoice(int index);
    
    /** Reduces the number of voices to the specified count. */
    void reduceNumVoices(int newNumVoices);
    
    /** Returns the number of voices. */
    int getNumVoices() const noexcept { return voices.size(); }
    
    /** Returns a voice by index. */
    Voice* getVoice(int index) const;
    
    //==============================================================================
    // Voice Allocation/Deallocation
    
    /** Finds a free voice for the given note. */
    Voice* findFreeVoice(MPENote noteToFindVoiceFor, bool stealIfNoneAvailable) const;
    
    /** Finds a voice to steal based on the stealing algorithm. */
    Voice* findVoiceToSteal(MPENote noteToStealVoiceFor = MPENote()) const;
    
    /** Starts a voice playing the given note. */
    void startVoice(Voice* voice, MPENote noteToStart);
    
    /** Stops a voice playing the given note. */
    void stopVoice(Voice* voice, MPENote noteToStop, bool allowTailOff);
    
    //==============================================================================
    // Voice Control
    
    /** Turns off all voices. */
    void turnOffAllVoices(bool allowTailOff);
    
    /** Enables or disables voice stealing. */
    void setVoiceStealingEnabled(bool shouldSteal) noexcept { shouldStealVoices = shouldSteal; }
    
    /** Returns whether voice stealing is enabled. */
    bool isVoiceStealingEnabled() const noexcept { return shouldStealVoices; }
    
    //==============================================================================
    // Play Rate Management
    
    /** Sets the current play rate for all voices. */
    void setCurrentPlayRate(double newRate);
    
    /** Returns the current play rate. */
    double getPlayRate() const noexcept { return currentPlayRate; }
    
    //==============================================================================
    // Rendering
    
    /** Renders the next block through all active voices. */
    void renderNextBlock(MidiBuffer& outputMidi, int startSample, int numSamples);
    
    //==============================================================================
    // Voice Finding Utilities
    
    /** Finds a voice currently playing the given note. */
    Voice* findVoicePlayingNote(MPENote note) const;
    
    /** Calls a function for each voice playing the given note. */
    void forEachVoicePlayingNote(MPENote note, std::function<void(Voice*)> callback);
    
    /** Calls a function for each voice playing any note matching the given criteria. */
    template <typename Predicate>
    void forEachVoiceWhere(Predicate&& predicate, std::function<void(Voice*)> callback);
    
private:
    //==============================================================================
    OwnedArray<Voice> voices;
    mutable CriticalSection voicesLock;
    
    std::atomic<bool> shouldStealVoices{false};
    uint32 lastNoteOnCounter = 0;
    double currentPlayRate = 0.0;
    
    // Voice stealing optimization
    mutable CriticalSection stealLock;
    mutable Array<Voice*> usableVoicesToStealArray;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceManager)
};

//==============================================================================
// Template Implementation
//==============================================================================

template <class Voice>
void VoiceManager<Voice>::addVoice(Voice* newVoice) {
    {
        const ScopedLock sl(voicesLock);
        newVoice->setCurrentPlayRate(currentPlayRate);
        voices.add(newVoice);
    }
    
    {
        const ScopedLock sl(stealLock);
        usableVoicesToStealArray.ensureStorageAllocated(voices.size() + 1);
    }
}

template <class Voice>
void VoiceManager<Voice>::clearVoices() {
    const ScopedLock sl(voicesLock);
    voices.clear();
}

template <class Voice>
Voice* VoiceManager<Voice>::getVoice(int index) const {
    const ScopedLock sl(voicesLock);
    return voices[index];
}

template <class Voice>
void VoiceManager<Voice>::removeVoice(int index) {
    const ScopedLock sl(voicesLock);
    voices.remove(index);
}

template <class Voice>
void VoiceManager<Voice>::reduceNumVoices(int newNumVoices) {
    jassert(newNumVoices >= 0);
    
    const ScopedLock sl(voicesLock);
    
    while (voices.size() > newNumVoices) {
        if (auto* voice = findFreeVoice({}, true))
            voices.removeObject(voice);
        else
            voices.remove(0);
    }
}

template <class Voice>
void VoiceManager<Voice>::setCurrentPlayRate(double newRate) {
    if (!approximatelyEqual(currentPlayRate, newRate)) {
        const ScopedLock sl(voicesLock);
        
        turnOffAllVoices(false);
        currentPlayRate = newRate;
        
        for (auto* voice : voices)
            voice->setCurrentPlayRate(newRate);
    }
}

template <class Voice>
void VoiceManager<Voice>::startVoice(Voice* voice, MPENote noteToStart) {
    jassert(voice != nullptr);
    
    voice->currentlyPlayingNote = noteToStart;
    voice->noteOnOrder = lastNoteOnCounter++;
    voice->noteStarted();
}

template <class Voice>
void VoiceManager<Voice>::stopVoice(Voice* voice, MPENote noteToStop, bool allowTailOff) {
    jassert(voice != nullptr);
    
    voice->currentlyPlayingNote = noteToStop;
    voice->noteStopped(allowTailOff);
}

template <class Voice>
Voice* VoiceManager<Voice>::findFreeVoice(MPENote noteToFindVoiceFor, bool stealIfNoneAvailable) const {
    const ScopedLock sl(voicesLock);
    
    for (auto* voice : voices) {
        if (!voice->isActive())
            return voice;
    }
    
    if (stealIfNoneAvailable)
        return findVoiceToSteal(noteToFindVoiceFor);
    
    return nullptr;
}

template <class Voice>
Voice* VoiceManager<Voice>::findVoiceToSteal(MPENote noteToStealVoiceFor) const {
    jassert(voices.size() > 0);
    
    Voice* low = nullptr;
    Voice* top = nullptr;
    
    const ScopedLock sl(stealLock);
    
    usableVoicesToStealArray.clear();
    
    for (auto* voice : voices) {
        jassert(voice->isActive());
        
        usableVoicesToStealArray.add(voice);
        
        struct Sorter {
            bool operator()(const Voice* a, const Voice* b) const noexcept {
                return a->noteOnOrder < b->noteOnOrder;
            }
        };
        
        std::sort(usableVoicesToStealArray.begin(), usableVoicesToStealArray.end(), Sorter());
        
        if (!voice->isPlayingButReleased()) {
            auto noteNumber = voice->getCurrentlyPlayingNote().initialNote;
            
            if (low == nullptr || noteNumber < low->getCurrentlyPlayingNote().initialNote)
                low = voice;
            
            if (top == nullptr || noteNumber > top->getCurrentlyPlayingNote().initialNote)
                top = voice;
        }
    }
    
    if (top == low)
        top = nullptr;
    
    if (noteToStealVoiceFor.isValid())
        for (auto* voice : usableVoicesToStealArray)
            if (voice->getCurrentlyPlayingNote().initialNote == noteToStealVoiceFor.initialNote)
                return voice;
    
    for (auto* voice : usableVoicesToStealArray)
        if (voice != low && voice != top && voice->isPlayingButReleased())
            return voice;
    
    for (auto* voice : usableVoicesToStealArray)
        if (voice != low && voice != top && voice->getCurrentlyPlayingNote().keyState != MPENote::keyDown
            && voice->getCurrentlyPlayingNote().keyState != MPENote::keyDownAndSustained)
            return voice;
    
    for (auto* voice : usableVoicesToStealArray)
        if (voice != low && voice != top)
            return voice;
    
    jassert(low != nullptr);
    
    if (top != nullptr)
        return top;
    
    return low;
}

template <class Voice>
void VoiceManager<Voice>::turnOffAllVoices(bool allowTailOff) {
    const ScopedLock sl(voicesLock);
    
    for (auto* voice : voices) {
        voice->currentlyPlayingNote.noteOffVelocity = MPEValue::from7BitInt(64);
        voice->currentlyPlayingNote.keyState = MPENote::off;
        voice->noteStopped(allowTailOff);
    }
}

template <class Voice>
void VoiceManager<Voice>::renderNextBlock(MidiBuffer& outputMidi, int startSample, int numSamples) {
    const ScopedLock sl(voicesLock);
    
    for (auto* voice : voices) {
        if (voice->isActive())
            voice->renderNextBlock(outputMidi, startSample, numSamples);
    }
}

template <class Voice>
Voice* VoiceManager<Voice>::findVoicePlayingNote(MPENote note) const {
    const ScopedLock sl(voicesLock);
    
    for (auto* voice : voices) {
        if (voice->isCurrentlyPlayingNote(note))
            return voice;
    }
    
    return nullptr;
}

template <class Voice>
void VoiceManager<Voice>::forEachVoicePlayingNote(MPENote note, std::function<void(Voice*)> callback) {
    const ScopedLock sl(voicesLock);
    
    for (auto* voice : voices) {
        if (voice->isCurrentlyPlayingNote(note))
            callback(voice);
    }
}

template <class Voice>
template <typename Predicate>
void VoiceManager<Voice>::forEachVoiceWhere(Predicate&& predicate, std::function<void(Voice*)> callback) {
    const ScopedLock sl(voicesLock);
    
    for (auto* voice : voices) {
        if (predicate(voice))
            callback(voice);
    }
}

}  // namespace MoTool::uZX
#pragma once

#include <JuceHeader.h>
#include <functional>

#include "MPEEffectVoice.h"

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
template <class Voice, class OwnerMidiFx>
class VoiceManager {
public:
    //==============================================================================
    VoiceManager(OwnerMidiFx& owner) : midiFx(owner) {
        ensureNumVoices(3);
        setReuseSameVoiceForSameNote(true);
    }

    ~VoiceManager() = default;

    void setReuseSameVoiceForSameNote(bool reuse) noexcept {
        reuseSameVoiceForSameNote = reuse;
    }

    bool shouldReuseSameVoiceForSameNote() const noexcept {
        return reuseSameVoiceForSameNote;
    }

    //==============================================================================
    // Voice Collection Management

    /** Adds a new voice to the manager. */
    void addVoice(Voice* newVoice) {
        {
            const ScopedLock sl(voicesLock);
            // newVoice->setCurrentPlayRate(currentPlayRate);
            voices.add(newVoice);
        }

        {
            const ScopedLock sl(stealLock);
            usableVoicesToStealArray.ensureStorageAllocated(voices.size() + 1);
        }
    }

    /** Removes all voices. */
    void clearVoices() {
        const ScopedLock sl(voicesLock);
        voices.clear();
    }

    /** Removes a voice by index. */
    void removeVoice(int index) {
        const ScopedLock sl(voicesLock);
        voices.remove(index);
    }

    void ensureNumVoices(int newNumVoices) {
        jassert(newNumVoices >= 0);

        reduceNumVoices(newNumVoices);

        {
            const ScopedLock sl(voicesLock);
            voices.ensureStorageAllocated(newNumVoices + 1);
        }

        while (voices.size() < newNumVoices) {
            addVoice(new Voice(midiFx));
        }
    }

    /** Reduces the number of voices to the specified count. */
    void reduceNumVoices(int newNumVoices) {
        jassert(newNumVoices >= 0);

        const ScopedLock sl(voicesLock);

        while (voices.size() > newNumVoices) {
            if (auto* voice = findFreeVoice({}, true))
                voices.removeObject(voice);
            else
                voices.remove(0);
        }
    }

    /** Returns the number of voices. */
    int getNumVoices() const noexcept { return voices.size(); }

    /** Returns a voice by index. */
    Voice* getVoice(int index) const {
        const ScopedLock sl(voicesLock);
        return voices[index];
    }

    //==============================================================================
    // Voice Allocation/Deallocation

    /** Finds a free voice for the given note. */
    Voice* findFreeVoice(MPENote noteToFindVoiceFor, bool stealIfNoneAvailable) const {
        const ScopedLock sl(voicesLock);

        if (shouldReuseSameVoiceForSameNote()) {
            for (auto* voice : voices) {
                if (voice->isCurrentlyPlayingNote(noteToFindVoiceFor))
                    return voice;
            }
        }

        for (auto* voice : voices) {
            if (!voice->isActive())
                return voice;
        }

        if (stealIfNoneAvailable)
            return findVoiceToSteal(noteToFindVoiceFor);

        return nullptr;
    }

    /** Finds a voice to steal based on the stealing algorithm. */
    Voice* findVoiceToSteal(MPENote noteToStealVoiceFor = MPENote()) const {
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

    /** Starts a voice playing the given note. */
    void startVoice(Voice* voice, MPENote noteToStart) {
        jassert(voice != nullptr);

        voice->currentlyPlayingNote = noteToStart;
        voice->noteOnOrder = lastNoteOnCounter++;
        voice->noteStarted();
    }

    void addNote(MPENote newNote) {
        const ScopedLock sl(voicesLock);
        if (auto* voice = findFreeVoice(newNote, isVoiceStealingEnabled())) {
            startVoice(voice, newNote);
        } else {
            DBG("No voice available to play note " << newNote.initialNote);
        }
    }

    void releaseNote(MPENote finishedNote) {
        const ScopedLock sl(voicesLock);

        for (auto i = voices.size(); --i >= 0;) {
            auto* voice = voices.getUnchecked(i);

            if (voice->isCurrentlyPlayingNote(finishedNote))
                stopVoice(voice, finishedNote, true);
        }
    }

    void pitchbendNote(MPENote bendNote) {
        const ScopedLock sl(voicesLock);

        for (auto i = voices.size(); --i >= 0;) {
            auto* voice = voices.getUnchecked(i);

            if (voice->isCurrentlyPlayingNote(bendNote))
                voice->currentlyPlayingNote = bendNote;
                voice->notePitchbendChanged();
        }
    }

    void pressureNote(MPENote pressureNote) {
        const ScopedLock sl(voicesLock);

        for (auto i = voices.size(); --i >= 0;) {
            auto* voice = voices.getUnchecked(i);

            if (voice->isCurrentlyPlayingNote(pressureNote))
                voice->currentlyPlayingNote = pressureNote;
                voice->notePressureChanged();
        }
    }

    void timbreNote(MPENote timbreNote) {
        const ScopedLock sl(voicesLock);

        for (auto i = voices.size(); --i >= 0;) {
            auto* voice = voices.getUnchecked(i);

            if (voice->isCurrentlyPlayingNote(timbreNote))
                voice->currentlyPlayingNote = timbreNote;
                voice->noteTimbreChanged();
        }
    }

    /** Stops a voice playing the given note. */
    void stopVoice(Voice* voice, MPENote noteToStop, bool allowTailOff) {
        // DBG("stopVoice " << noteToStop.initialNote << (allowTailOff ? " tail" : " no tail")
        //     << " state = " << noteToStop.keyState
        // );
        jassert(voice != nullptr);

        voice->currentlyPlayingNote = noteToStop;
        voice->noteStopped(allowTailOff);
    }

    //==============================================================================
    // Voice Control

    /** Turns off all voices. */
    void turnOffAllVoices(bool allowTailOff) {
        // DBG("turnOffAllVoices " << (allowTailOff ? " tail off" : " no tail off") << " ===================");
        const ScopedLock sl(voicesLock);

        for (auto* voice : voices) {
            voice->currentlyPlayingNote.noteOffVelocity = MPEValue::from7BitInt(64);
            voice->currentlyPlayingNote.keyState = MPENote::off;
            voice->noteStopped(allowTailOff);
        }
    }

    /** Enables or disables voice stealing. */
    void setVoiceStealingEnabled(bool shouldSteal) noexcept { shouldStealVoices = shouldSteal; }

    /** Returns whether voice stealing is enabled. */
    bool isVoiceStealingEnabled() const noexcept { return shouldStealVoices; }

    //==============================================================================
    // Play Rate Management

    /** Sets the current play rate for all voices. */
    void setCurrentPlayRate(double newRate) {
        if (!approximatelyEqual(currentPlayRate, newRate)) {
            const ScopedLock sl(voicesLock);

            turnOffAllVoices(false);
            currentPlayRate = newRate;

            for (auto* voice : voices)
                voice->setCurrentPlayRate(newRate);
        }
    }

    /** Returns the current play rate. */
    double getPlayRate() const noexcept { return currentPlayRate; }

    //==============================================================================
    // Voice Finding Utilities

    /** Calls a function for each active voice. */
    void forEachActiveVoice(std::function<void(Voice*)> callback) {
        const ScopedLock sl(voicesLock);

        for (auto* voice : voices) {
            if (voice->isActive())
                callback(voice);
        }
    }

    /** Finds a voice currently playing the given note. */
    Voice* findVoicePlayingNote(MPENote note) const {
        const ScopedLock sl(voicesLock);

        for (auto* voice : voices) {
            if (voice->isCurrentlyPlayingNote(note))
                return voice;
        }

        return nullptr;
    }

    /** Calls a function for each voice playing the given note. */
    void forEachVoicePlayingNote(MPENote note, std::function<void(Voice*)> callback) {
        const ScopedLock sl(voicesLock);

        for (auto* voice : voices) {
            if (voice->isCurrentlyPlayingNote(note))
                callback(voice);
        }
    }

    /** Calls a function for each voice playing any note matching the given criteria. */
    template <typename Predicate>
    void forEachVoiceWhere(Predicate&& predicate, std::function<void(Voice*)> callback) {
        const ScopedLock sl(voicesLock);

        for (auto* voice : voices) {
            if (predicate(voice))
                callback(voice);
        }
    }

private:
    //==============================================================================
    OwnedArray<Voice> voices;
    mutable CriticalSection voicesLock;
    OwnerMidiFx& midiFx;

    std::atomic<bool> shouldStealVoices {true};
    std::atomic<bool> reuseSameVoiceForSameNote {true};
    uint32 lastNoteOnCounter = 0;
    double currentPlayRate = 0.0;

    // Voice stealing optimization
    mutable CriticalSection stealLock;
    mutable Array<Voice*> usableVoicesToStealArray;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceManager)
};


}  // namespace MoTool::uZX
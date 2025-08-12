#include "MPEEffect.h"

namespace MoTool::uZX {

MPEEffect::MPEEffect() {}

MPEEffect::MPEEffect(MPEInstrument& mpeInstrument) : MPEEffectBase(mpeInstrument) {}

MPEEffect::~MPEEffect() {}

//==============================================================================
void MPEEffect::startVoice(MPEEffectVoice* voice, MPENote noteToStart) {
    jassert(voice != nullptr);

    voice->currentlyPlayingNote = noteToStart;
    voice->noteOnTime = lastNoteOnCounter++;
    voice->noteStarted();
}

void MPEEffect::stopVoice(MPEEffectVoice* voice, MPENote noteToStop, bool allowTailOff) {
    jassert(voice != nullptr);

    voice->currentlyPlayingNote = noteToStop;
    voice->noteStopped(allowTailOff);
}

//==============================================================================
void MPEEffect::noteAdded(MPENote newNote) {
    const ScopedLock sl(voicesLock);

    if (auto* voice = findFreeVoice(newNote, shouldStealVoices))
        startVoice(voice, newNote);
}

void MPEEffect::notePressureChanged(MPENote changedNote) {
    const ScopedLock sl(voicesLock);

    for (auto* voice : voices) {
        if (voice->isCurrentlyPlayingNote(changedNote)) {
            voice->currentlyPlayingNote = changedNote;
            voice->notePressureChanged();
        }
    }
}

void MPEEffect::notePitchbendChanged(MPENote changedNote) {
    const ScopedLock sl(voicesLock);

    for (auto* voice : voices) {
        if (voice->isCurrentlyPlayingNote(changedNote)) {
            voice->currentlyPlayingNote = changedNote;
            voice->notePitchbendChanged();
        }
    }
}

void MPEEffect::noteTimbreChanged(MPENote changedNote) {
    const ScopedLock sl(voicesLock);

    for (auto* voice : voices) {
        if (voice->isCurrentlyPlayingNote(changedNote)) {
            voice->currentlyPlayingNote = changedNote;
            voice->noteTimbreChanged();
        }
    }
}

void MPEEffect::noteKeyStateChanged(MPENote changedNote) {
    const ScopedLock sl(voicesLock);

    for (auto* voice : voices) {
        if (voice->isCurrentlyPlayingNote(changedNote)) {
            voice->currentlyPlayingNote = changedNote;
            voice->noteKeyStateChanged();
        }
    }
}

void MPEEffect::noteReleased(MPENote finishedNote) {
    const ScopedLock sl(voicesLock);

    for (auto i = voices.size(); --i >= 0;) {
        auto* voice = voices.getUnchecked(i);

        if (voice->isCurrentlyPlayingNote(finishedNote))
            stopVoice(voice, finishedNote, true);
    }
}

void MPEEffect::setCurrentPlaybackSampleRate(const double newRate) {
    MPEEffectBase::setCurrentPlaybackSampleRate(newRate);

    const ScopedLock sl(voicesLock);

    turnOffAllVoices(false);

    for (auto i = voices.size(); --i >= 0;)
        voices.getUnchecked(i)->setCurrentPlayRate(newRate);
}

void MPEEffect::handleMidiEvent(const MidiMessage& m) {
    if (m.isController())
        handleController(m.getChannel(), m.getControllerNumber(), m.getControllerValue());
    else if (m.isProgramChange())
        handleProgramChange(m.getChannel(), m.getProgramChangeNumber());

    MPEEffectBase::handleMidiEvent(m);
}

MPEEffectVoice* MPEEffect::findFreeVoice(MPENote noteToFindVoiceFor, bool stealIfNoneAvailable) const {
    const ScopedLock sl(voicesLock);

    for (auto* voice : voices) {
        if (!voice->isActive())
            return voice;
    }

    if (stealIfNoneAvailable)
        return findVoiceToSteal(noteToFindVoiceFor);

    return nullptr;
}

MPEEffectVoice* MPEEffect::findVoiceToSteal(MPENote noteToStealVoiceFor) const {
    // This voice-stealing algorithm applies the following heuristics:
    // - Re-use the oldest notes first
    // - Protect the lowest & topmost notes, even if sustained, but not if they've been released.


    // apparently you are trying to render audio without having any voices...
    jassert(voices.size() > 0);

    // These are the voices we want to protect (ie: only steal if unavoidable)
    MPEEffectVoice* low = nullptr;  // Lowest sounding note, might be sustained, but NOT in release phase
    MPEEffectVoice* top = nullptr;  // Highest sounding note, might be sustained, but NOT in release phase

    // All major OSes use double-locking so this will be lock- and wait-free as long as stealLock is not
    // contended. This is always the case if you do not call findVoiceToSteal on multiple threads at
    // the same time.
    const ScopedLock sl(stealLock);

    // this is a list of voices we can steal, sorted by how long they've been running
    usableVoicesToStealArray.clear();

    for (auto* voice : voices) {
        jassert(voice->isActive());  // We wouldn't be here otherwise

        usableVoicesToStealArray.add(voice);

        // NB: Using a functor rather than a lambda here due to scare-stories about
        // compilers generating code containing heap allocations..
        struct Sorter
        {
            bool operator()(const MPEEffectVoice* a, const MPEEffectVoice* b) const noexcept {
                return a->noteOnTime < b->noteOnTime;
            }
        };

        std::sort(usableVoicesToStealArray.begin(), usableVoicesToStealArray.end(), Sorter());

        if (!voice->isPlayingButReleased())  // Don't protect released notes
        {
            auto noteNumber = voice->getCurrentlyPlayingNote().initialNote;

            if (low == nullptr || noteNumber < low->getCurrentlyPlayingNote().initialNote)
                low = voice;

            if (top == nullptr || noteNumber > top->getCurrentlyPlayingNote().initialNote)
                top = voice;
        }
    }

    // Eliminate pathological cases (ie: only 1 note playing): we always give precedence to the lowest note(s)
    if (top == low)
        top = nullptr;

    // If we want to re-use the voice to trigger a new note,
    // then The oldest note that's playing the same note number is ideal.
    if (noteToStealVoiceFor.isValid())
        for (auto* voice : usableVoicesToStealArray)
            if (voice->getCurrentlyPlayingNote().initialNote == noteToStealVoiceFor.initialNote)
                return voice;

    // Oldest voice that has been released (no finger on it and not held by sustain pedal)
    for (auto* voice : usableVoicesToStealArray)
        if (voice != low && voice != top && voice->isPlayingButReleased())
            return voice;

    // Oldest voice that doesn't have a finger on it:
    for (auto* voice : usableVoicesToStealArray)
        if (voice != low && voice != top && voice->getCurrentlyPlayingNote().keyState != MPENote::keyDown
            && voice->getCurrentlyPlayingNote().keyState != MPENote::keyDownAndSustained)
            return voice;

    // Oldest voice that isn't protected
    for (auto* voice : usableVoicesToStealArray)
        if (voice != low && voice != top)
            return voice;

    // We've only got "protected" voices now: lowest note takes priority
    jassert(low != nullptr);

    // Duophonic synth: give priority to the bass note:
    if (top != nullptr)
        return top;

    return low;
}

//==============================================================================
void MPEEffect::addVoice(MPEEffectVoice* const newVoice) {
    {
        const ScopedLock sl(voicesLock);
        newVoice->setCurrentPlayRate(getSampleRate());
        voices.add(newVoice);
    }

    {
        const ScopedLock sl(stealLock);
        usableVoicesToStealArray.ensureStorageAllocated(voices.size() + 1);
    }
}

void MPEEffect::clearVoices() {
    const ScopedLock sl(voicesLock);
    voices.clear();
}

MPEEffectVoice* MPEEffect::getVoice(const int index) const {
    const ScopedLock sl(voicesLock);
    return voices[index];
}

void MPEEffect::removeVoice(const int index) {
    const ScopedLock sl(voicesLock);
    voices.remove(index);
}

void MPEEffect::reduceNumVoices(const int newNumVoices) {
    // we can't possibly get to a negative number of voices...
    jassert(newNumVoices >= 0);

    const ScopedLock sl(voicesLock);

    while (voices.size() > newNumVoices) {
        if (auto* voice = findFreeVoice({}, true))
            voices.removeObject(voice);
        else
            voices.remove(0);  // if there's no voice to steal, kill the oldest voice
    }
}

void MPEEffect::turnOffAllVoices(bool allowTailOff) {
    {
        const ScopedLock sl(voicesLock);

        // first turn off all voices (it's more efficient to do this immediately
        // rather than to go through the MPEInstrument for this).
        for (auto* voice : voices) {
            voice->currentlyPlayingNote.noteOffVelocity = MPEValue::from7BitInt(64);  // some reasonable number
            voice->currentlyPlayingNote.keyState = MPENote::off;

            voice->noteStopped(allowTailOff);
        }
    }

    // finally make sure the MPE Instrument also doesn't have any notes anymore.
    mpeInstrument.releaseAllNotes();
}

//==============================================================================
void MPEEffect::renderNextSubBlock(MidiBuffer& outputMidi, int startSample, int numSamples) {
    const ScopedLock sl(voicesLock);

    for (auto* voice : voices) {
        if (voice->isActive())
            voice->renderNextBlock(outputMidi, startSample, numSamples);
    }
}

}  // namespace MoTool::uZX

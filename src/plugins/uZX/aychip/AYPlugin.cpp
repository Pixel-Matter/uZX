#include "AYPlugin.h"

#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool::uZX {


// this must be high enough for low freq sounds not to click
static constexpr int minimumSamplesToPlayWhenStopping = 8;
static constexpr int maximumSimultaneousNotes = 32;


// struct AYChipPlugin::SampledNote : public te::ReferenceCountedObject {
// public:
//     SampledNote(int midiNote, int keyNote,
//                 float velocity,
//                 const te::AudioFile& file,
//                 double sampleRate,
//                 int sampleDelayFromBufferStart,
//                 const juce::AudioBuffer<float>& data,
//                 int lengthInSamples,
//                 float gainDb,
//                 float pan,
//                 bool openEnded_)
//        : note (midiNote)
//        , offset (-sampleDelayFromBufferStart)
//        , audioData (data)
//        , openEnded (openEnded_)
//     {
//         resampler[0].reset();
//         resampler[1].reset();

//         const float volumeSliderPos = te::decibelsToVolumeFaderPosition (gainDb - (20.0f * (1.0f - velocity)));
//         getGainsFromVolumeFaderPositionAndPan (volumeSliderPos, pan, te::getDefaultPanLaw(), gains[0], gains[1]);

//         const double hz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
//         playbackRatio = hz / juce::MidiMessage::getMidiNoteInHertz (keyNote);
//         playbackRatio *= file.getSampleRate() / sampleRate;
//         samplesLeftToPlay = playbackRatio > 0 ? (1 + (int) (lengthInSamples / playbackRatio)) : 0;
//     }

//     void addNextBlock(juce::AudioBuffer<float>& outBuffer, int startSamp, int numSamples) {
//         jassert (! isFinished);

//         if (offset < 0) {
//             const int num = std::min (-offset, numSamples);
//             startSamp += num;
//             numSamples -= num;
//             offset += num;
//         }

//         auto numSamps = std::min (numSamples, samplesLeftToPlay);

//         if (numSamps > 0) {
//             int numUsed = 0;

//             for (int i = std::min (2, outBuffer.getNumChannels()); --i >= 0;) {
//                 numUsed = resampler[i]
//                             .processAdding (playbackRatio,
//                                             audioData.getReadPointer (std::min (i, audioData.getNumChannels() - 1), offset),
//                                             outBuffer.getWritePointer (i, startSamp),
//                                             numSamps,
//                                             gains[i]);
//             }

//             offset += numUsed;
//             samplesLeftToPlay -= numSamps;

//             jassert (offset <= audioData.getNumSamples());
//         }

//         if (numSamples > numSamps && startFade > 0.0f) {
//             startSamp += numSamps;
//             numSamps = numSamples - numSamps;
//             float endFade;

//             if (numSamps > 100) {
//                 endFade = 0.0f;
//                 numSamps = 100;
//             } else {
//                 endFade = std::max (0.0f, startFade - numSamps * 0.01f);
//             }

//             const int numSampsNeeded = 2 + juce::roundToInt ((numSamps + 2) * playbackRatio);
//             te::AudioScratchBuffer scratch (audioData.getNumChannels(), numSampsNeeded + 8);

//             if (offset + numSampsNeeded < audioData.getNumSamples()) {
//                 for (int i = scratch.buffer.getNumChannels(); --i >= 0;)
//                     scratch.buffer.copyFrom (i, 0, audioData, i, offset, numSampsNeeded);
//             } else {
//                 scratch.buffer.clear();
//             }

//             if (numSampsNeeded > 2)
//                 te::AudioFadeCurve::applyCrossfadeSection(scratch.buffer, 0, numSampsNeeded - 2,
//                                                           te::AudioFadeCurve::linear, startFade, endFade);

//             startFade = endFade;

//             int numUsed = 0;

//             for (int i = std::min (2, outBuffer.getNumChannels()); --i >= 0;)
//                 numUsed = resampler[i].processAdding (playbackRatio,
//                                                       scratch.buffer.getReadPointer (std::min (i, scratch.buffer.getNumChannels() - 1)),
//                                                       outBuffer.getWritePointer (i, startSamp),
//                                                       numSamps, gains[i]);

//             offset += numUsed;

//             if (startFade <= 0.0f)
//                 isFinished = true;
//         }
//     }

//     juce::LagrangeInterpolator resampler[2];
//     int note;
//     int offset, samplesLeftToPlay = 0;
//     float gains[2];
//     double playbackRatio = 1.0;
//     const juce::AudioBuffer<float>& audioData;
//     float lastVals[4] = { 0, 0, 0, 0 };
//     float startFade = 1.0f;
//     bool openEnded, isFinished = false;

// private:
//     JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampledNote)
// };


//==============================================================================
AYChipPlugin::AYChipPlugin(te::PluginCreationInfo info)
    : Plugin(info) {
    triggerAsyncUpdate();
}

AYChipPlugin::~AYChipPlugin() {
    notifyListenersOfDeletion();
}

const char* AYChipPlugin::xmlTypeName = "aychip";

void AYChipPlugin::valueTreeChanged() {
    triggerAsyncUpdate();
    te::Plugin::valueTreeChanged();
}

void AYChipPlugin::handleAsyncUpdate() {
    juce::OwnedArray<SamplerSound> newSounds;

    auto numSounds = state.getNumChildren();

    for (int i = 0; i < numSounds; ++i) {
        auto v = getSound(i);

        if (v.hasType(te::IDs::SOUND)) {
            auto s = new SamplerSound(*this,
                                     v[te::IDs::source].toString(),
                                     v[te::IDs::name],
                                     v[te::IDs::startTime],
                                     v[te::IDs::length],
                                     v[te::IDs::gainDb]);

            s->keyNote      = juce::jlimit(0, 127, static_cast<int> (v[te::IDs::keyNote]));
            s->minNote      = juce::jlimit(0, 127, static_cast<int> (v[te::IDs::minNote]));
            s->maxNote      = juce::jlimit(0, 127, static_cast<int> (v[te::IDs::maxNote]));
            s->pan          = juce::jlimit(-1.0f, 1.0f, static_cast<float> (v[te::IDs::pan]));
            s->openEnded    = v[te::IDs::openEnded];

            newSounds.add(s);
        }
    }

    for (auto newSound : newSounds) {
        for (auto s : soundList) {
            if (s->source == newSound->source
                && s->startTime == newSound->startTime
                && s->length == newSound->length) {
                newSound->audioFile = s->audioFile;
                newSound->fileStartSample = s->fileStartSample;
                newSound->fileLengthSamples = s->fileLengthSamples;
                newSound->audioData = s->audioData;
            }
        }
    }

    {
        const juce::ScopedLock sl (lock);
        allNotesOff();
        soundList.swapWith (newSounds);

        sourceMediaChanged();
    }

    newSounds.clear();
    changed();
}

void AYChipPlugin::initialise(const te::PluginInitialisationInfo&) {
    const juce::ScopedLock sl (lock);
    allNotesOff();
}

void AYChipPlugin::deinitialise() {
    allNotesOff();
}

//==============================================================================
void AYChipPlugin::playNotes([[maybe_unused]] const juce::BigInteger& keysDown) {
    // const juce::ScopedLock sl(lock);

    // if (highlightedNotes != keysDown) {
    //     for (int i = playingNotes.size(); --i >= 0;)
    //         if ((! keysDown [playingNotes.getUnchecked(i)->note])
    //              && highlightedNotes [playingNotes.getUnchecked(i)->note]
    //              && ! playingNotes.getUnchecked(i)->openEnded)
    //             playingNotes.getUnchecked(i)->samplesLeftToPlay = minimumSamplesToPlayWhenStopping;

    //     for (int note = 128; --note >= 0;) {
    //         if (keysDown [note] && ! highlightedNotes [note]) {
    //             for (auto ss : soundList) {
    //                 if (ss->minNote <= note
    //                      && ss->maxNote >= note
    //                      && ss->audioData.getNumSamples() > 0
    //                      && (! ss->audioFile.isNull())
    //                      && playingNotes.size() < maximumSimultaneousNotes) {
    //                     playingNotes.add (new SampledNote(note,
    //                                                       ss->keyNote,
    //                                                       0.75f,
    //                                                       ss->audioFile,
    //                                                       sampleRate,
    //                                                       0,
    //                                                       ss->audioData,
    //                                                       ss->fileLengthSamples,
    //                                                       ss->gainDb,
    //                                                       ss->pan,
    //                                                       ss->openEnded));
    //                 }
    //             }
    //         }
    //     }
    //     highlightedNotes = keysDown;
    // }
}

void AYChipPlugin::allNotesOff() {
    const juce::ScopedLock sl(lock);
    // playingNotes.clear();
    highlightedNotes.clear();
}

void AYChipPlugin::applyToBuffer(const te::PluginRenderContext& fc) {
    if (fc.destBuffer != nullptr) {
        SCOPED_REALTIME_CHECK

        const juce::ScopedLock sl(lock);

        te::clearChannels(*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

        // if (fc.bufferForMidiMessages != nullptr) {
        //     if (fc.bufferForMidiMessages->isAllNotesOff) {
        //         playingNotes.clear();
        //         highlightedNotes.clear();
        //     }

        //     for (auto& m : *fc.bufferForMidiMessages) {
        //         if (m.isNoteOn()) {
        //             const int note = m.getNoteNumber();
        //             const int noteTimeSample = juce::roundToInt (m.getTimeStamp() * sampleRate);

        //             // for (auto playingNote : playingNotes) {
        //             //     if (playingNote->note == note && ! playingNote->openEnded) {
        //             //         playingNote->samplesLeftToPlay = std::min(playingNote->samplesLeftToPlay,
        //             //                                                   std::max(minimumSamplesToPlayWhenStopping,
        //             //                                                            noteTimeSample));
        //             //         highlightedNotes.clearBit (note);
        //             //     }
        //             // }

        //             // for (auto ss : soundList) {
        //             //     if (ss->minNote <= note
        //             //         && ss->maxNote >= note
        //             //         && ss->audioData.getNumSamples() > 0
        //             //         && playingNotes.size() < maximumSimultaneousNotes) {
        //             //         highlightedNotes.setBit (note);

        //             //         playingNotes.add (new SampledNote(note,
        //             //                                           ss->keyNote,
        //             //                                           m.getVelocity() / 127.0f,
        //             //                                           ss->audioFile,
        //             //                                           sampleRate,
        //             //                                           noteTimeSample,
        //             //                                           ss->audioData,
        //             //                                           ss->fileLengthSamples,
        //             //                                           ss->gainDb,
        //             //                                           ss->pan,
        //             //                                           ss->openEnded));
        //             //     }
        //             // }
        //         } else if (m.isNoteOff()) {
        //             const int note = m.getNoteNumber();
        //             const int noteTimeSample = juce::roundToInt (m.getTimeStamp() * sampleRate);

        //             for (auto playingNote : playingNotes) {
        //                 if (playingNote->note == note && ! playingNote->openEnded) {
        //                     playingNote->samplesLeftToPlay = std::min (playingNote->samplesLeftToPlay,
        //                                                                std::max (minimumSamplesToPlayWhenStopping,
        //                                                                          noteTimeSample));

        //                     highlightedNotes.clearBit (note);
        //                 }
        //             }
        //         } else if (m.isAllNotesOff() || m.isAllSoundOff()) {
        //             playingNotes.clear();
        //             highlightedNotes.clear();
        //         }
        //     }
        // }

        // for (int i = playingNotes.size(); --i >= 0;) {
        //     auto sn = playingNotes.getUnchecked (i);

        //     sn->addNextBlock(*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples);

        //     if (sn->isFinished)
        //         playingNotes.remove (i);
        // }
    }
}

//==============================================================================
int AYChipPlugin::getNumSounds() const {
    return std::accumulate (state.begin(), state.end(), 0,
                            [] (int total, auto v) { return total + (v.hasType (te::IDs::SOUND) ? 1 : 0); });
}

juce::String AYChipPlugin::getSoundName(int index) const {
    return getSound (index)[te::IDs::name];
}

void AYChipPlugin::setSoundName (int index, const juce::String& n) {
    getSound (index).setProperty (te::IDs::name, n, getUndoManager());
}

bool AYChipPlugin::hasNameForMidiNoteNumber (int note, int, juce::String& noteName) {
    juce::String s;

    {
        const juce::ScopedLock sl (lock);

        for (auto ss : soundList) {
            if (ss->minNote <= note && ss->maxNote >= note) {
                if (s.isNotEmpty())
                    s << " + " << ss->name;
                else
                    s = ss->name;
            }
        }
    }

    noteName = s;
    return true;
}

te::AudioFile AYChipPlugin::getSoundFile(int index) const {
    const juce::ScopedLock sl (lock);

    if (auto s = soundList[index])
        return s->audioFile;

    return te::AudioFile(edit.engine);
}

juce::String AYChipPlugin::getSoundMedia(int index) const {
    const juce::ScopedLock sl (lock);

    if (auto s = soundList[index])
        return s->source;

    return {};
}

int AYChipPlugin::getKeyNote(int index) const             { return getSound(index)[te::IDs::keyNote]; }
int AYChipPlugin::getMinKey(int index) const              { return getSound(index)[te::IDs::minNote]; }
int AYChipPlugin::getMaxKey(int index) const              { return getSound(index)[te::IDs::maxNote]; }
float AYChipPlugin::getSoundGainDb(int index) const       { return getSound(index)[te::IDs::gainDb]; }
float AYChipPlugin::getSoundPan(int index) const          { return getSound(index)[te::IDs::pan]; }
double AYChipPlugin::getSoundStartTime(int index) const   { return getSound(index)[te::IDs::startTime]; }
bool AYChipPlugin::isSoundOpenEnded(int index) const      { return getSound(index)[te::IDs::openEnded]; }

double AYChipPlugin::getSoundLength (int index) const {
    const double l = getSound (index)[te::IDs::length];

    if (l == 0.0) {
        const juce::ScopedLock sl (lock);

        if (auto s = soundList[index])
            return s->length;
    }

    return l;
}

juce::String AYChipPlugin::addSound(const juce::String& source, const juce::String& name,
                                    double startTime, double length, float gainDb) {
    const int maxNumSamples = 64;

    if (getNumSounds() >= maxNumSamples)
        return TRANS("Can't load any more samples");

    auto v = te::createValueTree(te::IDs::SOUND,
                                 te::IDs::source, source,
                                 te::IDs::name, name,
                                 te::IDs::startTime, startTime,
                                 te::IDs::length, length,
                                 te::IDs::keyNote, 72,
                                 te::IDs::minNote, 72 - 24,
                                 te::IDs::maxNote, 72 + 24,
                                 te::IDs::gainDb, gainDb,
                                 te::IDs::pan, (double) 0);

    state.addChild (v, -1, getUndoManager());
    return {};
}

void AYChipPlugin::removeSound (int index) {
    state.removeChild (index, getUndoManager());

    const juce::ScopedLock sl (lock);
    // playingNotes.clear();
    highlightedNotes.clear();
}

void AYChipPlugin::setSoundParams (int index, int keyNote, int minNote, int maxNote) {
    auto um = getUndoManager();

    auto v = getSound (index);
    v.setProperty(te::IDs::keyNote, juce::jlimit (0, 127, keyNote), um);
    v.setProperty(te::IDs::minNote, juce::jlimit (0, 127, std::min (minNote, maxNote)), um);
    v.setProperty(te::IDs::maxNote, juce::jlimit (0, 127, std::max (minNote, maxNote)), um);
}

void AYChipPlugin::setSoundGains (int index, float gainDb, float pan) {
    auto um = getUndoManager();

    auto v = getSound (index);
    v.setProperty(te::IDs::gainDb, juce::jlimit (-48.0f, 48.0f, gainDb), um);
    v.setProperty(te::IDs::pan,    juce::jlimit (-1.0f,  1.0f,  pan), um);
}

void AYChipPlugin::setSoundExcerpt (int index, double start, double length) {
    auto um = getUndoManager();

    auto v = getSound (index);
    v.setProperty(te::IDs::startTime, start, um);
    v.setProperty(te::IDs::length, length, um);
}

void AYChipPlugin::setSoundOpenEnded (int index, bool b) {
    auto um = getUndoManager();

    auto v = getSound (index);
    v.setProperty(te::IDs::openEnded, b, um);
}

void AYChipPlugin::setSoundMedia (int index, const juce::String& source) {
    auto v = getSound (index);
    v.setProperty(te::IDs::source, source, getUndoManager());
    triggerAsyncUpdate();
}

juce::ValueTree AYChipPlugin::getSound (int soundIndex) const {
    int index = 0;

    for (auto v : state)
        if (v.hasType(te::IDs::SOUND))
            if (index++ == soundIndex)
                return v;

    return {};
}

//==============================================================================
juce::Array<te::Exportable::ReferencedItem> AYChipPlugin::getReferencedItems() {
    juce::Array<ReferencedItem> results;

    // must be careful to generate this list in the right order..
    for (int i = 0; i < getNumSounds(); ++i) {
        auto v = getSound(i);

        Exportable::ReferencedItem ref;
        ref.itemID = te::ProjectItemID::fromProperty(v, te::IDs::source);
        ref.firstTimeUsed = v[te::IDs::startTime];
        ref.lengthUsed = v[te::IDs::length];
        results.add(ref);
    }

    return results;
}

void AYChipPlugin::reassignReferencedItem (const ReferencedItem& item, te::ProjectItemID newID, double newStartTime) {
    auto index = getReferencedItems().indexOf(item);

    if (index >= 0) {
        auto um = getUndoManager();

        auto v = getSound (index);
        v.setProperty(te::IDs::source, newID.toString(), um);
        v.setProperty(te::IDs::startTime, static_cast<double> (v[te::IDs::startTime]) - newStartTime, um);
    } else {
        jassertfalse;
    }
}

void AYChipPlugin::sourceMediaChanged() {
    const juce::ScopedLock sl(lock);

    for (auto s : soundList)
        s->refreshFile();
}

void AYChipPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v) {
    te::copyValueTree(state, v, getUndoManager());
}

//==============================================================================
AYChipPlugin::SamplerSound::SamplerSound(AYChipPlugin& sf,
                                         const juce::String& source_,
                                         const juce::String& name_,
                                         const double startTime_,
                                         const double length_,
                                         const float gainDb_)
    : owner (sf),
      source (source_),
      name (name_),
      gainDb (juce::jlimit (-48.0f, 48.0f, gainDb_)),
      startTime (startTime_),
      length (length_),
      audioFile (owner.edit.engine, te::SourceFileReference::findFileFromString (owner.edit, source))
{
    setExcerpt (startTime_, length_);

    keyNote = audioFile.getInfo().loopInfo.getRootNote();

    if (keyNote < 0)
        keyNote = 72;

    maxNote = keyNote + 24;
    minNote = keyNote - 24;
}

void AYChipPlugin::SamplerSound::setExcerpt(double startTime_, double length_) {
    using namespace te;
    CRASH_TRACER

    if (! audioFile.isValid()) {
        audioFile = te::AudioFile(owner.edit.engine, te::SourceFileReference::findFileFromString(owner.edit, source));

       #if JUCE_DEBUG
        if (! audioFile.isValid() && te::ProjectItemID(source).isValid())
            DBG ("Failed to find media: " << source);
       #endif
    }

    if (audioFile.isValid()) {
        const double minLength = 32.0 / audioFile.getSampleRate();

        startTime = juce::jlimit(0.0, audioFile.getLength() - minLength, startTime_);

        if (length_ > 0)
            length = juce::jlimit(minLength, audioFile.getLength() - startTime, length_);
        else
            length = audioFile.getLength();

        fileStartSample   = juce::roundToInt(startTime * audioFile.getSampleRate());
        fileLengthSamples = juce::roundToInt(length * audioFile.getSampleRate());

        if (auto reader = owner.engine.getAudioFileManager().cache.createReader(audioFile)) {
            audioData.setSize (audioFile.getNumChannels(), fileLengthSamples + 32);
            audioData.clear();

            auto audioDataChannelSet = juce::AudioChannelSet::canonicalChannelSet(audioFile.getNumChannels());
            auto channelsToUse = juce::AudioChannelSet::stereo();

            int total = fileLengthSamples;
            int offset = 0;

            while (total > 0) {
                const int numThisTime = std::min(8192, total);
                reader->setReadPosition(fileStartSample + offset);

                if (! reader->readSamples(numThisTime, audioData, audioDataChannelSet, offset, channelsToUse, 2000)) {
                    jassertfalse;
                    break;
                }

                offset += numThisTime;
                total -= numThisTime;
            }
        } else {
            audioData.clear();
        }

        // add a quick fade-in if needed..
        int fadeLen = 0;
        for (int i = audioData.getNumChannels(); --i >= 0;) {
            const float* d = audioData.getReadPointer (i);

            if (std::abs (*d) > 0.01f)
                fadeLen = 30;
        }

        if (fadeLen > 0)
            AudioFadeCurve::applyCrossfadeSection(audioData, 0, fadeLen, AudioFadeCurve::concave, 0.0f, 1.0f);
    } else {
        audioFile = te::AudioFile(owner.edit.engine);
    }
}

void AYChipPlugin::SamplerSound::refreshFile() {
    audioFile = te::AudioFile(owner.edit.engine);
    setExcerpt(startTime, length);
}

} // namespace MoTool::uZX

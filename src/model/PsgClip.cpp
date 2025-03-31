#include <JuceHeader.h>
#include <memory>

#include "PsgClip.h"
#include "Ids.h"
#include "PsgMidi.h"
#include "PsgList.h"
#include "../formats/psg/PsgFile.h"

namespace te = tracktion;

namespace MoTool {

void PsgClip::initialise() {
    te::MidiClip::initialise();

    // loaad PSG list
    auto um = getUndoManager();
    auto psg = state.getChildWithName(IDs::PSG);
    if (psg.isValid())
        psgList = std::make_unique<PsgList>(psg, um);
    else
        state.addChild(PsgList::createPsgList(), -1, um);

    if (getColour() == getDefaultColour()) {
        auto track = getTrack();
        // TODO make clip colors use themed palette
        float hue = (track->getIndexInEditTrackList() % 18) * 1.0f / 18.0f + 0.3f;
        setColour(getDefaultColour().withHue(hue));
    }
    // Not sure we should have clip plugins yet
    // ensureHasAYPlugin();
}

String PsgClip::getSelectableDescription() {
    return "PSG AY data clip - " + getName();
}

PsgClip::Ptr PsgClip::insertTo(
    te::ClipOwner& owner,
    uZX::PsgFile& psgFile,
    te::ClipPosition position
) {
    auto* clip = dynamic_cast<PsgClip*>(CustomClip::insertClipWithState(
        owner,
        /*stateToUse=*/ {},
        psgFile.getFile().getFileNameWithoutExtension(),
        CustomClip::Type::psg,
        position,
        te::DeleteExistingClips::yes,
        false
    ));
    jassert(clip != nullptr);
    clip->getUndoManager()->beginNewTransaction();
    clip->loadFromFile(psgFile);
    return clip;
}

void PsgClip::loadFromFile(uZX::PsgFile &psgFile) {
    psgFile.ensureRead();
    auto *um = getUndoManager();

    getSequence().clear(um);
    // Fastest midi inport
    // 1. construct MidiList state detached from everything,
    // 2. remove old sequence from the state
    // 3. add the new sequence tree directly to the clips state in one operation
    auto seqState = getSequence().state.createCopy();
    double timeElapsed;
    {
        juce::ScopedTimeMeasurement measurement(timeElapsed);
        loadMidiListStateFrom(edit, seqState, psgFile);
        PsgList psg;
        psg.loadFrom(psgFile, edit, um);
        DBG("Frames " << psg.getFrames().size());
        // for (auto frame : psg.getFrames()) {
        //     DBG("Beat " << frame->getBeatPosition());
        // }
        state.removeChild(state.getChildWithName(IDs::PSG), um);
        state.addChild(psg.state, -1, um);
        // state.removeChild(state.getChildWithName(te::IDs::SEQUENCE), um);
        // state.addChild(seqState, -1, um);
    }
    DBG("PSG clip constructed in " << timeElapsed << "s");
    changed();
    scaleVerticallyToFit();
}

}  // namespace MoTool
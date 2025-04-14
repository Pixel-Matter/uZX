#include "PsgClip.h"
#include "Ids.h"
#include "PsgMidi.h"
#include "PsgList.h"
#include "../formats/psg/PsgFile.h"

#include <cstddef>
#include <memory>

namespace te = tracktion;

namespace MoTool {

void PsgClip::initialise() {
    te::MidiClip::initialise();

    // load PSG list
    auto um = getUndoManager();
    auto psg = state.getChildWithName(IDs::PSG);
    if (psg.isValid())
        psgList = std::make_unique<PsgList>(psg, um);
    else {
        state.addChild(PsgList::createPsgList(), -1, um);
        psgList = std::make_unique<PsgList>();
    }

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
    psgFile.ensureRead();
    return insertTo(
        owner,
        psgFile.getData(),
        position,
        psgFile.getFile().getFileNameWithoutExtension()
    );
}

PsgClip::Ptr PsgClip::insertTo(
    te::ClipOwner& owner,
    uZX::PsgData& data,
    te::ClipPosition position,
    String name
) {
    auto* clip = dynamic_cast<PsgClip*>(CustomClip::insertClipWithState(
        owner,
        /*stateToUse=*/ {},
        name,
        CustomClip::Type::psg,
        position,
        te::DeleteExistingClips::yes,
        false
    ));
    jassert(clip != nullptr);
    clip->getUndoManager()->beginNewTransaction();
    clip->loadFrom(data);

    // debug checks
    auto seq = clip->getPsg().exportToPlaybackMidiSequence(*clip, te::MidiList::TimeBase::seconds);
    DBG("PSG clip created with " << seq.getNumEvents() << " events");

    PsgParamsMidiReader reader {1};
    std::vector<PsgParamFrameData> paramFromSeq;
    for (auto e : seq) {
        auto newParams = reader.read(e->message);
        if (newParams.has_value()) {
            paramFromSeq.push_back(newParams.value());
        }
    }
    paramFromSeq.push_back(reader.getParams());
    DBG("PSG clip converted to " << paramFromSeq.size() << " frames");
    // ^^^ That is sparce, this vvv is dense
    DBG("PSG data has " << data.frames.size() << " frames");

    uZX::PsgRegsFrame regsFromSeq, regsFromPsg;
    DBG("------------- Regs from params 0 -------------");
    regsFromSeq.debugPrint();
    size_t idxPsg = 0;
    for (size_t i = 0; i < paramFromSeq.size() && idxPsg < data.frames.size(); ++i, ++idxPsg) {
        paramFromSeq[i].updateRegisters(regsFromSeq);
        // auto regsFromSeq = paramFromSeq[i].toRegisters();

        while (idxPsg < data.frames.size() && data.frames[idxPsg].isEmpty()) {
            ++idxPsg;
            // DBG("Skipping empty frame " << idxPsg);
        };
        regsFromPsg.update(data.frames[idxPsg]);
        if (i < 7) {
            DBG("------------------------------------------------------------ at frame " << i << ", psg frame " << idxPsg);
            if (regsFromPsg.registers != regsFromSeq.registers) {
            // if (data.frames[idxPsg].isSupersetOf(regsFromSeq)) {
                DBG("Registers do NOT match after conversion from params");
            }
            DBG("------------- Params -------------");
            paramFromSeq[i].debugPrint();
            DBG("------------- Regs from params -------------");
            regsFromSeq.debugPrint();
            DBG("------------- PSG -------------");
            regsFromPsg.debugPrint();
            // break;
        }
        regsFromSeq.clear();
        regsFromPsg.clear();
    }
    // DBG("------------------------------------------------------------");
    // if (idxPsg < data.frames.size()) {
    //     DBG("PSG data has " << data.frames.size() << " frames, but only " << paramFromSeq.size() << " params");
    // }
    // end of debug checks

    return clip;
}

void PsgClip::loadFrom(uZX::PsgData &data) {
    auto *um = getUndoManager();

    // Fastest midi inport
    // 1. construct MidiList state detached from everything,
    // 2. remove old sequence from the state
    // 3. add the new sequence tree directly to the clips state in one operation
    getSequence().clear(um);
    auto seqState = getSequence().state.createCopy();
    double timeElapsed;
    {
        juce::ScopedTimeMeasurement measurement(timeElapsed);
        loadMidiListStateFrom(edit, seqState, data);
        state.removeChild(state.getChildWithName(te::IDs::SEQUENCE), um);
        state.addChild(seqState, -1, um);

        getPsg().clear();
        PsgList psg;
        psg.loadFrom(data, edit, um);
        state.removeChild(state.getChildWithName(IDs::PSG), um);
        state.addChild(psg.state, -1, um);
        initialise();
    }
    DBG("PSG clip constructed in " << timeElapsed << "s");
    changed();
    scaleVerticallyToFit();
}

void PsgClip::loadFrom(uZX::PsgFile &psgFile) {
    psgFile.ensureRead();
    loadFrom(psgFile.getData());
}

}  // namespace MoTool
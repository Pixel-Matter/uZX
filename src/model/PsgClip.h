#pragma once

#include <JuceHeader.h>

#include "CustomClip.h"
#include "../formats/psg/PsgFile.h"
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "tracktion_engine/tracktion_engine.h"

namespace te = tracktion;

namespace MoTool {

namespace {

double roundTo(double value, int decimalPlaces = 2) {
    double factor = std::pow(10.0, decimalPlaces);
    return std::round(value * factor) / factor;
}

}

class PsgClip : public te::MidiClip, public CustomClip {
public:
    using Ptr = juce::ReferenceCountedObjectPtr<PsgClip>;

    PsgClip(const juce::ValueTree& v, te::EditItemID id, te::ClipOwner& parent_)
        : te::MidiClip(v, id, parent_)
    {}

    void initialise() override {
        te::MidiClip::initialise();
        if (getColour() == getDefaultColour()) {
            auto track = getTrack();
            // TODO make clip colors use themed palette
            float hue = (track->getIndexInEditTrackList() % 18) * 1.0f / 18.0f + 0.3f;
            setColour(getDefaultColour().withHue(hue));
        }
        // Not sure we should have clip plugins yet
        // ensureHasAYPlugin();
    }

    String getSelectableDescription() override {
        return "PSG AY data clip - " + getName();
    }

    Colour getDefaultColour() const override {
        return Colours::blue;
    }

    static Ptr insertTo(
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
            te::DeleteExistingClips::no,
            false
        ));
        jassert(clip != nullptr);
        clip->getUndoManager()->beginNewTransaction();
        clip->loadFromFile(psgFile);
        return clip;
    }

private:
    inline ValueTree createRegValueTree(te::BeatRange range, int reg, int val) {
        // N.B. Tracktion store controller values in edit's MidiList with extra precision
        // but then rounds them to 7 bits
        return te::createValueTree (
            te::IDs::CONTROL,
            te::IDs::b,     roundTo(range.getStart().inBeats()),
            te::IDs::type,  20 + reg,
            te::IDs::val,   (val & 255)  // store as is, then break down to 2 times by 4 bits
        );
    }

    void loadFromFile(uZX::PsgFile& psgFile) {
        te::MidiList& seq = getSequence();
        seq.clear(getUndoManager());
        psgFile.ensureRead();
        // auto& edit = getEdit();
        const double frameRate = 50; // TODO get from edit timeOptions
        auto& data = psgFile.getData();
        for (size_t i = 0; i < data.frames.size(); i++) {
            auto& frame = data.frames[i];
            auto timeSec = psgFile.frameNumToSeconds(i, frameRate);
            auto startBeat = getContentBeatAtTime(te::TimePosition::fromSeconds(timeSec));
            auto endBeat = getContentBeatAtTime(te::TimePosition::fromSeconds(timeSec + 1.0 / frameRate));
            // DBG("Frame " << i << " time=" << timeBeat);
            for (size_t j = 0; j < frame.registers.size(); j++) {
                if (frame.mask[j]) {
                    // DBG("Register " << j << " = " << reg);
                    auto regVal = frame.registers[j];
                    // NOTE It is too slow to call seq.addControllerEvent
                    auto v = createRegValueTree({startBeat, endBeat}, static_cast<int>(j), regVal);
                    seq.state.appendChild(v, getUndoManager());
                }
            }
        }
        changed();
        scaleVerticallyToFit();
    }

    void ensureHasAYPlugin() {
        // check plugins first
        for (auto plugin : getAllPlugins()) {
            if (dynamic_cast<uZX::AYChipPlugin*>(plugin)) {
                return;
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgClip)
};

}  // namespace MoTool
#pragma once

#include <JuceHeader.h>


namespace te = tracktion;

namespace MoTool {

using namespace tracktion::literals;

namespace IDs {
    #define DECLARE_ID(name)  const juce::Identifier name(#name);
    DECLARE_ID(PSGCLIP)
    #undef DECLARE_ID
}  // namespace IDs


class CustomClip {
public:

    enum class Type {
        unknown,    /**< A placeholder for unknown items. */
        psg,        /**< PSG clip. @see PsgClip */
    };

    static te::Clip* insertClipWithState(te::ClipOwner& parent,
                                         const juce::ValueTree& stateToUse, const juce::String& name, Type type,
                                         te::ClipPosition position, te::DeleteExistingClips deleteExistingClips, bool allowSpottingAdjustment)
    {
        using namespace te;
        CRASH_TRACER
        auto& edit = parent.getClipOwnerEdit();

        if (position.getStart() >= Edit::getMaximumEditEnd())
            return {};

        if (position.time.getEnd() > Edit::getMaximumEditEnd())
            position.time.getEnd() = Edit::getMaximumEditEnd();

        if (auto track = dynamic_cast<Track*> (&parent))
            track->setFrozen(false, Track::groupFreeze);

        if (deleteExistingClips == DeleteExistingClips::yes)
            deleteRegion(parent, position.time);

        auto newClipID = edit.createNewItemID();

        juce::ValueTree newState;

        if (stateToUse.isValid()) {
            // Custom clips cannot use Tracktion clip types
            jassert(stateToUse.hasType(clipTypeToXMLType(type)));
            newState = stateToUse;
            updateClipState(newState, name, newClipID, position);
        } else {
            newState = createNewClipState(name, type, newClipID, position);
        }

        if (auto newClip = te::insertClipWithState(parent, newState)) {
            if (allowSpottingAdjustment)
                newClip->setStart(std::max(0_tp, newClip->getPosition().getStart() - toDuration (newClip->getSpottingPoint())), false, false);

            return newClip;
        }

        return {};
    }

private:
    static juce::Identifier clipTypeToXMLType(Type t) {
        switch (t) {
            case Type::psg:           return IDs::PSGCLIP;
            case Type::unknown:
            default:                  jassertfalse; return nullptr;
        }
    }

    inline static void updateClipState(ValueTree& state, const String& name,
                                       te::EditItemID itemID, te::ClipPosition position) {
        te::addValueTreeProperties(state,
            te::IDs::name, name,
            te::IDs::start, position.getStart().inSeconds(),
            te::IDs::length, position.getLength().inSeconds(),
            te::IDs::offset, position.getOffset().inSeconds()
        );

        itemID.writeID(state, nullptr);
    }

    inline static ValueTree createNewClipState(const String& name, Type type,
                                               te::EditItemID itemID, te::ClipPosition position) {
        juce::ValueTree state(clipTypeToXMLType(type));
        updateClipState(state, name, itemID, position);
        return state;
    }
};

}  // namespace MoTool
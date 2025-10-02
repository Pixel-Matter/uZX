#pragma once

#include <JuceHeader.h>
#include "../../controllers/ParamAttachments.h"
#include "../../controllers/Parameters.h"
#include "../../util/enumchoice.h"

namespace MoTool {

namespace te = tracktion;

class ChoiceButton : public TextButton,
                     private ChangeListener  // MidiMapping change listener
{
public:
    template <Util::EnumChoiceConcept Type>
    ChoiceButton(te::Plugin& plugin, ParameterValue<Type>& value)
        : attachment(this, plugin.getAutomatableParameterByID(value.definition.paramID), value)
    {
        const auto& def = value.definition;

        setText(def.shortLabel, dontSendNotification);
    }
private:
    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &attachment.midiMapping) {
            repaint();
        }
    }

    ButtonAutoParamAttachment attachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChoiceButton)
};

}  // namespace MoTool

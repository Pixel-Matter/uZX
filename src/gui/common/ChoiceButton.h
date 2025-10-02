#pragma once

#include <JuceHeader.h>
#include "ParamAttachments.h"
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
        : binding(*this, plugin.getAutomatableParameterByID(value.definition.paramID), value)
    {
        const auto& def = value.definition;

        setText(def.shortLabel, dontSendNotification);

        binding.midiMapping.addChangeListener(this);
    }

    ~ChoiceButton() override {
        binding.midiMapping.removeChangeListener(this);
    }
private:
    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &binding.midiMapping) {
            repaint();
        }
    }

    ButtonParamBinding binding;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChoiceButton)
};

}  // namespace MoTool

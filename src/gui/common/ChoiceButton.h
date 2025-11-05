#pragma once

#include <JuceHeader.h>
#include "ParamBindings.h"
#include "../../controllers/Parameters.h"
#include "../../util/enumchoice.h"

namespace MoTool {

namespace te = tracktion;

class ChoiceButton : public TextButton,
                     private ChangeListener  // MidiMapping change listener
{
public:
    template <Util::EnumChoiceConcept Type>
    ChoiceButton(te::AutomatableEditItem& plugin, ParameterValue<Type>& value)
        : binding(*this, makeResolveParamEndpoint(plugin, value))
    {
        binding.midiMapping.addChangeListener(this);
    }

    ~ChoiceButton() override {
        binding.midiMapping.removeChangeListener(this);
    }

private:
    void changeListenerCallback(ChangeBroadcaster* /*source*/) override {
        // TODO if mapped button should show different state
        // if (source == &binding.midiMapping) {
        //     repaint();
        // }
    }

    ButtonParamEndpointBinding binding;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChoiceButton)
};

}  // namespace MoTool

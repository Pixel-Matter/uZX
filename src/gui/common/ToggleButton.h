#pragma once

#include <JuceHeader.h>
#include "ParamBindings.h"
#include "../../controllers/Parameters.h"

namespace MoTool {

namespace te = tracktion;

class ToggleButton : public TextButton,
                     private ChangeListener  // MidiMapping change listener
{
public:
    ToggleButton(te::AutomatableEditItem& plugin, ParameterValue<bool>& value)
        : binding(*this, makeResolveParamEndpoint(plugin, value), value.definition.shortLabel)
    {
        binding.midiMapping.addChangeListener(this);
    }

    ~ToggleButton() override {
        binding.midiMapping.removeChangeListener(this);
    }

private:
    void changeListenerCallback(ChangeBroadcaster* /*source*/) override {
        // TODO if mapped button should show different state
        // if (source == &binding.midiMapping) {
        //     repaint();
        // }
    }

    ToggleParamEndpointBinding binding;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToggleButton)
};

}  // namespace MoTool

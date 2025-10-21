#pragma once

#include <JuceHeader.h>
#include "../../controllers/Parameters.h"

#include "ParamBindings.h"
#include "MidiParameterMapping.h"
#include "juce_audio_processors/juce_audio_processors.h"

namespace MoTool {

namespace te = tracktion;

/**
    Compound control that displays a `ComboBox` with a caption and keeps it bound
    to a parameter.
*/
class LabeledComboBox : public Component,
                        private ChangeListener  // MidiMapping change listener
{
public:
    LabeledComboBox(std::unique_ptr<ParameterEndpoint> endpoint)
        : binding(comboBox, std::move(endpoint))
    {
        label.setText(binding.endpoint().getName(), dontSendNotification);
        label.setJustificationType(Justification::left);
        label.setFont(label.getFont().withPointHeight((float) labelHeight));

        addAndMakeVisible(comboBox);
        addAndMakeVisible(label);

        binding.midiMapping.addChangeListener(this);
    }

    template <typename Type>
    LabeledComboBox(ParameterValue<Type>& value)
        : LabeledComboBox(makeResolveParamEndpoint(value))
    {}

    template <typename Type>
    LabeledComboBox(te::AutomatableEditItem& editItem, ParameterValue<Type>& value)
        : LabeledComboBox(makeResolveParamEndpoint(editItem, value))
    {}

    ~LabeledComboBox() override {
        binding.midiMapping.removeChangeListener(this);
    }

    void resized() override;
    void paint(Graphics& g) override;

    inline static constexpr int labelHeight = 10;

    int getLabelHeight() const { return labelHeight; }

private:
    // listener to update on mapping changes
    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &binding.midiMapping) {
            repaint();
        }
    }

    juce::ComboBox comboBox;
    Label label;

    ComboBoxParamEndpointBinding binding;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabeledComboBox)
};

}  // namespace MoTool

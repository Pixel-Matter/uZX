#pragma once

#include <JuceHeader.h>

#include "AYPlugin.h"
#include "../../../gui/common/LookAndFeel.h"
#include "juce_data_structures/juce_data_structures.h"
#include "juce_graphics/juce_graphics.h"
#include "juce_gui_basics/juce_gui_basics.h"


namespace te = tracktion;

namespace MoTool::uZX {

class SliderAttachment : private Slider::Listener {
public:
    SliderAttachment(
        ParamAttachment<double>& param,
        Slider::SliderStyle style = Slider::LinearHorizontal,
        Slider::TextEntryBoxPosition textBoxPosition = Slider::TextBoxLeft
    )
      : slider_(style, textBoxPosition)
      , value_(param.value)
    {
        label_.setText(param.name, dontSendNotification);
        // label.attachToComponent(&slider, true);
        slider_.addListener(this);
        slider_.setNormalisableRange(param.range);
        slider_.setValue(value_.get(), dontSendNotification);
    }

    void addWidgets(Component& component) {
        component.addAndMakeVisible(label_);
        component.addAndMakeVisible(slider_);
    }

    void setBounds(Rectangle<int> r) {
        label_.setBounds(r.removeFromTop(24).reduced(4));
        slider_.setBounds(r.removeFromTop(48).reduced(4));
    }

private:
    // TODO contruct widgets here, do not take them as arguments
    Label label_;
    Slider slider_;
    CachedValue<double>& value_;

    void sliderValueChanged(Slider* emitter) override {
        value_ = emitter->getValue();
    }
};


class AYPluginEditor : public te::Plugin::EditorComponent {
public:
    AYPluginEditor(AYChipPlugin& p)
      : plugin_(p)
    {
        constrainer_.setMinimumWidth(200);
        setSize(200, 200);

        clockAttachment.addWidgets(*this);
    }

    bool allowWindowResizing() override {
        return true;
    }

    void paint(Graphics& g) override {
        g.fillAll(Colors::Theme::backgroundAlt);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(8);
        clockAttachment.setBounds(r.removeFromTop(72));
    }

    ComponentBoundsConstrainer* getBoundsConstrainer() override {
        return &constrainer_;
    }

private:
    AYChipPlugin& plugin_;
    ComponentBoundsConstrainer constrainer_;

    SliderAttachment clockAttachment { plugin_.staticParams.clockValue };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYPluginEditor)
};

}

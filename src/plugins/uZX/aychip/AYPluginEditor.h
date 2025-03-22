#pragma once

#include <JuceHeader.h>

#include "AYPlugin.h"
#include "../../../gui/common/LookAndFeel.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "juce_data_structures/juce_data_structures.h"
#include "juce_graphics/juce_graphics.h"
#include "juce_gui_basics/juce_gui_basics.h"


namespace te = tracktion;

namespace MoTool::uZX {


// Base class for parameter widgets
template <typename Type, typename WidgetType>
class ParameterComponent : public Component {
public:
    ParameterComponent(ParamAttachment<Type>& param)
      : attachment(param),
        widget(),
        label(param.name, "")
    {
        configureWidget();

        // Initialize widget with parameter value
        widget.setValue(attachment.get(), dontSendNotification);

        addAndMakeVisible(label);
        addAndMakeVisible(widget);
    }

    // Override in derived classes to configure the specific widget
    virtual void configureWidget() = 0;

    void resized() override {
        auto r = getLocalBounds();
        label.setBounds(r.removeFromTop(24).reduced(4));
        widget.setBounds(r.reduced(4));
    }

protected:
    ParamAttachment<Type>& attachment;
    WidgetType widget;
    Label label;
};

// Specialized for sliders
class SliderParameterComponent : public ParameterComponent<double, Slider>, private Slider::Listener  {
public:
    SliderParameterComponent(
        ParamAttachment<double>& param,
        Slider::SliderStyle style = Slider::LinearHorizontal,
        Slider::TextEntryBoxPosition textBoxPosition = Slider::TextBoxLeft)
      : ParameterComponent<double, Slider>(param),
        sliderStyle(style),
        boxPosition(textBoxPosition)
    {}

    void configureWidget() override {
        widget.setSliderStyle(sliderStyle);
        widget.setTextBoxStyle(boxPosition, false, widget.getTextBoxWidth(), widget.getTextBoxHeight());
        widget.setRange(attachment.range.start, attachment.range.end, attachment.range.interval);
        widget.setValue(attachment.get(), dontSendNotification);
        widget.addListener(this);
    }

    private:
    Slider::SliderStyle sliderStyle;
    Slider::TextEntryBoxPosition boxPosition;

    // void updateWidgetFromValue() override {
    //     widget.setValue(attachment.get(), dontSendNotification);
    // }

    void sliderValueChanged(Slider*) override {
        attachment = widget.getValue();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliderParameterComponent)
};


// Similar specializations for other widget types
class ToggleParameterComponent : public ParameterComponent<bool, ToggleButton> {
    // Implementation...
};

class ComboParameterComponent : public ParameterComponent<int, ComboBox> {
    // Implementation...
};

class AYPluginEditor : public te::Plugin::EditorComponent {
public:
    AYPluginEditor(AYChipPlugin& p)
      : plugin_(p)
    {
        constrainer_.setMinimumWidth(200);
        setSize(200, 200);

        addAndMakeVisible(clockParameter);
    }

    bool allowWindowResizing() override {
        return true;
    }

    void paint(Graphics& g) override {
        g.fillAll(Colors::Theme::backgroundAlt);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(8);
        // chipAttachment.setBounds(r.removeFromTop(72));
        clockParameter.setBounds(r.removeFromTop(72));
    }

    ComponentBoundsConstrainer* getBoundsConstrainer() override {
        return &constrainer_;
    }

private:
    AYChipPlugin& plugin_;
    ComponentBoundsConstrainer constrainer_;

    // SliderAttachment<AYInterface::ChipType> chipAttachment  { plugin_.staticParams.chipTypeValue };
    SliderParameterComponent                 clockParameter { plugin_.staticParams.clockValue };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYPluginEditor)
};

}

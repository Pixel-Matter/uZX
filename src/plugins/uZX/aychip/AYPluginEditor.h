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
    ParameterComponent(ParamAttachment<Type>& att)
      : attachment(att)
      , label("", att.name)
      , widget()
    {
        addAndMakeVisible(label);
        addAndMakeVisible(widget);
    }

    void resized() override {
        auto r = getLocalBounds();
        label.setBounds(r.removeFromTop(30));
        widget.setBounds(r.removeFromTop(30));
    }

protected:
    ParamAttachment<Type>& attachment;
    Label label;
    WidgetType widget;
};

// Specialized for sliders
class SliderParameterComponent : public ParameterComponent<double, Slider> {
public:
    SliderParameterComponent(
        ParamAttachment<double>& param,
        Slider::SliderStyle style = Slider::LinearHorizontal,
        Slider::TextEntryBoxPosition textBoxPosition = Slider::TextBoxLeft)
      : ParameterComponent<double, Slider>(param),
        sliderStyle(style),
        boxPosition(textBoxPosition)
    {
        widget.setSliderStyle(sliderStyle);
        widget.setTextBoxStyle(boxPosition, false, widget.getTextBoxWidth(), widget.getTextBoxHeight());
        widget.setRange(attachment.range.start, attachment.range.end, attachment.range.interval);
        widget.setValue(attachment.get(), dontSendNotification);
        widget.getValueObject().referTo(attachment.getPropertyAsValue());
    }

private:
    Slider::SliderStyle sliderStyle;
    Slider::TextEntryBoxPosition boxPosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliderParameterComponent)
};


// Similar specializations for other widget types
template <typename Type>
class ComboParameterComponent : public ParameterComponent<Type, ComboBox> {
    public:
    ComboParameterComponent(
        ParamAttachment<Type>& att)
      : ParameterComponent<Type, ComboBox>(att)
    {
        this->widget.addItemList(this->attachment.getChioces(), 1);
        this->widget.setSelectedItemIndex(this->attachment.get() + 1, dontSendNotification);
        this->widget.getSelectedIdAsValue().referTo(this->attachment.getPropertyAsValue());
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ComboParameterComponent)
};


class AYPluginEditor : public te::Plugin::EditorComponent {
public:
    AYPluginEditor(AYChipPlugin& p)
      : plugin_(p)
    {
        constrainer_.setMinimumWidth(200);
        setSize(200, 200);

        addAndMakeVisible(chipParameter);
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
        chipParameter.setBounds(r.removeFromTop(60));
        clockParameter.setBounds(r.removeFromTop(90));
    }

    ComponentBoundsConstrainer* getBoundsConstrainer() override {
        return &constrainer_;
    }

    static constexpr int itemHeight = 30;
    static constexpr int itemSpacing = 7;

private:
    AYChipPlugin& plugin_;
    ComponentBoundsConstrainer constrainer_;

    ComboParameterComponent<AYInterface::ChipType> chipParameter  { plugin_.staticParams.chipTypeValue };
    SliderParameterComponent                       clockParameter { plugin_.staticParams.clockValue };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYPluginEditor)
};

}

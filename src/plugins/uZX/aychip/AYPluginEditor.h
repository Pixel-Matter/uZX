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

class SliderParameterComponent : public ParameterComponent<double, Slider>, private Slider::Listener, private Value::Listener {
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
        // NOTE referTo tracks values poorly, doesnt track edge values
        // widget.getValueObject().referTo(attachment.getPropertyAsValue());
        attachment.getPropertyAsValue().addListener(this);
        widget.addListener(this);
    }

private:
    Slider::SliderStyle sliderStyle;
    Slider::TextEntryBoxPosition boxPosition;

    void sliderValueChanged(Slider*) override {
        attachment = widget.getValue();
    }

    void valueChanged(Value&) override {
        widget.setValue(attachment.get(), dontSendNotification);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliderParameterComponent)
};


template <typename Type>
class ComboParameterComponent : public ParameterComponent<Type, ComboBox>, private ComboBox::Listener, private Value::Listener {
    public:
    ComboParameterComponent(
        ParamAttachment<Type>& att)
      : ParameterComponent<Type, ComboBox>(att)
    {
        for (const auto& [idx, label] : this->attachment.getChoices()) {
            this->widget.addItem(label, idx + 1);
        }
        this->widget.setSelectedItemIndex(valueToIdx(this->attachment.get()), dontSendNotification);
        this->attachment.getPropertyAsValue().addListener(this);
        this->widget.addListener(this);
    }

private:
    int valueToIdx(Type value) {
        for (size_t i = 0; i < this->attachment.getChoices().size(); ++i) {
            if (this->attachment.getChoices()[i].first == value) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    Type idxToValue(int idx) {
        jassert(idx >= 0 && idx < static_cast<int>(this->attachment.getChoices().size()));
        return this->attachment.getChoices()[static_cast<size_t>(idx)].first;
    }

    void comboBoxChanged(ComboBox* comboBox) override {
        if (comboBox == &this->widget) {
            if (auto idx = this->widget.getSelectedItemIndex(); idx >= 0) {
                this->attachment = idxToValue(this->widget.getSelectedItemIndex());
            }
        }
    }

    void valueChanged(Value&) override {
        this->widget.setSelectedItemIndex(valueToIdx(this->attachment.get()), dontSendNotification);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ComboParameterComponent)
};


class AYPluginEditor : public te::Plugin::EditorComponent {
public:
    AYPluginEditor(AYChipPlugin& p)
      : plugin_(p)
    {
        constrainer_.setMinimumWidth(200);
        setSize(200, 300);

        addAndMakeVisible(chipParameter);
        addAndMakeVisible(clockParameter);
        addAndMakeVisible(channelsParameter);
        addAndMakeVisible(stereoParameter);
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
        clockParameter.setBounds(r.removeFromTop(60));
        channelsParameter.setBounds(r.removeFromTop(60));
        stereoParameter.setBounds(r.removeFromTop(60));
    }

    ComponentBoundsConstrainer* getBoundsConstrainer() override {
        return &constrainer_;
    }

    static constexpr int itemHeight = 30;
    static constexpr int itemSpacing = 7;

private:
    AYChipPlugin& plugin_;
    ComponentBoundsConstrainer constrainer_;

    ComboParameterComponent<AYInterface::ChipType>       chipParameter  { plugin_.staticParams.chipTypeValue };
    SliderParameterComponent                             clockParameter { plugin_.staticParams.clockValue };
    ComboParameterComponent<AYInterface::ChannelsLayout> channelsParameter { plugin_.staticParams.channelsLayoutValue };
    SliderParameterComponent                             stereoParameter { plugin_.staticParams.stereoWidthValue };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYPluginEditor)
};

}

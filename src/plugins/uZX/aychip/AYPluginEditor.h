#pragma once

#include <JuceHeader.h>

#include "AYPlugin.h"
#include "../../../gui/common/LookAndFeel.h"

namespace te = tracktion;

namespace MoTool::uZX {


// Base class for parameter widgets
template <typename Type, typename WidgetType>
class ParameterComponent : public Component {
public:
    ParameterComponent(ParamAttachment<Type>& att, bool useLabel_ = true)
      : attachment(att)
      , useLabel(useLabel_)
      , label("", att.name)
      , widget()
    {
        if (useLabel)
            addAndMakeVisible(label);
        addAndMakeVisible(widget);
    }

    void resized() override {
        auto r = getLocalBounds();
        if (useLabel)
            label.setBounds(r.removeFromTop(30));
        widget.setBounds(r.removeFromTop(30));
    }

protected:
    ParamAttachment<Type>& attachment;
    bool useLabel;
    Label label;
    WidgetType widget;
};

template <typename Type>
class SliderParameterComponent : public ParameterComponent<Type, Slider>, private Slider::Listener, private Value::Listener {
public:
    SliderParameterComponent(
        ParamAttachment<Type>& param,
        Slider::SliderStyle style = Slider::LinearHorizontal,
        Slider::TextEntryBoxPosition textBoxPosition = Slider::TextBoxLeft)
      : ParameterComponent<Type, Slider>(param),
        sliderStyle(style),
        boxPosition(textBoxPosition)
    {
        this->widget.setSliderStyle(sliderStyle);
        this->widget.setTextBoxStyle(boxPosition, false, this->widget.getTextBoxWidth(), this->widget.getTextBoxHeight());
        this->widget.setRange(this->attachment.range.start, this->attachment.range.end, this->attachment.range.interval);
        this->widget.setValue(this->attachment.get(), dontSendNotification);
        // NOTE referTo tracks values poorly, doesnt track edge values
        // widget.getValueObject().referTo(attachment.getPropertyAsValue());
        this->attachment.getPropertyAsValue().addListener(this);
        this->widget.addListener(this);

        DBG("Range interval for " << this->attachment.name  << " is " << this->attachment.range.interval);
    }

private:
    Slider::SliderStyle sliderStyle;
    Slider::TextEntryBoxPosition boxPosition;

    void sliderValueChanged(Slider*) override {
        this->attachment = static_cast<Type>(this->widget.getValue());
    }

    void valueChanged(Value&) override {
        this->widget.setValue(this->attachment.get(), dontSendNotification);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliderParameterComponent)
};

class ToggleParameterComponent : public ParameterComponent<bool, ToggleButton>, private Button::Listener, private Value::Listener {
public:
    ToggleParameterComponent(
        ParamAttachment<bool>& param
    )
      : ParameterComponent<bool, ToggleButton>(param, /* useLabel_ = */ false)
    {
        widget.setButtonText(attachment.name);
        widget.setToggleState(attachment.get(), dontSendNotification);
        attachment.getPropertyAsValue().addListener(this);
        widget.addListener(this);
    }

private:
    void buttonClicked(Button*) override {
        attachment = widget.getToggleState();
    }

    void buttonStateChanged(Button*) override {
        attachment = widget.getToggleState();
    }

    void valueChanged(Value&) override {
        widget.setToggleState(attachment.get(), dontSendNotification);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToggleParameterComponent)
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
        setSize(200, 350);

        addAndMakeVisible(chipParameter);
        addAndMakeVisible(clockParameter);
        addAndMakeVisible(channelsParameter);
        addAndMakeVisible(stereoParameter);
        addAndMakeVisible(removeDCParameter);
        addAndMakeVisible(midiParameter);
    }

    bool allowWindowResizing() override {
        return true;
    }

    void paint(Graphics& g) override {
        g.fillAll(Colors::Theme::backgroundAlt);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(8);
        chipParameter.setBounds(r.removeFromTop(itemHeight * 2));
        clockParameter.setBounds(r.removeFromTop(itemHeight * 2));
        channelsParameter.setBounds(r.removeFromTop(itemHeight * 2));
        stereoParameter.setBounds(r.removeFromTop(itemHeight * 2));
        DBG("removeDCParameter height is " << removeDCParameter.getHeight());
        removeDCParameter.setBounds(r.removeFromTop(itemHeight));
        DBG("removeDCParameter height is " << removeDCParameter.getHeight());
        midiParameter.setBounds(r.removeFromTop(itemHeight * 2));
    }

    ComponentBoundsConstrainer* getBoundsConstrainer() override {
        return &constrainer_;
    }

    static constexpr int itemHeight = 30;
    static constexpr int itemSpacing = 7;

private:
    AYChipPlugin& plugin_;
    ComponentBoundsConstrainer constrainer_;

    ComboParameterComponent<AYInterface::ChipType>       chipParameter     { plugin_.staticParams.chipTypeValue };
    SliderParameterComponent<double>                     clockParameter    { plugin_.staticParams.clockValue };
    ComboParameterComponent<AYInterface::ChannelsLayout> channelsParameter { plugin_.staticParams.channelsLayoutValue };
    SliderParameterComponent<double>                     stereoParameter   { plugin_.staticParams.stereoWidthValue };
    ToggleParameterComponent                             removeDCParameter { plugin_.staticParams.removeDCValue };
    SliderParameterComponent<int>                        midiParameter     { plugin_.staticParams.baseMidiChannelValue };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYPluginEditor)
};

}

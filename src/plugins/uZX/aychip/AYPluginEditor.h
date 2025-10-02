#pragma once

#include <JuceHeader.h>

#include "AYPlugin.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/common/LabeledSlider.h"
#include "../../../gui/common/ChoiceButton.h"
#include "../../../gui/devices/PluginDeviceUI.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"


namespace MoTool::uZX {

//==============================================================================
// Base CRTP class for parameter widgets
// Depracated
template <typename Att, typename WidgetType>
class ParameterComponent : public Component {
public:
    ParameterComponent(Att& att, bool useLabel_ = true)
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
        if (useLabel) {
            label.setBounds(r.removeFromTop(roundToInt(label.getFont().getHeight()) + 4));
        }
        widget.setBounds(r);
    }

protected:
    Att& attachment;
    bool useLabel;
    Label label;
    WidgetType widget;
};

//==============================================================================
template <typename Type>
class SliderParameterComponent : public ParameterComponent<RangedParamAttachment<Type>, Slider>,
                                 private Slider::Listener,
                                 private Value::Listener
{
public:
    SliderParameterComponent(
        RangedParamAttachment<Type>& param,
        Slider::SliderStyle style = Slider::LinearHorizontal,
        Slider::TextEntryBoxPosition textBoxPosition = Slider::TextBoxLeft
    )
      : ParameterComponent<RangedParamAttachment<Type>, Slider>(param)
      , sliderStyle(style)
      , boxPosition(textBoxPosition)
    {
        this->widget.setSliderStyle(sliderStyle);
        this->widget.setTextBoxStyle(boxPosition, false, this->widget.getTextBoxWidth(), this->widget.getTextBoxHeight());
        this->widget.setRange(this->attachment.getRange().start, this->attachment.getRange().end, this->attachment.getRange().interval);
        this->widget.setValue(this->attachment.get(), dontSendNotification);
        // NOTE referTo tracks values poorly, doesnt track edge values
        // widget.getValueObject().referTo(attachment.getPropertyAsValue());
        this->attachment.addListener(this);
        this->widget.addListener(this);

        // DBG("Range interval for " << this->attachment.name  << " is " << this->attachment.range.interval);
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

//==============================================================================
class ToggleParameterComponent : public ParameterComponent<ParamAttachment<bool>, ToggleButton>,
                                 private Button::Listener,
                                 private Value::Listener
{
public:
    ToggleParameterComponent(ParamAttachment<bool>& param)
      : ParameterComponent<ParamAttachment<bool>, ToggleButton>(param, /* useLabel_ = */ false)
    {
        widget.setButtonText(attachment.name);
        widget.setToggleState(attachment.get(), dontSendNotification);
        attachment.getValue().addListener(this);
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

//==============================================================================
template <typename Type>
class ComboParameterComponent : public ParameterComponent<ChoiceParamAttachment<Type>, ComboBox>,
                                private ComboBox::Listener,
                                private Value::Listener {
    public:
    ComboParameterComponent(
        ChoiceParamAttachment<Type>& att)
      : ParameterComponent<ChoiceParamAttachment<Type>, ComboBox>(att)
    {
        for (const auto& [idx, label] : this->attachment.getChoices()) {
            this->widget.addItem(label, static_cast<int>(idx) + 1);
        }
        this->widget.setSelectedItemIndex(valueToIdx(this->attachment.get()), dontSendNotification);
        this->attachment.addListener(this);
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

//==============================================================================
// Editor for AYChipPlugin
//==============================================================================
class AYPluginUI : public PluginDeviceUI {
public:
    AYPluginUI(tracktion::Plugin::Ptr pluginPtr)
       : PluginDeviceUI(pluginPtr)
       , plugin_(*dynamic_cast<AYChipPlugin*>(pluginPtr.get()))
    {
        constrainer_.setMinimumWidth(160);
        setSize(160, 320);

        addAndMakeVisible(volumeKnob);
        addAndMakeVisible(layoutButton);
        // addAndMakeVisible(layoutSlider);
        addAndMakeVisible(stereoKnob);

        addAndMakeVisible(midiControl);
        addAndMakeVisible(clockControl);
        addAndMakeVisible(chipTypeControl);
        addAndMakeVisible(DCControl);
    }

    void paint(Graphics& g) override {
        g.fillAll(Colors::Theme::backgroundAlt);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(8, 0);

        // automatable
        r.removeFromTop(itemSpacing);
        layoutButton.setBounds(r.removeFromTop(itemHeight * 2));
        // layoutSlider.setBounds(r.removeFromTop(itemHeight * 2));

        r.removeFromTop(itemSpacing);
        auto knobsRow = r.removeFromTop(itemHeight * 2);

        stereoKnob.setBounds(knobsRow.removeFromLeft(knobsRow.getWidth() / 2));
        volumeKnob.setBounds(knobsRow);

        // static
        r.removeFromTop(itemSpacing);
        midiControl.setBounds(r.removeFromTop(itemHeight * 2));
        r.removeFromTop(itemSpacing);
        clockControl.setBounds(r.removeFromTop(itemHeight * 2));
        r.removeFromTop(itemSpacing);
        chipTypeControl.setBounds(r.removeFromTop(itemHeight * 2));
        r.removeFromTop(itemSpacing);
        DCControl.setBounds(r.removeFromTop(itemHeight));
        r.removeFromTop(itemSpacing);
    }

    ComponentBoundsConstrainer* getBoundsConstrainer() {
        return &constrainer_;
    }

    static constexpr int itemHeight = 20;
    static constexpr int itemSpacing = 4;

private:
    AYChipPlugin& plugin_;
    ComponentBoundsConstrainer constrainer_;

    // dynamic, can be automated
    LabeledSlider volumeKnob   { plugin_, plugin_.dynamicParams.volume };
    ChoiceButton layoutButton  { plugin_, plugin_.dynamicParams.layout };
    // LabeledSlider layoutSlider { plugin_, plugin_.dynamicParams.layout, Slider::LinearHorizontal };
    LabeledSlider stereoKnob   { plugin_, plugin_.dynamicParams.stereoWidth };

    // legacy static
    SliderParameterComponent<int>     midiControl     { plugin_.legacyParams.baseMidiChannel };
    SliderParameterComponent<double>  clockControl    { plugin_.legacyParams.clock };
    ComboParameterComponent<ChipType> chipTypeControl { plugin_.legacyParams.chipType };
    ToggleParameterComponent          DCControl       { plugin_.legacyParams.removeDC };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYPluginUI)
};

REGISTER_PLUGIN_UI_ADAPTER(AYChipPlugin, AYPluginUI)


//==============================================================================
// Editor component for AYChipPlugin is a simple wrapper around AYPluginUI
//==============================================================================
class AYPluginEditor : public tracktion::Plugin::EditorComponent,
                       private AYPluginUI
{
public:
    AYPluginEditor(tracktion::Plugin::Ptr p)
      : AYPluginUI(p)
    {}

    bool allowWindowResizing() override {
        return true;
    }

    ComponentBoundsConstrainer* getBoundsConstrainer() override {
        return AYPluginUI::getBoundsConstrainer();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYPluginEditor)
};

}

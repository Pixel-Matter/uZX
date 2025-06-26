#pragma once

#include <JuceHeader.h>
#include "../util/convert.h"


using namespace juce;
namespace te = tracktion;

namespace MoTool {


class SliderAttachment : private te::AutomatableParameter::Listener {
public:
    SliderAttachment (Slider& s, te::AutomatableParameter& p)
        : slider (s)
        , param (p)
    {
        slider.setRange(p.getValueRange().getStart(), p.getValueRange().getEnd(), 0.0);
        slider.setValue(p.getCurrentValue(), dontSendNotification);

        slider.onValueChange = [this] {
            juce::ScopedValueSetter<bool> svs(updatingSlider, true);
            param.setParameter((float)slider.getValue(), juce::sendNotification);
        };
        slider.setPopupDisplayEnabled(true, false, nullptr);
        slider.onDragStart = [&]{ param.parameterChangeGestureBegin(); };
        slider.onDragEnd   = [&]{ param.parameterChangeGestureEnd(); };

        param.addListener(this);
    }

    ~SliderAttachment() override {
        param.removeListener(this);
    }

private:
    // called from MessageThread (see AsyncCaller)
    void currentValueChanged(te::AutomatableParameter& p) override {
        if (updatingSlider)
            return;  // don't update the parameter if we're already updating it
        slider.setValue(p.getCurrentValue(), dontSendNotification);
    }

    void curveHasChanged(te::AutomatableParameter&) override {}

    Slider& slider;
    te::AutomatableParameter& param;
    bool updatingSlider { false };
    // bool updatingFromParam { false };
};



template <typename Type>
struct ParamAttachment {
    using type = Type;

    ParamAttachment(ValueTree& tree, UndoManager* undoMgr)
        : valueTree(tree)
        , undoManager(undoMgr)
    {}

    // ctor that refers to an existing ValueTree property

    void referTo(const Identifier& id, const String& n, const Type& def, const String& u) {
        name = n;
        units = u;
        cachedValue.referTo(valueTree, id, undoManager, def);
        // value = cachedValue.getPropertyAsValue();
    }

    void referTo(const Identifier& id, const String& n, const NormalisableRange<Type>& r, const Type& def, const String& u) {
        referTo(id, n, def, u);
        range = r;
    }

    void referTo(const Identifier& id, const String& n, const StringArray& ch, const Type& def, const String& u) {
        referTo(id, n, def, u);
        choices.clear();
        for (int i = 0; i < ch.size(); ++i) {
            choices.push_back({static_cast<Type>(i), ch[i]});
        }
    }

    template <size_t N>
    void referTo(const Identifier& id, const String& n, const std::array<std::string_view, N>& ch, const Type& def, const String& u) {
        referTo(id, n, toStringArray(ch), def, u);
    }

    void referTo(const Identifier& id, const String& n, const std::vector<std::pair<Type, String>>& ch, const Type& def, const String& u) {
        referTo(id, n, def, u);
        choices = ch;
    }

    inline operator CachedValue<Type>&() noexcept { return cachedValue; }

    // for CachedValue-like transparent access
    inline operator Type() const noexcept         { return cachedValue.get(); }

    inline Type get() const noexcept              { return cachedValue.get(); }

    inline const Type& operator*() const noexcept        { return *cachedValue; }

    template <typename OtherType>
    inline bool operator== (const OtherType& other) const { return cachedValue == other; }

    template <typename OtherType>
    inline bool operator!= (const OtherType& other) const   { return ! operator== (other); }

    inline Type getDefault() const                          { return cachedValue.getDefault(); }

    inline ParamAttachment& operator= (const Type& newValue) {
        cachedValue = newValue;
        return *this;
    }

    inline Value getPropertyAsValue() {
        return cachedValue.getPropertyAsValue();
    }

    const std::vector<std::pair<Type, String>>& getChoices() const {
        return choices;
    }

    // ======================================================================================
    ValueTree& valueTree;
    UndoManager* undoManager;
    String name;
    String units;
    // TODO CachedValue<te::AtomicWrapper<Type>> value;
    CachedValue<Type> cachedValue;
    // Value value;
    NormalisableRange<Type> range;
    std::vector<std::pair<Type, String>> choices;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamAttachment)
};


}  // namespace MoTool

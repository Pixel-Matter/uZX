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
        slider.setRange(static_cast<double>(p.getValueRange().getStart()), static_cast<double>(p.getValueRange().getEnd()), static_cast<double>(p.valueRange.interval));
        slider.setSkewFactor(static_cast<double>(p.valueRange.skew));
        slider.setValue(static_cast<double>(p.getCurrentValue()), dontSendNotification);

        slider.onValueChange = [this] {
            juce::ScopedValueSetter<bool> svs(updatingSlider, true);
            param.setParameter((float)slider.getValue(), juce::sendNotification);
        };
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
        slider.setValue(static_cast<double>(p.getCurrentValue()), dontSendNotification);
    }

    void curveHasChanged(te::AutomatableParameter&) override {}

    Slider& slider;
    te::AutomatableParameter& param;
    bool updatingSlider { false };
    // bool updatingFromParam { false };
};

static inline String toString(bool v) {
    return v ? "true" : "false";
}

static inline String toString(auto v) {
    return String(v);
}

inline String& operator<< (String& str, bool b) {
    return str << toString(b);
}

template <typename Type>
class ParamAttachment {
public:
    using type = Type;

    ParamAttachment(ValueTree& tree, UndoManager* undoMgr)
        : valueTree(tree)
        , undoManager(undoMgr)
    {}

    ParamAttachment(ValueTree& tree, const Identifier& id, const String& n, UndoManager* undoMgr, const Type& deflt)
        : valueTree(tree)
        , undoManager(undoMgr)
        , name(n)
        , cachedValue(tree, id, undoMgr, deflt)
        , value(cachedValue.getPropertyAsValue())
    {
        // The problem is that setting initial value in ParamAttachment constructor
        // before adding listeners results in not calling valueChanged
        // Or we can not init and bind in the constructor but rather in referTo after addListener
        // cachedValue = deflt; // Initialize with default value

        // DBG("ParamAttachment::ctor and binding for type " << typeid(Type).name() << " with id " << id.toString()
            // << ", default " << toString(deflt)
            // << ", value " << toString(static_cast<Type>(value.getValue())));
    }

    ParamAttachment(ValueTree& tree, const Identifier& id, UndoManager* undoMgr, const Type& deflt)
        : ParamAttachment(tree, id, id.toString(), undoMgr, deflt)
    {}

    // ctor that refers to an existing ValueTree property

    void referTo(const Identifier& id, const Type& deflt) {
        // DBG("ParamAttachment::referTo for type " << typeid(Type).name() << " with id " << id.toString());
        // NOTE not vice versa
        cachedValue.referTo(valueTree, id, undoManager, deflt);
        value = cachedValue.getPropertyAsValue();
    }

    void referTo(const Identifier& id, const String& n, const Type& def) {
        name = n;
        referTo(id, def);
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

    inline Value& getValue() {
        return value;
    }

    void addListener(Value::Listener* listener) {
        // DBG("ParamAttachment::addListener for type " << typeid(Type).name() << " with name " << name);
        value.addListener(listener);
    }

    void removeListener(Value::Listener* listener) {
        value.removeListener(listener);
    }

    // ======================================================================================
    ValueTree& valueTree;
    UndoManager* undoManager;
    String name;
    // TODO CachedValue<te::AtomicWrapper<Type>> value;
    CachedValue<Type> cachedValue;
    Value value;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamAttachment)
};


template <typename Type>
class ChoiceParamAttachment : public ParamAttachment<Type> {
public:
    using type = Type;
    using ParamAttachment<Type>::ParamAttachment;
    using ParamAttachment<Type>::referTo;

    ChoiceParamAttachment(ValueTree& tree, const Identifier& id, const String& n,
                          const std::vector<std::pair<Type, String>>& ch,
                          UndoManager* undoMgr, const Type& deflt)
        : ParamAttachment<Type>(tree, id, n, undoMgr, deflt)
        , choices(ch)
    {
        // DBG("ParamAttachment::ctor and binding for type " << typeid(Type).name() << " with id " << id.toString());
    }

    ChoiceParamAttachment(ValueTree& tree, const Identifier& id, const String& n,
                          const StringArray& ch,
                          UndoManager* undoMgr, const Type& deflt)
        : ParamAttachment<Type>(tree, id, n, undoMgr, deflt)
        , choices(toChoices(ch))
    {}

    ChoiceParamAttachment(ValueTree& tree, const Identifier& id,
                          const StringArray& ch,
                          UndoManager* undoMgr, const Type& deflt)
        : ParamAttachment<Type>(tree, id, undoMgr, deflt)
        , choices(toChoices(ch))
    {}

    template <size_t N>
    ChoiceParamAttachment(ValueTree& tree, const Identifier& id,
                          const std::array<std::string_view, N>& ch,
                          UndoManager* undoMgr, const Type& deflt)
        : ParamAttachment<Type>(tree, id, undoMgr, deflt)
        , choices(toChoices(toStringArray(ch)))
    {}

    template <size_t N>
    ChoiceParamAttachment(ValueTree& tree, const Identifier& id, const String& n,
                          const std::array<std::string_view, N>& ch,
                          UndoManager* undoMgr, const Type& deflt)
        : ParamAttachment<Type>(tree, id, n, undoMgr, deflt)
        , choices(toChoices(toStringArray(ch)))
    {}

    inline ChoiceParamAttachment& operator= (const Type& newValue) {
        this->cachedValue = newValue;
        return *this;
    }

    inline static std::vector<std::pair<Type, String>> toChoices(const StringArray& ch) {
        std::vector<std::pair<Type, String>> choices;
        for (int i = 0; i < ch.size(); ++i) {
            choices.push_back({static_cast<Type>(i), ch[i]});
        }
        return choices;
    }

    void referTo(const Identifier& id, const String& n, const StringArray& ch, const Type& def) {
        choices = toChoices(ch);
        referTo(id, n, def);
    }

    template <size_t N>
    void referTo(const Identifier& id, const String& n, const std::array<std::string_view, N>& ch, const Type& def) {
        choices = toChoices(toStringArray(ch));
        referTo(id, n, def);
    }

    const std::vector<std::pair<Type, String>>& getChoices() const {
        return choices;
    }

    // ======================================================================================
private:
    std::vector<std::pair<Type, String>> choices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChoiceParamAttachment)
};


template <typename Type>
class RangedParamAttachment : public ParamAttachment<Type> {
public:
    using type = Type;
    using ParamAttachment<Type>::ParamAttachment;
    using ParamAttachment<Type>::referTo;

    RangedParamAttachment(ValueTree& tree, const Identifier& id,
                          const NormalisableRange<Type>& r,
                          UndoManager* undoMgr, const Type& deflt, const String& u = String{})
        : ParamAttachment<Type>(tree, id, id.toString(), undoMgr, deflt)
        , range(r)
        , units(u)
    {}

    void referTo(const Identifier& id, const String& n, const NormalisableRange<Type>& r, const Type& def, const String& u = String{}) {
        referTo(id, n, def);
        range = r;
        units = u;
    }

    inline RangedParamAttachment& operator= (const Type& newValue) {
        this->cachedValue = newValue;
        return *this;
    }

    const String& getUnits() const {
        return units;
    }

    const NormalisableRange<Type>& getRange() const {
        return range;
    }

    // ======================================================================================
private:
    NormalisableRange<Type> range;
    String units;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RangedParamAttachment)
};


// Bind ComboBox to a EnumChoice parameter with shift to 1-based ComboBox index
template <typename ChoiceType>
class ComboBoxBinding : private Value::Listener,
                        private ComboBox::Listener
{
public:
    ComboBoxBinding(ComboBox& cb, ChoiceParamAttachment<ChoiceType>& cp)
        : comboBox(cb)
        , choiceParam(cp)
    {
        comboBox.addListener(this);
        choiceParam.addListener(this);
        // For non-derived classes, call init() directly since virtual functions work
        if (typeid(*this) == typeid(ComboBoxBinding<ChoiceType>)) {
            init();
        }
    }

    void init() {
        fillItems();
        updateComboBox();
    }

    virtual void fillItems() {
        comboBox.clear();
        for (auto [i, item] : choiceParam.getChoices()) {
            comboBox.addItem(item, (int) i + 1);
        }
    }

    virtual ~ComboBoxBinding() override {
        comboBox.removeListener(this);
        choiceParam.removeListener(this);
    }

protected:
    ComboBox& comboBox;
    ChoiceParamAttachment<ChoiceType>& choiceParam;

private:
    void valueChanged(Value& v) override {
        if (v.refersToSameSourceAs(choiceParam.getValue())) {
            updateComboBox();
        }
    }

    void comboBoxChanged(ComboBox* cb) override {
        if (cb == &comboBox && comboBox.getSelectedId() > 0) {
            // DBG("ComboBoxBinding::comboBoxChanged: " << comboBox.getSelectedId());
            choiceParam = ChoiceType(comboBox.getSelectedId() - 1);
        }
    }

    void updateComboBox() {
        if (auto param = choiceParam.get(); param.isValid()) {
            int id = static_cast<int>(param);
            // DBG("ComboBoxBinding::updateComboBox: " << param.getLabel().data());
            comboBox.setSelectedId(id + 1, dontSendNotification);
        } else {
            comboBox.setSelectedId(0, dontSendNotification);
        }
    }
};

}  // namespace MoTool

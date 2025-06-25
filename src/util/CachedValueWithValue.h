#pragma once

#include <JuceHeader.h>

namespace MoTool {

template<typename Type>
class CachedValueWithValue : public juce::CachedValue<Type> {
public:
    CachedValueWithValue() = default;
    
    CachedValueWithValue(juce::ValueTree& tree, const juce::Identifier& propertyID, 
                         juce::UndoManager* undoManager)
        : juce::CachedValue<Type>(tree, propertyID, undoManager)
        , value_(this->getPropertyAsValue())
    {
    }
    
    CachedValueWithValue(juce::ValueTree& tree, const juce::Identifier& propertyID,
                         juce::UndoManager* undoManager, const Type& defaultValue)
        : juce::CachedValue<Type>(tree, propertyID, undoManager, defaultValue)
        , value_(this->getPropertyAsValue())
    {
    }
    
    juce::Value& getValue() { return value_; }
    const juce::Value& getValue() const { return value_; }
    
    void addListener(juce::Value::Listener* listener) { value_.addListener(listener); }
    void removeListener(juce::Value::Listener* listener) { value_.removeListener(listener); }
    
    CachedValueWithValue& operator=(const Type& newValue)
    {
        juce::CachedValue<Type>::operator=(newValue);
        return *this;
    }
    
private:
    juce::Value value_;
};

}
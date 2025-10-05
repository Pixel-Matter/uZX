
# Parameter Binding Architecture

The parameter subsystem bridges persistent edit state, automation metadata, and interactive controls. Typed parameter definitions live beside the ValueTree state, while UI bindings observe both automation callbacks and stored values.

Actually what I wanted is to prevent combinatorial explosion of classes and templates for types (int, float, bool, string, enum), bindings (`tracktion::AutomatableParameter` attached to `juce::CachedValue<T>` or `juce::CachedValue<T>` only) and widget classes (`Slider`, `Label`, `TextButton`, `Combobox`, ...). So I want to have simple system of classes that can bind any `ParameterValue<T>` to any widget and optionally to `tracktion::AutomatableParameter`.

`ParameterDef<T>` is needed for defining parameter name, id, description, default value, range, units, conversion functions between T and float/string.

`ParameterValue<T>` needed for grouping `CachedValue<T>` with `ParameterDef<T>`. Plugins maintain sctructs of parameters, automated and static.

`tracktion::Plugin` maintains a list of `tracktion:AutomatedParameter` instances, each of which is attached to a `CachedValue<T>` instance.

Because `tracktion::AutomatableParameter` is not a template, we need type erasure to bind `ParameterValue<T>` to `tracktion::AutomatableParameter`. But we are able to subclass `tracktion::AutomatableParameter` to create templated subclasses aware of type `T`: `BindedAutoParameter<T>`.

Thus `ParameterValue<T>` values must be converted to/from float and also has float-string conversion functions utilized by `tracktion::AutomatableParameter`. The question is where to put these conversion functions: in `ParameterDef<T>` callbacks or in separate static traits class `ParameterTraits<T>`

Binding between `ParameterValue<T>` and `tracktion::AutomatableParameter` is optional. We can introduce `ParameterSource` interface, subclass `BindedAutoParameter<T>` from it, and keep references to each other.

`CachedValue<T>` needed for storing parameter values in plugin's ValueTree state, juce conversion traits used to map T to/from ValueTree property types, for example human-readable strings for EnumChoice types. Also this ensures backwards compatibility when reading old ValueTree states.

Widgets should be configured by `ParameterDef<T>` for displaying name, units, range, etc
Widgets bind to `ParameterValue<T>` instances, which may be static or automated `ParameterValue<T>` instances.
Static binding binds widget to `CachedValue<T>` only, converts widget values to/from T. We should introduce conversion scheme without need to define conversion functions/structs for every pair Widget<->T. In widget binding we can fix for every wighet type which types `T` (or subset) is works with. Widget binding should be a separate class from wigget class itself.

## Subclassing AutomatableParameter

We actually can subclass it to:

- Instantiate concrete types aware of type `T`, `BindedAutoParameter<T>`, no neeed to maintain separate bindings lists in plugins.
- Add constructor to accept `ParameterValue<T>` and configure parameter by it.
- Override `isDiscrete()`, `getNumberOfStates()`, etc for handling `EmumChoice` types
- Store binding to `ParameterValue<T>`

But alas we can not subclass `AutomatableParameter::AttachedValue` because it is not exposed in public API.
So we can not attach even subclassed parameter to `CachedValue<T>` where `T` is not float, int or bool, for example, enum serializable to `String` in `Var`;

## Conversion functions

`juce::Var` to type `T` conversion is handled by `juce::VariantConverter` specializations, for example `juce::VariantConverter<EnumChoice>`.

`tracktion::AutomatableParameter` uses float values, so we need conversion functions between T and float. These are provided by `ParameterConversionTraits<T>` specializations, for example `ParameterConversionTraits<EnumChoice>`.

TODO

## Domain overview

```mermaid
classDiagram

    AutomatedParameter .. ParameterDef~T~ : configured by
    AutomatedParameter o-- CachedValue~T~ : attaches to

    CachedValue~T~ o-- ValueTree : uses

    MyPlugin *-- ValueTree : has
    MyPlugin *-- "*" AutomatedParameter : has
    MyPlugin *--  "*" ParameterValue~T~: has static
    MyPlugin *--  "*" ParameterValue~T~: has automated

    ParameterValue~T~ *-- CachedValue~T~ : has
    ParameterValue~T~ *-- ParameterDef~T~ : defined by

class AutomatedParameter {
    <<tracktion::>>
    function~String->float~ valueToStringFunction
    function~float->String~ stringToValueFunction

    attachToCurrentValue(CachedValue~T~&)
    parameterChangeGestureBegin()
    setParameter(float, bool)
    parameterChangeGestureEnd()
    getCurrentValue() float
}

class ParameterDef~T~ {
    String identifier
    Identifier propertyID
    String description
    T defaultValue
    Range~T~ valueRange
    String units

    getFloatValueRange() Range~float~
}

class ParameterValue~T~ {
    ParameterDef~T~ definition
    CachedValue~T~ value

    referTo(ValueTree&, UndoManager*)
    getValue() T
    setValue(T)
}

class CachedValue {
    <<juce::>>
}

class ValueTree {
    <<juce::>>
}

```

## Plugin-centered design overview

```mermaid
classDiagram
    MyPlugin *-- ValueTree : has
    MyPlugin *-- "*" AutomatedParameter : has

    MyPlugin *--  "*" ParameterValue~T~: has static
    MyPlugin *--  "*" ParameterValue~T~   : has automated

    AutomatedParameter <|-- BindedAutoParameter~T~ : is
    BindedAutoParameter~T~ o-- ParameterValue~T~

    ParameterValue *-- CachedValue~T~ : has
    AutomatedParameter o-- CachedValue~T~ : synced to
    ParameterValue *-- ParameterDef~T~ : defined by
    CachedValue~T~ o-- ValueTree : uses

class CachedValue {
    <<juce::>>
}

class ValueTree {
    <<juce::>>
}

class AutomatedParameter {
    <<tracktion::>>
}

class BindedAutoParameter~T~  {
    ParameterValue~T~ parameterValue
}

```

## WidgetParamBinding-centered design overview

```mermaid
classDiagram
    WidgetParamBinding *-- MidiParameterMapping : manages
    MidiParameterMapping o-- AutomatedParameter : maps

    ParamBindingBase <|-- WidgetParamBinding : is
    ParamBindingBase -- ParameterValue~T~ : configured by
    ParamBindingBase o-- AutomatedParameter : optional uses

    WidgetParamBinding o-- Widget : binds
    WidgetParamBinding o-- Widget : listens RMB

    ParameterValue~T~ .. AutomatedParameter : optional binds

    AutomatedParameter o-- CachedValue~T~ : synced to

    ParameterValue *-- ParameterDef~T~ : defined by
    ParameterValue~T~ *-- CachedValue~T~ : has
    CachedValue~T~ o-- ValueTree : uses

class CachedValue {
    <<juce::>>
}

class ValueTree {
    <<juce::>>
}

class AutomatedParameter {
    <<tracktion::>>
}

class Widget {
    <<juce::Component>>
    setValue()/setText()
}

class ParamBindingBase {
    Value storedValue
    fetchValue()
    applyValue()
    beginGesture()
    endGesture()
}

class ParameterValue~T~ {
    getStoredValue()
    getLiveValue()
}

class ParameterDef~T~ {
    ...
    valueToStringFunction(T) String
    stringToValueFunction(String) T
}

```

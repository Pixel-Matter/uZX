
# Parameter Binding Architecture

The parameter subsystem bridges persistent edit state, automation metadata, and interactive controls. Typed parameter definitions live beside the ValueTree state, while UI bindings observe both automation callbacks and stored values.

Actually what I wanted is to prevent combinatorial explosion of classes and templates for types (int, float, bool, string, enum), bindings (`tracktion::AutomatableParameter` attached to `juce::CachedValue<T>` or `juce::CachedValue<T>` only) and widget classes (`Slider`, `Label`, `TextButton`, `Combobox`, ...). So I want to have simple system of classes that can bind any `ParameterValue<T>` to any widget and optionally to `tracktion::AutomatableParameter`.

## Type erasure

Because `tracktion::AutomatableParameter` is not a template, we need type erasure to bind `ParameterValue<T>` to `tracktion::AutomatableParameter`. Thus `ParameterValue<T>` values must be converted to/from float and also has float-string conversion functions utilized by `tracktion::AutomatableParameter`. The question is where to put these conversion functions: in `ParameterDef<T>` callbacks or in separate static traits class `ParameterTraits<T>`?

`CachedValue<T>` needed for storing parameter values in plugin's ValueTree state, juce conversion traits used to map T to/from ValueTree property types, for example human-readable strings for EnumChoice types. Also this ensures backwards compatibility when reading old ValueTree states.

`tracktion::Plugin` maintains a list of `tracktion:AutomatedParameter` instances, each of which is linked to a `CachedValue<T>` instance.

`ParameterDef<T>` is needed for defining parameter name, id, description, default value, range, units, conversion functions between T and float/string.

`ParameterValue<T>` needed for grouping `CachedValue<T>` with `ParameterDef<T>`. Plugins maintain sctructs of parameters, automated and static.

Widgets should be configured by `ParameterDef<T>` for displaying name, units, range, etc
Widgets bind to `ParameterValue<T>` instances, which may be static or automated `ParameterValue<T>` instances.
Static binding binds widget to `CachedValue<T>` only, converts widget values to/from T. We should introduce conversion scheme without need to define conversion functions/structs for every pair Widget<->T. In widget binding we can fix for every wighet type which types `T` (or subset) is works with. Widget binding should be a separate class from wigget class itself.

## Subclassing AutomatableParameter

We actually can subclass it to:

- Instantiate concrete types aware of type `T`, `AutomatableBindedParameters<T>`,
  no neeed to maintain separate bindings lists in plugins.
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

## Parameter-related core design overview

Adding bindings between `ParameterValue<T>` and `AutomatedParameter`.

```mermaid
classDiagram

    AutomatedParameter .. ParameterDef~T~ : configured by

    ParameterValue~T~ o.. ParameterConversionTraits~T~ : uses
    ParameterValue~T~ *-- LiveAccessor~T~ : has
    ParameterValue~T~ *-- ParameterDef~T~ : defined by
    ParameterValue~T~ *-- CachedValue~T~ : has
    CachedValue~T~ o-- ValueTree : uses

    ParameterValue~T~ --o ParameterAutomationBinding : binds
    ParameterAutomationBinding o-- AutomatedParameter : binds

    LiveAccessor~T~ o.. AutomatedParameter : optional uses
    AutomatedParameter o-- CachedValue~T~ : attaches to


class AutomatedParameter {
    <<tracktion::>>
    attachToCurrentValue(CachedValue~T~&)
    parameterChangeGestureBegin()
    setParameter(float, bool)
    parameterChangeGestureEnd()
    getCurrentValue() float
}

class ParameterValue~T~ {
    using StorageType = Traits::StorageType
    ParameterDef~T~ definition
    CachedValue~StorageType~ value
    LiveAccessor liveAccessor

    referTo(ValueTree&, UndoManager*)
    getStoredValue() T
    getLiveValue() T
    hasLiveReader() bool
    setStoredValue(T)
    setLiveReader(Reader, ...)
    clearLiveReader()
}

class LiveAccessor~T~ {
    -Reader reader
    -void* context

    set(Reader, ...)
    readOr(T fb) T
    hasReader()
    reset()
}

class ParameterDef~T~ {
    String identifier
    Identifier propertyID
    String description
    T defaultValue
    NormalisableRange~T~ valueRange
    String units

    getFloatValueRange() NormalisableRange~float~
    valueToStringFunction(T) String
    stringToValueFunction(String) T
}

class ParameterConversionTraits~T~ {
    using StorageType = ...$
    toStorage(T value) StoredType$
    fromStorage(StoredType stored) T$
    toFloat(T value) float$
    fromFloat(float f) T$
}

class CachedValue {
    <<juce::>>
}

class ValueTree {
    <<juce::>>
}

class AutomatedParameter {
    <<tracktion::>>
}

```

## Plugin-centered design overview

```mermaid
classDiagram
    MyPlugin *-- ValueTree : has
    MyPlugin *-- "*" AutomatedParameter : has
    MyPlugin *-- "*" ParameterAutomationBinding : keeps

    MyPlugin *--  "*" ParameterValue~T~: has static
    MyPlugin *--  "*" ParameterValue~T~   : has automated

    ParameterAutomationBinding o-- AutomatedParameter
    ParameterAutomationBinding o-- ParameterValue~T~
    AutomatedParameter o-- CachedValue~T~ : synced to
    AutomatedParameter o.. ParameterDef~T~ : added with

    ParameterValue *-- CachedValue~T~ : has
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

```text
leaving out for a while for clarity

    class MyPluginUI {
        <<juce::Component>>
    }

    MyPlugin *-- "*" AutomatedParameter : has

    MyPluginUI *-- "*" Widget : contains
    MyPluginUI *-- "*" WidgetParamBinding : contains

    Plugin <|-- MyPlugin : is

    DynamicParams *-- "*" ParameterValue~T~ : contains
    StaticParams *-- "*" ParameterValue : contains

    ParamBindingBase <|-- WidgetParamBinding : is

    WidgetParamBinding *-- MouseListenerWithCallback : has
    MouseListenerWithCallback o.. WidgetParamBinding : calls back
    MouseListenerWithCallback o-- Widget : listens to

    MyPlugin *-- ValueTree : has
    MyPlugin *--  "*" ParameterValue~T~: has static
    MyPlugin *--  "*" ParameterValue~T~   : has automated
    MyPluginUI o-- MyPlugin : edits



```

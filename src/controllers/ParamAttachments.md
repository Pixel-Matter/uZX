# Parameter Attachments Documentation

## Overview

The Parameter Attachments system provides a unified, type-safe way to bind UI components to data parameters using JUCE's ValueTree and UndoManager. This system replaces the previous CachedValueWithValue approach with a more robust template-based solution.

## Core Classes

### ParamAttachment<Type>

Base template class for parameter binding with ValueTree persistence and undo support.

```cpp
ParamAttachment<int> myParam(valueTree, "paramId", undoManager, defaultValue);
myParam = 42;  // Sets value with undo support
int value = myParam.get();  // Gets current value
```

### ChoiceParamAttachment<Type>

Specialized for enum/choice parameters with string labels.

```cpp
ChoiceParamAttachment<Scale::Key> keyParam(
    valueTree, "key", "Key Selection", 
    {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"},
    undoManager, Scale::Key::C
);
```

### RangedParamAttachment<Type>

For numeric parameters with min/max ranges and units.

```cpp
RangedParamAttachment<float> freqParam(
    valueTree, "frequency",
    NormalisableRange<float>(20.0f, 20000.0f, 1.0f),
    undoManager, 440.0f, "Hz"
);
```

## UI Bindings

### ComboBoxBinding

Automatically syncs ComboBox with ChoiceParamAttachment (handles 1-based indexing).

```cpp
ComboBoxBinding binding(myComboBox, choiceParam);
// ComboBox automatically updates when parameter changes
// Parameter updates when user selects different item
```

### SliderAttachment

Binds Slider to Tracktion AutomatableParameter with gesture support.

```cpp
SliderAttachment attachment(mySlider, tracktionParam);
// Handles parameter automation, gestures, and popup display
```

## Key Features

- **Type Safety**: Template-based system prevents type mismatches
- **Undo Support**: All changes recorded in UndoManager
- **ValueTree Persistence**: Parameters saved/loaded automatically
- **Listener Management**: Proper cleanup and lifecycle handling
- **Bidirectional Sync**: UI and parameters stay synchronized

## Usage Pattern

1. Create ValueTree and UndoManager
2. Create ParamAttachment instances with unique IDs
3. Create UI bindings to connect components
4. Parameters automatically persist and support undo/redo

```cpp
// Setup
ValueTree state("MyComponent");
UndoManager undoManager;

// Create parameter
ChoiceParamAttachment<MyEnum> param(state, "choice", choices, &undoManager, defaultValue);

// Bind to UI
ComboBoxBinding binding(comboBox, param);

// Use parameter
param = MyEnum::SomeValue;  // Updates UI automatically
```
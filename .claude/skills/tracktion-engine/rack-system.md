# Rack System

Guide to Tracktion Engine's modular rack system for complex plugin routing and processing.

## Overview

The Rack system provides modular, template-based plugin architectures with flexible routing:
- **RackType**: Template/blueprint for rack configuration
- **RackInstance**: Instantiation of a RackType on a track
- Multi-bus routing within racks
- Parallel and series processing chains

## RackType - Creating Templates

```cpp
// Create rack type
auto rackType = new te::RackType(edit, juce::ValueTree(te::IDs::RACKTYPE));
rackType->setName("My Rack");

// Add plugin slots
rackType->addPlugin(te::EqualizerPlugin::create(), -1);
rackType->addPlugin(te::CompressorPlugin::create(), -1);

// Configure routing
// ... setup connections
```

## RackInstance - Using Racks

```cpp
// Create rack instance on track
auto track = edit.getAudioTracks()[0];
auto rackInstance = track->pluginList.insertPlugin(
    te::RackInstance::create(rackType), -1);

// Access plugins in instance
auto& plugins = rackInstance->getPlugins();
```

## Routing Configuration

```cpp
// Define input/output buses
rackType->addInputBus("Main In");
rackType->addOutputBus("Main Out");

// Connect plugin chains
rackType->connectPlugins(plugin1, plugin2);

// Parallel processing
rackType->addParallelChain(plugin3);
```

## Use Cases

1. **Multi-effect chains**: EQ → Compressor → Reverb
2. **Parallel processing**: Dry/wet mixing
3. **Multi-bus routing**: Complex signal flow
4. **Reusable templates**: Save and load rack configurations
5. **Modular instruments**: Layered synth architectures

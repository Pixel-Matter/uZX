#pragma once

#include <JuceHeader.h>
#include <concepts>

namespace MoTool::uZX {

// C++20 concept for parameter sources
template<typename T>
concept ParameterSource = requires(const T& t) {
    { t.getValue() } -> std::convertible_to<float>;
    { t.hasChanged() } -> std::convertible_to<bool>;
};

// Template-based parameter sources - zero runtime overhead
struct TracktionParameterSource {
    tracktion::AutomatableParameter& param_;

    TracktionParameterSource(tracktion::AutomatableParameter& p) : param_(p) {}
    float getValue() const { return param_.getCurrentValue(); }
    bool hasChanged() const { return false; } // Can implement if needed
};

struct JuceParameterSource {
    std::atomic<float>* valuePtr_;

    JuceParameterSource(std::atomic<float>& value) : valuePtr_(&value) {}
    float getValue() const { return valuePtr_->load(); }
    bool hasChanged() const { return false; } // Can implement if needed
};

// Static assertions to verify our types satisfy the concept
static_assert(ParameterSource<TracktionParameterSource>);
static_assert(ParameterSource<JuceParameterSource>);

// Parameter bundle concept - allows instruments to define their parameter sets
template<typename T>
concept ParameterBundle = requires(const T& bundle) {
    typename T::ParameterSourceType;
    requires ParameterSource<typename T::ParameterSourceType>;
};

// Base template for parameter bundles
template<ParameterSource ParamType>
struct ParameterBundleBase {
    using ParameterSourceType = ParamType;
};

// Template-based voice class
template<ParameterSource ParamSourceType>
class ChipInstrumentVoiceTemplate {
public:
    using ParameterSourceType = ParamSourceType;

protected:
    // Voice classes will be specialized with specific parameter sets
    virtual void updateParametersFromBundle() = 0;

public:
    virtual ~ChipInstrumentVoiceTemplate() = default;

    void renderNextBlock() {
        updateParametersFromBundle();
        // Rendering logic uses updated parameters
    }
};

// Type aliases for convenience
using TracktionChipVoice = ChipInstrumentVoiceTemplate<TracktionParameterSource>;
using JuceChipVoice = ChipInstrumentVoiceTemplate<JuceParameterSource>;

}  // namespace MoTool::uZX
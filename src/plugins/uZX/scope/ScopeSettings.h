#pragma once

#include "../../../controllers/Parameters.h"
#include "TriggerStrategy.h"

namespace MoTool::uZX {

namespace ScopeIDs {
    #define DECLARE_ID(name) inline const juce::Identifier name(#name);
    DECLARE_ID(windowSamples)
    DECLARE_ID(gain)
    DECLARE_ID(triggerMode)
    DECLARE_ID(triggerLevel)
    #undef DECLARE_ID
}  // namespace ScopeIDs


/**
 * Shared oscilloscope settings for both ScopePlugin and AYPlugin.
 * These parameters control waveform display appearance and behavior.
 */
struct ScopeSettings : ParamsBase<ScopeSettings> {
    using ParamsBase<ScopeSettings>::ParamsBase;

    template<typename Visitor>
    void visit(Visitor&& visitor) {
        visitor(windowSamples);
        visitor(gain);
        visitor(triggerMode);
        visitor(triggerLevel);
    }

    ParameterValue<int> windowSamples {
        {"window", ScopeIDs::windowSamples, "Window", "Display window size (samples)",
         1024, {256, 4096, 256}}
    };

    ParameterValue<float> gain {
        {"gain", ScopeIDs::gain, "Gain", "Display gain multiplier",
         1.0f, {0.1f, 10.0f, 0.1f}}
    };

    ParameterValue<TriggerMode> triggerMode {
        {"trigger", ScopeIDs::triggerMode, "Trigger", "Trigger mode",
         TriggerMode::RisingEdge}
    };

    ParameterValue<float> triggerLevel {
        {"trigLevel", ScopeIDs::triggerLevel, "Level", "Trigger threshold level",
         0.0f, {-1.0f, 1.0f, 0.01f}}
    };
};

}  // namespace MoTool::uZX

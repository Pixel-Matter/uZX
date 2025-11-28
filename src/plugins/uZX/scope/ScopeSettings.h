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
    DECLARE_ID(sourceMode)
    DECLARE_ID(channelOffset)
    #undef DECLARE_ID
}  // namespace ScopeIDs


/**
 * Source mode selection for oscilloscope.
 */
struct SourceModeEnum {
    enum Enum {
        StereoMix = 0,       // Display channels 0-1 (L, R stereo mix)
        SeparateChannels = 1 // Display separate channels starting from offset
    };
};
using SourceMode = Util::EnumChoice<SourceModeEnum>;


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
        visitor(sourceMode);
        visitor(channelOffset);
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

    ParameterValue<SourceMode> sourceMode {
        {"source", ScopeIDs::sourceMode, "Source", "Channel source (0=Stereo Mix L/R, 1=Separate Channels)",
         SourceMode::StereoMix}
    };

    ParameterValue<int> channelOffset {
        {"offset", ScopeIDs::channelOffset, "Offset", "Starting channel for separate mode (default 2 = A,B,C)",
         2, {0, 4, 1}}
    };
};

}  // namespace MoTool::uZX


namespace juce {

using namespace MoTool::uZX;
using namespace MoTool::Util;

template <>
struct VariantConverter<SourceMode> : public EnumVariantConverter<SourceMode> {};

}

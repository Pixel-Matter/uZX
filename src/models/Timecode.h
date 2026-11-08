#pragma once

#include <JuceHeader.h>

#include <optional>

namespace te = tracktion;

namespace MoTool {

/** The display layout of the timeline timecode, independent of the frame rate.
    The frame rate is edit-global and lives in the IDs::projectFps property. */
enum class TimecodeDisplayMode {
    seconds,          // MM:SS:FF at the project fps
    barsBeatsFrames,  // bar.beat.frame at the project fps
    framesOnly,       // absolute frames at the project fps
};

/** A display mode and optional fps recovered from a legacy timecodeFormat string,
    as written by uZX versions that encoded both in one enum value. */
struct LegacyTimecodeFormat {
    TimecodeDisplayMode mode = TimecodeDisplayMode::barsBeatsFrames;
    std::optional<double> fps;
};

/** Parses the legacy te::IDs::timecodeFormat property values ("seconds", "beats",
    "fps50", "barsBeatsFps50", ...). Unknown or empty strings, and the dropped
    frameless "beats", map to the default barsBeatsFrames mode with no fps. */
inline LegacyTimecodeFormat parseLegacyTimecodeFormat(const juce::String& v) {
    if (v == "seconds")
        return { TimecodeDisplayMode::seconds, {} };

    auto fpsFrom = [](const juce::String& s) -> std::optional<double> {
        auto fps = s.getDoubleValue();
        return fps > 0.0 ? std::optional<double>(fps) : std::nullopt;
    };

    if (v.startsWith("barsBeatsFps"))
        return { TimecodeDisplayMode::barsBeatsFrames, fpsFrom(v.substring(12)) };
    if (v.startsWith("fps"))
        return { TimecodeDisplayMode::framesOnly, fpsFrom(v.substring(3)) };

    return {};
}

}  // namespace MoTool

namespace juce {

template<>
struct VariantConverter<MoTool::TimecodeDisplayMode> {
    static MoTool::TimecodeDisplayMode fromVar(const var& v) {
        if (v == "seconds")    return MoTool::TimecodeDisplayMode::seconds;
        if (v == "framesOnly") return MoTool::TimecodeDisplayMode::framesOnly;
        return MoTool::TimecodeDisplayMode::barsBeatsFrames;
    }

    static var toVar(MoTool::TimecodeDisplayMode mode) {
        switch (mode) {
            case MoTool::TimecodeDisplayMode::seconds:         return "seconds";
            case MoTool::TimecodeDisplayMode::barsBeatsFrames: return "barsBeatsFrames";
            case MoTool::TimecodeDisplayMode::framesOnly:      return "framesOnly";
        }
        jassertfalse;
        return "barsBeatsFrames";
    }
};

}

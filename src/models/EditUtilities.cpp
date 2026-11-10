
#include "EditUtilities.h"

#include <numeric>

#include "Ids.h"

namespace te = tracktion;

namespace MoTool::Helpers {

TimecodeDisplayMode getTimecodeDisplayMode(te::Edit& edit) {
    if (edit.state.hasProperty(IDs::timecodeDisplayMode))
        return VariantConverter<TimecodeDisplayMode>::fromVar(edit.state[IDs::timecodeDisplayMode]);

    // Edits saved before this property existed encoded mode and fps together
    // in the te timecodeFormat property.
    return parseLegacyTimecodeFormat(edit.state[te::IDs::timecodeFormat].toString()).mode;
}

void setTimecodeDisplayMode(te::Edit& edit, TimecodeDisplayMode mode) {
    // A view setting: saved with the edit, but outside undo, so switching it
    // neither lands on Cmd+Z nor marks the edit as changed.
    edit.state.setProperty(IDs::timecodeDisplayMode, VariantConverter<TimecodeDisplayMode>::toVar(mode), nullptr);

    // Keep the te-native property on a value Tracktion understands.
    const bool musical = mode == TimecodeDisplayMode::barsBeatsFrames;
    edit.state.setProperty(te::IDs::timecodeFormat, musical ? "beats" : "seconds", nullptr);
}

void migrateTimecodeDisplaySettings(te::Edit& edit) {
    if (edit.state.hasProperty(IDs::timecodeDisplayMode))
        return;

    const auto legacy = parseLegacyTimecodeFormat(edit.state[te::IDs::timecodeFormat].toString());
    if (!edit.state.hasProperty(IDs::projectFps)) {
        if (legacy.fps.has_value())
            setProjectFps(edit, *legacy.fps);
    }

    setTimecodeDisplayMode(edit, legacy.mode);
}

double getProjectFps(te::Edit& edit) {
    if (edit.state.hasProperty(IDs::projectFps))
        return (double) edit.state.getProperty(IDs::projectFps);

    // Back-fill old edits from the timecode format they were saved with.
    if (auto fps = parseLegacyTimecodeFormat(edit.state[te::IDs::timecodeFormat].toString()).fps)
        return *fps;

    return 50.0;
}

void setProjectFps(te::Edit& edit, double fps) {
    edit.state.setProperty(IDs::projectFps, fps, &edit.getUndoManager());
}

std::vector<int> getDefaultGridSubdivisionPattern(int framesPerBeat) {
    if (framesPerBeat < 6)
        return {};

    if (framesPerBeat % 4 == 0)
        return std::vector<int>(4, framesPerBeat / 4);

    if (framesPerBeat % 2 == 0)
        return std::vector<int>(2, framesPerBeat / 2);

    return {};
}

std::vector<int> parseGridSubdivisionPattern(const juce::String& text, int framesPerBeat) {
    if (framesPerBeat < 6)
        return {};

    std::vector<int> pattern;
    String token;
    for (const auto c : text) {
        if (c >= '0' && c <= '9') {
            token += c;
        } else if (token.isNotEmpty()) {
            if (auto frames = token.getIntValue(); frames > 0)
                pattern.push_back(frames);
            token.clear();
        }
    }
    if (token.isNotEmpty()) {
        if (auto frames = token.getIntValue(); frames > 0)
            pattern.push_back(frames);
    }

    if (pattern.empty())
        return {};

    auto sum = std::accumulate(pattern.begin(), pattern.end(), 0);
    if (sum < framesPerBeat) {
        pattern.back() += framesPerBeat - sum;
    } else if (sum > framesPerBeat) {
        auto excess = sum - framesPerBeat;
        while (excess > 0 && !pattern.empty()) {
            auto& last = pattern.back();
            const auto reduction = std::min(last, excess);
            last -= reduction;
            excess -= reduction;
            if (last == 0)
                pattern.pop_back();
        }
    }

    if (pattern.size() < 2)
        pattern.clear();
    return pattern;
}

std::vector<int> getGridSubdivisionPattern(te::Edit& edit, int framesPerBeat) {
    if (!edit.state.hasProperty(IDs::gridSubdivision))
        return getDefaultGridSubdivisionPattern(framesPerBeat);

    return parseGridSubdivisionPattern(edit.state[IDs::gridSubdivision].toString(), framesPerBeat);
}

juce::Optional<std::pair<int, int>> parseTimeSig(const juce::String& text, int currentDenominator) {
    const auto trimmed = text.trim();
    const auto numeratorText = trimmed.upToFirstOccurrenceOf("/", false, false).trim();
    const auto denominatorText = trimmed.fromFirstOccurrenceOf("/", false, false).trim();
    if (!numeratorText.containsOnly("0123456789") || numeratorText.isEmpty()
        || (trimmed.containsChar('/') && (denominatorText.isEmpty() || !denominatorText.containsOnly("0123456789"))))
        return {};
    const int numerator = numeratorText.getIntValue();
    const int denominator = trimmed.containsChar('/') ? denominatorText.getIntValue() : currentDenominator;
    if (numerator < 1 || numerator > 32 || denominator < 1 || denominator > 32 || !juce::isPowerOfTwo(denominator))
        return {};
    return std::pair{numerator, denominator};
}

String formatGridSubdivisionPattern(const std::vector<int>& pattern) {
    StringArray tokens;
    for (auto frames : pattern)
        tokens.add(String(frames));
    return tokens.joinIntoString(" ");
}

void setGridSubdivisionPattern(te::Edit& edit, const std::vector<int>& pattern) {
    if (pattern.size() < 2) {
        edit.state.removeProperty(IDs::gridSubdivision, &edit.getUndoManager());
        return;
    }

    edit.state.setProperty(IDs::gridSubdivision, formatGridSubdivisionPattern(pattern), &edit.getUndoManager());
}

SnapLadder makeSnapLadder(te::Edit& edit, te::TimePosition tempoAt) {
    return makeSnapLadder(edit, tempoAt, getTimecodeDisplayMode(edit));
}

SnapLadder makeSnapLadder(te::Edit& edit, te::TimePosition tempoAt, TimecodeDisplayMode mode) {
    const auto fps = getProjectFps(edit);
    const auto& tempo = edit.tempoSequence.getTempoAt(tempoAt);
    const auto framesPerBeat = jmax(1, roundToInt(fps * tempo.getApproxBeatLength().inSeconds()));

    return SnapLadder(mode, fps, framesPerBeat, getGridSubdivisionPattern(edit, framesPerBeat));
}

}  // namespace MoTool::Helpers

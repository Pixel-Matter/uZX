
#include "EditUtilities.h"

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
    edit.state.setProperty(IDs::timecodeDisplayMode,
                           VariantConverter<TimecodeDisplayMode>::toVar(mode),
                           &edit.getUndoManager());

    // Keep the te-native property on a value Tracktion understands.
    const bool musical = mode == TimecodeDisplayMode::barsBeats
                      || mode == TimecodeDisplayMode::barsBeatsFrames;
    edit.state.setProperty(te::IDs::timecodeFormat, musical ? "beats" : "seconds",
                           &edit.getUndoManager());
}

void migrateTimecodeDisplaySettings(te::Edit& edit) {
    if (edit.state.hasProperty(IDs::timecodeDisplayMode))
        return;

    const auto legacy = parseLegacyTimecodeFormat(edit.state[te::IDs::timecodeFormat].toString());
    if (! edit.state.hasProperty(IDs::projectFps)) {
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

std::vector<int> getGridSubdivisionPattern(te::Edit& edit) {
    StringArray tokens;
    tokens.addTokens(edit.state[IDs::gridSubdivision].toString(), false);

    std::vector<int> pattern;
    for (const auto& token : tokens)
        if (auto frames = token.getIntValue(); frames > 0)
            pattern.push_back(frames);

    if (pattern.size() < 2)
        pattern.clear();
    return pattern;
}

void setGridSubdivisionPattern(te::Edit& edit, const std::vector<int>& pattern) {
    if (pattern.size() < 2) {
        edit.state.removeProperty(IDs::gridSubdivision, &edit.getUndoManager());
        return;
    }

    StringArray tokens;
    for (auto frames : pattern)
        tokens.add(String(frames));
    edit.state.setProperty(IDs::gridSubdivision, tokens.joinIntoString(" "), &edit.getUndoManager());
}

juce::PopupMenu buildTimecodeFormatMenu(te::Edit& edit) {
    const auto current = getTimecodeDisplayMode(edit);

    // The menu picks a display *mode* only; the frame rate stays the project fps
    // (changed separately via the transport fps readout).
    int nextId = 1;
    auto item = [&edit, current, &nextId](const String& name, TimecodeDisplayMode mode) {
        PopupMenu::Item it;
        it.itemID = nextId++;
        it.text = name;
        it.isTicked = (current == mode);
        it.action = [&edit, mode] {
            setTimecodeDisplayMode(edit, mode);
        };
        return it;
    };

    PopupMenu menu;
    menu.addItem(item("Bars | Beats", TimecodeDisplayMode::barsBeats));
    menu.addItem(item("Bars | Beats | Frames", TimecodeDisplayMode::barsBeatsFrames));
    menu.addItem(item("Seconds", TimecodeDisplayMode::seconds));
    menu.addItem(item("Frames Only", TimecodeDisplayMode::framesOnly));
    return menu;
}

} // namespace MoTool::Helpers


#include "EditUtilities.h"

#include "Ids.h"

namespace te = tracktion;

namespace MoTool::Helpers {

TimecodeDisplayFormatExt getEditTimecodeFormat(te::Edit& edit) {
    // TODO use Cached value in EditViewState?
    auto value = edit.state.getPropertyAsValue(te::IDs::timecodeFormat, nullptr);
    return VariantConverter<TimecodeDisplayFormatExt>::fromVar(value);
}

void setEditTimecodeFormat(te::Edit& edit, TimecodeDisplayFormatExt format) {
    edit.state.setProperty(te::IDs::timecodeFormat, VariantConverter<TimecodeDisplayFormatExt>::toVar(format), &edit.getUndoManager());
}

double getProjectFps(te::Edit& edit) {
    if (edit.state.hasProperty(IDs::projectFps))
        return (double) edit.state.getProperty(IDs::projectFps);
    // Back-fill old edits from the timecode format they were saved with.
    return getEditTimecodeFormat(edit).getFPS();
}

void setProjectFps(te::Edit& edit, double fps) {
    edit.state.setProperty(IDs::projectFps, fps, &edit.getUndoManager());

    // Keep the timecode display format on the same fps, preserving its display mode.
    auto current = getEditTimecodeFormat(edit);
    auto retimed = TimecodeDisplayFormatExt::makeTimecodeType(current.getDisplayMode(), fps);
    if (retimed != current.typeExt)
        setEditTimecodeFormat(edit, retimed);
}

juce::PopupMenu buildTimecodeFormatMenu(te::Edit& edit) {
    auto current = getEditTimecodeFormat(edit);
    const auto fps = getProjectFps(edit);

    // The menu picks a display *mode* only; the frame rate stays the project fps
    // (changed separately via the transport fps readout).
    int nextId = 1;
    auto item = [&edit, &current, &nextId, fps](const String& name, TimecodeDisplayMode mode) {
        PopupMenu::Item it;
        it.itemID = nextId++;
        it.text = name;
        it.isTicked = (current.getDisplayMode() == mode);
        it.action = [&edit, mode, fps] {
            setEditTimecodeFormat(edit, TimecodeDisplayFormatExt::makeTimecodeType(mode, fps));
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
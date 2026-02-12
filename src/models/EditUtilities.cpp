
#include "EditUtilities.h"

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

juce::PopupMenu buildTimecodeFormatMenu(te::Edit& edit) {
    auto current = getEditTimecodeFormat(edit);

    int nextId = 1;
    auto item = [&edit, &current, &nextId](const String& name, TimecodeTypeExt type) {
        PopupMenu::Item it;
        it.itemID = nextId++;
        it.text = name;
        it.isTicked = (current.typeExt == type);
        it.action = [&edit, type] { setEditTimecodeFormat(edit, type); };
        return it;
    };

    struct FpsEntry { const char* label; TimecodeTypeExt barsBeats; TimecodeTypeExt framesOnly; };
    const FpsEntry fpsEntries[] = {
        { "24 fps",  TimecodeTypeExt::barsBeatsFps24,  TimecodeTypeExt::fps24  },
        { "25 fps",  TimecodeTypeExt::barsBeatsFps25,  TimecodeTypeExt::fps25  },
        { "30 fps",  TimecodeTypeExt::barsBeatsFps30,  TimecodeTypeExt::fps30  },
        { "48 fps",  TimecodeTypeExt::barsBeatsFps48,  TimecodeTypeExt::fps48  },
        { "50 fps",  TimecodeTypeExt::barsBeatsFps50,  TimecodeTypeExt::fps50  },
        { "60 fps",  TimecodeTypeExt::barsBeatsFps60,  TimecodeTypeExt::fps60  },
        { "100 fps", TimecodeTypeExt::barsBeatsFps100, TimecodeTypeExt::fps100 },
        { "200 fps", TimecodeTypeExt::barsBeatsFps200, TimecodeTypeExt::fps200 },
    };

    PopupMenu menu;

    menu.addItem(item("Seconds", TimecodeTypeExt::millisecs));
    menu.addItem(item("Bars & Beats", TimecodeTypeExt::barsBeats));
    menu.addSeparator();

    // Bars | Beats | Frames submenu
    PopupMenu bbfMenu;
    for (auto& fps : fpsEntries)
        bbfMenu.addItem(item(fps.label, fps.barsBeats));
    menu.addSubMenu("Bars | Beats | Frames", bbfMenu,
                     true, nullptr, false);

    menu.addSeparator();

    // Frames only submenu
    PopupMenu framesMenu;
    for (auto& fps : fpsEntries)
        framesMenu.addItem(item(fps.label, fps.framesOnly));
    menu.addSubMenu("Frames Only", framesMenu,
                     true, nullptr, false);

    return menu;
}

} // namespace MoTool::Helpers
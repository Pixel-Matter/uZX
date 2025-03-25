#include <JuceHeader.h>

#include "App.h"
#include "Commands.h"

#include "../common/UIBehavior.h"
#include "../../model/Behavior.h"
#include "../common/LookAndFeel.h"
#include "../../plugins/uZX/aychip/AYPlugin.h"

#include <memory>

using namespace juce;
using namespace MoTool::Commands;
using namespace MoTool::EditFileOps;


namespace MoTool {

MoToolApp::MoToolApp()
    : engine_ { CharPointer_UTF8(ProjectInfo::projectName), std::make_unique<ExtUIBehaviour>(), std::make_unique<ExtEngineBehaviour>() }
    , lookAndFeel {std::make_unique<MoLookAndFeel>()}
{
    // DBG("Engine properties storage is " << engine_.getPropertyStorage().getPropertiesFile().getFile().getFullPathName());
    juce::LookAndFeel::setDefaultLookAndFeel(lookAndFeel.get());
    engine_.getPluginManager().createBuiltInType<uZX::AYChipPlugin>();
}

void MoToolApp::initialise(const String&) {
    auto title = getApplicationFancyName() + " v" + getApplicationVersion();
    mainWindow_ = std::make_unique<MainWindow>(std::move(title), engine_, getCommandManager());
}

void MoToolApp::shutdown() {
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    mainWindow_ = nullptr;
}

void MoToolApp::systemRequestedQuit() {
    quit();
}

ApplicationCommandManager& MoToolApp::getCommandManager() {
    return getApp().commandManager;
}

MoToolApp& MoToolApp::getApp() {
    return *dynamic_cast<MoToolApp*>(JUCEApplication::getInstance());
}

namespace Commands {

// ApplicationCommandManager& getGlobalCommandManager() {
//     return MoToolApp::getCommandManager();
// }

} // namespace Commands

} // namespace MoTool

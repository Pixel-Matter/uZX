#pragma once

#include <JuceHeader.h>

#include "MainController.h"
#include "PlayerController.h"
#include "../gui/common/LookAndFeel.h"

namespace MoTool {

class MoToolApp : public JUCEApplication {
public:

    enum Target {
        uZXStudio,
        uZXTuning,
        uZXPlayer,
        MoTool
    };

    MoToolApp();

    const String getApplicationName() override;
    const String getApplicationVersion() override;

    bool moreThanOneInstanceAllowed() override;

    void initialise(const String&) override;

    void shutdown() override;

    void systemRequestedQuit() override;

    static MoToolApp& getApp();

    static AppController& getAppController();
    static ArrangerController& getArrangerController();
    static PlayerController& getPlayerController();

    static ApplicationCommandManager& getCommandManager() {
        return getAppController().getCommandManager();
    }

    static te::SelectionManager& getSelectionManager() {
        return getAppController().getSelectionManager();
    }

    static Target getTarget();

    const MoLookAndFeel& getLookAndFeel() const { return lookAndFeel_; }

private:
    static Target target_;  // defined in Main.cpp via MOTOOL_APP_TARGET

    MoLookAndFeel lookAndFeel_;
    std::unique_ptr<AppController> appController_;
    std::unique_ptr<ArrangerController> arrangerController_;
    std::unique_ptr<PlayerController> playerController_;
};

} // namespace MoTool

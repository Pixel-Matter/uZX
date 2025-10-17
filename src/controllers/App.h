#pragma once

#include <JuceHeader.h>

#include "MainController.h"
#include "../gui/common/LookAndFeel.h"

namespace MoTool {

class MoToolApp : public JUCEApplication {
public:

    enum Target {
        uZXStudio,
        uZXTuning,
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
    // static TuningController* getTuningController();

    static ApplicationCommandManager& getCommandManager() {
        return getAppController().getCommandManager();
    }

    static te::SelectionManager& getSelectionManager() {
        return getAppController().getSelectionManager();
    }

    static Target getTarget();

    const MoLookAndFeel& getLookAndFeel() const { return lookAndFeel_; }

private:
    static inline Target target_ = String::fromUTF8(ProjectInfo::projectName) == "μZX Tuning" ? Target::uZXTuning : Target::uZXStudio;

    MoLookAndFeel lookAndFeel_;
    std::unique_ptr<AppController> appController_;
    std::unique_ptr<ArrangerController> arrangerController_;
};

} // namespace MoTool

#pragma once

#include <JuceHeader.h>

#include "MainController.h"

namespace MoTool {

class MoToolApp : public JUCEApplication {
public:

    enum Target {
        Main,
        Tuning
    };

    MoToolApp();

    const String getApplicationName() override;
    const String getApplicationVersion() override;

    const String getApplicationFancyName() const;
    const String getWindowTitle();

    bool moreThanOneInstanceAllowed() override ;

    void initialise(const String&) override;

    void shutdown() override;

    void systemRequestedQuit() override;

    static MoToolApp& getApp();

    static BaseController& getController();

    static Target getTarget();

    const MoLookAndFeel& getLookAndFeel() const { return lookAndFeel_; }

private:
static inline Target target_ = String::fromUTF8(ProjectInfo::projectName) == "μZXTuning" ? Target::Tuning : Target::Main;
    MoLookAndFeel lookAndFeel_;
    std::unique_ptr<BaseController> controller_;

};

} // namespace MoTool

#pragma once

#include <JuceHeader.h>

#include "MainController.h"

namespace MoTool {

class MoToolApp : public JUCEApplication {
public:
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

    static MainController& getController();

private:
    MoLookAndFeel lookAndFeel_;
    MainController controller_;
};

} // namespace MoTool

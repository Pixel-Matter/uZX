#pragma once

#include <JuceHeader.h>

#include "MainWindow.h"

#include <memory>

namespace MoTool {

class MoToolApp : public JUCEApplication {
public:
    MoToolApp();

    const String getApplicationFancyName();

    const String getApplicationName() override;

    const String getApplicationVersion() override;

    te::Engine& getEngine();

    bool moreThanOneInstanceAllowed() override ;

    void initialise(const String&) override;

    void shutdown() override;

    void systemRequestedQuit() override;

    MainWindow* getMainWindow() const;

    static ApplicationCommandManager& getCommandManager();

    static MoToolApp& getApp();

private:
    te::Engine engine_;
    std::unique_ptr<MainWindow> mainWindow_;
    std::unique_ptr<MoLookAndFeel> lookAndFeel;
    ApplicationCommandManager commandManager;
};

} // namespace MoTool

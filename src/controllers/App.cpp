#include "App.h"
#include "TuningController.h"
#include "PlayerController.h"
#include "MainController.h"

namespace MoTool {

MoToolApp::MoToolApp() {
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

    appController_ = std::make_unique<AppController>();

    if (target_ == Target::uZXPlayer) {
        playerController_ = std::make_unique<PlayerController>(appController_->getEngine());
    } else {
        arrangerController_ = std::make_unique<ArrangerController>(appController_->getEngine());
    }

    appController_->initialize();

    if (playerController_)
        playerController_->initialize();
    else
        arrangerController_->initialize();
}

const String MoToolApp::getApplicationName() {
    switch (target_) {
        case Target::uZXPlayer:  return String::fromUTF8("μZX Player");
        case Target::uZXTuning:  return String::fromUTF8("μZX Tuning");
        case Target::uZXStudio:  return String::fromUTF8("μZX Studio");
        case Target::MoTool:     return "MoTool";
    }
    return CharPointer_UTF8(ProjectInfo::projectName);
}

const String MoToolApp::getApplicationVersion() {
    return ProjectInfo::versionString;
}

bool MoToolApp::moreThanOneInstanceAllowed()  {
    return target_ != Target::uZXPlayer;
}

void MoToolApp::initialise(const String& commandLineParameters) {
    if (target_ == Target::uZXPlayer && commandLineParameters.isNotEmpty()) {
        // Handle file passed as command-line argument (e.g. double-click from Finder)
        auto file = File(commandLineParameters.unquoted().trim());
        if (file.existsAsFile())
            playerController_->handleOpenFile(file);
    }
}

void MoToolApp::anotherInstanceStarted(const String& commandLine) {
    if (target_ == Target::uZXPlayer && commandLine.isNotEmpty()) {
        auto file = File(commandLine.unquoted().trim());
        if (file.existsAsFile())
            playerController_->handleOpenFile(file);
        playerController_->bringWindowToFront();
    }
}

void MoToolApp::shutdown() {
    playerController_.reset();
    arrangerController_.reset();
    appController_.reset();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void MoToolApp::systemRequestedQuit() {
    quit();
}

MoToolApp& MoToolApp::getApp() {
    return *dynamic_cast<MoToolApp*>(JUCEApplication::getInstance());
}

AppController& MoToolApp::getAppController() {
    return *getApp().appController_;
}

ArrangerController& MoToolApp::getArrangerController() {
    return *getApp().arrangerController_;
}

PlayerController& MoToolApp::getPlayerController() {
    return *getApp().playerController_;
}

MoToolApp::Target MoToolApp::getTarget() {
    return target_;
}


} // namespace MoTool

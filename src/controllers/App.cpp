#include "App.h"
#include "TuningController.h"
#include "PlayerController.h"
#include "MainController.h"
#include "../version.h"

namespace MoTool {

MoToolApp::MoToolApp() {
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);
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
    return targetVersion_;
}

const char* MoToolApp::getTargetVersion() {
    return targetVersion_;
}

String MoToolApp::getReleaseVersion() {
    const auto channel = String::fromUTF8(Build::channel);
    return channel.isEmpty() ? String(targetVersion_) : targetVersion_ + ("-" + channel);
}

const char* MoToolApp::getReleaseTagPrefix() {
    return target_ == Target::uZXPlayer ? "player-v" : "v";
}

bool MoToolApp::hasMidiControl() {
    return target_ != Target::uZXPlayer;
}

bool MoToolApp::moreThanOneInstanceAllowed()  {
   #if JUCE_LINUX
    // ponytail: JUCE can't hand a file to the running Player on Linux (MessageManager::broadcastMessage is
    // a TODO there), so the file wouldn't open at all; each launch runs on its own instead. A socket or a
    // watched file in the settings folder would let a second launch hand its files over.
    return true;
   #else
    return target_ != Target::uZXPlayer;
   #endif
}

void MoToolApp::initialise(const String& commandLineParameters) {
    // Not in the constructor: a second launch that hands its files to the running Player never gets here,
    // so it doesn't start an engine, or load and save the startup edit, only to quit
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

    // Linux and Windows pass the files to open on the command line; macOS hands Finder's to anotherInstanceStarted
    if (target_ != Target::uZXTuning)
        getActiveController().handleOpenCommandLine(commandLineParameters);
}

void MoToolApp::anotherInstanceStarted(const String& commandLine) {
    if (target_ != Target::uZXTuning && commandLine.isNotEmpty()) {
        getActiveController().handleOpenCommandLine(commandLine);
        getActiveController().bringWindowToFront();
    }
}

void MoToolApp::shutdown() {
    playerController_.reset();
    arrangerController_.reset();
    appController_.reset();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void MoToolApp::systemRequestedQuit() {
    // Async dialogs (Audio/MIDI settings, plugin list) listen to the engine, but JUCE would only
    // delete them after shutdown() has destroyed it. Close them first, then quit once they're gone.
    if (ModalComponentManager::getInstance()->cancelAllModalComponents()) {
        Timer::callAfterDelay(100, [] {
            if (auto* app = JUCEApplicationBase::getInstance())
                app->systemRequestedQuit();
        });
        return;
    }
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

BaseController& MoToolApp::getActiveController() {
    if (getTarget() == Target::uZXPlayer)
        return getPlayerController();
    return getArrangerController();
}

MoToolApp::Target MoToolApp::getTarget() {
    return target_;
}


} // namespace MoTool

#include "App.h"
#include "TuningController.h"

namespace MoTool {

MoToolApp::MoToolApp()
{
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);
    switch (target_) {
        case Target::Main:
            controller_ = std::make_unique<MainController>();
            break;
        case Target::Tuning:
            controller_ = std::make_unique<TuningController>();
            break;
    }
    controller_->initialize();
    controller_->setMainWindowTitle(getWindowTitle());
}

const String MoToolApp::getApplicationFancyName() const {
    // TODO to controller
    if (getTarget() == Target::Tuning) {
        return CharPointer_UTF8("Pixel Matter μZX Tuning");
    }
    return CharPointer_UTF8("Pixel Matter μZX");
}

const String MoToolApp::getWindowTitle() {
    return getApplicationFancyName() + " v" + getApplicationVersion();
}

const String MoToolApp::getApplicationName() {
    return CharPointer_UTF8(ProjectInfo::projectName);
}

const String MoToolApp::getApplicationVersion() {
    return ProjectInfo::versionString;
}

bool MoToolApp::moreThanOneInstanceAllowed()  {
    return true;
}

void MoToolApp::initialise(const String&) {
}

void MoToolApp::shutdown() {
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void MoToolApp::systemRequestedQuit() {
    quit();
}

MoToolApp& MoToolApp::getApp() {
    return *dynamic_cast<MoToolApp*>(JUCEApplication::getInstance());
}

BaseController& MoToolApp::getController() {
    return *getApp().controller_;
}

MoToolApp::Target MoToolApp::getTarget() {
    return target_;
}


} // namespace MoTool

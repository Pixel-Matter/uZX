#include "App.h"
#include "TuningController.h"
#include "MainController.h"

namespace MoTool {

MoToolApp::MoToolApp() {
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

    // switch (target_) {
    //     case Target::uZXStudio:
    //         mainController_ = std::make_unique<StudioController>();
    //         break;
    //     // case Target::uZXTuning:
    //     //     mainController_ = std::make_unique<TuningController>();
    //     //     break;
    //     case Target::MoTool:
    //         jassertfalse;
    //         break;
    // }
    appController_ = std::make_unique<AppController>();
    arrangerController_ = std::make_unique<ArrangerController>(appController_->getEngine());

    appController_->initialize();
    arrangerController_->setMainWindowTitle(getWindowTitle());
    arrangerController_->initialize();
}

const String MoToolApp::getApplicationFancyName() const {
    // TODO to controller
    if (getTarget() == Target::uZXTuning) {
        return CharPointer_UTF8("Pixel Matter μZX Tuning");
    }
    return CharPointer_UTF8("Pixel Matter μZX Studio");
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

AppController& MoToolApp::getAppController() {
    return *getApp().appController_;
}

ArrangerController& MoToolApp::getArrangerController() {
    return *getApp().arrangerController_;
}

// TuningController* MoToolApp::getTuningController() {
//     return getApp().tuningController_.get();
// }

MoToolApp::Target MoToolApp::getTarget() {
    return target_;
}


} // namespace MoTool

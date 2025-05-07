#include "App.h"

namespace MoTool {

MoToolApp::MoToolApp()
{
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);
    controller_.setMainWindowTitle(getWindowTitle());
}

const String MoToolApp::getApplicationFancyName() const {
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

MainController& MoToolApp::getController() {
    return getApp().controller_;
}

} // namespace MoTool

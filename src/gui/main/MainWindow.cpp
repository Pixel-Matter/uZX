#include "MainWindow.h"
#include "../../controllers/PropertyStorage.h"
#include "../../gui/common/LookAndFeel.h"

using namespace juce;

namespace MoTool {

MainWindow::MainWindow(tracktion::Engine& engine)
    : DocumentWindow("", Colors::Theme::backgroundAlt, DocumentWindow::allButtons)
    , engine_(engine)
{
    setComponentID("studio");
    setUsingNativeTitleBar(true);
    setResizable(true, true);

    // Set up minimum size constraints
    constrainer_.setMinimumSize(800, 480);
    setConstrainer(&constrainer_);

    restoreWindowBounds();
    setVisible(true);
}

MainWindow::~MainWindow() {
    saveWindowBounds();
    clearContentComponent();
}

void MainWindow::closeButtonPressed() {
    JUCEApplication::getInstance()->systemRequestedQuit();
}

void MainWindow::resized() {
    DocumentWindow::resized();
    saveWindowBounds();
}

void MainWindow::saveWindowBounds() {
    if (!windowBoundsRestored_)
        return; // Don't save if we haven't restored yet (first run)

    auto bounds = getBounds();

    auto& propertyStorage = static_cast<MoTool::PropertyStorage&>(engine_.getPropertyStorage());
    propertyStorage.setCustomProperty(getComponentID() + "WindowX", bounds.getX());
    propertyStorage.setCustomProperty(getComponentID() + "WindowY", bounds.getY());
    propertyStorage.setCustomProperty(getComponentID() + "WindowWidth", bounds.getWidth());
    propertyStorage.setCustomProperty(getComponentID() + "WindowHeight", bounds.getHeight());
}

void MainWindow::restoreWindowBounds() {
    auto& propertyStorage = static_cast<MoTool::PropertyStorage&>(engine_.getPropertyStorage());
    int x = propertyStorage.     getCustomProperty(getComponentID() + "WindowX", -1);
    int y = propertyStorage.     getCustomProperty(getComponentID() + "WindowY", -1);
    int width = propertyStorage. getCustomProperty(getComponentID() + "WindowWidth", 1024);
    int height = propertyStorage.getCustomProperty(getComponentID() + "WindowHeight", 740);

    // The constrainer will automatically enforce minimum size
    Rectangle<int> bounds(x, y, width, height);
    constrainer_.checkBounds(bounds, getBounds(),
                            Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea,
                            false, false, false, false);

    if (x >= 0 && y >= 0) {
        // Ensure the window is still on screen
        auto& desktop = Desktop::getInstance();
        auto displays = desktop.getDisplays();
        bool onScreen = false;

        for (auto& display : displays.displays) {
            if (display.userArea.contains(bounds.getTopLeft())) {
                onScreen = true;
                break;
            }
        }

        if (onScreen) {
            setBounds(bounds);
        } else {
            setSize(bounds.getWidth(), bounds.getHeight());
            centreWithSize(bounds.getWidth(), bounds.getHeight());
        }
    } else {
        setSize(bounds.getWidth(), bounds.getHeight());
        centreWithSize(bounds.getWidth(), bounds.getHeight());
    }
    windowBoundsRestored_ = true;
}

} // namespace MoTool
#include "TrackDevicesPanel.h"
#include "PluginTree.h"
#include "GenericPluginAdapters.h"
#include "DevicePanelItem.h"
#include "../common/LookAndFeel.h"

#include <common/Utilities.h>  // from Tracktion

namespace {
constexpr int deviceSpacing = 8;
constexpr int addButtonComponentWidth = 16;
}

namespace MoTool {

//==============================================================================
static te::Plugin::Ptr showMenuAndCreatePlugin(te::Edit& edit) {
    if (auto tree = EngineHelpers::createPluginTree(edit.engine)) {
        PluginTreeGroup root(edit, *tree, te::Plugin::Type::allPlugins);
        PluginMenu m(root);

        if (auto type = m.runMenu(root))
            return type->create(edit);
    }
    return {};
}

//==============================================================================
// AddButton implementation
AddButton::AddButton()
    : BackgroundlessTextButton("+")
{
    setTooltip("Add Plugin");
}

//==============================================================================
// AddButtonComponent implementation
AddButtonComponent::AddButtonComponent() {
    setSize(16, 16);
    addChildComponent(button);
}

void AddButtonComponent::resized() {
    auto width = getWidth();
    auto height = getHeight();
    // set a square button, centered vertically
    auto buttonSize = jmin(width, height);
    button.setBounds(0, 0, width, buttonSize);
}

void AddButtonComponent::mouseEnter(const MouseEvent&) {
    button.setVisible(true);
    startTimer(100); // Check every 100ms
}

void AddButtonComponent::timerCallback() {
    auto mousePos = getMouseXYRelative();
    if (!getLocalBounds().contains(mousePos)) {
        button.setVisible(false);
        stopTimer();
    }
}

//==============================================================================
TrackDevicesPanel::TrackDevicesPanel(EditViewState& evs)
    : editViewState(evs)
{
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(false, true);

    addAndMakeVisible(viewport);

    editViewState.selectionManager.addChangeListener(this);
}

TrackDevicesPanel::~TrackDevicesPanel() {
    editViewState.selectionManager.removeChangeListener(this);

    if (currentTrack.get())
        currentTrack.get()->state.removeListener(this);

    for (auto* device : devices) {
        if (device != nullptr)
            device->removeComponentListener(this);
    }
}

void TrackDevicesPanel::addButtonClicked(int index) {
    if (auto* track = currentTrack.get()) {
        if (auto plugin = showMenuAndCreatePlugin(track->edit)) {
            track->pluginList.insertPlugin(plugin, index, &editViewState.selectionManager);
        }
    }
}

void TrackDevicesPanel::setCurrentTrack(tracktion::Track* track) {
    if (currentTrack.get() == track)
        return;

    // Remove listener from old track
    if (currentTrack.get())
        currentTrack.get()->state.removeListener(this);

    currentTrack = track;

    // Add listener to new track
    if (currentTrack.get())
        currentTrack.get()->state.addListener(this);

    markAndUpdate(updateDevices);
}

void TrackDevicesPanel::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &editViewState.selectionManager) {
        if (auto* selectedTrack = editViewState.selectionManager.getFirstItemOfType<tracktion::Track>()) {
            setCurrentTrack(selectedTrack);
        }
        // else {
        //     setCurrentTrack(nullptr);
        // }
    }
}

bool TrackDevicesPanel::shouldBeShown(tracktion::Plugin* p) {
    // Skip VolumeAndPanPlugin only (we now show LevelMeterPlugin)
    if (dynamic_cast<tracktion::VolumeAndPanPlugin*>(p))
        return false;

    return true;
}

void TrackDevicesPanel::createAndAddNewPluginButton(int index) {
    auto addButton = new AddButtonComponent{};
    addButton->button.onClick = [this, index] {
        addButtonClicked(index);
    };
    addButtons.add(addButton);
    content.addAndMakeVisible(addButton);
}

void TrackDevicesPanel::buildPlugins() {
    for (auto* device : devices) {
        if (device != nullptr)
            device->removeComponentListener(this);
    }
    devices.clear();
    content.removeAllChildren();
    addButtons.clear();

    auto* track = currentTrack.get();
    if (track == nullptr) {
        content.setSize(0, getHeight());
        repaint();
        return;
    }

    int index = 0;

    juce::ScopedValueSetter<bool> rebuildingGuard(rebuildingDevices_, true, false);
    createAndAddNewPluginButton(index);

    for (auto plugin : track->pluginList) {
        ++index;
        // Skip VolumeAndPanPlugin only (we now show LevelMeterPlugin)
        if (!shouldBeShown(plugin)) {
            continue;
        }

        // Try to create device UI using adapter registry
        auto& registry = PluginUIAdapterRegistry::getInstance();
        std::unique_ptr<DeviceItem> item;
        bool canHasPlusButtonAfter = true;

        if (auto deviceUI = registry.createDeviceUI(plugin)) {
            // Custom device UI from registry - check if it needs framing
            if (deviceUI->hasCustomDeviceUI()) {
                // Frameless for custom UIs like LevelMeter
                item = std::make_unique<FramelessDeviceItem>(std::move(deviceUI));
            } else {
                // Framed for regular plugins
                item = std::make_unique<FramedDeviceItem>(std::move(deviceUI));
            }
            canHasPlusButtonAfter = item->getDeviceUI()->canHasPlusButtonAfter();
        } else {
            // Fallback: create generic UI for unknown plugins
            // TODO move fallback to registry.createDeviceUI method?
            auto genericUI = GenericPluginUIFactory::createGenericUI(plugin);
            if (genericUI) {
                item = std::make_unique<FramedDeviceItem>(std::move(genericUI));
                canHasPlusButtonAfter = item->getDeviceUI()->canHasPlusButtonAfter();
            }
        }

        if (item) {
            item->addComponentListener(this);
            content.addAndMakeVisible(item.get());
            devices.add(item.release());
        }
        if (canHasPlusButtonAfter) {
            createAndAddNewPluginButton(index);
        }
    }
    layoutDeviceComponents();
}

void TrackDevicesPanel::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::background);

    if (!currentTrack.get()) {
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(16.0f);
        g.drawText("Select a track to view its devices", getLocalBounds(), juce::Justification::centred, true);
    }
}

void TrackDevicesPanel::resized() {
    viewport.setBounds(getLocalBounds());
    layoutDeviceComponents();
}

// ValueTree::Listener methods
void TrackDevicesPanel::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& c) {
    if (c.hasType(tracktion::IDs::PLUGIN))
        markAndUpdate(updateDevices);
}

void TrackDevicesPanel::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& c, int) {
    if (c.hasType(tracktion::IDs::PLUGIN))
        markAndUpdate(updateDevices);
}

void TrackDevicesPanel::valueTreeChildOrderChanged(juce::ValueTree&, int, int) {
    markAndUpdate(updateDevices);
}

// FlaggedAsyncUpdater method
void TrackDevicesPanel::handleAsyncUpdate() {
    if (compareAndReset(updateDevices))
        buildPlugins();
}

void TrackDevicesPanel::componentMovedOrResized(juce::Component& component, bool, bool wasResized) {
    if (!wasResized || rebuildingDevices_)
        return;

    if (dynamic_cast<DeviceItem*>(&component) != nullptr) {
        layoutDeviceComponents();
    }
}

void TrackDevicesPanel::layoutDeviceComponents() {
    juce::ScopedValueSetter<bool> rebuildingGuard(rebuildingDevices_, true, false);

    auto height = getHeight();
    const int childHeight = juce::jmax(0, height - (2 * deviceSpacing));
    int x = 0;

    for (int i = 0; i < content.getNumChildComponents(); ++i) {
        auto* child = content.getChildComponent(i);
        if (child == nullptr || dynamic_cast<DevicePanelItemBase*>(child) == nullptr) {
            continue;
        }
        const int width = child->getWidth();
        child->setBounds(x, deviceSpacing, width, childHeight);
        x += width;
    }

    content.setSize(x, height);

    viewport.getViewedComponent()->repaint();
    repaint();
}

}  // namespace MoTool

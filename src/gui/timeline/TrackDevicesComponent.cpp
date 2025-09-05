#include "TrackDevicesComponent.h"

#include "../common/LookAndFeel.h"

namespace MoTool {

TrackDevicesComponent::TrackDevicesComponent(EditViewState& evs)
    : editViewState(evs)
{
    viewport.setViewedComponent(&contentComponent, false);
    viewport.setScrollBarsShown(false, true);

    addAndMakeVisible(viewport);

    // Set up the add button - add it to contentComponent so it scrolls with plugins
    contentComponent.addAndMakeVisible(addButton);

    addButton.onClick = [this] {
        if (auto* track = currentTrack.get()) {
            if (auto plugin = showMenuAndCreatePlugin(track->edit)) {
                track->pluginList.insertPlugin(plugin, 0, &editViewState.selectionManager);
            }
        }
    };

    editViewState.selectionManager.addChangeListener(this);
}

TrackDevicesComponent::~TrackDevicesComponent() {
    editViewState.selectionManager.removeChangeListener(this);

    if (currentTrack.get())
        currentTrack.get()->state.removeListener(this);
}

void TrackDevicesComponent::setCurrentTrack(tracktion::Track* track) {
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

void TrackDevicesComponent::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &editViewState.selectionManager) {
        if (auto* selectedTrack = editViewState.selectionManager.getFirstItemOfType<tracktion::Track>()) {
            setCurrentTrack(selectedTrack);
        }
        // else {
        //     setCurrentTrack(nullptr);
        // }
    }
}

bool TrackDevicesComponent::shouldBeShown(tracktion::Plugin* p) {
    // Skip VolumeAndPanPlugin only (we now show LevelMeterPlugin)
    if (dynamic_cast<tracktion::VolumeAndPanPlugin*>(p))
        return false;

    return true;
}

void TrackDevicesComponent::buildPlugins() {
    devices.clear();

    contentComponent.removeAllChildren();

    const int addButtonWidth = 16;
    const int deviceUIWidth = 16;  // Width for device UIs like level meters
    const int pluginWidth = 200;
    const int spacing = 8;
    const int yMargin = spacing;
    const int containerHeight = getHeight();
    const int deviceUIHeight = containerHeight - (2 * yMargin);
    int xPos = spacing;

    // Always add the "+" button first
    contentComponent.addAndMakeVisible(addButton);
    addButton.setBounds(xPos, yMargin, addButtonWidth, deviceUIHeight);
    addButton.setEnabled(true);

    // Position the "+" button and enable it
    xPos += addButtonWidth + spacing;

    if (auto* track = currentTrack.get()) {
        for (auto plugin : track->pluginList) {
            // Skip VolumeAndPanPlugin only (we now show LevelMeterPlugin)
            if (!shouldBeShown(plugin)) {
                continue;
            }

            // Check if plugin has a custom device UI
            if (PluginDeviceUI::hasCustomDeviceUI(plugin)) {
                auto deviceUI = PluginDeviceUI::createForPlugin(editViewState, plugin);
                if (deviceUI) {
                    deviceUI->setBounds(xPos, yMargin, deviceUI->getWidth(), deviceUIHeight);
                    contentComponent.addAndMakeVisible(deviceUI.get());
                    devices.add(deviceUI.release());
                    xPos += deviceUIWidth + spacing;
                }
            } else if (auto deviceUI = plugin->createEditor(); deviceUI != nullptr) {
                // If plugin has an editor, show it istead of a simple button
                deviceUI->setBounds(xPos, yMargin, pluginWidth, deviceUIHeight);
                contentComponent.addAndMakeVisible(deviceUI.get());
                devices.add(deviceUI.release());
                xPos += pluginWidth + spacing;
            } else {
                // Regular plugin button
                auto* c = new PluginPlaceholderComponent(editViewState, plugin);
                c->setBounds(xPos, yMargin, pluginWidth, deviceUIHeight);
                contentComponent.addAndMakeVisible(c);
                devices.add(c);
                xPos += pluginWidth + spacing;
            }
        }
    }
    // Update content size - width based on plugins + add button, height matches container
    contentComponent.setSize(juce::jmax(320, xPos), containerHeight);

    viewport.getViewedComponent()->repaint();
    repaint();
}

void TrackDevicesComponent::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);

    if (!currentTrack.get()) {
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(16.0f);
        g.drawText("Select a track to view its devices", getLocalBounds(), juce::Justification::centred, true);
    }
}

void TrackDevicesComponent::resized() {
    viewport.setBounds(getLocalBounds());
}

// ValueTree::Listener methods
void TrackDevicesComponent::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& c) {
    if (c.hasType(tracktion::IDs::PLUGIN))
        markAndUpdate(updateDevices);
}

void TrackDevicesComponent::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& c, int) {
    if (c.hasType(tracktion::IDs::PLUGIN))
        markAndUpdate(updateDevices);
}

void TrackDevicesComponent::valueTreeChildOrderChanged(juce::ValueTree&, int, int) {
    markAndUpdate(updateDevices);
}

// FlaggedAsyncUpdater method
void TrackDevicesComponent::handleAsyncUpdate() {
    if (compareAndReset(updateDevices))
        buildPlugins();
}

}  // namespace MoTool
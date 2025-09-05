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

    markAndUpdate(updatePlugins);
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

void TrackDevicesComponent::buildPlugins() {
    plugins.clear();
    deviceUIs.clear();

    contentComponent.removeAllChildren();

    // Always add the "+" button first
    contentComponent.addAndMakeVisible(addButton);

    const int addButtonWidth = 32;
    const int pluginWidth = 200;
    const int deviceUIWidth = 16;  // Width for device UIs like level meters
    const int spacing = 8;
    const int yMargin = spacing;
    const int containerHeight = getHeight();
    const int buttonHeight = containerHeight - (2 * yMargin);

    if (auto* track = currentTrack.get()) {
        int xPos = spacing;

        // Position the "+" button and enable it
        addButton.setBounds(xPos, yMargin, addButtonWidth, buttonHeight);
        addButton.setEnabled(true);
        xPos += addButtonWidth + spacing;

        // Get the plugin list for this track
        for (auto plugin : track->pluginList) {
            // Skip VolumeAndPanPlugin only (we now show LevelMeterPlugin)
            if (dynamic_cast<tracktion::VolumeAndPanPlugin*>(plugin)) {
                continue;
            }

            // Check if plugin has a custom device UI
            if (PluginDeviceUI::hasCustomDeviceUI(plugin)) {
                auto deviceUI = PluginDeviceUI::createForPlugin(editViewState, plugin);
                if (deviceUI) {
                    deviceUI->setBounds(xPos, yMargin, deviceUIWidth, buttonHeight);
                    contentComponent.addAndMakeVisible(deviceUI.get());
                    deviceUIs.add(deviceUI.release());
                    xPos += deviceUIWidth + spacing;
                }
            } else {
                // Regular plugin button
                auto* p = new PluginComponent(editViewState, plugin);
                p->setBounds(xPos, yMargin, pluginWidth, buttonHeight);
                contentComponent.addAndMakeVisible(p);
                plugins.add(p);
                xPos += pluginWidth + spacing;
            }
        }

        // Update content size - width based on plugins + add button, height matches container
        contentComponent.setSize(juce::jmax(320, xPos), containerHeight);
    } else {
        // If no track selected, still show the "+" button but disabled
        addButton.setBounds(10, yMargin, addButtonWidth, buttonHeight);
        addButton.setEnabled(false);
        contentComponent.setSize(320, containerHeight);
    }

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
        markAndUpdate(updatePlugins);
}

void TrackDevicesComponent::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& c, int) {
    if (c.hasType(tracktion::IDs::PLUGIN))
        markAndUpdate(updatePlugins);
}

void TrackDevicesComponent::valueTreeChildOrderChanged(juce::ValueTree&, int, int) {
    markAndUpdate(updatePlugins);
}

// FlaggedAsyncUpdater method
void TrackDevicesComponent::handleAsyncUpdate() {
    if (compareAndReset(updatePlugins))
        buildPlugins();
}

}  // namespace MoTool
#pragma once

#include <JuceHeader.h>
#include "PluginDeviceUI.h"
#include "PluginUIAdapterRegistry.h"
#include "DevicePanelItem.h"

#include "../../controllers/EditState.h"

#include <common/Utilities.h>  // from Tracktion, for FlaggedAsyncUpdater

namespace MoTool {


//==============================================================================
/**
 * AddButton - Button that draws only text, no background
 */
class AddButton : public BackgroundlessTextButton {
public:
    AddButton();
};

class AddButtonComponent : public DevicePanelItemBase, private Timer {
public:
    AddButtonComponent();
    void resized() override;
    void mouseEnter(const MouseEvent& event) override;

    AddButton button;

private:
    void timerCallback() override;
};

//==============================================================================
/**
 * TrackDevicesPanel - Panel that displays the list of devices (plugins)
 * on a track and, when editable, "+" buttons between them to add new plugins
 * and a menu item to remove each.
 */
class TrackDevicesPanel : public juce::Component,
                          private FlaggedAsyncUpdater,
                          private juce::ValueTree::Listener,
                          private juce::ComponentListener
{
public:
    explicit TrackDevicesPanel(EditViewState& evs, bool editable = true);
    ~TrackDevicesPanel() override;

    /** Shows the devices of `track` right away. */
    void setCurrentTrack(tracktion::Track* track);
    /** Height the devices need, padding included. */
    int getMinimumHeight() const;
    /** Space around the devices; by default 8 above and below. */
    void setPadding(juce::BorderSize<int> padding);

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // ValueTree::Listener overrides
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& c) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& c, int) override;
    void valueTreeChildOrderChanged(juce::ValueTree&, int, int) override;

    // FlaggedAsyncUpdater override
    void handleAsyncUpdate() override;

    // ComponentListener override
    void componentMovedOrResized(juce::Component& component, bool wasMoved, bool wasResized) override;

private:
    EditViewState& editViewState;
    const bool editable_;
    juce::BorderSize<int> padding_;
    tracktion::SafeSelectable<tracktion::Track> currentTrack;

    // TODO this belongs to some ViewModel class
    static bool shouldBeShown(tracktion::Plugin* p);
    void buildPlugins();
    void createAndAddNewPluginButton(int index);
    void layoutDeviceComponents();
    void addButtonClicked(int index);

    Viewport viewport;
    Component content;

    OwnedArray<AddButtonComponent> addButtons;
    OwnedArray<DeviceItem> devices;
    bool updateDevices = false;
    bool rebuildingDevices_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackDevicesPanel)
};

}  // namespace MoTool

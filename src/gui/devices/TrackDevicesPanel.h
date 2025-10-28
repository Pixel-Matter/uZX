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
 * on the currently selected track and "+" buttons between them to add new plugins.
 */
class TrackDevicesPanel : public juce::Component,
                          public juce::ChangeListener,
                          private FlaggedAsyncUpdater,
                          private juce::ValueTree::Listener,
                          private juce::ComponentListener
{
public:
    TrackDevicesPanel(EditViewState& evs);
    ~TrackDevicesPanel() override;

    void setCurrentTrack(tracktion::Track* track);

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // ChangeListener override
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

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

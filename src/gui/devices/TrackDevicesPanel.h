#pragma once

#include <JuceHeader.h>
#include "PluginDeviceUI.h"
#include "PluginUIAdapterRegistry.h"
#include "../../controllers/EditState.h"
#include <common/Utilities.h>  // from Tracktion, for FlaggedAsyncUpdater

namespace MoTool {

class DevicePanelItem {
public:
};

class TrackDevicesPanel : public juce::Component,
                          public juce::ChangeListener,
                          private FlaggedAsyncUpdater,
                          private juce::ValueTree::Listener
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

private:
    EditViewState& editViewState;
    tracktion::SafeSelectable<tracktion::Track> currentTrack;

    // TODO this belongs to some ViewModel class
    static bool shouldBeShown(tracktion::Plugin* p);
    void buildPlugins();
    int createAndAddNewPluginButton(int index, Rectangle<int>& area);
    void addButtonClicked(int index);

    Viewport viewport;
    Component content;

    OwnedArray<TextButton> addButtons;
    OwnedArray<PluginDeviceUI> devices;
    bool updateDevices = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackDevicesPanel)
};

}  // namespace MoTool
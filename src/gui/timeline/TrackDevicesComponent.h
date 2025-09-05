#pragma once

#include <JuceHeader.h>
#include "../../controllers/EditState.h"
#include "PluginComponent.h"
#include "PluginDeviceUI.h"
#include <common/Utilities.h>  // from Tracktion, for FlaggedAsyncUpdater

namespace MoTool {

class TrackDevicesComponent : public juce::Component,
                              public juce::ChangeListener,
                              private FlaggedAsyncUpdater,
                              private juce::ValueTree::Listener
{
public:
    TrackDevicesComponent(EditViewState& evs);
    ~TrackDevicesComponent() override;

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

    static bool shouldBeShown(tracktion::Plugin* p);
    void buildPlugins();

    Viewport viewport;
    Component contentComponent;

    TextButton addButton {"+"};
    OwnedArray<Component> devices;
    bool updateDevices = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackDevicesComponent)
};

}  // namespace MoTool
#include <JuceHeader.h>

#include "TrackDevicesPanel.h"

namespace MoTool::Tests {

class TrackDevicesPanelTests : public juce::UnitTest {
public:
    TrackDevicesPanelTests() : UnitTest("TrackDevicesPanel", "MoTool") {}

    void runTest() override {
        beginTest("Deleting the shown track empties the panel");
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        te::SelectionManager selection(engine);
        EditViewState view(*edit, selection);
        TrackDevicesPanel panel(view);
        panel.setSize(400, 200);

        TrackDevicesPanel empty(view);
        const int emptyHeight = empty.getMinimumHeight();

        auto* track = te::getAudioTracks(*edit)[0];
        track->pluginList.insertPlugin(edit->getPluginCache().createNewPlugin(te::ReverbPlugin::xmlTypeName, {}), 0,
                                       nullptr);
        panel.setCurrentTrack(track);
        expect(panel.getMinimumHeight() > emptyHeight, "the track shows its devices");

        edit->deleteTrack(track);
        MessageManager::getInstance()->runDispatchLoopUntil(50);
        expectEquals(panel.getMinimumHeight(), emptyHeight);
    }
};

static TrackDevicesPanelTests trackDevicesPanelTests;

}  // namespace MoTool::Tests

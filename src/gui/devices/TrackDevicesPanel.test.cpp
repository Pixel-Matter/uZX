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

        beginTest("The Player's devices have no title bar, so a device collapsed in Studio shows expanded");
        edit->ensureNumberOfAudioTracks(1);  // the test above deleted the only one
        auto* other = te::getAudioTracks(*edit)[0];
        other->pluginList.insertPlugin(edit->getPluginCache().createNewPlugin(te::ReverbPlugin::xmlTypeName, {}), 0,
                                       nullptr);
        other->pluginList[0]->state.setProperty("viewCollapsed", true, nullptr);  // as Studio saves a collapsed device
        const auto findFramed = [](TrackDevicesPanel& p) -> FramedDeviceItem* {
            std::function<FramedDeviceItem*(Component&)> find = [&](Component& c) -> FramedDeviceItem* {
                if (auto* framed = dynamic_cast<FramedDeviceItem*>(&c))
                    return framed;
                for (auto* child : c.getChildren())
                    if (auto* framed = find(*child))
                        return framed;
                return nullptr;
            };
            return find(p);
        };

        TrackDevicesPanel studio(view);
        studio.setCurrentTrack(other);
        auto* framed = findFramed(studio);
        expect(framed != nullptr && framed->isCollapsed(), "Studio: collapsed, as saved");

        TrackDevicesPanel player(view, false);
        player.setCurrentTrack(other);
        framed = findFramed(player);
        expect(framed != nullptr);
        if (framed != nullptr) {
            expect(! framed->isCollapsed());
            expect(framed->getDeviceUI()->isVisible());
            expectEquals(framed->getNumChildComponents(), 1, "only the device: no title bar");
        }
    }
};

static TrackDevicesPanelTests trackDevicesPanelTests;

}  // namespace MoTool::Tests

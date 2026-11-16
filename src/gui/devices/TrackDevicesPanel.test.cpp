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

        beginTest("The Player's level meter has no MIDI LEDs: its tracks put out no MIDI");
        // The meter's top LED, unlit: cyan where Studio shows MIDI activity, red where the level scale ends
        const auto topLedOfMeter = [](TrackDevicesPanel& p) {
            std::function<Component*(Component&)> find = [&](Component& c) -> Component* {
                if (auto* ui = dynamic_cast<PluginDeviceUI*>(&c);
                    ui != nullptr && dynamic_cast<te::LevelMeterPlugin*>(ui->getPlugin().get()) != nullptr)
                    return ui;
                for (auto* child : c.getChildren())
                    if (auto* found = find(*child))
                        return found;
                return nullptr;
            };
            auto* meter = find(p);
            if (meter == nullptr)
                return Colour();
            meter->setSize(16, 200);
            return meter->createComponentSnapshot(meter->getLocalBounds(), false, 1.0f).getPixelAt(3, 2);  // in the top LED, with or without the MIDI section
        };
        const auto studioLed = topLedOfMeter(studio), playerLed = topLedOfMeter(player);
        expect(studioLed.getBlue() > studioLed.getRed(), "Studio: MIDI LEDs on top, " + studioLed.toString());
        expect(playerLed.getRed() > playerLed.getBlue(), "Player: the level's red zone on top, " + playerLed.toString());
    }
};

static TrackDevicesPanelTests trackDevicesPanelTests;

}  // namespace MoTool::Tests

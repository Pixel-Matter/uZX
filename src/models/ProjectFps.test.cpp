#include <JuceHeader.h>

#include "EditUtilities.h"
#include "Timecode.h"
#include "Ids.h"

namespace MoTool {

namespace te = tracktion;

//==============================================================================
class ProjectFpsTests : public juce::UnitTest {
public:
    ProjectFpsTests() : juce::UnitTest("ProjectFps", "MoTool") {}

    void runTest() override {
        testParseLegacyTimecodeFormat();
        testGetProjectFpsFallback();
        testMigrateLegacyTimecodeDisplaySettings();
        testModeAndFpsRoundTrip();
        testGridSubdivisionPattern();
    }

private:
    void testParseLegacyTimecodeFormat() {
        beginTest("parseLegacyTimecodeFormat decodes legacy combined strings");

        {
            auto result = parseLegacyTimecodeFormat("barsBeatsFps50");
            expect(result.mode == TimecodeDisplayMode::barsBeatsFrames,
                   "barsBeatsFps50 should map to barsBeatsFrames");
            expect(result.fps.has_value() && *result.fps == 50.0,
                   "barsBeatsFps50 should carry fps 50");
        }
        {
            auto result = parseLegacyTimecodeFormat("fps50");
            expect(result.mode == TimecodeDisplayMode::framesOnly,
                   "fps50 should map to framesOnly");
            expect(result.fps.has_value() && *result.fps == 50.0,
                   "fps50 should carry fps 50");
        }
        {
            auto result = parseLegacyTimecodeFormat("fps25");
            expect(result.mode == TimecodeDisplayMode::framesOnly,
                   "fps25 should map to framesOnly");
            expect(result.fps.has_value() && *result.fps == 25.0,
                   "fps25 should carry fps 25");
        }
        {
            auto result = parseLegacyTimecodeFormat("beats");
            expect(result.mode == TimecodeDisplayMode::barsBeats,
                   "beats should map to barsBeats");
            expect(! result.fps.has_value(), "beats should carry no fps");
        }
        {
            auto result = parseLegacyTimecodeFormat("seconds");
            expect(result.mode == TimecodeDisplayMode::seconds,
                   "seconds should map to seconds");
            expect(! result.fps.has_value(), "seconds should carry no fps");
        }
        {
            auto result = parseLegacyTimecodeFormat("");
            expect(result.mode == TimecodeDisplayMode::barsBeatsFrames,
                   "empty string should fall back to barsBeatsFrames");
            expect(! result.fps.has_value(), "empty string should carry no fps");
        }
        {
            auto result = parseLegacyTimecodeFormat("garbage");
            expect(result.mode == TimecodeDisplayMode::barsBeatsFrames,
                   "unknown string should fall back to barsBeatsFrames");
            expect(! result.fps.has_value(), "unknown string should carry no fps");
        }
    }

    void testGetProjectFpsFallback() {
        beginTest("getProjectFps falls back to timecode fps when property absent");

        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);

        // Set the legacy property directly — no IDs::projectFps yet.
        edit->state.setProperty(te::IDs::timecodeFormat, "barsBeatsFps30", nullptr);
        expect(! edit->state.hasProperty(IDs::projectFps), "projectFps must not be set yet");
        expectEquals(Helpers::getProjectFps(*edit), 30.0, "fallback to fps encoded in legacy property");
        expect(Helpers::getTimecodeDisplayMode(*edit) == TimecodeDisplayMode::barsBeatsFrames,
               "legacy barsBeatsFps30 should decode to barsBeatsFrames display mode");
    }

    void testModeAndFpsRoundTrip() {
        beginTest("setTimecodeDisplayMode persists mode and syncs legacy te property");

        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);

        Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::framesOnly);
        expect(Helpers::getTimecodeDisplayMode(*edit) == TimecodeDisplayMode::framesOnly,
               "framesOnly mode should round-trip");
        expectEquals(edit->state[te::IDs::timecodeFormat].toString(),
                     juce::String("seconds"),
                     "framesOnly should write te timecodeFormat as 'seconds'");

        Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::barsBeatsFrames);
        expect(Helpers::getTimecodeDisplayMode(*edit) == TimecodeDisplayMode::barsBeatsFrames,
               "barsBeatsFrames mode should round-trip");
        expectEquals(edit->state[te::IDs::timecodeFormat].toString(),
                     juce::String("beats"),
                     "barsBeatsFrames should write te timecodeFormat as 'beats'");

        Helpers::setProjectFps(*edit, 100.0);
        expectEquals(Helpers::getProjectFps(*edit), 100.0, "project fps should be stored");
        expect(Helpers::getTimecodeDisplayMode(*edit) == TimecodeDisplayMode::barsBeatsFrames,
               "setProjectFps must not change the display mode");
    }

    void testMigrateLegacyTimecodeDisplaySettings() {
        beginTest("migrateTimecodeDisplaySettings preserves legacy fps before syncing timecodeFormat");

        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);

        edit->state.setProperty(te::IDs::timecodeFormat, "barsBeatsFps30", nullptr);
        expect(! edit->state.hasProperty(IDs::timecodeDisplayMode), "mode should start unset");
        expect(! edit->state.hasProperty(IDs::projectFps), "fps should start unset");

        Helpers::migrateTimecodeDisplaySettings(*edit);

        expect(Helpers::getTimecodeDisplayMode(*edit) == TimecodeDisplayMode::barsBeatsFrames,
               "legacy mode should migrate to barsBeatsFrames");
        expectEquals(Helpers::getProjectFps(*edit), 30.0, "legacy fps should be stored before the old property is synced");
        expectEquals(edit->state[te::IDs::timecodeFormat].toString(),
                     juce::String("beats"),
                     "legacy te timecodeFormat should be rewritten only after migration");
    }

    void testGridSubdivisionPattern() {
        beginTest("setGridSubdivisionPattern stores and retrieves subdivision patterns");

        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);

        Helpers::setGridSubdivisionPattern(*edit, {6, 7, 6, 7});
        auto result = Helpers::getGridSubdivisionPattern(*edit);
        expect(result == std::vector<int>({6, 7, 6, 7}),
               "{6,7,6,7} pattern should round-trip");

        // Fewer than 2 entries should be treated as empty / remove the property.
        Helpers::setGridSubdivisionPattern(*edit, {5});
        auto cleared = Helpers::getGridSubdivisionPattern(*edit);
        expect(cleared.empty(), "single-entry pattern should be stored as empty");
        expect(! edit->state.hasProperty(IDs::gridSubdivision),
               "single-entry pattern should remove the property");
    }
};

static ProjectFpsTests projectFpsTests;

}  // namespace MoTool

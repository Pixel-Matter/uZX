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
        testMakeTimecodeTypeRoundTrip();
        testGetProjectFpsFallback();
        testSetProjectFpsLockstep();
    }

private:
    void testMakeTimecodeTypeRoundTrip() {
        beginTest("makeTimecodeType round-trips fps and display mode");

        for (double fps : Helpers::kAllowedProjectFps) {
            for (auto mode : { TimecodeDisplayMode::barsBeatsFrames,
                               TimecodeDisplayMode::framesOnly }) {
                TimecodeDisplayFormatExt fmt { TimecodeDisplayFormatExt::makeTimecodeType(mode, fps) };
                expectEquals((double) fmt.getFPS(), fps,
                             "fps preserved for mode " + juce::String((int) mode));
                expect(fmt.getDisplayMode() == mode,
                       "display mode preserved for fps " + juce::String(fps));
            }
        }

        // Modes that carry no fps ignore the rate but keep their layout.
        TimecodeDisplayFormatExt seconds { TimecodeDisplayFormatExt::makeTimecodeType(TimecodeDisplayMode::seconds, 60) };
        expect(seconds.getDisplayMode() == TimecodeDisplayMode::seconds, "seconds mode");
        TimecodeDisplayFormatExt bb { TimecodeDisplayFormatExt::makeTimecodeType(TimecodeDisplayMode::barsBeats, 60) };
        expect(bb.getDisplayMode() == TimecodeDisplayMode::barsBeats, "barsBeats mode");
    }

    void testGetProjectFpsFallback() {
        beginTest("getProjectFps falls back to timecode fps when property absent");

        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);

        // No projectFps property yet: fall back to the timecode format's fps.
        Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps30);
        expect(! edit->state.hasProperty(IDs::projectFps), "projectFps not yet set");
        expectEquals(Helpers::getProjectFps(*edit), 30.0, "fallback to timecode fps");
    }

    void testSetProjectFpsLockstep() {
        beginTest("setProjectFps updates fps and keeps timecode display mode");

        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);

        Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);
        Helpers::setProjectFps(*edit, 100.0);

        expectEquals(Helpers::getProjectFps(*edit), 100.0, "project fps stored");
        auto fmt = Helpers::getEditTimecodeFormat(*edit);
        expectEquals((double) fmt.getFPS(), 100.0, "timecode fps followed");
        expect(fmt.getDisplayMode() == TimecodeDisplayMode::barsBeatsFrames,
               "display mode preserved through fps change");
    }
};

static ProjectFpsTests projectFpsTests;

}  // namespace MoTool

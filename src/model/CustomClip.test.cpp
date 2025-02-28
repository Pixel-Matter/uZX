#include <JuceHeader.h>

#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>
#include <model/PsgClip.h>


namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion;
using namespace juce;
using namespace std::literals;

//==============================================================================

// Helper method to create and add a PsgClip to a track
static PsgClip::Ptr createCustomClip(ClipOwner& track, TimeRange position) {
    auto& edit = track.getClipOwnerEdit();

    ValueTree state(MoTool::IDs::PSGCLIP);
    state.setProperty(tracktion::IDs::start, position.getStart().inSeconds(), nullptr);
    state.setProperty(tracktion::IDs::length, position.getLength().inSeconds(), nullptr);
    state.setProperty(tracktion::IDs::id, edit.createNewItemID().toString(), nullptr);
    state.setProperty("isCustomClip", true, nullptr);

    // Insert the clip using the standard method
    auto* clip = dynamic_cast<PsgClip*>(insertClipWithState(track, state));
    return clip;
}


//==============================================================================
//==============================================================================
class CustomClipTests  : public UnitTest {

public:
    CustomClipTests() : UnitTest("CustomClip", "MoTool") {}

    void runTest() override {
        auto& engine = *Engine::getEngines()[0];
        Clipboard clipboard;
        auto edit = Edit::createSingleTrackEdit(engine);

        beginTest("MIDI Clip sequence");
        {
            const TimeDuration duration = 8.0s;
            auto t = getAudioTracks(*edit)[0];
            auto c = createCustomClip(*t, {0.0s, duration});
            expect(c != nullptr, "CustomClip created");
            expect(dynamic_cast<PsgClip*>(c.get()) != nullptr, "CustomClip is a PsgClip");
            expect(dynamic_cast<MidiClip*>(c.get()) != nullptr, "CustomClip is a MidiClip");
        }
    }
};

// Register the test
static CustomClipTests customClipTests;

}  // namespace MoTool::Tests
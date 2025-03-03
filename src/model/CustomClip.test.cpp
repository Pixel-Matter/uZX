#include <JuceHeader.h>

#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>
#include <model/PsgClip.h>


namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace tracktion;
using namespace juce;
using namespace std::literals;

//==============================================================================
//==============================================================================
class CustomClipTests  : public UnitTest {

public:
    CustomClipTests() : UnitTest("CustomClip", "MoTool") {}

    void runTest() override {
        auto& engine = *Engine::getEngines()[0];
        Clipboard clipboard;
        auto edit = Edit::createSingleTrackEdit(engine);

        beginTest("CustomClip creation");
        {
            auto t = getAudioTracks(*edit)[0];
            auto* c = CustomClip::insertClipWithState(*t, {}, {}, CustomClip::Type::psg, {{0_tp, 4_td}, {}},
                      te::DeleteExistingClips::no, false);

            expect(c != nullptr, "CustomClip created");
            auto* psgClip = dynamic_cast<PsgClip*>(c);
            expect(psgClip != nullptr, "CustomClip is a PsgClip");

            // add one note for testing
            auto& seq = psgClip->getSequence();
            seq.addNote(36, 0_bp, 4_bd, 127, 0, &edit->getUndoManager());
        }

        beginTest("CustomClip loading from edit");
        {
            auto t = getAudioTracks(*edit)[0];
            auto& clips = t->getClips();

            expect(clips.size() == 1, "Clip added to track");

            auto* c = dynamic_cast<PsgClip*>(clips.getFirst());
            expect(c != nullptr, "CustomClip is a PsgClip");
        }

        beginTest("Edit loading by copying the state");
        {
            auto editStateCopy = edit->state.createCopy();
            Edit editCopy ({ engine, editStateCopy, ProjectItemID::createNewID(0) });
            jassert(getAudioTracks(editCopy).size() == 1);
            auto t = getAudioTracks(editCopy)[0];
            auto& clips = t->getClips();
            jassert(clips.size() == 1);
        }
    }
};

static CustomClipTests customClipTests;

}  // namespace MoTool::Tests
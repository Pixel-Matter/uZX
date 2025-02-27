#include "tracktion_core/utilities/tracktion_Time.h"
#include "tracktion_engine/tracktion_engine.h"
#include <JuceHeader.h>

#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>


namespace MoTool::Tests {

namespace te = tracktion;
using namespace std::literals;

//==============================================================================
class PsgClip : public te::Clip {
public:
    using Ptr = juce::ReferenceCountedObjectPtr<Clip>;

    PsgClip(const juce::ValueTree& v, te::EditItemID id, te::ClipOwner& parent_)
        : te::Clip(v, parent_, id, te::TrackItem::Type::midi)
    {
        // Set a custom property to identify our clip type
        state.setProperty("isPsgClip", true, nullptr);
    }

    ~PsgClip() override = default;

    juce::String getSelectableDescription() override {
        return "PSG Clip";
    }

    [[ nodiscard ]] virtual bool isMidi() const override {
        return true;
    }

    [[ nodiscard ]] virtual bool isPsg() const {
        return true;
    }

    // te::HashCode getHash() const override {
    //     // Return a dummy hash - for testing only
    //     return 12345;
    // }

    // te::TimeDuration getSourceLength() const override {
    //     // Return a dummy source length - for testing only
    //     return te::TimeDuration::fromSeconds(2.0);
    // }

    // void setLoopDefaults() override {
    //     // Do nothing for tests
    // }

    // Helper method to create and add a PsgClip to a track
    static Ptr create(te::ClipOwner& track, te::TimeRange position) {
        auto& edit = track.getClipOwnerEdit();
        auto newID = edit.createNewItemID();

        // Create a ValueTree with a recognized type but our custom properties
        juce::ValueTree state(te::IDs::AUDIOCLIP);
        state.setProperty(te::IDs::start, position.getStart().inSeconds(), nullptr);
        state.setProperty(te::IDs::length, position.getLength().inSeconds(), nullptr);
        state.setProperty(te::IDs::id, newID.toString(), nullptr);
        state.setProperty("isPsgClip", true, nullptr);

        DBG("State is " << state.toXmlString());

        // Insert the clip using the standard method
        auto* clip = dynamic_cast<Clip*>(insertClipWithState(track, state));

        // If insertion failed (the engine created a standard AudioClip instead of our PsgClip)
        // we need to implement our own handling
        // if (!clip)
        // {
        //     // Remove the standard clip that was created
        //     if (auto standardClip = te::findClipForState(track, state))
        //         standardClip->removeFromParent();

        //     // Create and add our custom clip
        //     auto newClip = new PsgClip(state, newID, track);
        //     if (auto* clipTrack = dynamic_cast<te::ClipTrack*>(&track))
        //         clipTrack->addClip(newClip);

        //     clip = newClip;
        // }

        return clip;
    }

    // Helper method to check if a clip is a PsgClip
    static bool isPsgClip(const te::Clip* clip) {
        return clip && clip->state.getProperty("isPsgClip", false);
    }
};


//==============================================================================
//==============================================================================
class CustomClipTests  : public juce::UnitTest {

    class TrackListener : public juce::ValueTree::Listener {
        void valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) override {
            if (parent.hasType(te::IDs::TRACK) && child.hasType(te::IDs::AUDIOCLIP)) {
                DBG("valueTreeChildAdded: " << child.getType().toString() << " to " << parent.getType().toString());
            }
        }
    };

public:
    CustomClipTests() : juce::UnitTest("CustomClip", "MoTool") {}

    void runTest() override {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        te::Clipboard clipboard;
        auto edit = te::Edit::createSingleTrackEdit(engine);
        juce::Random r(42);

        beginTest("MIDI Clip sequence");
        {
            const te::TimeDuration duration = 8.0s;
            auto t = getAudioTracks(*edit)[0];
            TrackListener listener;
            t->state.addListener(&listener);

            auto c = PsgClip::create(*t, {0.0s, duration});
            expect(c != nullptr, "PsgClip created");

            t->state.removeListener(&listener);
        }
    }
};

// Register the test
static CustomClipTests customClipTests;

}  // namespace MoTool::Tests
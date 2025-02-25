#include <JuceHeader.h>

namespace MoTool {

// #if TRACKTION_UNIT_TESTS

#include <JuceHeader.h>
#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

//==============================================================================
class PsgClip : public te::AudioClipBase {
public:
    using Ptr = juce::ReferenceCountedObjectPtr<PsgClip>;

    PsgClip(const juce::ValueTree& v, te::EditItemID id, te::ClipOwner& parent_)
        : te::AudioClipBase(v, id, te::TrackItem::Type::wave, parent_)
    {
        // Set a custom property to identify our clip type
        state.setProperty("isPsgClip", true, nullptr);
    }

    ~PsgClip() override = default;

    juce::String getSelectableDescription() override {
        return "PSG Clip";
    }

    [[ nodiscard ]] virtual bool isMidi() const override {
        return false;
    }

    [[ nodiscard ]] virtual bool isPsg() const {
        return true;
    }

    // Implementation of pure virtual methods from AudioClipBase
    juce::File getOriginalFile() const override {
        // Return a dummy file - for testing only
        return juce::File::getCurrentWorkingDirectory().getChildFile("dummy.wav");
    }

    te::HashCode getHash() const override {
        // Return a dummy hash - for testing only
        return 12345;
    }

    te::TimeDuration getSourceLength() const override {
        // Return a dummy source length - for testing only
        return te::TimeDuration::fromSeconds(2.0);
    }

    void setLoopDefaults() override {
        // Do nothing for tests
    }

    // Helper method to create and add a PsgClip to a track
    static Ptr create(te::ClipOwner& track, te::TimeRange position) {
        auto& edit = track.getClipOwnerEdit();

        // Create a ValueTree with a recognized type but our custom properties
        juce::ValueTree state(te::IDs::AUDIOCLIP);
        state.setProperty(te::IDs::start, position.getStart().inSeconds(), nullptr);
        state.setProperty(te::IDs::length, position.getLength().inSeconds(), nullptr);
        state.setProperty("isPsgClip", true, nullptr);

        // Create a unique ID for this clip
        te::EditItemID newID;
        if (! newID.isValid())
            newID = edit.createNewItemID();

        state.setProperty(te::IDs::id, newID.toString(), nullptr);

        // Insert the clip using the standard method
        auto* clip = dynamic_cast<PsgClip*>(insertClipWithState(track, state));

        // If insertion failed (the engine created a standard AudioClip instead of our PsgClip)
        // we need to implement our own handling
        if (!clip)
        {
            // Remove the standard clip that was created
            if (auto standardClip = te::findClipForState(track, state))
                standardClip->removeFromParent();

            // Create and add our custom clip
            auto newClip = new PsgClip(state, newID, track);
            if (auto* clipTrack = dynamic_cast<te::ClipTrack*>(&track))
                clipTrack->addClip(newClip);

            clip = newClip;
        }

        return clip;
    }

    // Helper method to check if a clip is a PsgClip
    static bool isPsgClip(const te::Clip* clip)
    {
        return clip && clip->state.getProperty("isPsgClip", false);
    }
};

//==============================================================================
class CustomClipTests : public juce::UnitTest {
public:
    CustomClipTests() : UnitTest("Custom Clip Tests") {}

    void runTest() override
    {
        beginTest("Custom PsgClip Test");

        // // Create an engine
        // te::Engine engine("UnitTestEngine");

        // // Create an empty edit for testing
        // te::Edit edit(engine, te::createEmptyEdit(), te::Edit::forEditing);

        // // Create an audio track
        // auto track = te::getOrInsertAudioTrackAt(edit, 0);
        // expect(track != nullptr, "Failed to create audio track");

        // // Create a PSG clip
        // using namespace std::chrono_literals;
        // auto position = te::TimeRange(0.0s, 2.0s);
        // auto psgClip = PsgClip::create(*track, position);

        // // Verify the clip was created and added to the track
        // expect(psgClip != nullptr, "Failed to create PSG clip");
        // expect(track->getClips().size() == 1, "Clip not added to track");

        // // Verify we can retrieve the clip from the track
        // auto retrievedClip = track->getClips()[0];
        // expect(retrievedClip != nullptr, "Failed to retrieve clip from track");

        // // Check if it's recognized as a PsgClip
        // expect(PsgClip::isPsgClip(retrievedClip), "Clip not recognized as PSG clip");

        // // Test dynamic casting
        // auto castClip = dynamic_cast<PsgClip*>(retrievedClip);
        // expect(castClip != nullptr, "Failed to dynamic_cast to PsgClip");

        // // Test clip properties
        // expect(retrievedClip->getPosition().time.getStart() == te::TimePosition::fromSeconds(0.0),
        //        "Incorrect clip start position");
        // expect(retrievedClip->getPosition().time.getLength() == te::TimeDuration::fromSeconds(2.0),
        //        "Incorrect clip length");

        // // Cleanup
        // engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
    }
};

// Register the test
static CustomClipTests customClipTests;

// #endif // TRACKTION_UNIT_TESTS

}  // namespace MoTool
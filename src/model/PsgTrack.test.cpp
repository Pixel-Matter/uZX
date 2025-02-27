#include <JuceHeader.h>
#include "PsgTrack.h"
// #include "tracktion_engine/tracktion_engine.h"

namespace MoTool {

//==============================================================================
//==============================================================================
class PsgTrackTests : public juce::UnitTest
{
public:
    PsgTrackTests()
        : juce::UnitTest("PsgTrack", "MoTool")
    {}

    void runTest() override {
        // testDirectInjection();
        testWorkaroundApproach();
    }

private:
    void testDirectInjection() {
        beginTest("Testing direct ValueTree injection of PSGTRACK");

        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);

        // Set up a listener to verify track addition at the ValueTree level
        struct TrackListener : public juce::ValueTree::Listener {
            void valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) override {
                if (parent.hasType(te::IDs::EDIT) && te::TrackList::isTrack(child.getType())) {
                    trackAdded = true;
                    trackType = child.getType();
                    addedNodeXml = child.toXmlString();
                }
            }

            bool trackAdded = false;
            juce::Identifier trackType;
            juce::String addedNodeXml;

            // JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR8(TrackListener)
        };

        TrackListener listener;
        edit->state.addListener(&listener);

        logMessage("Step 1: Create and add a PSGTRACK ValueTree node directly to edit");

        // Create a PSG track state
        juce::ValueTree psgTrackState(IDs::PSGTRACK);
        auto newClipID = te::EditItemID::readOrCreateNewID(*edit, psgTrackState);
        psgTrackState.setProperty(te::IDs::id, newClipID.toString(), nullptr);

        // Add the track directly to the edit state
        edit->getTrackList().createNewObject(psgTrackState);

        // Process any pending track operations
        // edit->triggerAsyncUpdate();

        // Verify listener was triggered
        expect(listener.trackAdded, "ValueTree listener should detect track addition");
        expect(listener.trackType == IDs::PSGTRACK, "Added track should be PSGTRACK type");

        logMessage("ValueTree node added successfully: " + listener.addedNodeXml);

        // Now check if an actual PsgTrack object was created
        logMessage("Step 2: Verify if a PsgTrack object was created from the ValueTree");

        bool foundPsgTrack = false;
        int trackCount = 0;

        edit->visitAllTracksRecursive([&](te::Track& track) {
            trackCount++;
            if (track.state.hasType(IDs::PSGTRACK)) {
                foundPsgTrack = (dynamic_cast<PsgTrack*>(&track) != nullptr);
                logMessage("Found track with PSGTRACK state, actual class type: " +
                           juce::String(typeid(track).name()));
            }
            return true;
        });

        logMessage("Total tracks found: " + juce::String(trackCount));
        expect(foundPsgTrack == false, "No PsgTrack object should be created without custom factory");

        // Clean up
        edit->state.removeListener(&listener);

        logMessage("CONCLUSION: The ValueTree node is added successfully to the edit tree,");
        logMessage("but Edit::createTrack doesn't know how to create a PsgTrack object from it.");
        logMessage("This explains why the direct approach fails in MidiTimeline::addNewPsgTrack()");
    }

    void testWorkaroundApproach() {
        beginTest("Testing workaround approach for creating PsgTrack");
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);

        logMessage("Step 1: Demonstrate that insertNewTrack with PSGTRACK fails");

        // First prove that the direct approach fails
        te::SelectionManager selectionManager(engine);
        auto insertPoint = te::TrackInsertPoint(te::TrackInsertPoint::getEndOfTracks(*edit));
        auto directAttempt = edit->insertNewTrack(insertPoint, IDs::PSGTRACK, &selectionManager);

        expect(directAttempt == nullptr, "Direct insertNewTrack with PSGTRACK should fail without a custom factory");

        logMessage("Step 2: Create a standard AudioTrack as a template");
        auto audioTrack = edit->insertNewAudioTrack(insertPoint, nullptr);
        expect(audioTrack != nullptr, "Should be able to create a standard AudioTrack");

        logMessage("Step 3: Copy AudioTrack state");
        juce::ValueTree psgState = audioTrack->state.createCopy();
        psgState.setProperty(te::IDs::id, edit->createNewItemID().toString(), nullptr);

        // Remember the position for later insertion
        auto originalPosition = audioTrack->getIndexInEditTrackList();
        logMessage("Track position in list: " + juce::String(originalPosition));

        logMessage("Step 4: Delete the original AudioTrack");
        edit->deleteTrack(audioTrack.get());

        logMessage("Step 5: Manually create a PsgTrack instance with our modified state");
        auto psgTrack = new PsgTrack(*edit, psgState);
        psgTrack->initialise();

        logMessage("Step 6: Add the PSG track state to the edit at the same position");
        // edit->getTrackList().createNewObject(psgState, originalPosition, &edit->getUndoManager());

        delete psgTrack;
    //     // Process pending operations to ensure track list is updated
    //     edit->updateTrackStatuses();

    //     // Now verify the track was created properly
    //     logMessage("Step 7: Verify the PsgTrack was created correctly");

    //     bool properPsgTrackFound = false;
    //     te::Track* foundTrackPtr = nullptr;

    //     edit->visitAllTracksRecursive([&](te::Track& track) {
    //         if (track.state.hasType(IDs::PSGTRACK))
    //         {
    //             auto asPsgTrack = dynamic_cast<PsgTrack*>(&track);
    //             properPsgTrackFound = (asPsgTrack != nullptr);
    //             foundTrackPtr = &track;
    //             logMessage("Found a track with PSGTRACK state, class type: " +
    //                        juce::String(typeid(track).name()));
    //             return false; // Stop searching
    //         }
    //         return true; // Continue searching
    //     });

    //     expect(properPsgTrackFound, "Should find a proper PsgTrack instance in the edit");

    //     if (foundTrackPtr != nullptr)
    //     {
    //         logMessage("Verifying track properties");
    //         expectEquals(foundTrackPtr->getSelectableDescription(), juce::String("PSG Track"));
    //     }

    //     logMessage("CONCLUSION: The workaround approach successfully creates a PsgTrack");
    //     logMessage("In MidiTimeline::addNewPsgTrack(), we should:");
    //     logMessage("1. Create a standard AudioTrack");
    //     logMessage("2. Copy its state and change the type to PSGTRACK");
    //     logMessage("3. Delete the original track");
    //     logMessage("4. Create a PsgTrack with the modified state");
    //     logMessage("5. Add the PsgTrack state back to the edit's track list");
    }
};

static PsgTrackTests psgTrackTests;

}  // namespace MoTool
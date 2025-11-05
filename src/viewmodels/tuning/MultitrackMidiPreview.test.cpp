#include "MultitrackMidiPreview.h"
#include "../../models/PsgMidi.h"
#include <JuceHeader.h>

namespace MoTool {

class MultitrackMidiPreviewTest : public juce::UnitTest {
public:
    MultitrackMidiPreviewTest() : UnitTest("MultitrackMidiPreview", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("Single note with tone enabled produces correct sequence");
        {
            te::Edit edit(engine, te::Edit::EditRole::forEditing);
            MultitrackMidiPreview preview(edit);
            size_t ch = 1;

            // Play a single note with tone enabled
            preview.playSingleNote(60, 0.5, true, false, 0, 0);

            // Inspect the sequence on track 0
            auto& clips = preview.getChannelClips();
            auto& sequence = clips[ch]->getSequence();

            // Should have 1 CC message (tone switch) and 1 note
            const auto& notes = sequence.getNotes();
            const auto& controllers = sequence.getControllerEvents();

            expectEquals(static_cast<int>(controllers.size()), 1, "Should have 1 CC message for tone switch");
            expectEquals(static_cast<int>(notes.size()), 1, "Should have 1 note");

            if (controllers.size() > 0) {
                auto ccEvent = controllers[0];
                expectEquals(ccEvent->getType(), static_cast<int>(MidiCCType::GPB1ToneSwitch));
                expectEquals(ccEvent->getControllerValue(), 127 << 7);
            }

            if (notes.size() > 0) {
                auto noteEvent = notes[0];
                expectEquals(noteEvent->getNoteNumber(), 60);
            }
        }

        beginTest("Single note with envelope enabled produces correct sequence");
        {
            te::Edit edit(engine, te::Edit::EditRole::forEditing);
            MultitrackMidiPreview preview(edit);
            size_t ch = 1;

            // Play a single note with envelope enabled
            preview.playSingleNote(60, 0.5, true, true, 5, 12);

            // Inspect the sequence on track 0 (main note)
            auto& clips = preview.getChannelClips();
            auto& sequence0 = clips[ch]->getSequence();

            const auto& notes0 = sequence0.getNotes();
            const auto& controllers0 = sequence0.getControllerEvents();

            bool foundToneSwitch = false;
            bool foundEnvSwitchOn = false;

            for (auto ccEvent : controllers0) {
                if (ccEvent->getType() == static_cast<int>(MidiCCType::GPB1ToneSwitch)) {
                    foundToneSwitch = true;
                }
                if (ccEvent->getType() == static_cast<int>(MidiCCType::GPB3EnvSwitch)) {
                    // Look specifically for the ON message (127 << 7), ignore OFF message (0)
                    if (ccEvent->getControllerValue() == 127 << 7) {
                        foundEnvSwitchOn = true;
                    }
                }
            }

            expect(foundToneSwitch, "Should have tone switch CC");
            expect(foundEnvSwitchOn, "Should have envelope switch ON CC");
            expectEquals(static_cast<int>(notes0.size()), 1, "Should have 1 note on track 0");

            // Inspect envelope track (track 3)
            auto& sequence3 = clips[3]->getSequence();
            const auto& notes3 = sequence3.getNotes();

            expectEquals(static_cast<int>(notes3.size()), 1, "Should have 1 envelope note on track 3");

            if (notes3.size() > 0) {
                auto noteEvent = notes3[0];
                expectEquals(noteEvent->getNoteNumber(), 60 + 12, "Envelope note should be offset by modulation semitones");
            }
        }

        beginTest("Envelope shape CC is added to envelope track");
        {
            te::Edit edit(engine, te::Edit::EditRole::forEditing);
            MultitrackMidiPreview preview(edit);

            // Test envelope shape by playing a single note with envelope enabled
            preview.playSingleNote(60, 0.5, false, true, 7, 0);

            auto& clips = preview.getChannelClips();
            auto& sequence = clips[3]->getSequence(); // Check envelope channel
            const auto& controllers = sequence.getControllerEvents();

            bool foundShapeCC = false;
            for (auto ccEvent : controllers) {
                if (ccEvent->getType() == static_cast<int>(MidiCCType::SoundVariation)) {
                    foundShapeCC = true;
                    expectEquals(ccEvent->getControllerValue(), 7 << 7, "Envelope shape should be 7 << 7");
                }
            }

            expect(foundShapeCC, "Should have envelope shape CC on track 3");
        }

        beginTest("Chord playback distributes notes across tracks");
        {
            te::Edit edit(engine, te::Edit::EditRole::forEditing);
            MultitrackMidiPreview preview(edit);

            // Play a 3-note chord
            std::vector<int> chordNotes = {60, 64, 67};
            preview.playChord(chordNotes, 0.5, true, false, 0, 0);

            // Check that each track gets one note
            auto& clips = preview.getChannelClips();
            for (size_t trackIndex = 0; trackIndex < 3; ++trackIndex) {
                auto& sequence = clips[trackIndex]->getSequence();
                const auto& notes = sequence.getNotes();

                int expectedNote = chordNotes[trackIndex];

                expectEquals(static_cast<int>(notes.size()), 1, "Each track should have exactly 1 note");

                if (notes.size() > 0) {
                    auto noteEvent = notes[0];
                    expectEquals(noteEvent->getNoteNumber(), expectedNote,
                               "Track " + juce::String(static_cast<int>(trackIndex)) + " should have note " + juce::String(expectedNote));
                }
            }
        }

        beginTest("Arpeggio creates sequential notes with synchronized CCs");
        {
            te::Edit edit(engine, te::Edit::EditRole::forEditing);
            MultitrackMidiPreview preview(edit);
            size_t ch = 1;

            // Play an arpeggio
            std::vector<int> arpeggioNotes = {60, 64, 67, 72};
            preview.playArpeggio(arpeggioNotes, 0.25, true, false, 0, 0);

            // Check that track 0 has all notes at different times
            auto& clips = preview.getChannelClips();
            auto& sequence = clips[ch]->getSequence();
            const auto& notes = sequence.getNotes();
            const auto& controllers = sequence.getControllerEvents();

            std::vector<std::pair<double, int>> noteTimings;
            std::vector<double> ccTimings;

            for (auto noteEvent : notes) {
                double beatTime = noteEvent->getBeatPosition().inBeats();
                int noteNumber = noteEvent->getNoteNumber();
                noteTimings.push_back({beatTime, noteNumber});
            }

            for (auto ccEvent : controllers) {
                double beatTime = ccEvent->getBeatPosition().inBeats();
                ccTimings.push_back(beatTime);
            }

            expectEquals(static_cast<int>(noteTimings.size()), 4, "Should have 4 notes in arpeggio");
            expectEquals(static_cast<int>(ccTimings.size()), 4, "Should have 4 CC messages synchronized with notes");

            // Verify notes and CCs are in correct order and timing
            for (size_t i = 0; i < noteTimings.size(); ++i) {
                expectEquals(noteTimings[i].second, arpeggioNotes[i],
                           "Note " + juce::String(static_cast<int>(i)) + " should be correct pitch");
                expectWithinAbsoluteError(noteTimings[i].first, static_cast<double>(i) * 0.25, 0.001,
                                        "Note " + juce::String(static_cast<int>(i)) + " should be at correct time");

                // Verify CC timing matches note timing
                if (i < ccTimings.size()) {
                    expectWithinAbsoluteError(ccTimings[i], noteTimings[i].first, 0.001,
                                            "CC " + juce::String(static_cast<int>(i)) + " should be synchronized with note");
                }
            }
        }

        beginTest("Playback MIDI sequence contains CC messages");
        {
            te::Edit edit(engine, te::Edit::EditRole::forEditing);
            MultitrackMidiPreview preview(edit);
            size_t ch = 1;

            // Play a single note with tone enabled
            preview.playSingleNote(60, 0.5, true, false, 0, 0);

            auto& clips = preview.getChannelClips();
            auto& midiClip = clips[ch];

            // Get the actual playback MIDI sequence
            auto playbackSequence = tracktion::MidiList::createDefaultPlaybackMidiSequence(
                midiClip->getSequence(),
                *midiClip,
                tracktion::MidiList::TimeBase::beats,
                false   // generateMPE
            );

            bool foundToneCC = false;
            int ccCount = 0;
            int noteCount = 0;

            // Iterate through all MIDI messages in the playback sequence
            for (auto* eventHolder : playbackSequence) {
                auto message = eventHolder->message;

                if (message.isController()) {
                    ccCount++;
                    DBG("Playback CC: Controller " << message.getControllerNumber() << " = " << message.getControllerValue()
                        << " on channel " << message.getChannel());

                    if (message.getControllerNumber() == static_cast<int>(MidiCCType::GPB1ToneSwitch)) {
                        foundToneCC = true;
                        expectEquals(message.getControllerValue(), 127, "Tone switch should be 127 in playback sequence");
                        expectEquals(message.getChannel(), (int) ch + 1, "Tone switch should be on channel " + String((int)ch + 1));
                    }
                } else if (message.isNoteOn()) {
                    noteCount++;
                    DBG("Playback Note: " << message.getNoteNumber() << " on channel " << message.getChannel());
                }
            }

            expect(foundToneCC, "Should have tone switch CC in playback sequence");
            expectEquals(ccCount, 1, "Should have 1 CC message in playback sequence");
            expectEquals(noteCount, 1, "Should have 1 note in playback sequence");
        }
    }
};

static MultitrackMidiPreviewTest multitrackMidiPreviewTest;

}
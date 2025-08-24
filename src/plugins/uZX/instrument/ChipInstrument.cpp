// #include "ChipInstrument.h"
// #include "tracktion_core/utilities/tracktion_Time.h"
// #include "tracktion_engine/tracktion_engine.h"

// namespace MoTool::uZX {

// namespace te = tracktion;

// ChipInstrument::ChipInstrument(te::Edit& e)
//     : MPEEffect<ChipInstrumentVoice, ChipInstrument>()
//     , edit(e)
// {
//     mpeInstrument.enableLegacyMode();
//     mpeInstrument.setPitchbendTrackingMode(juce::MPEInstrument::allNotesOnChannel);
//     // voiceManager.setVoiceStealingEnabled(true);
// }

// ChipInstrument::~ChipInstrument() = default;

// void ChipInstrument::reset() {
//     currentTempo = 120.0f;  // Reset to default tempo
//     mpeInstrument.releaseAllNotes();
//     // voiceManager.reset();
//     // TODO see 4OSC code
// }

// CriticalSection& ChipInstrument::getVoiceLock() {
//     return voiceLock;
// }

// double ChipInstrument::getTailLength() const {
//     // TODO
//     return 0.0;
// }

// void ChipInstrument::setCurrentTempo(float newTempo) {
//     currentTempo = newTempo;
// }

// void ChipInstrument::setPlayRate(double newRate) {
//     playRate = newRate;
// }

// void ChipInstrument::restoreStateFromValueTree(const ValueTree& state) {
//     // TODO: Implement state restoration from ValueTree
// }

// void ChipInstrument::renderNextBlock(te::MidiMessageArray& midiBuffer, double time, double len, double editPos) {

//     // EVERYTHING IS WRONG !!!

//     // DBG("ChipInstrument::renderNextBlock " << editPos << " - " << editPos + len << " (" << len << "s)");
//     // TODO calculate where is starting point of frames with respect to this block
//     // while (todo > 0) {
//     //     const auto size = std::min(frameSize, todo);
//     //     updateParams()  // like from param->getCurrentValue();  if needed
//     //     renderNextFrame(midiOut, time, size);
//     //     todo -= size;
//     //     time += size;
//     // }
// }

// }  // namespace MoTool::uZX
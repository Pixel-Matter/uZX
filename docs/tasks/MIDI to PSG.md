 Complex task:
 Create in /Users/ruguevara/projects/MoTool/src/plugins/uZX NotesToPsgPlugin code
 - that processes only midi messages
 - tracks all midi channels
 – listens note on and off messages
 - tracks currently playing notes (see if Tracktion has this functionality somewhere)
 - emits "PSG" midi CC events MidiCCType::
   - Volume (from velocity, mapped (0,127)->(0,15))
   - CC20PeriodCoarse + CC52PeriodFine (using current TuningSystem set for plugin)
   - GPB1 for tone on/off
 - for midi notes to "PSG" midi CC conversion with note tracking uses separate testable class


 You can see
 - src/plugins/uZX/aychip/AYPlugin.h as an example, but that plugin generates audio. I need
 NotesToPsgPlugin plugin to complement it and generate midi CC messages for AYChipPlugin from midi
 notes first.
 - src/models/PsgMidi.test.cpp - test on PSG params write and read. You should read it to learn what
 PSG params are because plugin must generate them from midi notes

 First come up with a plan and understaning and wait for approval
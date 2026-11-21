#pragma once

#include "Model.h"

#include "binary/container.h"

namespace MoTool::InstrDump {

/** Reads the instruments and their usage from a module of one format, everything of Module but
    its `format`. False when the bytes are not a module of that format.
*/
using Reader = bool (*)(const Binary::Container& data, Module& module);

// One per zxtune decoder, each defined in the formats/ file of its tracker.
bool readProTracker3(const Binary::Container& data, Module& module);
bool readVortexTracker2(const Binary::Container& data, Module& module);
bool readProTracker2(const Binary::Container& data, Module& module);
bool readProTracker1(const Binary::Container& data, Module& module);
bool readSoundTracker(const Binary::Container& data, Module& module);
bool readSoundTrackerCompiled(const Binary::Container& data, Module& module);
bool readSoundTracker3(const Binary::Container& data, Module& module);
bool readASCSoundMaster0(const Binary::Container& data, Module& module);
bool readASCSoundMaster1(const Binary::Container& data, Module& module);
bool readSoundTrackerPro(const Binary::Container& data, Module& module);
bool readProSoundMaker(const Binary::Container& data, Module& module);
bool readGlobalTracker(const Binary::Container& data, Module& module);
bool readSQTracker(const Binary::Container& data, Module& module);
bool readProSoundCreator(const Binary::Container& data, Module& module);
bool readFastTracker(const Binary::Container& data, Module& module);

}  // namespace MoTool::InstrDump

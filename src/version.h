#pragma once

/** Build metadata. The generated version.cpp (cmake/version.cpp.in) defines it in its own library, so a new stamp
    costs a relink instead of a recompile. */
namespace MoTool::Build
{
extern const char* const gitHash;
extern const bool gitDirty;
extern const char* const coreVersion;
extern const char* const buildTimestamp;
extern const char* const channel;
}

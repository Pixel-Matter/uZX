# Core (shared engine/library) version; Studio and Tuning follow it. Matches `v*` tags.
set(PROJECT_CORE_VERSION "0.7.0")

# Per-target application versions
set(UZX_STUDIO_VERSION "${PROJECT_CORE_VERSION}")
set(UZX_TUNING_VERSION "${PROJECT_CORE_VERSION}")
# Player is versioned on its own. Matches `player-v*` tags.
set(UZX_PLAYER_VERSION "0.6.0")

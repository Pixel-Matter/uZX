# Source files used by both GUI app and tests
set(SHARED_SOURCES
    util/FileOps.cpp
    util/Midi.cpp

    models/PsgMidi.cpp
    models/PsgList.cpp
    models/PsgClip.cpp
    models/Selectable.cpp
    models/EditUtilities.cpp

    models/tuning/Ratios.cpp
    models/tuning/Scales.cpp
    models/tuning/TuningRegistry.cpp
    models/tuning/TemperamentSystem.cpp
    models/tuning/TuningSystemBase.cpp
    models/tuning/AutoTuning.cpp
    models/tuning/TuningTable.cpp

    plugins/uZX/aychip/aychip.cpp
    plugins/uZX/aychip/AYPlugin.cpp
)

# GUI-specific files
set(GUI_SOURCES
    controllers/MainController.cpp
    controllers/App.cpp
    controllers/Main.cpp
    controllers/EditState.cpp

    # gui/nodes/GraphEditorPanel.cpp
    gui/common/Utilities.cpp
    gui/common/Transport.cpp

    gui/timeline/PlayheadComponent.cpp
    gui/timeline/PluginComponent.cpp
    gui/timeline/ClipComponents.cpp
    gui/timeline/TrackComponents.cpp
    gui/timeline/EditComponent.cpp
    gui/timeline/PsgClipComponent.cpp

    gui/tuning/TuningPreview.cpp
)

# Test-specific files to include in the test target
set(TEST_SOURCES
    ## models/PsgTrack.test.cpp
    # # models/CustomClip.test.cpp
    # models/PsgMidi.test.cpp
    # models/tuning/Ratios.test.cpp
    # models/tuning/TuningTable.test.cpp
    # models/tuning/EqualTemperamentTuning.test.cpp
    # models/tuning/AutoTuning.test.cpp
    models/tuning/Scales.test.cpp
    # viewmodels/tuning/TuningViewModel.test.cpp
    # plugins/uZX/aychip/AYChip.test.cpp
)

# Source files used by both GUI app and tests
set(SHARED_SOURCES
    util/FileOps.cpp
    util/Midi.cpp
    models/PsgMidi.cpp
    models/PsgList.cpp
    models/PsgClip.cpp
    models/Selectable.cpp
    models/EditUtilities.cpp
    plugins/uZX/aychip/aychip.cpp
    plugins/uZX/aychip/AYPlugin.cpp
)

# GUI-specific files
set(GUI_SOURCES
    controllers/MainController.cpp
    controllers/App.cpp
    controllers/Main.cpp
    # gui/nodes/GraphEditorPanel.cpp
    gui/common/Components.cpp
    gui/timeline/ClipComponents.cpp
    gui/timeline/TrackComponents.cpp
    gui/timeline/EditComponent.cpp
)

# Test-specific files to include in the test target
set(TEST_SOURCES
    ## models/PsgTrack.test.cpp
    # models/CustomClip.test.cpp
    models/PsgMidi.test.cpp
    # plugins/uZX/aychip/AYChip.test.cpp
)

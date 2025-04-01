# Source files used by both GUI app and tests
set(SHARED_SOURCES
    util/FileOps.cpp
    util/Midi.cpp
    model/PsgMidi.cpp
    model/PsgList.cpp
    model/PsgClip.cpp
    model/Selectable.cpp
    model/EditUtilities.cpp
    plugins/uZX/aychip/aychip.cpp
    plugins/uZX/aychip/AYPlugin.cpp
)

# GUI-specific files
set(GUI_SOURCES
    gui/app/App.cpp
    gui/Main.cpp
    # gui/nodes/GraphEditorPanel.cpp
    gui/common/Components.cpp
    gui/timeline/ClipComponents.cpp
    gui/timeline/TrackComponents.cpp
    gui/timeline/EditComponent.cpp
)

# Test-specific files to include in the test target
set(TEST_SOURCES
    ## model/PsgTrack.test.cpp
    # model/CustomClip.test.cpp
    model/PsgMidi.test.cpp
    # plugins/uZX/aychip/AYChip.test.cpp
)

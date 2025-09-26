# Source files used by both GUI app and tests
set(SHARED_SOURCES
    util/FileOps.cpp
    util/Midi.cpp
    util/Helpers.cpp

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

    viewmodels/tuning/TuningPlayer.cpp
    viewmodels/tuning/MultitrackMidiPreview.cpp

    plugins/uZX/aychip/aychip.cpp
    plugins/uZX/aychip/AYPlugin.cpp
    plugins/uZX/notes_to_psg/NotesToPsgMapper.cpp
    plugins/uZX/notes_to_psg/NotesToPsgPlugin.cpp
    plugins/uZX/instrument/ChipInstrument.cpp
    plugins/uZX/instrument/ChipInstrumentVoice.cpp
    plugins/uZX/instrument/ChipInstrumentPlugin.cpp

    gui/common/Utilities.cpp
    gui/common/LookAndFeel.cpp
    gui/common/LabeledKnob.cpp
    gui/common/MidiParameterMapping.cpp
)

# GUI-specific files
set(GUI_SOURCES
    controllers/MainController.cpp
    controllers/TuningController.cpp
    controllers/App.cpp
    controllers/Main.cpp
    controllers/EditState.cpp

    # gui/nodes/GraphEditorPanel.cpp
    gui/common/Transport.cpp
    gui/main/MainWindow.cpp
    gui/main/Footer.cpp
    gui/tuning/TuningPreview.cpp

    gui/timeline/PlayheadComponent.cpp
    gui/timeline/TimelineGrid.cpp
    gui/timeline/ClipComponents.cpp
    gui/timeline/TrackComponents.cpp
    gui/timeline/EditComponent.cpp
    gui/timeline/PsgClipComponent.cpp
    gui/timeline/PsgParamEditorComponent.cpp
    gui/timeline/Ruler.cpp
    gui/timeline/DetailsPanelComponent.cpp

    gui/devices/PluginTree.cpp
    gui/devices/TrackDevicesPanel.cpp
    gui/devices/PluginDeviceUI.cpp
    gui/devices/PluginUIAdapterRegistry.cpp
    gui/devices/DevicePanelItem.cpp
    gui/devices/GenericPluginAdapters.cpp
    gui/devices/LevelMeterUI.cpp

    plugins/uZX/instrument/ChipInstrumentUI.cpp
)

# Test-specific files to include in the test target
set(TEST_SOURCES
    ## models/PsgTrack.test.cpp
    ## models/CustomClip.test.cpp
    models/PsgMidi.test.cpp
    models/tuning/Ratios.test.cpp
    models/tuning/TuningTable.test.cpp
    models/tuning/EqualTemperamentTuning.test.cpp
    models/tuning/AutoTuning.test.cpp
    models/tuning/Scales.test.cpp
    models/tuning/RationalTuning.test.cpp
    viewmodels/tuning/TuningViewModel.test.cpp
    viewmodels/tuning/MultitrackMidiPreview.test.cpp
    plugins/uZX/aychip/AYChip.test.cpp
    plugins/uZX/notes_to_psg/NotesToPsgMapper.test.cpp
)

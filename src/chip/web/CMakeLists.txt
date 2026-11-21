add_library(uzx_web_engine OBJECT Engine.cpp)
target_link_libraries(uzx_web_engine PUBLIC uzx_chip ayumi)
target_compile_features(uzx_web_engine PUBLIC cxx_std_23)

if(EMSCRIPTEN)
    # Export a standalone reactor. The AudioWorklet owns the render loop and the WASM instance.
    add_executable(uzx_worklet_wasm $<TARGET_OBJECTS:uzx_web_engine>)
    target_link_libraries(uzx_worklet_wasm PRIVATE uzx_chip ayumi)
    target_link_options(uzx_worklet_wasm PRIVATE
        --no-entry
        -sSTANDALONE_WASM=1
        -sFILESYSTEM=0
        -sALLOW_MEMORY_GROWTH=0
        "-sEXPORTED_FUNCTIONS=['_uzx_init','_uzx_reset','_uzx_note_sound','_uzx_note_on','_uzx_note_off','_uzx_render','_uzx_audio_left','_uzx_audio_right','_uzx_trace_data','_uzx_trace_count','_uzx_trace_clear','_uzx_dropped_note_count']"
    )
    set_target_properties(uzx_worklet_wasm PROPERTIES
        OUTPUT_NAME engine
        SUFFIX .wasm
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../web/strudel/public"
    )
endif()

if(BUILD_TESTING)
    add_executable(uzx_web_engine_tests ../../../tests/chip/WebEngineTest.cpp)
    target_link_libraries(uzx_web_engine_tests PRIVATE uzx_web_engine)
    add_test(NAME WebEngine COMMAND uzx_web_engine_tests)
endif()

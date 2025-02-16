# File: src/plugins/uZX/CMakeLists.txt

set(TARGET_NAME uZX)

# Create Library
add_library(${TARGET_NAME}
    aychip/aychip.cpp
    aychip/aychip.h
    # aychip/freq_tables.h
)

# Include Directories
target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME} PUBLIC
    ayumi
)

# Compiler Settings
target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)

set_target_properties(${TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

# Export Target
add_library(motool::uZX ALIAS uZX)
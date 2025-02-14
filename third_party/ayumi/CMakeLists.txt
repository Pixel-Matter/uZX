add_library(ayumi STATIC
    ayumi.h
    ayumi.c
)

target_include_directories(ayumi
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(ayumi PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

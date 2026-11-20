cmake_minimum_required(VERSION 3.20)

# This subdirectory can also be configured on its own with Emscripten.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(uZXChip LANGUAGES CXX)
    include(CTest)
endif()

add_library(uzx_chip STATIC ChipAllocator.cpp)
target_compile_features(uzx_chip PUBLIC cxx_std_23)
target_include_directories(uzx_chip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

if(BUILD_TESTING)
    add_executable(uzx_chip_tests ../../tests/chip/ChipAllocatorTest.cpp)
    target_link_libraries(uzx_chip_tests PRIVATE uzx_chip)
    add_test(NAME ChipAllocator COMMAND uzx_chip_tests)
endif()

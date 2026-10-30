# cmake/GenerateIconset.cmake
# Generates a .iconset directory and .icns file from source PNGs.
# 16px comes from <prefix>_tiny.png, 32px from <prefix>_small.png, larger sizes from <prefix>_big.png.
#
# Required variables:
#   SOURCE_PREFIX - Path prefix of the source PNGs: <prefix>_big.png (1024x1024), _small.png (32x32), _tiny.png (16x16)
#   ICONSET_DIR  - Path to output .iconset directory
#   ICNS_OUTPUT  - Path to output .icns file

foreach(variant big small tiny)
    if(NOT EXISTS "${SOURCE_PREFIX}_${variant}.png")
        message(FATAL_ERROR "Source PNG not found: ${SOURCE_PREFIX}_${variant}.png")
    endif()
endforeach()

# Filenames and corresponding pixel sizes (two parallel lists)
set(ICON_NAMES
    icon_16x16.png
    icon_16x16@2x.png
    icon_32x32.png
    icon_32x32@2x.png
    icon_128x128.png
    icon_128x128@2x.png
    icon_256x256.png
    icon_256x256@2x.png
    icon_512x512.png
    icon_512x512@2x.png
)
set(ICON_PIXELS
    16 32 32 64 128 256 256 512 512 1024
)

file(MAKE_DIRECTORY "${ICONSET_DIR}")

list(LENGTH ICON_NAMES count)
math(EXPR last "${count} - 1")

foreach(i RANGE ${last})
    list(GET ICON_NAMES  ${i} filename)
    list(GET ICON_PIXELS ${i} size)

    set(output_file "${ICONSET_DIR}/${filename}")

    if(size EQUAL 16)
        set(source_png "${SOURCE_PREFIX}_tiny.png")
    elseif(size EQUAL 32)
        set(source_png "${SOURCE_PREFIX}_small.png")
    else()
        set(source_png "${SOURCE_PREFIX}_big.png")
    endif()

    # Copy source then resize in-place with sips
    file(COPY_FILE "${source_png}" "${output_file}")
    execute_process(
        COMMAND sips -z ${size} ${size} "${output_file}"
        OUTPUT_QUIET
        ERROR_VARIABLE sips_error
        RESULT_VARIABLE sips_result
    )
    if(NOT sips_result EQUAL 0)
        message(FATAL_ERROR "sips failed for ${filename} (${size}x${size}): ${sips_error}")
    endif()
endforeach()

# Generate .icns from the iconset
execute_process(
    COMMAND iconutil -c icns "${ICONSET_DIR}" -o "${ICNS_OUTPUT}"
    ERROR_VARIABLE iconutil_error
    RESULT_VARIABLE iconutil_result
)
if(NOT iconutil_result EQUAL 0)
    message(FATAL_ERROR "iconutil failed: ${iconutil_error}")
endif()

message(STATUS "Generated ${ICNS_OUTPUT} from ${SOURCE_PREFIX}_*.png")

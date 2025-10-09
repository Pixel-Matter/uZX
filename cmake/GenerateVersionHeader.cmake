# This script runs at build time to produce src/version.h with current metadata.

if(NOT DEFINED INPUT_TEMPLATE)
    message(FATAL_ERROR "INPUT_TEMPLATE variable not provided")
endif()

if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE variable not provided")
endif()

if(NOT DEFINED MOTOOL_CHANNEL)
    set(MOTOOL_CHANNEL "dev")
endif()

if(NOT DEFINED MOTOOL_SOURCE_DIR)
    set(MOTOOL_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
endif()

if(DEFINED VERSION_FILE)
    if(EXISTS "${VERSION_FILE}")
        file(READ "${VERSION_FILE}" _motool_version_from_file)
        string(STRIP "${_motool_version_from_file}" _motool_version_from_file)
        if(_motool_version_from_file STREQUAL "")
            message(FATAL_ERROR "VERSION_FILE '${VERSION_FILE}' is empty")
        endif()
        set(MOTOOL_VERSION_BASE "${_motool_version_from_file}")
    else()
        message(FATAL_ERROR "VERSION_FILE '${VERSION_FILE}' does not exist")
    endif()
endif()

if(NOT DEFINED MOTOOL_VERSION_BASE)
    message(FATAL_ERROR "MOTOOL_VERSION_BASE variable not provided")
endif()

string(TOLOWER "${MOTOOL_CHANNEL}" MOTOOL_CHANNEL_LOWER)
set(MOTOOL_VERSION_SUFFIX "")
set(MOTOOL_VERSION_DISPLAY "${MOTOOL_VERSION_BASE}")
set(MOTOOL_GIT_COMMIT "")
set(MOTOOL_GIT_DIRTY 0)

if(MOTOOL_CHANNEL_LOWER)
    set(MOTOOL_VERSION_SUFFIX "-${MOTOOL_CHANNEL_LOWER}")
    set(MOTOOL_VERSION_DISPLAY "${MOTOOL_VERSION_BASE}${MOTOOL_VERSION_SUFFIX}")
endif()

string(TIMESTAMP MOTOOL_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

if(NOT DEFINED GIT_EXECUTABLE)
    find_program(GIT_EXECUTABLE git)
endif()

if(GIT_EXECUTABLE AND EXISTS "${MOTOOL_SOURCE_DIR}")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${MOTOOL_SOURCE_DIR}
        OUTPUT_VARIABLE MOTOOL_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _motool_git_commit_result
    )

    if(_motool_git_commit_result EQUAL 0 AND MOTOOL_GIT_COMMIT)
        if(MOTOOL_CHANNEL_LOWER STREQUAL "dev")
            if(MOTOOL_VERSION_SUFFIX)
                string(APPEND MOTOOL_VERSION_SUFFIX "+g${MOTOOL_GIT_COMMIT}")
            else()
                set(MOTOOL_VERSION_SUFFIX "+g${MOTOOL_GIT_COMMIT}")
            endif()
            set(MOTOOL_VERSION_DISPLAY "${MOTOOL_VERSION_BASE}${MOTOOL_VERSION_SUFFIX}")
        endif()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} status --porcelain
            WORKING_DIRECTORY ${MOTOOL_SOURCE_DIR}
            OUTPUT_VARIABLE _motool_git_status_output
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _motool_git_status_result
        )

        if(_motool_git_status_result EQUAL 0 AND _motool_git_status_output)
            string(APPEND MOTOOL_VERSION_SUFFIX "-dirty")
            set(MOTOOL_VERSION_DISPLAY "${MOTOOL_VERSION_BASE}${MOTOOL_VERSION_SUFFIX}")
            set(MOTOOL_GIT_DIRTY 1)
        endif()
    elseif(NOT _motool_git_commit_result EQUAL 0)
        set(MOTOOL_GIT_COMMIT "unknown")
    endif()
endif()

configure_file(
    "${INPUT_TEMPLATE}"
    "${OUTPUT_FILE}"
    @ONLY
)

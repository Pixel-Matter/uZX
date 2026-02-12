# This script runs at build time to produce src/version.h with current metadata.

if(NOT DEFINED INPUT_TEMPLATE)
    message(FATAL_ERROR "INPUT_TEMPLATE variable not provided")
endif()

if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE variable not provided")
endif()

if(NOT DEFINED PROJECT_CHANNEL)
    set(PROJECT_CHANNEL "dev")
endif()

if(NOT DEFINED PROJECT_SOURCE_DIR)
    set(PROJECT_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
endif()

if(NOT DEFINED PROJECT_VERSION_BASE)
    message(FATAL_ERROR "PROJECT_VERSION_BASE variable not provided")
endif()

string(TOLOWER "${PROJECT_CHANNEL}" PROJECT_CHANNEL_LOWER)
set(PROJECT_VERSION_SUFFIX "")
set(PROJECT_VERSION_DISPLAY "${PROJECT_VERSION_BASE}")
set(PROJECT_GIT_COMMIT "")
set(PROJECT_GIT_DIRTY 0)

if(PROJECT_CHANNEL_LOWER)
    set(PROJECT_VERSION_SUFFIX "-${PROJECT_CHANNEL_LOWER}")
    set(PROJECT_VERSION_DISPLAY "${PROJECT_VERSION_BASE}${PROJECT_VERSION_SUFFIX}")
endif()

string(TIMESTAMP VERSION_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

if(NOT DEFINED GIT_EXECUTABLE)
    find_program(GIT_EXECUTABLE git)
endif()

if(GIT_EXECUTABLE AND EXISTS "${PROJECT_SOURCE_DIR}")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE PROJECT_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _repo_git_commit_result
    )

    if(_repo_git_commit_result EQUAL 0 AND PROJECT_GIT_COMMIT)
        if(PROJECT_CHANNEL_LOWER STREQUAL "dev")
            if(PROJECT_VERSION_SUFFIX)
                string(APPEND PROJECT_VERSION_SUFFIX "+g${PROJECT_GIT_COMMIT}")
            else()
                set(PROJECT_VERSION_SUFFIX "+g${PROJECT_GIT_COMMIT}")
            endif()
            set(PROJECT_VERSION_DISPLAY "${PROJECT_VERSION_BASE}${PROJECT_VERSION_SUFFIX}")
        endif()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} status --porcelain
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE _project_git_status_output
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _project_git_status_result
        )

        if(_project_git_status_result EQUAL 0 AND _project_git_status_output)
            string(APPEND PROJECT_VERSION_SUFFIX "-dirty")
            set(PROJECT_VERSION_DISPLAY "${PROJECT_VERSION_BASE}${PROJECT_VERSION_SUFFIX}")
            set(PROJECT_GIT_DIRTY 1)
        endif()
    elseif(NOT _repo_git_commit_result EQUAL 0)
        set(PROJECT_GIT_COMMIT "unknown")
    endif()
endif()

configure_file(
    "${INPUT_TEMPLATE}"
    "${OUTPUT_FILE}"
    @ONLY
)

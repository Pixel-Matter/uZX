# Runs at build time to write version.cpp with the current git state and build time.

foreach(_var INPUT_TEMPLATE OUTPUT_FILE PROJECT_SOURCE_DIR PROJECT_VERSION_BASE PROJECT_CHANNEL)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "${_var} variable not provided")
    endif()
endforeach()

set(PROJECT_GIT_COMMIT "")
set(PROJECT_GIT_DIRTY 0)
string(TIMESTAMP VERSION_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

if(GIT_EXECUTABLE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE PROJECT_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _commit_result
    )
    if(NOT _commit_result EQUAL 0)
        set(PROJECT_GIT_COMMIT "unknown")
    else()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} status --porcelain
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE _status
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _status_result
        )
        if(_status_result EQUAL 0 AND _status)
            set(PROJECT_GIT_DIRTY 1)
        endif()
    endif()
endif()

configure_file("${INPUT_TEMPLATE}" "${OUTPUT_FILE}" @ONLY)

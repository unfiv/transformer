if(NOT DEFINED CASE_DIR)
    message(FATAL_ERROR "CASE_DIR is required")
endif()

if(NOT DEFINED CASE_NAME)
    message(FATAL_ERROR "CASE_NAME is required")
endif()

if(NOT DEFINED TRANSFORMER_BIN)
    message(FATAL_ERROR "TRANSFORMER_BIN is required")
endif()

if(NOT DEFINED PARAMS_FILE)
    message(FATAL_ERROR "PARAMS_FILE is required")
endif()

if(NOT EXISTS "${PARAMS_FILE}")
    message(FATAL_ERROR "Params file does not exist: ${PARAMS_FILE}")
endif()

file(STRINGS "${PARAMS_FILE}" CASE_PARAMS)

foreach(raw_line IN LISTS CASE_PARAMS)
    string(STRIP "${raw_line}" line)

    if(line STREQUAL "" OR line MATCHES "^#")
        continue()
    endif()

    string(FIND "${line}" "=" delimiter_pos)
    if(delimiter_pos EQUAL -1)
        message(FATAL_ERROR "Invalid params line in ${PARAMS_FILE}: '${line}'. Expected key=value")
    endif()

    string(SUBSTRING "${line}" 0 ${delimiter_pos} key)
    math(EXPR value_start "${delimiter_pos} + 1")
    string(SUBSTRING "${line}" ${value_start} -1 value)

    string(STRIP "${key}" key)
    string(STRIP "${value}" value)

    if(key STREQUAL "mesh")
        set(MESH_FILE "${CASE_DIR}/${value}")
    elseif(key STREQUAL "weights")
        set(WEIGHTS_FILE "${CASE_DIR}/${value}")
    elseif(key STREQUAL "inverse_bind_pose")
        set(INVERSE_BIND_POSE_FILE "${CASE_DIR}/${value}")
    elseif(key STREQUAL "new_pose")
        set(NEW_POSE_FILE "${CASE_DIR}/${value}")
    elseif(key STREQUAL "expected_output")
        set(EXPECTED_OUTPUT_FILE "${CASE_DIR}/${value}")
    else()
        message(FATAL_ERROR "Unknown key '${key}' in params file: ${PARAMS_FILE}")
    endif()
endforeach()

set(REQUIRED_VARS
    MESH_FILE
    WEIGHTS_FILE
    INVERSE_BIND_POSE_FILE
    NEW_POSE_FILE
    EXPECTED_OUTPUT_FILE
)

foreach(required_var IN LISTS REQUIRED_VARS)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "Missing required key for ${required_var} in params file: ${PARAMS_FILE}")
    endif()
endforeach()

set(CASE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/integration/${CASE_NAME}")
file(MAKE_DIRECTORY "${CASE_OUTPUT_DIR}")

set(ACTUAL_OUTPUT_FILE "${CASE_OUTPUT_DIR}/result_mesh.obj")
set(STATS_FILE "${CASE_OUTPUT_DIR}/stats.json")

execute_process(
    COMMAND "${TRANSFORMER_BIN}"
        "${MESH_FILE}"
        "${WEIGHTS_FILE}"
        "${INVERSE_BIND_POSE_FILE}"
        "${NEW_POSE_FILE}"
        "${ACTUAL_OUTPUT_FILE}"
        "${STATS_FILE}"
    RESULT_VARIABLE APP_EXIT_CODE
)

if(NOT APP_EXIT_CODE EQUAL 0)
    message(FATAL_ERROR "Case '${CASE_NAME}' failed: transformer exited with code ${APP_EXIT_CODE}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${EXPECTED_OUTPUT_FILE}"
        "${ACTUAL_OUTPUT_FILE}"
    RESULT_VARIABLE COMPARE_EXIT_CODE
)

if(NOT COMPARE_EXIT_CODE EQUAL 0)
    # Provide detailed diagnostics: which files differ and the first differing line (if text)
    message(STATUS "Files differ: Expected='${EXPECTED_OUTPUT_FILE}' Actual='${ACTUAL_OUTPUT_FILE}'")

    # Attempt to read both files as text and compare line-by-line to find the first differing line.
    file(READ "${EXPECTED_OUTPUT_FILE}" _expected_raw)
    file(READ "${ACTUAL_OUTPUT_FILE}" _actual_raw)

    # Normalize line endings to LF
    string(REPLACE "\r\n" "\n" _expected_raw "${_expected_raw}")
    string(REPLACE "\r" "\n" _expected_raw "${_expected_raw}")
    string(REPLACE "\r\n" "\n" _actual_raw "${_actual_raw}")
    string(REPLACE "\r" "\n" _actual_raw "${_actual_raw}")

    # Split into lists of lines
    string(REPLACE "\n" ";" _expected_lines "${_expected_raw}")
    string(REPLACE "\n" ";" _actual_lines "${_actual_raw}")

    list(LENGTH _expected_lines _expected_count)
    list(LENGTH _actual_lines _actual_count)

    if(_expected_count LESS _actual_count)
        set(_min_count ${_expected_count})
    else()
        set(_min_count ${_actual_count})
    endif()

    set(_first_diff_index -1)
    if(_min_count GREATER 0)
        math(EXPR _last_index "${_min_count} - 1")
        foreach(_i RANGE 0 ${_last_index})
            list(GET _expected_lines ${_i} _e_line)
            list(GET _actual_lines ${_i} _a_line)
            if(NOT _e_line STREQUAL _a_line)
                set(_first_diff_index ${_i})
                set(_first_expected_line "${_e_line}")
                set(_first_actual_line "${_a_line}")
                break()
            endif()
        endforeach()
    endif()

    if(_first_diff_index GREATER -1)
        message(FATAL_ERROR "Case '${CASE_NAME}' failed: output mesh differs from expected\nExpected: ${EXPECTED_OUTPUT_FILE}\nActual:   ${ACTUAL_OUTPUT_FILE}\nFirst differing line (1-based): ${_first_diff_index}\nExpected line: '${_first_expected_line}'\nActual   line: '${_first_actual_line}'")
    else()
        # No differing line found within common range — probably different sizes or binary difference.
        message(FATAL_ERROR "Case '${CASE_NAME}' failed: output mesh differs from expected\nExpected: ${EXPECTED_OUTPUT_FILE}\nActual:   ${ACTUAL_OUTPUT_FILE}\nExpected lines: ${_expected_count}\nActual lines:   ${_actual_count}")
    endif()
endif()

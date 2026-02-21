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
    message(FATAL_ERROR "Case '${CASE_NAME}' failed: output mesh differs from expected")
endif()

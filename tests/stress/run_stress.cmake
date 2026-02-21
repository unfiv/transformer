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
if(NOT DEFINED RUNS)
    set(RUNS 100)
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
    endif()
endforeach()

set(CASE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/stress/${CASE_NAME}")
file(MAKE_DIRECTORY "${CASE_OUTPUT_DIR}")
set(ACTUAL_OUTPUT_FILE "${CASE_OUTPUT_DIR}/result_mesh.obj")
set(STATS_FILE "${CASE_OUTPUT_DIR}/stats.json")
set(AGGREGATE_FILE "${CASE_OUTPUT_DIR}/stress_stats.json")

execute_process(
    COMMAND "${TRANSFORMER_BIN}"
        --mesh "${MESH_FILE}"
        --bones-weights "${WEIGHTS_FILE}"
        --inverse-bind-pose "${INVERSE_BIND_POSE_FILE}"
        --new-pose "${NEW_POSE_FILE}"
        --output "${ACTUAL_OUTPUT_FILE}"
        --stats "${STATS_FILE}"
        --bench "${RUNS}"
    RESULT_VARIABLE _app_code
)
if(NOT _app_code EQUAL 0)
    message(FATAL_ERROR "Transformer exited with code ${_app_code}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${EXPECTED_OUTPUT_FILE}" "${ACTUAL_OUTPUT_FILE}"
    RESULT_VARIABLE _compare_code
)
if(NOT _compare_code EQUAL 0)
    message(FATAL_ERROR "Stress case '${CASE_NAME}' failed: output differs from expected")
endif()

file(READ "${STATS_FILE}" _stats_raw)

function(_extract_number out_var key)
    string(REGEX REPLACE ".*\"${key}\"[ \t]*:[ \t]*([0-9]+\\.?[0-9]*).*" "\\1" _value "${_stats_raw}")
    if(_value MATCHES "^[0-9]+(\\.[0-9]+)?$")
        set(${out_var} "${_value}" PARENT_SCOPE)
    else()
        set(${out_var} "null" PARENT_SCOPE)
    endif()
endfunction()

_extract_number(_bench_min min_microseconds)
_extract_number(_bench_max max_microseconds)
_extract_number(_bench_mean mean_microseconds)
_extract_number(_bench_median median_microseconds)
_extract_number(_bench_stddev stddev_microseconds)

file(WRITE "${AGGREGATE_FILE}"
    "{\n"
    "  \"case_name\": \"${CASE_NAME}\",\n"
    "  \"runs\": ${RUNS},\n"
    "  \"timing_summary_us\": {\n"
    "    \"min\": ${_bench_min},\n"
    "    \"max\": ${_bench_max},\n"
    "    \"mean\": ${_bench_mean},\n"
    "    \"median\": ${_bench_median},\n"
    "    \"stddev\": ${_bench_stddev}\n"
    "  },\n"
    "  \"stats_file\": \"${STATS_FILE}\"\n"
    "}\n"
)

message(STATUS "Stress run complete. Aggregate stats: ${AGGREGATE_FILE}")

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
    set(RUNS 10000)
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

if(DEFINED OUTPUT_ROOT_DIR)
    set(_stress_output_root "${OUTPUT_ROOT_DIR}")
else()
    set(_stress_output_root "${CMAKE_BINARY_DIR}/tests/stress")
endif()

set(CASE_OUTPUT_DIR "${_stress_output_root}/${CASE_NAME}")
file(MAKE_DIRECTORY "${CASE_OUTPUT_DIR}")
set(ACTUAL_OUTPUT_FILE "${CASE_OUTPUT_DIR}/result_mesh.obj")
set(STATS_FILE "${CASE_OUTPUT_DIR}/stats.json")
set(AGGREGATE_FILE "${CASE_OUTPUT_DIR}/stress_stats.json")

string(TIMESTAMP _start_iso "%Y-%m-%dT%H:%M:%S%z")
string(TIMESTAMP _start_epoch "%s")

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

string(TIMESTAMP _end_iso "%Y-%m-%dT%H:%M:%S%z")
string(TIMESTAMP _end_epoch "%s")
math(EXPR _duration_seconds "${_end_epoch} - ${_start_epoch}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${EXPECTED_OUTPUT_FILE}" "${ACTUAL_OUTPUT_FILE}"
    RESULT_VARIABLE _compare_code
)
if(NOT _compare_code EQUAL 0)
    message(FATAL_ERROR "Stress case '${CASE_NAME}' failed: output differs from expected")
endif()

file(READ "${STATS_FILE}" _stats_raw)

string(REGEX MATCHALL "\"stage\"[ \t]*:[ \t]*\"cpu_skinning\"" _cpu_skinning_matches "${_stats_raw}")
list(LENGTH _cpu_skinning_matches _cpu_skinning_runs)
if(NOT _cpu_skinning_runs EQUAL RUNS)
    message(FATAL_ERROR "Stress case '${CASE_NAME}' expected ${RUNS} cpu_skinning samples, got ${_cpu_skinning_runs}. Stats file: ${STATS_FILE}")
endif()

include(ProcessorCount)
ProcessorCount(_proc_count)
if(DEFINED STRESS_HOST_ARCH AND NOT "${STRESS_HOST_ARCH}" STREQUAL "")
    set(_cpu_arch "${STRESS_HOST_ARCH}")
else()
    set(_cpu_arch "unknown")
endif()
if(DEFINED STRESS_SYSTEM_NAME AND NOT "${STRESS_SYSTEM_NAME}" STREQUAL "")
    set(_system_name "${STRESS_SYSTEM_NAME}")
else()
    set(_system_name "unknown")
endif()
if(DEFINED STRESS_CMAKE_VERSION AND NOT "${STRESS_CMAKE_VERSION}" STREQUAL "")
    set(_cmake_version "${STRESS_CMAKE_VERSION}")
else()
    set(_cmake_version "unknown")
endif()
if(DEFINED STRESS_CXX_COMPILER AND NOT "${STRESS_CXX_COMPILER}" STREQUAL "")
    set(_cxx_compiler "${STRESS_CXX_COMPILER}")
else()
    set(_cxx_compiler "unknown")
endif()
if(DEFINED STRESS_CXX_COMPILER_ID AND NOT "${STRESS_CXX_COMPILER_ID}" STREQUAL "")
    set(_cxx_id "${STRESS_CXX_COMPILER_ID}")
else()
    set(_cxx_id "unknown")
endif()
if(DEFINED TRANSFORMER_BUILD_CONFIG AND NOT "${TRANSFORMER_BUILD_CONFIG}" STREQUAL "")
    set(_build_type "${TRANSFORMER_BUILD_CONFIG}")
elseif(DEFINED TRANSFORMER_BUILD_TYPE AND NOT "${TRANSFORMER_BUILD_TYPE}" STREQUAL "")
    set(_build_type "${TRANSFORMER_BUILD_TYPE}")
else()
    set(_build_type "${CMAKE_BUILD_TYPE}")
endif()

set(_total_memory "unknown")
set(_cpu_model "unknown")
if(WIN32)
    execute_process(COMMAND powershell -NoProfile -Command "(Get-CimInstance -ClassName Win32_ComputerSystem).TotalPhysicalMemory" OUTPUT_VARIABLE _tmp_mem OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_tmp_mem)
        set(_total_memory "${_tmp_mem}")
    endif()
    execute_process(COMMAND powershell -NoProfile -Command "(Get-CimInstance -ClassName Win32_Processor | Select-Object -First 1 -ExpandProperty Name)" OUTPUT_VARIABLE _tmp_cpu OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_tmp_cpu)
        set(_cpu_model "${_tmp_cpu}")
    endif()
elseif(APPLE)
    execute_process(COMMAND sysctl -n hw.memsize OUTPUT_VARIABLE _tmp_mem OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_tmp_mem)
        set(_total_memory "${_tmp_mem}")
    endif()
    execute_process(COMMAND sysctl -n machdep.cpu.brand_string OUTPUT_VARIABLE _tmp_cpu OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_tmp_cpu)
        set(_cpu_model "${_tmp_cpu}")
    endif()
elseif(UNIX)
    if(EXISTS "/proc/meminfo")
        execute_process(COMMAND bash -c "grep MemTotal /proc/meminfo | awk '{print $2}'" OUTPUT_VARIABLE _tmp_mem OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_tmp_mem)
            math(EXPR _tmp_bytes "${_tmp_mem} * 1024")
            set(_total_memory "${_tmp_bytes}")
        endif()
    endif()
    if(EXISTS "/proc/cpuinfo")
        execute_process(COMMAND bash -c "grep 'model name' /proc/cpuinfo | head -n1 | cut -d: -f2 | sed 's/^[[:space:]]*//'" OUTPUT_VARIABLE _tmp_cpu OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_tmp_cpu)
            set(_cpu_model "${_tmp_cpu}")
        endif()
    endif()
endif()

if(DEFINED TRANSFORMER_VERSION)
    set(_app_version "${TRANSFORMER_VERSION}")
else()
    set(_app_version "unknown")
endif()

file(WRITE "${AGGREGATE_FILE}"
    "{\n"
    "  \"case_name\": \"${CASE_NAME}\",\n"
    "  \"runs\": ${RUNS},\n"
    "  \"timing\": {\n"
    "    \"start\": \"${_start_iso}\",\n"
    "    \"end\": \"${_end_iso}\",\n"
    "    \"duration_seconds\": ${_duration_seconds}\n"
    "  },\n"
    "  \"environment\": {\n"
    "    \"system\": \"${_system_name}\",\n"
    "    \"host_arch\": \"${_cpu_arch}\",\n"
    "    \"processor_count\": ${_proc_count},\n"
    "    \"cpu_model\": \"${_cpu_model}\",\n"
    "    \"total_memory_bytes\": \"${_total_memory}\"\n"
    "  },\n"
    "  \"build\": {\n"
    "    \"cmake_version\": \"${_cmake_version}\",\n"
    "    \"cxx_compiler\": \"${_cxx_compiler}\",\n"
    "    \"cxx_id\": \"${_cxx_id}\",\n"
    "    \"build_type\": \"${_build_type}\"\n"
    "  },\n"
    "  \"app_version\": \"${_app_version}\",\n"
    "  \"stats_file\": \"${STATS_FILE}\",\n"
    "  \"transformer_stats\": "
)
file(APPEND "${AGGREGATE_FILE}" "${_stats_raw}")
file(APPEND "${AGGREGATE_FILE}" "\n}\n")

message(STATUS "Stress run complete. Aggregate stats: ${AGGREGATE_FILE}")

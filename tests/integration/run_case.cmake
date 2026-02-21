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

# Record start time
string(TIMESTAMP _start_iso "%Y-%m-%dT%H:%M:%S%z")
string(TIMESTAMP _start_epoch "%s")

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

# Record end time and duration (seconds)
string(TIMESTAMP _end_iso "%Y-%m-%dT%H:%M:%S%z")
string(TIMESTAMP _end_epoch "%s")
math(EXPR _duration_seconds "${_end_epoch} - ${_start_epoch}")

if(NOT APP_EXIT_CODE EQUAL 0)
    message(FATAL_ERROR "Case '${CASE_NAME}' failed: transformer exited with code ${APP_EXIT_CODE}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${EXPECTED_OUTPUT_FILE}"
        "${ACTUAL_OUTPUT_FILE}"
    RESULT_VARIABLE COMPARE_EXIT_CODE
)

# On success, write enriched stats for performance analysis
if(COMPARE_EXIT_CODE EQUAL 0)
    # Gather system info
    include(ProcessorCount)
    ProcessorCount(_proc_count)
    set(_cpu_arch "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    set(_system_name "${CMAKE_SYSTEM_NAME}")
    set(_cmake_version "${CMAKE_VERSION}")
    set(_cxx_compiler "${CMAKE_CXX_COMPILER}")
    set(_cxx_id "${CMAKE_CXX_COMPILER_ID}")
    set(_build_type "${CMAKE_BUILD_TYPE}")

    # Try to detect total physical memory and CPU model where possible
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
            execute_process(COMMAND bash -c "grep 'model name' /proc/cpuinfo | head -n1 | cut -d: -f2 | sed 's/^\s*//'" OUTPUT_VARIABLE _tmp_cpu OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(_tmp_cpu)
                set(_cpu_model "${_tmp_cpu}")
            endif()
        endif()
    endif()

    # Git info if available
    set(_git_commit "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
        execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse --short HEAD OUTPUT_VARIABLE _git_commit OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()

    # Prepare JSON-safe strings (basic escaping)
    foreach(_v IN ITEMS CASE_NAME CASE_DIR MESH_FILE WEIGHTS_FILE INVERSE_BIND_POSE_FILE NEW_POSE_FILE EXPECTED_OUTPUT_FILE ACTUAL_OUTPUT_FILE STATS_FILE _start_iso _end_iso _duration_seconds _proc_count _cpu_arch _system_name _cmake_version _cxx_compiler _cxx_id _build_type _total_memory _cpu_model _git_commit)
        # Read raw value
        set(_tmp_raw "${${_v}}")
        # Escape double quotes and newlines for JSON. Avoid manipulating backslashes to reduce parsing issues.
        string(REPLACE "\"" "\\\"" _tmp_escaped "${_tmp_raw}")
        string(REPLACE "\n" "\\n" _tmp_escaped "${_tmp_escaped}")
        set(${_v}_json "${_tmp_escaped}")
    endforeach()

    # Write minimal success stats (no per-run transformer stats) — used for performance analysis elsewhere
    file(WRITE "${STATS_FILE}"
        "{\n"
        "  \"case_name\": \"${CASE_NAME_json}\",\n"
        "  \"case_dir\": \"${CASE_DIR_json}\",\n"
        "  \"inputs\": {\n"
        "    \"mesh\": \"${MESH_FILE_json}\",\n"
        "    \"weights\": \"${WEIGHTS_FILE_json}\",\n"
        "    \"inverse_bind_pose\": \"${INVERSE_BIND_POSE_FILE_json}\",\n"
        "    \"new_pose\": \"${NEW_POSE_FILE_json}\"\n"
        "  },\n"
        "  \"expected_output\": \"${EXPECTED_OUTPUT_FILE_json}\",\n"
        "  \"actual_output\": \"${ACTUAL_OUTPUT_FILE_json}\",\n"
        "  \"timing\": {\n"
        "    \"start\": \"${_start_iso}\",\n"
        "    \"end\": \"${_end_iso}\",\n"
        "    \"duration_seconds\": ${_duration_seconds}\n"
        "  },\n"
        "  \"result\": { \"success\": true, \"compare_code\": ${COMPARE_EXIT_CODE}, \"app_exit_code\": ${APP_EXIT_CODE} }\n"
        "}\n"
    )

    message(STATUS "Case '${CASE_NAME}' succeeded: output matches expected. Minimal stats written to: ${STATS_FILE}")
else()
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
        # Write minimal failure stats: file paths and the first differing line (1-based) and its content
        math(EXPR _first_diff_1 "${_first_diff_index} + 1")
        file(WRITE "${STATS_FILE}"
            "{\n"
            "  \"case_name\": \"${CASE_NAME}\",\n"
            "  \"expected_output\": \"${EXPECTED_OUTPUT_FILE}\",\n"
            "  \"actual_output\": \"${ACTUAL_OUTPUT_FILE}\",\n"
            "  \"first_differing_line_1based\": ${_first_diff_1},\n"
            "  \"expected_line\": \"${_first_expected_line}\",\n"
            "  \"actual_line\": \"${_first_actual_line}\"\n"
            "}\n"
        )

        message(FATAL_ERROR "Case '${CASE_NAME}' failed: output mesh differs from expected\nExpected: ${EXPECTED_OUTPUT_FILE}\nActual:   ${ACTUAL_OUTPUT_FILE}\nFirst differing line (1-based): ${_first_diff_1}\nExpected line: '${_first_expected_line}'\nActual   line: '${_first_actual_line}'\nFailure details written to: ${STATS_FILE}")
    else()
        # No differing line found within common range — probably different sizes or binary difference.
        # Write minimal stats when sizes differ
        file(WRITE "${STATS_FILE}"
            "{\n"
            "  \"case_name\": \"${CASE_NAME}\",\n"
            "  \"expected_lines\": ${_expected_count},\n"
            "  \"actual_lines\": ${_actual_count},\n"
            "  \"expected\": \"${EXPECTED_OUTPUT_FILE}\",\n"
            "  \"actual\": \"${ACTUAL_OUTPUT_FILE}\"\n"
            "}\n"
        )
        message(FATAL_ERROR "Case '${CASE_NAME}' failed: output mesh differs from expected\nExpected: ${EXPECTED_OUTPUT_FILE}\nActual:   ${ACTUAL_OUTPUT_FILE}\nExpected lines: ${_expected_count}\nActual lines:   ${_actual_count}\nEnriched stats written to: ${STATS_FILE}")
    endif()
endif()

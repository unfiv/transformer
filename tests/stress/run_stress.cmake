# run_stress.cmake
# Usage: cmake -DCASE_DIR=... -DCASE_NAME=... -DTRANSFORMER_BIN=... -DPARAMS_FILE=... -DRUNS=100 -P run_stress.cmake

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
    else()
        message(FATAL_ERROR "Unknown key '${key}' in params file: ${PARAMS_FILE}")
    endif()
endforeach()

set(CASE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/stress/${CASE_NAME}")
file(MAKE_DIRECTORY "${CASE_OUTPUT_DIR}")

set(OUTPUT_BASE "${CASE_OUTPUT_DIR}/result_mesh")
set(RUN_STATS_DIR "${CASE_OUTPUT_DIR}/stats")
file(MAKE_DIRECTORY "${RUN_STATS_DIR}")

# Record stress run start time
string(TIMESTAMP _stress_start_iso "%Y-%m-%dT%H:%M:%S%z")
string(TIMESTAMP _stress_start_epoch "%s")

# Lists to collect per-run data
set(_durations)
set(_stats_raw_list)

# Run loop
math(EXPR _last_run "${RUNS} - 1")
foreach(_i RANGE 0 ${_last_run})
    math(EXPR _run_index "${_i} + 1")
    set(_actual_output "${OUTPUT_BASE}_${_run_index}.obj")
    set(_run_stats_file "${RUN_STATS_DIR}/stats_${_run_index}.json")
    message(STATUS "[stress] Run ${_run_index}/${RUNS}: invoking transformer...")

    # Run transformer - reuse same arguments as integration
    execute_process(
        COMMAND "${TRANSFORMER_BIN}"
            "${MESH_FILE}"
            "${WEIGHTS_FILE}"
            "${INVERSE_BIND_POSE_FILE}"
            "${NEW_POSE_FILE}"
            "${_actual_output}"
            "${_run_stats_file}"
        RESULT_VARIABLE _app_code
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(NOT _app_code EQUAL 0)
        message(FATAL_ERROR "Transformer exited with code ${_app_code} on run ${_run_index}")
    endif()

    # Try to extract total milliseconds from run stats file produced by transformer
    set(_duration_val "")
    if(EXISTS "${_run_stats_file}")
        file(READ "${_run_stats_file}" _run_stats_raw)
        # Trim
        string(REGEX REPLACE "^[ \t\r\n]+" "" _run_stats_trim "${_run_stats_raw}")
        string(REGEX REPLACE "[ \t\r\n]+$" "" _run_stats_trim "${_run_stats_trim}")
        # Find "stage" : "total" position
        string(FIND "${_run_stats_trim}" "\"stage\"" _stage_pos)
        if(NOT _stage_pos EQUAL -1)
            string(FIND "${_run_stats_trim}" "\"total\"" _total_pos)
            if(NOT _total_pos EQUAL -1)
                # Extract tail from total_pos
                math(EXPR _tail_start "${_total_pos} - 20")
                if(_tail_start LESS 0)
                    set(_tail_start 0)
                endif()
                string(SUBSTRING "${_run_stats_trim}" ${_tail_start} -1 _tail)
                # Try to extract milliseconds value
                string(REGEX REPLACE ".*\"milliseconds\"[ \t]*:[ \t]*([0-9]+\\.?[0-9]*).*" "\\1" _ms_candidate "${_tail}")
                # Validate candidate: must be numeric
                string(REGEX MATCH "^[0-9]+(\.[0-9]+)?$" _is_num "${_ms_candidate}")
                if(_is_num)
                    set(_duration_val "${_ms_candidate}")
                endif()
            endif()
        endif()

        # store raw stats content
        list(APPEND _stats_raw_list "${_run_stats_trim}")
    else()
        list(APPEND _stats_raw_list "")
    endif()

    # If couldn't parse duration from transformer stats, fallback to file timestamps (seconds)
    if(_duration_val STREQUAL "")
        # fallback: use file timestamp difference if expected output exists
        if(EXISTS "${_actual_output}")
            file(TIMESTAMP "${_actual_output}" _ts_mod FORMAT "%s")
            # We don't have start timestamp per-run; set duration unknown
            set(_duration_val "0")
        else()
            set(_duration_val "0")
        endif()
    endif()

    list(APPEND _durations "${_duration_val}")
    message(STATUS "[stress] Run ${_run_index} duration (ms): ${_duration_val}")
endforeach()

# Compute aggregate statistics: min, max, median
list(LENGTH _durations _n)
if(_n EQUAL 0)
    message(FATAL_ERROR "No runs recorded")
endif()

# Record stress run end time
string(TIMESTAMP _stress_end_iso "%Y-%m-%dT%H:%M:%S%z")
string(TIMESTAMP _stress_end_epoch "%s")
math(EXPR _stress_duration_seconds "${_stress_end_epoch} - ${_stress_start_epoch}")

# Gather system info (processor count, cpu model, total memory)
include(ProcessorCount)
ProcessorCount(_proc_count)
set(_cpu_model "unknown")
set(_total_memory "unknown")
set(_cpu_frequency "System-specific call is not implemented")
set(_memory_type "System-specific call is not implemented")
if(WIN32)
    execute_process(COMMAND powershell -NoProfile -Command "(Get-CimInstance -ClassName Win32_ComputerSystem).TotalPhysicalMemory" OUTPUT_VARIABLE _tmp_mem OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_tmp_mem)
        set(_total_memory "${_tmp_mem}")
    endif()
    execute_process(COMMAND powershell -NoProfile -Command "(Get-CimInstance -ClassName Win32_Processor | Select-Object -First 1 -ExpandProperty Name)" OUTPUT_VARIABLE _tmp_cpu OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_tmp_cpu)
        set(_cpu_model "${_tmp_cpu}")
    endif()
    # CPU frequency (MHz)
    execute_process(COMMAND powershell -NoProfile -Command "(Get-CimInstance -ClassName Win32_Processor | Select-Object -First 1 -ExpandProperty MaxClockSpeed)" OUTPUT_VARIABLE _tmp_freq OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_tmp_freq)
        # MaxClockSpeed is in MHz
        math(EXPR _freq_hz "${_tmp_freq} * 1000000")
        set(_cpu_frequency "${_freq_hz}")
    endif()
    # Memory type (numeric code) and speed if available
    execute_process(COMMAND powershell -NoProfile -Command "(Get-CimInstance -ClassName Win32_PhysicalMemory | Select-Object -First 1 -Property MemoryType,Speed | ConvertTo-Json -Compress)" OUTPUT_VARIABLE _tmp_meminfo OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_tmp_meminfo)
        string(REPLACE "\"" "\\\"" _tmp_meminfo_escaped "${_tmp_meminfo}")
        set(_memory_type "${_tmp_meminfo_escaped}")
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
    # CPU frequency
    execute_process(COMMAND sysctl -n hw.cpufrequency OUTPUT_VARIABLE _tmp_freq OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_tmp_freq)
        set(_cpu_frequency "${_tmp_freq}")
    endif()
    # Memory type: not generally available via sysctl
    set(_memory_type "System-specific call is not implemented")
elseif(UNIX)
    if(EXISTS "/proc/meminfo")
        execute_process(COMMAND bash -c "grep MemTotal /proc/meminfo | awk '{print $2}'" OUTPUT_VARIABLE _tmp_mem OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_tmp_mem)
            math(EXPR _tmp_bytes "${_tmp_mem} * 1024")
            set(_total_memory "${_tmp_bytes}")
        endif()
    endif()
    if(EXISTS "/proc/cpuinfo")
        execute_process(COMMAND bash -c "grep 'model name' /proc/cpuinfo | head -n1 | cut -d: -f2 | sed 's/^\\s*//'" OUTPUT_VARIABLE _tmp_cpu OUTPUT_STRIP_TRAILING_WHITESPACE)
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

# App version (try git describe)
set(_app_version "unknown")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" describe --tags --always OUTPUT_VARIABLE _app_version OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# Prefer explicit version from CMake target invocation; fallback to git describe value.
set(_build_version "${TRANSFORMER_VERSION}")
if(_build_version STREQUAL "")
    set(_build_version "${_app_version}")
endif()

# Prefer per-build config for multi-config generators, then CMAKE_BUILD_TYPE.
set(_effective_build_type "${TRANSFORMER_BUILD_CONFIG}")
if(_effective_build_type STREQUAL "")
    set(_effective_build_type "${TRANSFORMER_BUILD_TYPE}")
endif()
if(_effective_build_type STREQUAL "")
    set(_effective_build_type "${CMAKE_BUILD_TYPE}")
endif()
if(_effective_build_type STREQUAL "")
    set(_effective_build_type "unknown")
endif()

# Build sorted index list by insertion sort (stable)
set(_sorted_indices)
math(EXPR _n_minus1 "${_n} - 1")
foreach(_i RANGE 0 ${_n_minus1})
    list(GET _durations ${_i} _val)
    list(LENGTH _sorted_indices _sorted_len)
    if(_sorted_len EQUAL 0)
        list(APPEND _sorted_indices ${_i})
    else()
        set(_inserted FALSE)
        set(_pos 0)
        while(_pos LESS _sorted_len)
            list(GET _sorted_indices ${_pos} _sidx)
            list(GET _durations ${_sidx} _sval)
            if(_val LESS _sval)
                list(INSERT _sorted_indices ${_pos} ${_i})
                set(_inserted TRUE)
                break()
            endif()
            math(EXPR _pos "${_pos} + 1")
        endwhile()
        if(NOT _inserted)
            list(APPEND _sorted_indices ${_i})
        endif()
    endif()
endforeach()

# Extract indices
list(GET _sorted_indices 0 _fast_idx)
math(EXPR _last_pos "${_n} - 1")
list(GET _sorted_indices ${_last_pos} _slow_idx)
math(EXPR _median_pos "${_n} / 2")
list(GET _sorted_indices ${_median_pos} _median_idx)

list(GET _durations ${_fast_idx} _fast_val)
list(GET _durations ${_slow_idx} _slow_val)
list(GET _durations ${_median_idx} _median_val)

# Helper to prepare transformer_stats subsections
function(_prepare_stats_entry out_var raw_str)
    # raw_str is trimmed
    string(SUBSTRING "${raw_str}" 0 1 _first)
    if(_first STREQUAL "{" OR _first STREQUAL "[")
        set(${out_var} "${raw_str}" PARENT_SCOPE)
    else()
        # escape
        string(REPLACE "\"" "\\\"" _tmp "${raw_str}")
        string(REPLACE "\n" "\\n" _tmp "${_tmp}")
        set(${out_var} "\"${_tmp}\"" PARENT_SCOPE)
    endif()
endfunction()

list(GET _stats_raw_list ${_fast_idx} _fast_raw)
list(GET _stats_raw_list ${_slow_idx} _slow_raw)
list(GET _stats_raw_list ${_median_idx} _median_raw)

_prepare_stats_entry(_fast_stats _fast_raw)
_prepare_stats_entry(_slow_stats _slow_raw)
_prepare_stats_entry(_median_stats _median_raw)

# Write aggregate JSON

set(AGGREGATE_FILE "${CASE_OUTPUT_DIR}/stress_stats.json")

# Parse per-run transformer stats using pure CMake and compute per-stage averages (scaled integers)
# We'll scale milliseconds by 1000 to preserve three decimal places as integers.

function(_scale_ms out val)
    # val is like 123.456 or 123
    string(FIND "${val}" "." _pos)
    if(_pos EQUAL -1)
        set(_int "${val}")
        set(_frac "000")
    else()
        string(SUBSTRING "${val}" 0 ${_pos} _int)
        math(EXPR _start "${_pos} + 1")
        string(SUBSTRING "${val}" ${_start} -1 _frac_raw)
        string(LENGTH "${_frac_raw}" _flen)
        if(_flen GREATER 3)
            string(SUBSTRING "${_frac_raw}" 0 3 _frac)
        else()
            set(_frac "${_frac_raw}")
            while(_flen LESS 3)
                set(_frac "${_frac}0")
                math(EXPR _flen "${_flen} + 1")
            endwhile()
        endif()
    endif()
    if(_int STREQUAL "")
        set(_int 0)
    endif()
    math(EXPR _scaled "${_int} * 1000 + ${_frac}")
    set(${out} "${_scaled}" PARENT_SCOPE)
endfunction()

function(_format_scaled out scaled)
    math(EXPR _intpart "${scaled} / 1000")
    math(EXPR _rem "${scaled} % 1000")
    if(_rem LESS 10)
        set(_rem_str "00${_rem}")
    elseif(_rem LESS 100)
        set(_rem_str "0${_rem}")
    else()
        set(_rem_str "${_rem}")
    endif()
    set(${out} "${_intpart}.${_rem_str}" PARENT_SCOPE)
endfunction()

set(STAGE_NAMES)
set(STAGE_SUMS)
set(STAGE_COUNTS)
set(TOTALS_SCALED)

list(LENGTH _stats_raw_list _runs_count)
math(EXPR _last_stat "${_runs_count} - 1")
foreach(_idx RANGE 0 ${_last_stat})
    # get item safely
    list(GET _stats_raw_list ${_idx} _raw)
    if("${_raw}" STREQUAL "")
        continue()
    endif()
    # Try to extract the contents of the "stages" array first; if missing, fall back to matching objects containing "stage"
    set(_stages_content "")
    string(FIND "${_raw}" "\"stages\"" _pos_stage)
    if(NOT _pos_stage EQUAL -1)
        math(EXPR _after_stage "${_pos_stage} + 8")
        string(SUBSTRING "${_raw}" ${_after_stage} -1 _after)
        string(FIND "${_after}" "[" _pos_lb)
        if(NOT _pos_lb EQUAL -1)
            math(EXPR _lb_pos "${_after_stage} + ${_pos_lb}")
            string(SUBSTRING "${_raw}" ${_lb_pos} -1 _from_lb)
            string(FIND "${_from_lb}" "]" _pos_rb)
            if(NOT _pos_rb EQUAL -1)
                math(EXPR _content_len "${_pos_rb} - 1")
                if(_content_len GREATER 0)
                    string(SUBSTRING "${_from_lb}" 1 ${_content_len} _stages_content)
                else()
                    set(_stages_content "")
                endif()
            endif()
        endif()
    endif()
    if(_stages_content STREQUAL "")
        # no stages array found; fallback to matching objects with "stage" anywhere
        string(REGEX MATCHALL "\\{[^\\}]*\"stage\"[^\\}]*\\}" _entries "${_raw}")
    else()
        # match each object inside the extracted stages content
        string(REGEX MATCHALL "\\{[^\\}]*\\}" _entries "${_stages_content}")
    endif()
    foreach(_entry IN LISTS _entries)
        # extract stage name
        string(REGEX REPLACE ".*\"stage\"[ \t]*:[ \t]*\"([^\"]+)\".*" "\\1" _sname "${_entry}")
        # extract milliseconds
        string(REGEX REPLACE ".*\"milliseconds\"[ \t]*:[ \t]*([0-9]+\.?[0-9]*).*" "\\1" _ms "${_entry}")
        if(NOT _ms STREQUAL "")
            _scale_ms(_ms_scaled "${_ms}")
            # find index of stage
            list(FIND STAGE_NAMES "${_sname}" _found)
            if(_found EQUAL -1)
                list(APPEND STAGE_NAMES "${_sname}")
                list(APPEND STAGE_SUMS "${_ms_scaled}")
                list(APPEND STAGE_COUNTS "1")
            else()
                list(GET STAGE_SUMS ${_found} _cur_sum)
                math(EXPR _new_sum "${_cur_sum} + ${_ms_scaled}")
                list(REMOVE_AT STAGE_SUMS ${_found})
                list(INSERT STAGE_SUMS ${_found} ${_new_sum})
                list(GET STAGE_COUNTS ${_found} _cur_cnt)
                math(EXPR _new_cnt "${_cur_cnt} + 1")
                list(REMOVE_AT STAGE_COUNTS ${_found})
                list(INSERT STAGE_COUNTS ${_found} ${_new_cnt})
            endif()
        endif()
    endforeach()
    # record total from _durations if numeric and >0
    list(GET _durations ${_idx} _dval)
    if(NOT _dval STREQUAL "" AND NOT _dval STREQUAL "0")
        _scale_ms(_d_scaled "${_dval}")
        list(APPEND TOTALS_SCALED ${_d_scaled})
    endif()
endforeach()

# compute averages per stage and total_mean
set(_avg_stages_json "[")
list(LENGTH STAGE_NAMES _slen)
math(EXPR _slast "${_slen} - 1")
if(_slen GREATER 0)
    foreach(_si RANGE 0 ${_slast})
        list(GET STAGE_NAMES ${_si} _sname)
        list(GET STAGE_SUMS ${_si} _ssum)
        list(GET STAGE_COUNTS ${_si} _scnt)
        if(NOT _scnt STREQUAL "0")
            math(EXPR _avg_scaled "${_ssum} / ${_scnt}")
        else()
            set(_avg_scaled 0)
        endif()
        _format_scaled(_avg_str ${_avg_scaled})
        if(NOT _si EQUAL 0)
            set(_avg_stages_json "${_avg_stages_json}, ")
        endif()
        set(_avg_stages_json "${_avg_stages_json}{ \"stage\": \"${_sname}\", \"milliseconds\": ${_avg_str} }")
    endforeach()
endif()
set(_avg_stages_json "${_avg_stages_json}]")

# compute total mean
list(LENGTH TOTALS_SCALED _tlen)
if(_tlen GREATER 0)
    set(_tot_sum 0)
    foreach(_tv IN LISTS TOTALS_SCALED)
        math(EXPR _tot_sum "${_tot_sum} + ${_tv}")
    endforeach()
    math(EXPR _total_mean_scaled "${_tot_sum} / ${_tlen}")
    _format_scaled(_total_mean_str ${_total_mean_scaled})
else()
    set(_total_mean_str "null")
endif()

# Find fastest and slowest run indices (use previously computed _fast_idx/_slow_idx)
list(GET _stats_raw_list ${_fast_idx} _fast_raw)
list(GET _stats_raw_list ${_slow_idx} _slow_raw)

# Build transformer aggregate JSON string with pretty indentation
# Indent fastest/slowest raw JSON blocks by 4 spaces per line
set(_fast_indented "null")
set(_slow_indented "null")
if(NOT _fast_raw STREQUAL "")
    string(REPLACE "\n" "\n    " _fast_indented "${_fast_raw}")
    string(REPLACE "\n" "\n    " _slow_indented "${_slow_raw}")
endif()

# Pretty-format average stages array (one item per line, indented)
string(SUBSTRING "${_avg_stages_json}" 1 -1 _avg_inner)
string(REPLACE "}, " "},\n      " _avg_inner_pretty "${_avg_inner}")
set(_avg_stages_pretty "[\n      ${_avg_inner_pretty}\n    ]")

if(NOT _fast_raw STREQUAL "")
    set(_transformer_aggregate_json "{\n  \"fastest\": ${_fast_indented},\n  \"slowest\": ${_slow_indented},\n  \"average\": {\n    \"stages\": ${_avg_stages_pretty},\n    \"total_mean\": ${_total_mean_str}\n  }\n}")
else()
    set(_transformer_aggregate_json "{\n  \"fastest\": null,\n  \"slowest\": null,\n  \"average\": {\n    \"stages\": ${_avg_stages_pretty},\n    \"total_mean\": ${_total_mean_str}\n  }\n}")
endif()

# Write main aggregate JSON
file(WRITE "${AGGREGATE_FILE}"
    "{\n"
    "  \"case_name\": \"${CASE_NAME}\",\n"
    "  \"case_dir\": \"${CASE_DIR}\",\n"
    "  \"inputs\": {\n"
    "    \"mesh\": \"${MESH_FILE}\",\n"
    "    \"weights\": \"${WEIGHTS_FILE}\",\n"
    "    \"inverse_bind_pose\": \"${INVERSE_BIND_POSE_FILE}\",\n"
    "    \"new_pose\": \"${NEW_POSE_FILE}\"\n"
    "  },\n"
    "  \"runs\": ${RUNS},\n"
    "  \"timing_summary_ms\": {\n"
    "    \"fastest\": ${_fast_val},\n"
    "    \"slowest\": ${_slow_val},\n"
    "    \"median\": ${_median_val}\n"
    "  },\n"
    "  \"timing\": {\n"
    "    \"start\": \"${_stress_start_iso}\",\n"
    "    \"end\": \"${_stress_end_iso}\",\n"
    "    \"duration_seconds\": ${_stress_duration_seconds}\n"
    "  },\n"
    "  \"environment\": {\n"
    "    \"system\": \"${CMAKE_SYSTEM_NAME}\",\n"
    "    \"host_arch\": \"${CMAKE_HOST_SYSTEM_PROCESSOR}\",\n"
    "    \"processor_count\": ${_proc_count},\n"
    "    \"cpu_model\": \"${_cpu_model}\",\n"
    "    \"cpu_frequency_hz\": \"${_cpu_frequency}\",\n"
    "    \"total_memory_bytes\": \"${_total_memory}\",\n"
    "    \"memory_type\": \"${_memory_type}\"\n"
    "  },\n"
    "  \"build\": {\n"
    "    \"cmake_version\": \"${CMAKE_VERSION}\",\n"
    "    \"cxx_compiler\": \"${CMAKE_CXX_COMPILER}\",\n"
    "    \"cxx_id\": \"${CMAKE_CXX_COMPILER_ID}\",\n"
    "    \"build_type\": \"${_effective_build_type}\",\n"
    "    \"version\": \"${_build_version}\"\n"
    "  },\n"
    "  \"app_version\": \"${_app_version}\",\n"
)
# append durations array (single-line, no trailing comma)
string(REPLACE ";" ", " _durations_joined "${_durations}")
file(APPEND "${AGGREGATE_FILE}" "  \"durations_ms\": [${_durations_joined}],\n  \"transformer_stats\": " )
if(_transformer_aggregate_json)
    # embed directly
    file(APPEND "${AGGREGATE_FILE}" "${_transformer_aggregate_json}\n")
else()
    file(APPEND "${AGGREGATE_FILE}" "{\n    \"fastest\": null,\n    \"slowest\": null,\n    \"average\": null\n  }\n")
endif()
file(APPEND "${AGGREGATE_FILE}" "}\n")

message(STATUS "Stress run complete. Aggregate stats: ${AGGREGATE_FILE}")

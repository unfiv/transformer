cmake_minimum_required(VERSION 3.27)

# Stress test runner - runs transformer multiple times and records aggregated stats.
# Usage: build with CMake then run the 'stress' target (e.g., cmake --build . --target stress)

if(NOT DEFINED STRESS_CASE_DIR)
    set(STRESS_CASE_DIR "${CMAKE_SOURCE_DIR}/tests/integration/cases/basic")
endif()
if(NOT DEFINED STRESS_CASE_NAME)
    set(STRESS_CASE_NAME "basic")
endif()
if(NOT DEFINED STRESS_PARAMS_FILE)
    set(STRESS_PARAMS_FILE "${STRESS_CASE_DIR}/params.txt")
endif()
if(NOT DEFINED STRESS_RUNS)
    set(STRESS_RUNS 100)
endif()

add_custom_target(stress
    COMMAND ${CMAKE_COMMAND}
        -DCASE_DIR=${STRESS_CASE_DIR}
        -DCASE_NAME=${STRESS_CASE_NAME}
        -DTRANSFORMER_BIN=$<TARGET_FILE:transformer>
        -DTRANSFORMER_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DTRANSFORMER_BUILD_CONFIG=$<CONFIG>
        -DTRANSFORMER_VERSION=${PROJECT_VERSION}
        -DPARAMS_FILE=${STRESS_PARAMS_FILE}
        -DRUNS=${STRESS_RUNS}
        -P ${CMAKE_SOURCE_DIR}/tests/stress/run_stress.cmake
    DEPENDS transformer
    COMMENT "Run stress test (runs transformer multiple times and collects aggregated stats)"
)

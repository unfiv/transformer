cmake_minimum_required(VERSION 3.27)
project(transformer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Environment policy checks -------------------------------------------------
# Enforce recommended generator, build type and compiler/toolchain to keep
# developer environments consistent. These checks are helpful for CI and
# reproducible benchmarking. Developers can override behavior by setting
# -DENFORCE_ENV=OFF when invoking CMake if absolutely necessary.
if(NOT DEFINED ENFORCE_ENV)
    set(ENFORCE_ENV ON)
endif()

if(ENFORCE_ENV)
    # Generator: require Ninja for consistent, fast builds. If you need to use
    # Visual Studio or another generator temporarily, set -DENFORCE_ENV=OFF.
    if(NOT CMAKE_GENERATOR MATCHES "Ninja")
        message(FATAL_ERROR "This project expects the Ninja generator. Configure with: cmake -G Ninja -S . -B build\nDetected generator: ${CMAKE_GENERATOR}\nIf you must use a different generator, reconfigure with -DENFORCE_ENV=OFF to bypass this check.")
    endif()

    # Build type: require explicit Debug or Release for single-config generators
    if(NOT DEFINED CMAKE_CONFIGURATION_TYPES)
        if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
            message(FATAL_ERROR "CMAKE_BUILD_TYPE is required for single-config generators. Configure with -DCMAKE_BUILD_TYPE=Release or -DCMAKE_BUILD_TYPE=Debug")
        endif()
        if(NOT (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
            message(WARNING "Unusual CMAKE_BUILD_TYPE='${CMAKE_BUILD_TYPE}'. Recommended: Release or Debug.")
        endif()
    else()
        # multi-config generators (e.g., Visual Studio) choose configuration at build time
        message(STATUS "Multi-config generator detected. Remember to pass --config Release when building.")
    endif()

    # Compiler: require a supported toolchain. On Windows prefer MSVC; on other
    # platforms accept GCC/Clang. This keeps behaviour consistent for perf runs.
    if(WIN32)
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            message(FATAL_ERROR "On Windows, MSVC toolchain is required for this project. Detected: ${CMAKE_CXX_COMPILER_ID}")
        endif()
    else()
        if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
            message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}. Supported: GNU or Clang.")
        endif()
    endif()

    # Toolchain file: if set, ensure the file exists and warn if not set.
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_TOOLCHAIN_FILE STREQUAL "")
        if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
            message(FATAL_ERROR "CMAKE_TOOLCHAIN_FILE is set to '${CMAKE_TOOLCHAIN_FILE}' but the file does not exist.")
        else()
            message(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
        endif()
    else()
        message(STATUS "No CMake toolchain file in use (CMAKE_TOOLCHAIN_FILE is not set).")
    endif()
endif()

# ------------------------------------------------------------------------------

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.hpp)

# Formatter
find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format.exe)
# Check for repository clang-format config to provide a better message
set(_clang_config_exists FALSE)
if(EXISTS "${CMAKE_SOURCE_DIR}/.clang-format" OR EXISTS "${CMAKE_SOURCE_DIR}/.clang-format.lua")
    set(_clang_config_exists TRUE)
endif()
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${SOURCES}
        COMMENT "Formatting all source files in src/"
    )
else()
    if(_clang_config_exists)
        message(WARNING "Found '.clang-format' in repository but 'clang-format' executable was not found in PATH. Install clang-format or set CLANG_FORMAT_EXE to its path to enable the 'format' target.")
    else()
        message(WARNING "clang-format not found. Target 'format' will not be available.")
    endif()
endif()

# Optional: add stress tests (separate track, custom target)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/stress/CMakeLists.txt")
    add_subdirectory(tests/stress)
endif()

# Executable
add_executable(transformer ${SOURCES})

target_include_directories(transformer PUBLIC src/)

set_target_properties(transformer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT transformer)

set(ASSETS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(EXISTS "${ASSETS_SOURCE_DIR}")
    # Keep assets near executable for local runs.
    add_custom_target(copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASSETS_SOURCE_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMENT "Copying assets to output directory"
    )

    add_dependencies(copy_assets ${PROJECT_NAME})

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSETS_SOURCE_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMENT "Post-build: Synchronizing assets to output directory"
        VERBATIM
    )
else()
    message(STATUS "assets directory not found; skipping asset copy targets")
endif()

enable_testing()

set(INTEGRATION_CASES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/cases")
set(INTEGRATION_RUNNER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/run_case.cmake")

if(EXISTS "${INTEGRATION_CASES_DIR}")
    file(GLOB INTEGRATION_CASE_DIRS LIST_DIRECTORIES true "${INTEGRATION_CASES_DIR}/*")

    foreach(case_dir IN LISTS INTEGRATION_CASE_DIRS)
        if(NOT IS_DIRECTORY "${case_dir}")
            continue()
        endif()

        get_filename_component(case_name "${case_dir}" NAME)
        set(params_file "${case_dir}/params.txt")

        if(NOT EXISTS "${params_file}")
            message(STATUS "Skipping integration case '${case_name}': params.txt not found")
            continue()
        endif()

        add_test(
            NAME integration.${case_name}
            COMMAND "${CMAKE_COMMAND}"
                -DCASE_NAME=${case_name}
                -DCASE_DIR=${case_dir}
                -DTRANSFORMER_BIN=$<TARGET_FILE:transformer>
                -DPARAMS_FILE=${params_file}
                -P "${INTEGRATION_RUNNER_SCRIPT}"
        )
    endforeach()
endif()

cmake_minimum_required(VERSION 3.27)
project(transformer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.hpp)

# Formatter
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${SOURCES}
        COMMENT "Formatting all source files in src/"
    )
else()
    message(WARNING "clang-format not found. Target 'format' will not be available.")
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

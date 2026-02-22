set(INTEGRATION_CASES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cases")
set(INTEGRATION_RUNNER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/run_case.cmake")

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
                -DOUTPUT_ROOT_DIR=${CMAKE_BINARY_DIR}/tests/integration
                -P "${INTEGRATION_RUNNER_SCRIPT}"
        )
    endforeach()
endif()

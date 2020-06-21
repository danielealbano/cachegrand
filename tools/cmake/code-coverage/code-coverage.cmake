function(target_add_code_coverage TARGET)
    get_target_property(
            CODECOVERAGE_TARGET_BINARY_DIRECTORY
            ${TARGET}
            BINARY_DIR)

    set(CODECOVERAGE_TARGET_NAME "generate-${TARGET}-code-coverage")
    set(CODECOVERAGE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/code-coverage")

    add_custom_target(
            ${CODECOVERAGE_TARGET_NAME}
            COMMAND test -d code-coverage && rm -rf code-coverage
            COMMAND mkdir -p code-coverage
            COMMAND ${PROJECT_SOURCE_DIR}/tools/code_coverage_cmake_support/code_coverage_cmake_support.sh -o ${CODECOVERAGE_TARGET_BINARY_DIRECTORY} -c ${CODECOVERAGE_OUTPUT_PATH}
            COMMAND echo "-- Code coverage files have been output to ${CODECOVERAGE_OUTPUT_PATH}"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endfunction()

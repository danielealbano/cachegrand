file(GLOB SRC_FILES_T1HA "t1ha/src/*.c")
list(FILTER SRC_FILES_T1HA EXCLUDE REGEX ".*t1ha0_ia32aes_[a-z0-9]+\\.c$")

message(STATUS "Checking accelerated implementations for t1ha0")

if(ARCH_IS_X86_64)
        message(STATUS "Enabling accelerated t1ha0 -- aes-ni + avx2")
        list(APPEND SRC_FILES_T1HA "${CMAKE_CURRENT_SOURCE_DIR}/t1ha/src/t1ha0_ia32aes_avx2.c")
        set_source_files_properties(
                "t1ha/src/t1ha0_ia32aes_avx2.c"
                PROPERTIES COMPILE_FLAGS
                "-mno-avx256-split-unaligned-load -maes -mavx2 -mbmi -mfma -mtune=haswell")

        message(STATUS "Enabling accelerated t1ha0 -- aes-ni + avx")
        list(APPEND SRC_FILES_T1HA "${CMAKE_CURRENT_SOURCE_DIR}/t1ha/src/t1ha0_ia32aes_avx.c")
        set_source_files_properties(
                "t1ha/src/t1ha0_ia32aes_avx.c"
                PROPERTIES COMPILE_FLAGS
                "-mno-avx256-split-unaligned-load -mno-avx2 -mavx -mbmi -maes")

        message(STATUS "Enabling accelerated t1ha0 -- aes-ni w/o avx w/o avx2")
        list(APPEND SRC_FILES_T1HA "${CMAKE_CURRENT_SOURCE_DIR}/t1ha/src/t1ha0_ia32aes_noavx.c")
        set_source_files_properties(
                "t1ha/src/t1ha0_ia32aes_noavx.c"
                PROPERTIES COMPILE_FLAGS
                "-mno-avx256-split-unaligned-load -mno-avx2 -mno-avx -maes")

elseif(ARCH_IS_AARCH64)
        message(STATUS "No accelerated implementation for AARCH64, using portable implementation")
else()
        message(FATAL_ERROR "Unsupported architecture")
endif()

add_library(
        t1ha
        ${SRC_FILES_T1HA})

target_compile_options(
        t1ha
        PRIVATE
        -ffunction-sections -std=c99 -O3 -D_DEFAULT_SOURCE -DNDEBUG -fno-stack-protector -fvisibility=hidden)

target_compile_definitions(
        t1ha
        PUBLIC
        -DT1HA0_RUNTIME_SELECT=1 -DT1HA_USE_INDIRECT_FUNCTIONS=1 -Dt1ha_EXPORTS)

target_include_directories(
        t1ha
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/t1ha)

list(APPEND DEPS_LIST_LIBRARIES "t1ha")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/t1ha")

# Build t1ha2
add_library(
        t1ha2
        t1ha/src/t1ha2.c
        )

target_include_directories(
        t1ha2
        PUBLIC
        t1ha/
        t1ha/src
        )

target_compile_options(
        t1ha2
        PUBLIC
        -ffunction-sections -std=c99 -O3 -DNDEBUG -D_DEFAULT_SOURCE -fno-stack-protector -mtune=native -fvisibility=hidden -Dt1ha_EXPORTS
        )

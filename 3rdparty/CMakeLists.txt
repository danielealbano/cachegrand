# include_directories(.)

add_library(
        t1ha
        t1ha/src/t1ha2.c
        )

target_include_directories(
        t1ha
        PUBLIC
        t1ha/
        t1ha/src
        )

target_compile_options(
        t1ha
        PUBLIC
        -ffunction-sections -std=c99 -O3 -DNDEBUG -D_DEFAULT_SOURCE -fno-stack-protector -mtune=native -fvisibility=hidden -Dt1ha_EXPORTS
        )

# Enables a number of GCC flags to improve the overall security of the compiled binaries trying both to catch odd
# behaviours and reducing the risks of unnoticed stack overflows
# https://airbus-seclab.github.io/c-compiler-security/gcc_compilation.html

set(COMPILER_OPTIONS
        "-Wall"; "-Wextra"; "-Wpedantic"; "-Wformat=2"; "-Wformat-overflow=2"; "-Wformat-truncation=2";
        "-Wformat-security"; "-Wnull-dereference"; "-Wstack-protector"; "-Wtrampolines"; "-Walloca"; "-Wvla";
        "-Warray-bounds=2"; "-Wimplicit-fallthrough=3"; "-Wtraditional-conversion"; "-Wshift-overflow=2";
        "-Wcast-qual"; "-Wstringop-overflow=4"; "-Wconversion"; "-Warith-conversion"; "-Wlogical-op";
        "-Wduplicated-cond"; "-Wduplicated-branches"; "-Wformat-signedness"; "-Wshadow"; "-Wstrict-overflow=4";
        "-Wundef"; "-Wstrict-prototypes"; "-Wswitch-default"; "-Wswitch-enum"; "-Wstack-usage=1000000";
        "-Wcast-align=strict";
        "-D_FORTIFY_SOURCE=2";
        "-fstack-protector-strong"; "-fstack-clash-protection"; "-fPIE";
        "-Wl,-z,relro"; "-Wl,-z,now"; "-Wl,-z,noexecstack"; "-Wl,-z,separate-code";
        )
foreach(OPTION IN LISTS COMPILER_OPTIONS)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:${OPTION}>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${OPTION}>)
endforeach()

# If -fPIE is supported enables it
include(CheckPIESupported)
check_pie_supported()

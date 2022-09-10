file(GLOB_RECURSE SRC_FILES_ANALYZER "*.c")
list(REMOVE_ITEM SRC_FILES_ANALYZER analyzer.c)

##################################################
# Build the executable
##################################################
add_executable(
        redis-commands-analyzer
        analyzer.c
        ${SRC_FILES_ANALYZER})

set_target_properties(
        redis-commands-analyzer
        PROPERTIES
        LINKER_LANGUAGE C)
#!/bin/bash
set -eu

TEMP_LCOV_PATH=~/.lcov/

mkdir -p $TEMP_LCOV_PATH

# Generate the initial lcov version and strips out anything related to /usr
/usr/bin/lcov --gcov-tool $INPUT_GCOV_PATH -c -d . -o $TEMP_LCOV_PATH/info
/usr/bin/lcov --gcov-tool $INPUT_GCOV_PATH -r $TEMP_LCOV_PATH/info '/usr/*' -o $TEMP_LCOV_PATH/filtered

if [ -n "$INPUT_REMOVE_PATTERNS" ];
then
    # Strips out the requested paths
    IFS=, read -ra REMOVE_PATTERNS_ARR <<< "$INPUT_REMOVE_PATTERNS"
    for REMOVE_PATTERN in "${REMOVE_PATTERNS_ARR[@]}"
    do
        /usr/bin/lcov --gcov-tool $INPUT_GCOV_PATH -r $TEMP_LCOV_PATH/filtered "*${REMOVE_PATTERN}*" -o $TEMP_LCOV_PATH/filtered
    done
fi

# Logging
/usr/bin/lcov --gcov-tool $INPUT_GCOV_PATH -l $TEMP_LCOV_PATH/filtered

# Copy the filtered lcov output to the requested path
cp $TEMP_LCOV_PATH/filtered $INPUT_OUTPUT_LCOV_INFO

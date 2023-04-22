#!/bin/bash

# Copyright (C) 2018-2023 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

function show_help() {
    echo "Usage: $0 -o <objects root directory> -c <coverage output files directory>" 1>&2;
    exit 0;
}

OPTIND=1

declare -A FILES_TO_SKIP=(
  [cmake_config.c]=1
)

OBJECTS_ROOT_DIRECTORY=""
COVERAGE_OUTPUT_DIRECTORY=""
VERBOSE=0

while getopts "h?vo:c:" opt; do
    case "$opt" in
    h|\?)
        show_help
        ;;
    v)  VERBOSE=1
        ;;
    c)  COVERAGE_OUTPUT_DIRECTORY=$OPTARG
        ;;
    o)  OBJECTS_ROOT_DIRECTORY=$OPTARG
        ;;
    esac
done

shift $((OPTIND-1))

if [ -z "${COVERAGE_OUTPUT_DIRECTORY}" ] || [ -z "${OBJECTS_ROOT_DIRECTORY}" ];
then
    show_help
fi

[[ "${COVERAGE_OUTPUT_DIRECTORY}" != */ ]] && COVERAGE_OUTPUT_DIRECTORY="${COVERAGE_OUTPUT_DIRECTORY}/"
[[ "${OBJECTS_ROOT_DIRECTORY}" != */ ]] && OBJECTS_ROOT_DIRECTORY="${OBJECTS_ROOT_DIRECTORY}/"

echo "Checking in <${OBJECTS_ROOT_DIRECTORY}> for object files"

while IFS= read -r -d '' OBJECT_PATH;
do
    OBJECT_REL_PATH="${OBJECT_PATH:${#OBJECTS_ROOT_DIRECTORY}}"
    OBJECT_FILE=$(basename -- "${OBJECT_PATH}")
    OBJECT_FILE_NAME_C="${OBJECT_FILE%.*}"

    if [[ -n "${FILES_TO_SKIP[$OBJECT_FILE_NAME_C]}" ]];
    then
      echo "- skipping <${OBJECT_REL_PATH}>"
      continue
    fi

    OBJECT_SOURCE_PATH=$(strings "${OBJECT_PATH}" | grep -E "\/${OBJECT_FILE_NAME_C}\$" | sort | uniq)

    echo "- processing <${OBJECT_REL_PATH}>"

    # If it's not possible to find the source path using strings it relies on GDB, way slower but
    # should works always
    if [ -z "${OBJECT_SOURCE_PATH}" ];
    then
        echo "  - can't find the source path, trying gdb"
        OBJECT_SOURCE_PATH=$(gdb -q -ex "set height 0" -ex "info sources" -ex quit "${OBJECT_PATH}" | grep -v "${OBJECT_FILE}" | grep "${OBJECT_FILE_NAME_C}")
    fi

    # If the path can't be detected at all, we give up and skip the file
    if [ -z "${OBJECT_SOURCE_PATH}" ];
    then
        echo "  - can't find the source path, giving up"
        continue
    fi

    echo "  - source path <${OBJECT_SOURCE_PATH}>"
    echo "  - generating code coverage"

    CURRENT_PWD="$(pwd)"
    if ! cd "${COVERAGE_OUTPUT_DIRECTORY}";
    then
      echo "    unable to access <${COVERAGE_OUTPUT_DIRECTORY}>, critical failure! Exiting!"
      exit 1
    fi

    OUTPUT=$(gcov -xlpb "${OBJECT_SOURCE_PATH}" -o "${OBJECT_PATH}" 2>&1)
    res=$?

    if ! cd "${CURRENT_PWD}";
    then
      echo "    unable to move back to <${CURRENT_PWD}>, critical failure! Exiting!"
      exit 1
    fi

    if [ $res != 0 ];
    then
      echo "  - unable to generate code coverage"
      echo "${OUTPUT}"
      continue
    fi

    if (echo "${OUTPUT}" | grep -v "assuming not executed" >/dev/null);
    then
        echo "  - code coverage generated"
    else
        echo "  - code coverage not generated, object file never executed (may be expected)"
    fi

    if [ ${VERBOSE} == 1 ];
    then
      echo "${OUTPUT}"
    fi
done < <(find "${OBJECTS_ROOT_DIRECTORY}" -name \*.o -type f -print0);

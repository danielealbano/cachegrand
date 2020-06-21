#!/bin/bash

function show_help() {
    echo "Usage: $0 -o <output directory> -c <commit id>" 1>&2;
    exit 0;
}

OPTIND=1

COMMIT_ID=""
OUTPUT_DIR=""

while getopts "h?o:c:" opt; do
    case "$opt" in
    h|\?)
        show_help
        ;;
    c)  COMMIT_ID=$OPTARG
        ;;
    o)  OUTPUT_DIR=$OPTARG
        ;;
    esac
done

shift $((OPTIND-1))

if [ -z "${COMMIT_ID}" ] || [ -z "${OUTPUT_DIR}" ];
then
    show_help
fi

[[ "${OUTPUT_DIR}" != */ ]] && OUTPUT_DIR="${OUTPUT_DIR}/"

mkdir -p "${OUTPUT_DIR}"

cd "${OUTPUT_DIR}" || (echo "Unable to access <${OUTPUT_DIR}>, failing!" >&2; exit 1)

echo "Fetching sources..."
if ! (git clone https://github.com/axboe/liburing && cd liburing && git checkout "${COMMIT_ID}");
then
  echo "Unable to fetch the liburing sources, failing!" >&2
  exit 1
fi

echo "Configuring library..."
if ! (cd liburing && ./configure --prefix="${OUTPUT_DIR}install");
then
  echo "Unable to configure liburing, failing!" >&2
  exit 1
fi

echo "Building library..."
if ! (cd liburing && make);
then
  echo "Unable to build liburing, failing!" >&2
  exit 1
fi

echo "Installing library..."
if ! (cd liburing && make install);
then
  echo "Unable to install liburing, failing!" >&2
  exit 1
fi

#!/bin/bash

# Copyright (C) 2018-2023 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

set -e

# The load-test.sh script takes care of starting & stopping the benchmarked server and currently
# this is supported only on the local system so the only allowed value for MEMTIER_HOST is
# currently localhost.

# Test run settings
MEMTIER_PORT=6379
MEMTIER_HOST="localhost"
MEMTIER_CPUS="1-3"
MEMTIER_THREADS=3
MEMTIER_WARMUP_RUNS=3
MEMTIER_TEST_RUNS=5
MEMTIER_BIN_PATH="/home/daalbano/dev/memtier_benchmark/memtier_benchmark"
OUTPUT_PATH_DIR="/home/daalbano/benchmarks"
OUTPUT_PATH_TEST_CONFIG_SET_NAME="cachegrand-temp"
OUTPUT_PATH_TEST_TYPE="getset"
SERVER_BIN_NAME="cachegrand-server"

# General settings
CACHEGRAND_SERVER_BIN_PATH="/home/daalbano/dev/cachegrand/cmake-build-release/src/cachegrand-server"
CACHEGRAND_SERVER_CONFIG_PATH="/home/daalbano/dev/cachegrand/etc/cachegrand.yaml"
REDIS_SERVER_BIN_PATH="/home/daalbano/dev/redis-7.0.0/src/redis-server"
REDIS_SERVER_CONFIG_PATH="/etc/redis/redis.conf"
REDIS_SERVER_CPU="0"

# Disable syn flooding protection as it might impact
sudo sysctl net.ipv4.tcp_syncookies=0
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

for CLIENTS_PER_THREAD in 100 150 200;
do
    CLIENTS_TOTAL=$((CLIENTS_PER_THREAD * MEMTIER_THREADS))
    OUTPUT_PREFIX="${OUTPUT_PATH_DIR}/${OUTPUT_PATH_TEST_CONFIG_SET_NAME}/${OUTPUT_PATH_TEST_TYPE}/${CLIENTS_TOTAL}c-1t"
    OUTPUT_SERVER_STDOUT_PATH="${OUTPUT_PREFIX}/server.stdout"
    OUTPUT_MEMTIER_STDOUT_PATH="${OUTPUT_PREFIX}/memtier.stdout"
    OUTPUT_MEMTIER_JSON_PATH="${OUTPUT_PREFIX}/memtier.json"
    OUTPUT_MEMTIER_HDR_FILE_PREFIX="${OUTPUT_PREFIX}/memtier.hdr"
    OUTPUT_CPU_REPORT="${OUTPUT_PREFIX}/cpu-report.csv"

    if [ ! -d ${OUTPUT_PREFIX} ];
    then
        mkdir -p ${OUTPUT_PREFIX} >/dev/null
    fi

    if [ -f ${OUTPUT_SERVER_STDOUT_PATH} ];
    then
        rm ${OUTPUT_SERVER_STDOUT_PATH} >/dev/null
    fi

    if [ -f ${OUTPUT_MEMTIER_STDOUT_PATH} ];
    then
        rm ${OUTPUT_MEMTIER_STDOUT_PATH} >/dev/null
    fi

    if [ -f ${OUTPUT_MEMTIER_JSON_PATH} ];
    then
        rm ${OUTPUT_MEMTIER_JSON_PATH} >/dev/null
    fi

    if [ -f ${OUTPUT_CPU_REPORT} ];
    then
        rm ${OUTPUT_CPU_REPORT} >/dev/null
    fi

    if [ -f "${OUTPUT_MEMTIER_HDR_FILE_PREFIX}_*" ];
    then
        rm ${OUTPUT_MEMTIER_HDR_FILE_PREFIX}_* >/dev/null
    fi

    TEMP_SERVER_PID=$(ps aux | grep ${SERVER_BIN_NAME} | grep -v "load-test" | grep -v "report-cpu-load-temp-freq" | grep -v clion | grep -v memcheck | grep -v grep | awk '{ print $2 }')
    if [ "x${TEMP_SERVER_PID}" != "x" ];
    then
        echo "- server still running with PID ${TEMP_SERVER_PID}, killing it"
        kill "${TEMP_SERVER_PID}"
        sleep 2
        TEMP_SERVER_PID=$(ps aux | grep ${SERVER_BIN_NAME} | grep -v "load-test" | grep -v "report-cpu-load-temp-freq" | grep -v clion | grep -v memcheck | grep -v grep | awk '{ print $2 }')
        if [ "x${TEMP_SERVER_PID}" != "x" ];
        then
            echo "  server still running after killing, unable to continue!"
            exit 1
        fi
    fi
    
    echo "- clients per thread ${CLIENTS_PER_THREAD}, total ${CLIENTS_TOTAL}"
    echo "  output path: ${OUTPUT_MEMTIER_STDOUT_PATH}"

    echo "  starting ${SERVER_BIN_NAME}"

    if [ "${SERVER_BIN_NAME}" == "cachegrand-server" ];
    then
        ${CACHEGRAND_SERVER_BIN_PATH} -c ${CACHEGRAND_SERVER_CONFIG_PATH} >${OUTPUT_SERVER_STDOUT_PATH} 2>&1 &
    elif [ "${SERVER_BIN_NAME}" == "redis-server" ];
    then
        taskset -c ${REDIS_SERVER_CPU} "${REDIS_SERVER_BIN_PATH}" "${REDIS_SERVER_CONFIG_PATH}" --daemonize no &
    else
        echo "Unsupported server ${SERVER_BIN_NAME}"
        exit 1
    fi

    sleep 2
    SERVER_PID=$(ps aux | grep ${SERVER_BIN_NAME} | grep -v "load-test" | grep -v "report-cpu-load-temp-freq" | grep -v clion | grep -v memcheck | grep -v grep | awk '{ print $2 }')

    echo "  pid ${SERVER_PID}"
    echo "  warming up server"

    if [ "${OUTPUT_PATH_TEST_TYPE}" == "getset" ];
    then
        taskset -c ${MEMTIER_CPUS} ${MEMTIER_BIN_PATH} \
            -s "${MEMTIER_HOST}" \
            -p "${MEMTIER_PORT}" \
            -c "${CLIENTS_PER_THREAD}" \
            -t "${MEMTIER_THREADS}" \
            --hide-histogram \
            --random-data \
            --randomize \
            --distinct-client-seed \
            --hide-histogram \
            --data-size-range=100-500 \
            --data-size-pattern=S \
            --key-minimum=200 \
            --key-maximum=800 \
            --key-pattern=G:G \
            --key-stddev=20 \
            --key-median=400 \
            -x ${MEMTIER_WARMUP_RUNS}
    elif [ "${OUTPUT_PATH_TEST_TYPE}" == "ping" ];
    then
        taskset -c ${MEMTIER_CPUS} ${MEMTIER_BIN_PATH} \
            -s "${MEMTIER_HOST}" \
            -p "${MEMTIER_PORT}" \
            -c "${CLIENTS_PER_THREAD}" \
            -t "${MEMTIER_THREADS}" \
            --command="PING" \
            -x ${MEMTIER_WARMUP_RUNS}
    fi

    echo "  warmup completed"

    sleep 1

    echo "  starting load testing"

    if [ "${OUTPUT_PATH_TEST_TYPE}" == "getset" ];
    then
        taskset -c ${MEMTIER_CPUS} ${MEMTIER_BIN_PATH} \
            -s "${MEMTIER_HOST}" \
            -p "${MEMTIER_PORT}" \
            -c "${CLIENTS_PER_THREAD}" \
            -t "${MEMTIER_THREADS}" \
            --print-percentiles=50,90,95,99,99.5,99.9,100 \
            --random-data \
            --randomize \
            --distinct-client-seed \
            --data-size-range=100-500 \
            --data-size-pattern=S \
            --key-minimum=200 \
            --key-maximum=800 \
            --key-pattern=G:G \
            --key-stddev=20 \
            --key-median=400 \
            --json-out-file=${OUTPUT_MEMTIER_JSON_PATH} \
            --hdr-file-prefix=${OUTPUT_MEMTIER_HDR_FILE_PREFIX} \
            -x ${MEMTIER_TEST_RUNS} \
        | \
        tee ${OUTPUT_MEMTIER_STDOUT_PATH}
    elif [ "${OUTPUT_PATH_TEST_TYPE}" == "ping" ];
    then
        taskset -c ${MEMTIER_CPUS} ${MEMTIER_BIN_PATH} \
            -s "${MEMTIER_HOST}" \
            -p "${MEMTIER_PORT}" \
            -c "${CLIENTS_PER_THREAD}" \
            -t "${MEMTIER_THREADS}" \
            --print-percentiles=50,90,95,99,99.5,99.9,100 \
            --command="PING" \
            --json-out-file=${OUTPUT_MEMTIER_JSON_PATH} \
            --hdr-file-prefix=${OUTPUT_MEMTIER_HDR_FILE_PREFIX} \
            -x ${MEMTIER_TEST_RUNS} \
        | \
        tee ${OUTPUT_MEMTIER_STDOUT_PATH}
    fi

    echo "  load testing completed"
    echo "  killing server"
    sleep 1
    kill "${SERVER_PID}"
    sleep 1
    echo "  server killed"
done


echo "Threads,Clients/thread,,Type,Clients,Hits/sec,Misses/sec,Ops/sec,Avg. Latency,p50 Latency,p90 Latency,p95 Latency,p99 Latency,p99.5 Latency,p99.9 Latency,p100 Latency,KB/sec"
for I in $(find /home/daalbano/benchmarks/ -maxdepth 1 -mindepth 1 -type d);
do
  for X in $(find $I/getset/ -maxdepth 1 -mindepth 1 -type d | sort -n);
  do
    for L in $(cat $X/memtier.stdout | grep "BEST RUN RESULTS" -A 8 | grep "Totals" | awk '{ print $2 "," $3 "," $4 "," $5 "," $6 "," $7 "," $8 "," $9 "," $10 "," $11 "," $12 "," $13 }');
    do
      THREADS=$MEMTIER_THREADS
      CLIENTS_TOTAL=$(basename "$X" | cut -d '-' -f1 | rev | cut -c 2- | rev)
      CLIENTS_PER_THREAD=$((CLIENTS_TOTAL / $THREADS))
      echo "$MEMTIER_THREADS,$CLIENTS_PER_THREAD,,$(basename $I),$CLIENTS_TOTAL,$L";
    done
  done
done

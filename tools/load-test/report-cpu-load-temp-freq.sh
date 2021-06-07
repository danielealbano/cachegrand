#!/bin/bash

IS_NUMBER_RE='^[0-9]+$'

# Build the header

OUTPUT="TIMESTAMP"
for CPU_PATH in /sys/devices/system/cpu/cpu*;
do
    CPU_NUM=$(basename $CPU_PATH | cut -b4-)

    if ! [[ $CPU_NUM =~ $IS_NUMBER_RE ]];
    then
        continue
    fi

    OUTPUT="${OUTPUT},CPU_${CPU_NUM}_FREQ"
done
OUTPUT="${OUTPUT},CPU_TEMP"

# Print out the header
echo $OUTPUT


# Loop to print out the values
while [ true ];
do
    # Build the data
    OUTPUT="$(date +'%FT%TZ%:z')"
    for CPU_PATH in /sys/devices/system/cpu/cpu*;
    do
        CPU_NUM=$(basename ${CPU_PATH} | cut -b4-)

        if ! [[ $CPU_NUM =~ $IS_NUMBER_RE ]];
        then
            continue
        fi

        OUTPUT="${OUTPUT},$(($(cat ${CPU_PATH}/cpufreq/scaling_cur_freq) / 1000))"
    done
    OUTPUT="${OUTPUT},$(($(cat /sys/devices/platform/coretemp.0/hwmon/hwmon2/temp1_input) / 1000))"

    # Print out the data
    echo $OUTPUT

    sleep 1
done

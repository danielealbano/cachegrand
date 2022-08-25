#!/usr/bin/bash

set +x

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

function get_command() {
	local command_name=$1

	if [ -z "${command_name}" ];
	then
		echo "Command not specified" >&2
		exit 1
	fi

	local result=$(which "${command_name}")
	if [ -z "${result}" ];
    then
        echo "Unable to find "${command_name}", please install the necessary package(s)" >&2
        exit 1
    fi

    echo $result
    return 0
}

# Relies on set -e to terminate the execution if get_command fails instead of checking each single result, +e is
# restored at the end of the sequence
set -e
TCLSH=$(get_command tclsh)
CURL=$(get_command curl)
JQ=$(get_command jq)
TAR=$(get_command tar)
GREP=$(get_command grep)
EGREP=$(get_command egrep)
BC=$(get_command bc)
CAT=$(get_command cat)
DATE=$(get_command date)
AWK=$(get_command awk)
SED=$(get_command sed)
CUT=$(get_command cut)
PYTHON3=$(get_command python3)
set +e

yaml() {
    $PYTHON3 -c "import yaml;print(yaml.safe_load(open('$1'))$2)"
}

function cleanup_temp_directory {
	# Try to ensure that the / doesn't get deleted without having to enforce using /tmp :)
  	if [ ${#TEMP_FOLDER} -gt 1 ] && [ -d $TEMP_FOLDER ];
	then
		rm -r $TEMP_FOLDER
  	fi
}

function xmlencode {
    $SED -e 's/\&/\&amp;/g' -e 's/\"/\&quot;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g'
}

function strip_terminal_escape {
    $SED -e $'s/\x1B\[[0-9]*;[0-9]*[m|K|G|A]//g' -e $'s/\x1B\[[0-9]*[m|K|G|A]//g'
}

function timestamp {
	$DATE "+%Y-%m-%dT%H:%M:%S"
}

JUNIT=0
JUNIT_OUTPUT_PATH="junit-results.xml"
REDIS_TARBALL_PATH="/tmp/redis-latest.tar.gz"
CACHEGRAND_SERVER_PATH=$(realpath $(pwd)/../../../cmake-build-debug/src/cachegrand-server 2>/dev/null)
CACHEGRAND_CONFIG_PATH=$(realpath $(pwd)/../../../etc/cachegrand.yaml.skel 2>/dev/null)

function print_help() {
    [ -n "${1:-}" ] && echo -e "Error: $1\n" >&2

    cat <<EOF
Usage: $(basename $0) [--server <cachegrand-server-path>] [--config <config-file-path>] [--output <junit-output-file>] [--junit]
Options:
-h|--help           This help message
-t|--tarball=<file> Location of the redis tarball, if it doesn't exist will be downloaded [default: $REDIS_TARBALL_PATH]
-s|--server=<file>  Location of the cachegrand-server executable [default: $CACHEGRAND_SERVER_PATH]
-c|--config=<file>  Location of the config files to use [default: $CACHEGRAND_CONFIG_PATH]
-j|--junit          Enable junit xml writing
-o|--output=<file>  Location to write the junit xml file [default: $JUNIT_OUTPUT_PATH]
EOF
}

while true; do
    [[ $# == 0 ]] && break
    case $1 in
        -h | --help) print_help && exit 0;;
        -j | --junit) JUNIT=1; shift ;;
        -o | --output) JUNIT_OUTPUT_PATH=$2; shift 2 ;;
        -s | --server) CACHEGRAND_SERVER_PATH=$2; shift 2 ;;
        -c | --config) CACHEGRAND_CONFIG_PATH=$2; shift 2 ;;
        -t | --tarball) REDIS_TARBALL_PATH=$2; shift 2 ;;
        -- ) shift; break ;;
        -* ) (print_help "Invalid argument $1") >&2 && exit 1;;
        * ) break ;;
    esac
done

# Check if the correct parameters have been passed
if [ -z "$CACHEGRAND_SERVER_PATH" ] || [ ! -x "$CACHEGRAND_SERVER_PATH" ]
then
	(print_help "Invalid cachegrand-server path '$CACHEGRAND_SERVER_PATH'") >&2
	exit 1
fi

if [ -z "$CACHEGRAND_CONFIG_PATH" ] || [ ! -f "$CACHEGRAND_CONFIG_PATH" ];
then
	(print_help "Invalid cachegrand config file path '$CACHEGRAND_CONFIG_PATH'") >&2
	exit 1
fi

echo "> Bootstrapping tests"

# Create a temporary directory to be used to hold the redis tarball and the uncompressed package
TEMP_FOLDER=$(mktemp -d)
trap cleanup_temp_directory INT EXIT

if [ -e $REDIS_TARBALL_PATH ];
then
	echo "> The redis latest tarball already exists at ${REDIS_TARBALL_PATH}, skipping download"
else
	echo "> Fetching the URL for the tarball of the latest Redis release"

	# Get the url of the latest tarball
	REDIS_LATEST_TARBALL_URL=$( \
		$CURL \
			-s \
			-H "Accept: application/vnd.github+json" \
			https://api.github.com/repos/redis/redis/releases/latest \
		| \
		$JQ \
			-r \
			'.tarball_url' \
	)

	if [ -z "$REDIS_LATEST_TARBALL_URL" ];
	then
		echo "> Failed to fetch the URL for the tarball of the latest Redis release, please review the logs" >&2
		exit 1
	fi

	echo "> Fetching the tarball of the latest Redis release"
	$CURL -s -L $REDIS_LATEST_TARBALL_URL > $REDIS_TARBALL_PATH
	if [ $? -ne 0 ];
	then
		echo "> Failed to download the tarball of the latest Redis release, please review the logs" >&2
		exit 1
	fi
fi

echo "> Uncompressing the tarball"
$TAR -xzf $REDIS_TARBALL_PATH -C $TEMP_FOLDER --strip-components=1
if [ $? -ne 0 ];
then
	echo "> Failed to uncompress the tarball, please review the logs" >&2
	exit 1
fi

# Fetch the server port from the config file
CACHEGRAND_SERVER_PORT=$(yaml $CACHEGRAND_CONFIG_PATH "['modules'][0]['network']['bindings'][0]['port']")

echo "> Starting tests"

TESTS_RESULTS_PATH="${TEMP_FOLDER}/tests-results.txt"
TESTS_START_TIME=`date +%s.%N`

$TCLSH \
	./main.tcl \
	--server_path $CACHEGRAND_SERVER_PATH \
	--server_cfg $CACHEGRAND_CONFIG_PATH \
	--server_port $CACHEGRAND_SERVER_PORT \
	--test_path $TEMP_FOLDER/tests/unit \
	--tests keyspace \
	--tests type/string \
	2>&1 \
	> $TESTS_RESULTS_PATH

TESTS_END_TIME=`date +%s.%N`

if (grep "\[exception\]:" $TESTS_RESULTS_PATH >/dev/null 2>&1)
then
	echo -e "> Failed to run the tests, please review the logs" >&2
	cat $TESTS_RESULTS_PATH >&2
	exit 1
fi

TESTS_DURATION=$(printf %.2f $( echo "$TESTS_END_TIME - $TESTS_START_TIME" | $BC -l ))
echo "> Tests ended, duration ${TESTS_DURATION}s"
echo "> Extracting results"

# Identify the tests file
OLD_IFS=$IFS
NEWLINE_IFS=$'\n'

# Count the number of tests and the number of failed tests
TOTAL_TESTS_COUNT=0
FAILED_TESTS_COUNT=0
for TEST_FILE_PATH in $($CAT $TESTS_RESULTS_PATH | $GREP "Executing test file" | $CUT -d":" -f2)
do
	IFS=$NEWLINE_IFS
	for TEST_NAME in $($CAT $TEST_FILE_PATH | $GREP "test {" | $CUT -d"{" -f2 | $CUT -d"}" -f1);
	do
		IFS=$OLD_IFS
		TOTAL_TESTS_COUNT=$(($TOTAL_TESTS_COUNT + 1))
		TEST_NAME_GREP=$(printf '%q' "$TEST_NAME")

		TEST_RESULT=$($CAT $TESTS_RESULTS_PATH | $GREP -v "\->" | $EGREP -e "\]: ${TEST_NAME_GREP} (\([0-9]+ ms\)|in ${TEMP_FOLDER}/tests)" | $CUT -d"[" -f 2 | $CUT -d"]" -f 1)

		# Check that something was found and map it to success or failure
		if [ -z "${TEST_RESULT}" ]
		then
			echo "Failed to fetch the test result for '${TEST_NAME}' in '${TEST_FILE_PATH}'" >&2
			echo $TEST_RESULT
			exit 1
		elif [ "${TEST_RESULT}" != "ok" ] && [ "${TEST_RESULT}" != "err" ]
		then
			echo "The test result '${TEST_RESULT}' fetched for '${TEST_NAME}' in '${TEST_FILE_PATH}' doesn\'t match the expected 'ok' or 'err' values" >&2
			exit 1
		fi

		if [ $TEST_RESULT == "err" ]
		then
			FAILED_TESTS_COUNT=$((FAILED_TESTS_COUNT + 1))
		fi
	done
	IFS=$NEWLINE_IFS
done
IFS=$OLD_IFS

if [ $JUNIT -eq 1 ];
then
	if [ -e $JUNIT_OUTPUT_PATH ];
	then
		rm $JUNIT_OUTPUT_PATH
	fi

	$CAT <<EOF >> $JUNIT_OUTPUT_PATH
<?xml version="1.0" encoding="UTF-8"?>
<testsuites failures="${FAILED_TESTS_COUNT}" name="$0" tests="${TOTAL_TESTS_COUNT}" time="$TESTS_DURATION" timestamp="$(timestamp)" >
EOF
fi

echo "> Report"
echo ">   ${TOTAL_TESTS_COUNT} tests found"
echo -e ">     ${RED} Failed: ${FAILED_TESTS_COUNT}${NC}"
echo -e ">     ${GREEN} Passed: $(($TOTAL_TESTS_COUNT - $FAILED_TESTS_COUNT))${NC}"
echo ">   Test groups:"

TEST_NUMBER=0
for TEST_FILE_PATH in $($CAT $TESTS_RESULTS_PATH | $GREP "Executing test file" | $CUT -d":" -f2)
do
	# Remove the temp folder path and the /tests/ string from the TEST_FILE_PATH variable to build the
	# TEST_FILE_PATH_RELATIVE variable
	BASE_PATH_LEN=$((${#TEMP_FOLDER} + 7))
	TEST_FILE_PATH_LEN=${#TEST_FILE_PATH}
	TEST_FILE_PATH_RELATIVE=${TEST_FILE_PATH:$BASE_PATH_LEN:$(($TEST_FILE_PATH_LEN - $BASE_PATH_LEN))}

	echo ">     ${TEST_FILE_PATH_RELATIVE}"

	IFS=$NEWLINE_IFS
	for TEST_NAME in $($CAT $TEST_FILE_PATH | $GREP "test {" | $CUT -d"{" -f2 | $CUT -d"}" -f1);
	do
		IFS=$OLD_IFS
		TEST_NUMBER=$((TEST_NUMBER + 1))
		TEST_NAME_GREP=$(printf '%q' "$TEST_NAME")

		TEST_RESULT=$($CAT $TESTS_RESULTS_PATH | $GREP -v "\->" | $EGREP -e "\]: ${TEST_NAME_GREP} (\([0-9]+ ms\)|in ${TEMP_FOLDER}/tests)" | $CUT -d"[" -f 2 | $CUT -d"]" -f 1)

		if [ ! "${TEST_RESULT}" == "err" ] && [ ! "${TEST_RESULT}" == "ok" ]
		then
			echo "Unknown test result: ${TEST_RESULT}"
			$CAT $TESTS_RESULTS_PATH | $GREP -v "\->" | $EGREP -e "\]: ${TEST_NAME_GREP} (\([0-9]+ ms\)|in ${TEMP_FOLDER}/tests)"
		fi

		TEST_CASE_FAILURE_REASON=""
		if [ "${TEST_RESULT}" == "err" ]
		then
			TEST_CASE_FAILURE_REASON=$($CAT $TESTS_RESULTS_PATH | $EGREP -e "\-> \[err\]: ${TEST_NAME_GREP} in" -A 1 | tail -n 1)
		fi

		if [ $JUNIT -eq 1 ];
		then
			JUNIT_TEST_CASE_FAILURE_XML=""
			if [ "${TEST_RESULT}" == "err" ]
			then
				JUNIT_TEST_CASE_FAILURE_XML="<failure message=\"test failed\"><![CDATA[${TEST_CASE_FAILURE_REASON}]]></failure>
"
			fi

		$CAT <<EOF >> $JUNIT_OUTPUT_PATH
	<testcase classname="${TEST_FILE_PATH_RELATIVE}" name="$(echo ${TEST_NAME} | xmlencode)" time="0" timestamp="$(timestamp)">
		$JUNIT_TEST_CASE_FAILURE_XML<system-err></system-err>
		<system-out><![CDATA[$TEST_RESULT]]></system-out>
	</testcase>
EOF
		fi

		echo -n ">       [${TEST_NUMBER}/${TOTAL_TESTS_COUNT}] Test '${TEST_NAME}' "
		if [ "${TEST_RESULT}" == "err" ]
		then
			echo -ne $RED
			echo -n "failed: ${TEST_CASE_FAILURE_REASON}"
		else
			echo -ne $GREEN
			echo -n "successful"
		fi

		echo -e $NC
	done
	IFS=$NEWLINE_IFS
done
IFS=$OLD_IFS

if [ $JUNIT -eq 1 ];
then
	$CAT <<EOF >> $JUNIT_OUTPUT_PATH
</testsuites>
EOF

	echo "> junit report available at ${JUNIT_OUTPUT_PATH}"
fi

echo "> Done"

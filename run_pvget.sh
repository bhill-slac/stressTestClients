#!/bin/bash

if [ $# -lt 2 ]; then
	echo "Usage: ${BASH_SOURCE[0]} testDir PV1 ..."
	exit
fi
TEST_DIR=$1
shift
PVS=$*
echo TEST_DIR=$TEST_DIR
echo PVS=$PVS

PVGET=`which pvget`
if [ ! -e "$PVGET" ]; then
	echo "Error: pvget not found!"
	exit 1
fi

if [ -z "$TEST_PROVIDER" ]; then
	TEST_PROVIDER=pva
fi
if [ -z "$TEST_PVGET_TIMEOUT" ]; then
	TEST_PVGET_TIMEOUT=3
fi
if [ -z "$TEST_PVGET_SLEEP" ]; then
	TEST_PVGET_SLEEP=1
fi

mkdir -p $TEST_DIR
while (( 1 ));
do
	#echo "Checking for active pvget jobs:"
	#ps u -C pvget

	for PV in $PVS;
	do
		pvget -M json -p $TEST_PROVIDER -w $TEST_PVGET_TIMEOUT $PV 2>&1 | cat >> $TEST_DIR/$PV.pvget &
	done
	wait
	#echo "pvget jobs done"
	sleep $TEST_PVGET_SLEEP
done


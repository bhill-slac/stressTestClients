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

mkdir -p $TEST_DIR
while (( 1 ));
do
	#echo "Checking for active pvget jobs:"
	#ps u -C pvget
	for PV in $PVS;
	do
		#echo pvget -w 3 $PV log-to $TEST_DIR/$PV.pvget
		#echo pvget -w 3 $PV >> $TEST_DIR/$PV.pvget
		pvget -w 3 $PV 2>&1 | cat >> $TEST_DIR/$PV.pvget &
	done
	wait
	#echo "pvget jobs done"
	sleep 5
done


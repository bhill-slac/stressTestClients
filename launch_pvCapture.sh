#!/bin/bash
if [ $# -lt 1 -o "$1" == "-h" ]; then
	echo Usage: $(basename ${BASH_SOURCE[0]}) testName
	exit 1
fi
SCRIPTDIR=`readlink -f $(dirname ${BASH_SOURCE[0]})`
TESTNAME=$1

# Make sure we can find procServ and pyProcMgr.py
PROCSERV=`which procServ`
if [ ! -e "$PROCSERV" ]; then
	echo "Error: procServ not found!"
	exit 1
fi

PYPROCMGR=`which pyProcMgr.py`
if [ ! -e "$PYPROCMGR" ]; then
	echo "Error: pyProcMgr.py not found!"
	exit 1
fi

# This part can be replaced w/ python front end
# for each host
#	ssh to host and launch a stressTestMgr.py instance
#
# Start test: date > $STRESSTEST_TOP/$TESTNAME/startTime

# Each stressTestMgr.py instance does the following:
# Monitor startTime files
# if currentTime < startTime + 1 sec
#	Read configuration from env files
#	Generate pvlist if APPTYPE is pvCapture
#	Dump test env variables to log
#	Dump host info to $STRESSTEST_TOP/$TESTNAME/$HOSTNAME/*.info
#   For each client
#		if currentTime > startTime + clientStartDelay
#			launch client pyProcMgr instance
#		if currentTime > startTime + testDuration + clientStopDelay
#			kill client pyProcMgr instance
#	Do periodic timestamped cat of /proc/loadavg into TEST_LOG
#		% cat /proc/loadavg
#		0.01 0.04 0.05 1/811 22246
#		1min 5min 15min numExecuting/numProcessesAndThreads lastPID
#	if currentTime > stopTime
#   	For each client
#			if currentTime > startTime + testDuration + clientStopDelay
#				kill client pyProcMgr instance

#


# Get hostname
HOSTNAME=`hostname -s`

# Configure Test
TEST_APPTYPE=pvCapture
source $SCRIPTDIR/stressTestDefault.env
if [ -f $SCRIPTDIR/stressTestDefault.env.local ]; then
	source $SCRIPTDIR/stressTestDefault.env.local
fi
if [ -f $SCRIPTDIR/${TEST_APPTYPE}Default.env ]; then
	source $SCRIPTDIR/${TEST_APPTYPE}Default.env
fi
if [ -f $STRESSTEST_TOP/default.env ]; then
	source $STRESSTEST_TOP/default.env
fi
if [ -f $STRESSTEST_TOP/${TESTNAME}/test.env ]; then
	source $STRESSTEST_TOP/${TESTNAME}/test.env 
fi
if [ -f $STRESSTEST_TOP/${TESTNAME}/${TESTNAME}.env ]; then
	echo "Use of ${TESTNAME}/${TESTNAME}.env is deprecated"
	echo "Please rename to ${TESTNAME}/test.env"
	source $STRESSTEST_TOP/${TESTNAME}/${TESTNAME}.env 
fi

N_CNT_PER_SERVER=$TEST_N_COUNTERS
export N_PVS_PER_CLIENT=$((($TEST_N_LOADSERVERS*$N_CNT_PER_SERVER+$TEST_N_PVCAPTURE-1)/$TEST_N_PVCAPTURE))
# Hack
#TEST_N_PVCAPTURE=2
#N_PVS_PER_CLIENT=5

TEST_HOST_DIR=$STRESSTEST_TOP/$TESTNAME/$HOSTNAME
mkdir -p $TEST_HOST_DIR
TEST_LOG=$TEST_HOST_DIR/$TESTNAME-${TEST_APPTYPE}.log

echo TESTNAME=$TESTNAME | tee $TEST_LOG
echo "Launching $TEST_N_PVCAPTURE $TEST_APPTYPE IOCs w/ $TEST_N_COUNTERS Counters each" | tee -a $TEST_LOG
echo TEST_PVCAPTURE_BASEPORT=$TEST_PVCAPTURE_BASEPORT | tee -a $TEST_LOG
echo TEST_CIRCBUFF_SIZE=$TEST_CIRCBUFF_SIZE | tee -a $TEST_LOG
echo TEST_COUNTER_RATE=$TEST_COUNTER_RATE | tee -a $TEST_LOG
echo TEST_COUNTER_DELAY=$TEST_COUNTER_DELAY | tee -a $TEST_LOG
echo TEST_DRIVE=$TEST_DRIVE | tee -a $TEST_LOG
echo TEST_EPICS_PVA_SERVER_PORT=$TEST_EPICS_PVA_SERVER_PORT | tee -a $TEST_LOG
echo TEST_EPICS_PVA_BROADCAST_PORT=$TEST_EPICS_PVA_BROADCAST_PORT | tee -a $TEST_LOG
echo TEST_PV_PREFIX=$TEST_PV_PREFIX | tee -a $TEST_LOG
echo TEST_N_LOADSERVERS=$TEST_N_LOADSERVERS
echo N_CNT_PER_SERVER=$N_CNT_PER_SERVER
echo N_PVS_PER_CLIENT=$N_PVS_PER_CLIENT

# Create PV Lists for pvCapture clients
P=0
C=0
N_PVS=$(($TEST_N_PVCAPTURE * $N_PVS_PER_CLIENT))
CLIENT_NAME=${TEST_APPTYPE}00
mkdir -p $TEST_DIR/$CLIENT_NAME
cat /dev/null > $TEST_DIR/$CLIENT_NAME/pvs.list
for (( N = 0; N < $N_CNT_PER_SERVER ; ++N )) do
	for (( S = 0; S < $TEST_N_LOADSERVERS ; ++S )) do
		N_PV=$(($C * $N_PVS_PER_CLIENT + $P))
		if (( $N_PV >= $N_PVS )); then
			break;
		fi
		if (( $S >= 10 )); then
			PRE=${TEST_PV_PREFIX}$S
		else
			PRE=${TEST_PV_PREFIX}0$S
		fi
		if (( $N >= 10 )); then
			PV=${PRE}:Count${N}
		else
			PV=${PRE}:Count$0{N}
		fi
		echo $PV >> $TEST_DIR/$CLIENT_NAME/pvs.list
		P=$(($P+1))
		if (( $P >= $N_PVS_PER_CLIENT )) ; then
			P=0
			C=$(($C+1))
			if (( $C >= 10 )); then
				CLIENT_NAME=${TEST_APPTYPE}$C
			else
				CLIENT_NAME=${TEST_APPTYPE}0$C
			fi
			if (( $C < $TEST_N_PVCAPTURE )) ; then
				mkdir -p $TEST_DIR/$CLIENT_NAME
				cat /dev/null > $TEST_DIR/$CLIENT_NAME/pvs.list
			fi
		fi
	done
done

TEST_HOST_DIR=$STRESSTEST_TOP/$TESTNAME/$HOSTNAME
TEST_DIR=$TEST_HOST_DIR/clients

#!/bin/bash
# From here down can be used as a generic pvCapture launch script
cd $SCRIPTDIR

if [ `hostname -s` == "$HOSTNAME" ]; then
	# Run test on host
	echo Start: `date` | tee -a $TEST_LOG

	mkdir -p $TEST_DIR
	uname -a > $TEST_HOST_DIR/uname.info
	cat /proc/cpuinfo > $TEST_HOST_DIR/cpu.info
	cat /proc/meminfo > $TEST_HOST_DIR/mem.info
	cat /proc/loadavg > $TEST_HOST_DIR/loadavg.info

	# export variables that will be expanded by pyProcMgr
	export TEST_DIR 

	$PYPROCMGR -v -c $TEST_N_PVCAPTURE -n $TEST_APPTYPE -p $TEST_PVCAPTURE_BASEPORT -d 5.0 -D $TEST_DIR \
		'bin/$EPICS_HOST_ARCH/pvCapture -S -D $TEST_DIR/pvCapture$PYPROC_ID -f $TEST_DIR/pvCapture$PYPROC_ID/pvs.list'; \
	echo Done: `date` | tee -a $TEST_LOG
fi

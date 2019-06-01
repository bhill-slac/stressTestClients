#!/bin/bash
if [ $# -lt 1 -o "$1" == "-h" ]; then
	echo Usage: $(basename ${BASH_SOURCE[0]}) testName
	exit 1
fi
TESTNAME=$1

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

HOSTNAME=`hostname -s`

TOP=`readlink -f $(dirname ${BASH_SOURCE[0]})`

SCRIPTDIR=`dirname ${BASH_SOURCE[0]}`

# Configure Test
TEST_APPTYPE=pvCapture
source $SCRIPTDIR/stressTestDefault.env
if [ -f source $SCRIPTDIR/stressTestDefault.env.local ]; then
	source $SCRIPTDIR/stressTestDefault.env.local
fi
if [ -f $SCRIPTDIR/${TEST_APPTYPE}Default.env ]; then
	source $SCRIPTDIR/${TEST_APPTYPE}Default.env
fi
if [ -f $TEST_TOP/default.env ]; then
	source $TEST_TOP/default.env
fi
if [ -f $TEST_TOP/${TESTNAME}.env ]; then
	source $TEST_TOP/${TESTNAME}.env 
fi

N_CNT_PER_SERVER=$TEST_N_COUNTERS
export N_PVS_PER_CLIENT=$((($TEST_N_LOADSERVERS*$N_CNT_PER_SERVER+$TEST_N_PVCAPTURE-1)/$TEST_N_PVCAPTURE))
# Hack
#TEST_N_PVCAPTURE=2
#N_PVS_PER_CLIENT=5

TEST_HOST_DIR=$TEST_TOP/$TESTNAME/$HOSTNAME
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

echo Start: `date` | tee -a $TEST_LOG

# Run test
TEST_HOST_DIR=$TEST_TOP/$TESTNAME/$HOSTNAME
TEST_DIR=$TEST_HOST_DIR/clients
mkdir -p $TEST_DIR
uname -a > $TEST_HOST_DIR/uname.info
cat /proc/cpuinfo > $TEST_HOST_DIR/cpu.info
cat /proc/meminfo > $TEST_HOST_DIR/mem.info

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

cd $TOP

# export variables that will be expanded by pyProcMgr
export TEST_DIR 

#echo $PYPROCMGR -c $TEST_N_PVCAPTURE ...
$PYPROCMGR -v -c $TEST_N_PVCAPTURE -n $TEST_APPTYPE -p $TEST_PVCAPTURE_BASEPORT -d 5.0 -D $TEST_DIR \
	'bin/$EPICS_HOST_ARCH/pvCapture -D $TEST_DIR/pvCapture$PYPROC_ID -f $TEST_DIR/pvCapture$PYPROC_ID/pvs.list'; \
echo Done: `date` | tee -a $TEST_LOG


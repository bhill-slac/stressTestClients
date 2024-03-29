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
SCRIPTDIR=`dirname ${BASH_SOURCE[0]}`

# Configure Test
source $SCRIPTDIR/stressTestDefault.env
TEST_APPTYPE=run_pvget
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

CLIENT_NAME=pvget

# Kill any pending stuck pvget related processes
pkill -9 pvget
pkill -9 run_pvget.sh

TEST_HOST_DIR=$STRESSTEST_TOP/$TESTNAME/$HOSTNAME
mkdir -p $TEST_HOST_DIR
TEST_LOG=$TEST_HOST_DIR/$TESTNAME-${TEST_APPTYPE}.log

echo TESTNAME=$TESTNAME | tee $TEST_LOG
echo "Launching $TEST_N_RUN_PVGET_CLIENTS ${TEST_APPTYPE} apps ..." | tee -a $TEST_LOG

echo TEST_RUN_PVGET_BASEPORT=$TEST_RUN_PVGET_BASEPORT | tee -a $TEST_LOG

echo TEST_EPICS_PVA_SERVER_PORT=$TEST_EPICS_PVA_SERVER_PORT | tee -a $TEST_LOG
echo TEST_EPICS_PVA_BROADCAST_PORT=$TEST_EPICS_PVA_BROADCAST_PORT | tee -a $TEST_LOG
echo TEST_PV_PREFIX=$TEST_PV_PREFIX | tee -a $TEST_LOG
echo Start: `date` | tee -a $TEST_LOG

# Run test
TEST_HOST_DIR=$STRESSTEST_TOP/$TESTNAME/$HOSTNAME
TEST_DIR=$TEST_HOST_DIR/clients
mkdir -p $TEST_DIR
uname -a > $TEST_HOST_DIR/uname.info
cat /proc/cpuinfo > $TEST_HOST_DIR/cpu.info
cat /proc/meminfo > $TEST_HOST_DIR/mem.info

TEST_PVS=''
for (( S = 0; S < $TEST_N_LOADSERVERS ; ++S )) do
	if (( $S >= 10 )); then
		PRE=${TEST_PV_PREFIX}$S
	else
		PRE=${TEST_PV_PREFIX}0$S
	fi
	TEST_PVS+=" $PRE:Count\$PYPROC_ID"
	TEST_PVS+=" $PRE:Rate\$PYPROC_ID"
done

#echo TEST_PVS=$TEST_PVS

# export variables that will be expanded by pyProcMgr
export CLIENT_NAME TEST_DIR TEST_PVS

$PYPROCMGR -v -c $TEST_N_RUN_PVGET_CLIENTS -n $CLIENT_NAME \
	-p $TEST_RUN_PVGET_BASEPORT -D $TEST_DIR \
	"$SCRIPTDIR/run_pvget.sh" '$TEST_DIR/$CLIENT_NAME$PYPROC_ID' \
	'$TEST_PVS'; \
echo Done: `date` | tee -a $TEST_LOG


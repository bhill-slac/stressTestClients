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

LOADSERVER=`which loadServer`
if [ ! -e "$LOADSERVER" ]; then
	echo "Error: loadServer not found!"
	exit 1
fi
LOADSERVER_BIN=`dirname $LOADSERVER`
TOP=`readlink -f $LOADSERVER_BIN/../..`

PYPROCMGR=`which pyProcMgr.py`
if [ ! -e "$PYPROCMGR" ]; then
	echo "Error: pyProcMgr.py not found!"
	exit 1
fi
HOSTNAME=`hostname -s`
SCRIPTDIR=`dirname ${BASH_SOURCE[0]}`

# Configure Test
source $SCRIPTDIR/stressTestDefault.env
TEST_APPTYPE=loadServer
if [ -f $SCRIPTDIR/${TEST_APPTYPE}Default.env ]; then
	source $SCRIPTDIR/${TEST_APPTYPE}Default.env
fi
if [ -f $TEST_TOP/default.env ]; then
	source $TEST_TOP/default.env
fi
if [ -f $TEST_TOP/${TESTNAME}/test.env ]; then
	source $TEST_TOP/${TESTNAME}/test.env 
fi
if [ -f $TEST_TOP/${TESTNAME}/${TESTNAME}.env ]; then
	echo "Use of ${TESTNAME}/${TESTNAME}.env is deprecated"
	echo "Please rename to ${TESTNAME}/test.env"
	source $TEST_TOP/${TESTNAME}/${TESTNAME}.env 
fi

# Gateway port test env
#TEST_EPICS_PVA_SERVER_PORT=5086
#TEST_EPICS_PVA_BROADCAST_PORT=5086

TEST_HOST_DIR=$TEST_TOP/$TESTNAME/$HOSTNAME
mkdir -p $TEST_HOST_DIR
TEST_LOG=$TEST_HOST_DIR/$TESTNAME-${TEST_APPTYPE}.log
echo TESTNAME=$TESTNAME | tee $TEST_LOG
echo "Launching $TEST_N_LOADSERVERS ${TEST_APPTYPE} IOCs w/ $TEST_N_COUNTERS Counters and Circular buffers each" | tee -a $TEST_LOG

echo TEST_LOADSERVER_BASEPORT=$TEST_LOADSERVER_BASEPORT | tee -a $TEST_LOG
echo TEST_CIRCBUFF_SIZE=$TEST_CIRCBUFF_SIZE | tee -a $TEST_LOG
echo TEST_DRIVE=$TEST_DRIVE | tee -a $TEST_LOG
#if [ "$TEST_DRIVE" == "drive" ]; then
echo TEST_COUNTER_RATE=$TEST_COUNTER_RATE | tee -a $TEST_LOG
echo TEST_COUNTER_DELAY=$TEST_COUNTER_DELAY | tee -a $TEST_LOG
#else
echo TEST_CA_LNK=$TEST_CA_LNK | tee -a $TEST_LOG
#fi

echo TEST_EPICS_PVA_SERVER_PORT=$TEST_EPICS_PVA_SERVER_PORT | tee -a $TEST_LOG
echo TEST_EPICS_PVA_BROADCAST_PORT=$TEST_EPICS_PVA_BROADCAST_PORT | tee -a $TEST_LOG
echo TEST_PV_PREFIX=$TEST_PV_PREFIX | tee -a $TEST_LOG
echo Start: `date` | tee -a $TEST_LOG

# Run test
TEST_DIR=$TEST_HOST_DIR/servers
mkdir -p $TEST_DIR
uname -a > $TEST_HOST_DIR/uname.info
cat /proc/cpuinfo > $TEST_HOST_DIR/cpu.info
cat /proc/meminfo > $TEST_HOST_DIR/mem.info

cd $TOP
TEST_DB=db/${TEST_DRIVE}_${TEST_N_COUNTERS}CircBuff.db

# export variables that will be expanded by pyProcMgr
export TEST_COUNTER_DELAY TEST_PV_PREFIX TEST_CIRCBUFF_SIZE TEST_DB TEST_CA_LNK

# Launch pyProcMgr
EPICS_PVA_SERVER_PORT=$TEST_EPICS_PVA_SERVER_PORT EPICS_PVA_BROADCAST_PORT=$TEST_EPICS_PVA_BROADCAST_PORT \
$PYPROCMGR -v -c $TEST_N_LOADSERVERS  \
	-n $TEST_APPTYPE -p $TEST_LOADSERVER_BASEPORT \
	-D $TEST_DIR \
	$LOADSERVER \
	'-m DELAY=$TEST_COUNTER_DELAY,P=$TEST_PV_PREFIX$PYPROC_ID:,NELM=$TEST_CIRCBUFF_SIZE,CA_LNK=$TEST_CA_LNK -d $TEST_DB'; \
echo Done: `date` | tee -a $TEST_LOG


#!/bin/bash
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

PYPROCMGR=`which pyProcMgr.py`
if [ ! -e "$PYPROCMGR" ]; then
	echo "Error: pyProcMgr.py not found!"
	exit 1
fi

TOP=`readlink -f $(dirname $LOADSERVER)/../..`
echo TOP = $TOP

HOSTNAME=`hostname -s`
TESTNAME=pva-gw-ctrs4
TEST_DIR=/reg/d/iocData/gwTest/$TESTNAME/$HOSTNAME/servers
mkdir -p $TEST_DIR
cat /proc/cpuinfo > $TEST_DIR/cpu.info
cat /proc/meminfo > $TEST_DIR/mem.info
cd $TOP
EPICS_PVA_SERVER_PORT=5086 EPICS_PVA_BROADCAST_PORT=5086 $PYPROCMGR -c 10  \
	-n loadServer -p 50000 \
	-D $TEST_DIR \
	$LOADSERVER \
	'-m DELAY=0.010,P=PVA:GW:TEST:$PYPROC_ID:,NELM=10 -d db/drive_100Counters.db'

#$LOADSERVER	\
#	-m DELAY=1.0,P=PVA:GW:TEST:1:,NELM=10	\
#	-d $TOP/db/drive_10Counters.db



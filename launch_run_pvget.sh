#!/bin/bash

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

#echo PYPROCMGR = $PYPROCMGR
echo TOP = $TOP

HOSTNAME=`hostname -s`
TESTNAME=pva-ctrs-360

export N_CLIENTS=10
echo N_CLIENTS=$N_CLIENTS

export CLIENT=pvget
PORT=43000

export TEST_DIR=/reg/d/iocData/gwTest/$TESTNAME/$HOSTNAME/clients
#export TEST_DIR=/reg/d/iocData/gwTest/$TESTNAME/$HOSTNAME/$CLIENT
mkdir -p $TEST_DIR

pkill -9 run_pvget.sh
pkill -9 pvget

#echo TEST_DIR=$TEST_DIR
#echo $PYPROCMGR -c $N_CLIENTS ...
$PYPROCMGR -c $N_CLIENTS -n $CLIENT -p $PORT -D $TEST_DIR "$TOP/run_pvget.sh" '$TEST_DIR/$CLIENT$PYPROC_ID' 'PVA:GW:TEST:$PYPROC_ID:Rate0 PVA:GW:TEST:$PYPROC_ID:Rate1 PVA:GW:TEST:$PYPROC_ID:Rate2 PVA:GW:TEST:$PYPROC_ID:Rate3 PVA:GW:TEST:$PYPROC_ID:Rate4 PVA:GW:TEST:$PYPROC_ID:Rate5 PVA:GW:TEST:$PYPROC_ID:Rate6 PVA:GW:TEST:$PYPROC_ID:Rate7 PVA:GW:TEST:$PYPROC_ID:Rate8 PVA:GW:TEST:$PYPROC_ID:Rate9'


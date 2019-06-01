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

export N_CLIENTS=1
echo N_CLIENTS=$N_CLIENTS

export CLIENT=pvget_gw
PORT=43100

export TEST_DIR=/reg/d/iocData/gwTest/$TESTNAME/$HOSTNAME/clients
#export TEST_DIR=/reg/d/iocData/gwTest/$TESTNAME/$HOSTNAME/$CLIENT
mkdir -p $TEST_DIR

#pkill -9 run_pvget.sh
#pkill -9 pvget

#echo TEST_DIR=$TEST_DIR
#echo $PYPROCMGR -c $N_CLIENTS ...
$PYPROCMGR -v -c $N_CLIENTS -n $CLIENT -p $PORT -D $TEST_DIR "$TOP/run_pvget.sh" '$TEST_DIR/$CLIENT$PYPROC_ID' 'gw:stats gw:us:byhost:rx gw:us:byhost:tx gw:us:bypv:rx gw:us:bypv:tx gw:ds:byhost:rx gw:ds:byhost:tx gw:ds:bypv:rx gw:ds:bypv:tx'


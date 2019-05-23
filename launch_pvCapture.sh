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

export TEST_DIR=/reg/d/iocData/gwTest1

TOP=`readlink -f $(dirname ${BASH_SOURCE[0]})`

echo PYPROCMGR = $PYPROCMGR
echo TOP = $TOP

export N_CLIENTS=5
export N_SERVERS=10
export N_COUNTERS=10
export N_PVS_PER_CLIENT=$((($N_SERVERS*$N_COUNTERS+$N_CLIENTS-1)/$N_CLIENTS))
echo N_CLIENTS=$N_CLIENTS
echo N_SERVERS=$N_SERVERS
echo N_COUNTERS=$N_COUNTERS
echo N_PVS_PER_CLIENT=$N_PVS_PER_CLIENT

export CLIENT=pvCapture
PORT=42000

cd $TOP

P=0
C=0
mkdir -p $TEST_DIR/$CLIENT${C}
cat /dev/null > $TEST_DIR/$CLIENT${C}/pvs.list
for (( N = 0; N < $N_COUNTERS ; ++N )) do
	for (( S = 0; S < $N_SERVERS ; ++S )) do
		echo PVA:GW:TEST:${S}:Count${N} >> $TEST_DIR/$CLIENT${C}/pvs.list
		P=$(($P+1))
		if (( $P >= $N_PVS_PER_CLIENT )) ; then
			P=0
			C=$(($C+1))
			if (( $C < $N_CLIENTS )) ; then
				mkdir -p $TEST_DIR/$CLIENT${C}
				cat /dev/null > $TEST_DIR/$CLIENT${C}/pvs.list
			fi
		fi
	done
done
#echo $PYPROCMGR -c $N_CLIENTS ...
$PYPROCMGR -c $N_CLIENTS -n $CLIENT -p $PORT -D $TEST_DIR 'bin/$EPICS_HOST_ARCH/pvCapture -D $TEST_DIR -f $TEST_DIR/$CLIENT$PYPROC_ID/pvs.list'



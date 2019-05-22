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

export N_CLIENTS=3
export N_SERVERS=2
export N_COUNTERS=8
export N_PVS_PER_CLIENT=$((($N_SERVERS*$N_COUNTERS+$N_CLIENTS-1)/$N_CLIENTS))
echo N_CLIENTS=$N_CLIENTS
echo N_SERVERS=$N_SERVERS
echo N_COUNTERS=$N_COUNTERS
echo N_PVS_PER_CLIENT=$N_PVS_PER_CLIENT

export CLIENT=pvCapture
PORT=41000

cd $TOP
declare -a PVS
PVS=

C=0
P=0
for (( N = 0; N < $N_COUNTERS ; ++N )) do
	for (( S = 0; S < $N_SERVERS ; ++S )) do
		PVS[P]=PVA:GW:TEST:${S}:Count${N}
		P=$(($P+1))
		if (( $P >= $N_PVS_PER_CLIENT )) ; then
			mkdir -p $TEST_DIR/$CLIENT${C}
			echo ${PVS[*]} > $TEST_DIR/$CLIENT${C}/pvs.list
			PVS=
			P=0
			C=$(($C+1))
		fi
	done
done
if (( $P > 0 )) ; then
	mkdir -p $TEST_DIR/$CLIENT${C}
	echo -e ${PVS[*]// /\\n} > $TEST_DIR/$CLIENT${C}/pvs.list
	#echo client $C Length PVS=${#PVS[*]}
	#echo client $C PVS=${PVS[*]}
fi
$PYPROCMGR -c $N_CLIENTS -n $CLIENT -p $PORT -D $TEST_DIR bin/$EPICS_HOST_ARCH/pvCapture '-f $TEST_DIR/$CLIENT\$PYPROC_ID/pvs.list'



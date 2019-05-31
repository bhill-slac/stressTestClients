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

echo PYPROCMGR = $PYPROCMGR
echo TOP = $TOP

TESTNAME=pva-gw-ctrs4
export TEST_DIR=/reg/d/iocData/gwTest/$TESTNAME/$HOSTNAME/clients
mkdir -p $TEST_DIR
cat /proc/cpuinfo > $TEST_DIR/cpu.info
cat /proc/meminfo > $TEST_DIR/mem.info

export N_SERVERS=10
export N_CLIENTS=20
export N_CNT_PER_SERVER=100

# Testing
#export N_SERVERS=3
#export N_CNT_PER_SERVER=5
#export N_CLIENTS=4

export N_PVS_PER_CLIENT=$((($N_SERVERS*$N_CNT_PER_SERVER+$N_CLIENTS-1)/$N_CLIENTS))
echo N_CLIENTS=$N_CLIENTS
echo N_SERVERS=$N_SERVERS
echo N_CNT_PER_SERVER=$N_CNT_PER_SERVER
echo N_PVS_PER_CLIENT=$N_PVS_PER_CLIENT

export CLIENT=pvCapture
PORT=42000

P=0
C=0
mkdir -p $TEST_DIR/$CLIENT${C}
cat /dev/null > $TEST_DIR/$CLIENT${C}/pvs.list
for (( N = 0; N < $N_CNT_PER_SERVER ; ++N )) do
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
cd $TOP
$PYPROCMGR -v -c $N_CLIENTS -n $CLIENT -p $PORT -d 5.0 -D $TEST_DIR 'bin/$EPICS_HOST_ARCH/pvCapture -D $TEST_DIR/$CLIENT$PYPROC_ID -f $TEST_DIR/$CLIENT$PYPROC_ID/pvs.list'



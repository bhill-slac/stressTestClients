#!/bin/bash
LAUNCH_SCRIPT=$(basename ${BASH_SOURCE[0]})
SCRIPTDIR=`readlink -f $(dirname ${BASH_SOURCE[0]})`
if [ $# -lt 2 -o "$1" == "-h" -o "$2" == "-h" -o "$1" == "--help" ]; then
	echo Usage: $LAUNCH_SCRIPT testTop clientName
	exit 1
fi
TEST_TOP=$1
TEST_NAME=$(basename $TEST_TOP)
HOSTNAME=`hostname -s`

# Setup site specific environment
if [ -f $SCRIPTDIR/setup_site.sh ]; then
	source $SCRIPTDIR/setup_site.sh 
else
	echo Unable to setup site environment via soft link setup_site.sh
	echo Full path: $SCRIPTDIR/setup_site.sh 
	exit 1
fi

# Read env files for test
function readIfFound()
{
	if [ -r $1 ]; then
		source $1;
	fi
}
readIfFound $SCRIPTDIR/stressTestDefault.env
readIfFound $TEST_TOP/../siteDefault.env
readIfFound $TEST_TOP/siteDefault.env
readIfFound $TEST_TOP/test.env

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

TEST_HOST_DIR=$STRESSTEST_TOP/$TEST_NAME/$HOSTNAME
TEST_DIR=$TEST_HOST_DIR/clients
mkdir -p $TEST_DIR
#TEST_LOG=$TEST_HOST_DIR/$TEST_NAME-${TEST_APPTYPE}.log
#TEST_LOG=$TEST_HOST_DIR/$CLIENT_NAME/${CLIENT_NAME}.log
TEST_LOG=$TEST_DIR/${CLIENT_ROOT}.log

$SCRIPTDIR/logStartOfTest.sh | tee $TEST_LOG

# export variables that will be expanded by pyProcMgr
export TEST_DIR 

# Run test on host
$PYPROCMGR -v -c $TEST_N_CLIENTS -n $CLIENT_ROOT \
	-p $TEST_BASEPORT -d $TEST_DELAY_PER_CLIENT -D $TEST_DIR \
	"CLIENT_CMD"; \
echo Done: `date` | tee -a $TEST_LOG

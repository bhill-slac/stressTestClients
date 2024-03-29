#!/bin/bash
source /reg/g/pcds/package/conda/env-miniconda3.sh
conda activate pva-gw
source $SETUP_SITE_TOP/epicsenv-bleeding_edge-7.0.sh 
#source $SETUP_SITE_TOP/epicsenv-7.0.2-2.0.sh
source $PSPKG_ROOT/etc/add_env_pkg.sh procServ/2.7.0-1.3.0

echo $(echo EPICS_BASE=$EPICS_BASE)

P4P_TOP=/reg/neh/home/bhill/git-wa-neh/extensions/p4p-git 
PY_LD_VER=`python $P4P_TOP/get_PY_LD_VER.py`

#echo pythonpathmunge $PWD/python${PY_LD_VER}/${EPICS_HOST_ARCH}
#source /reg/g/pcds/setup/pathmunge.sh
pythonpathmunge $P4P_TOP/python${PY_LD_VER}/${EPICS_HOST_ARCH}

pathmunge /reg/neh/home/bhill/git-wa-neh/extensions/loadServer-git/bin/$EPICS_HOST_ARCH
pathmunge /reg/neh/home/bhill/git-wa-neh/misc/pyProcMgr-git

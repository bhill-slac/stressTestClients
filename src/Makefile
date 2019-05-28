TOP=..

include $(TOP)/configure/CONFIG

USR_CXXFLAGS += -O0

PROD_HOST += pvCapture
pvCapture_SRCS += pvCapture.cpp
pvCapture_SRCS += pvCollector.cpp

PROD_HOST += pvGet
pvGet_SRCS += pvGet.cpp

#PROD_HOST += pvget_tst
#pvget_tst_SRCS += pvget_tst.cpp
#pvget_tst_SRCS += pvutils.cpp

#PROD_HOST += getme_tst
#getme_tst_SRCS += getme_tst.cpp

#PROD_HOST += pvputx
#pvputx_SRCS += pvput.cpp

PROD_LIBS += pvAccessCA pvAccess pvData ca Com

PROD_SYS_LIBS_WIN32 += ws2_32

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE


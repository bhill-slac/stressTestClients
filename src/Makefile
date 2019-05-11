TOP=..

include $(TOP)/configure/CONFIG

PROD_HOST += pvCapture
pvCapture_SRCS += pvCapture.cpp

#PROD_HOST += pvputx
#pvputx_SRCS += pvput.cpp

PROD_LIBS += pvAccessCA pvAccess pvData ca Com

PROD_SYS_LIBS_WIN32 += ws2_32

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE


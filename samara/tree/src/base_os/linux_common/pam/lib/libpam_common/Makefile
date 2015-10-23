#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=pam_common
OBJ_PIC=1

SRCS=   \
	lpc_main.c \
	lpc_random.c \

LIB_BUILD_DIR=${ILIB_DIR}/base_os/linux_common/pam/lib/libpam_common

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/linux_common/pam/include \

DEFINES= \
	-D_GNU_SOURCE \


#WARNINGS_OVERRIDE=1
#WARNINGS= \
#	-Werror \
#	-Wall \

ifeq ($(call CC_VERSION_ATLEAST,40000,1,0),1)
	WARNINGS+= \
		-Wno-pointer-sign \

endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk

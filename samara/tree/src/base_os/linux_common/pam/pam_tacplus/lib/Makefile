#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=pam_tacplus
LIB_BUILD_DIR=${LIB_DIR}/security/pam_tacplus
LIB_INSTALL_DIR=lib/security
LIB_DYNAMIC=1
LDADD+= \
	-lpam \
	-lcrypto \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/linux_common/pam/pam_tacplus/libtac/libtac.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/linux_common/pam/lib/libpam_common/libpam_common.a \

LIB_DYNAMIC_SONAME=
LIB_DYNAMIC_REALNAME=
LIB_DYNAMIC_PREFIX=

vpath %.c ../src

SRCS=   \
	pam_tacplus.c \
	pam_tacplus.h \
	support.h \
	support.c \

INCLUDES= -I../src \
	-I../src/libtac/include \
	-I../src/libtac/lib \
	-I../../include \

DEFINES=-D_GNU_SOURCE

ifeq ($(call CC_VERSION_ATLEAST,40000,1,0),1)
	WARNINGS+= \
		-Wno-pointer-sign \

endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk

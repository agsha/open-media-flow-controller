#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=pam_radius_auth
LIB_BUILD_DIR=${LIB_DIR}/security/pam_radius
LIB_INSTALL_DIR=lib/security
LIB_DYNAMIC=1

LDADD+= \
	-lpam \
	-lcrypto \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/linux_common/pam/lib/libpam_common/libpam_common.a \

LIB_DYNAMIC_SONAME=
LIB_DYNAMIC_REALNAME=
LIB_DYNAMIC_PREFIX=

LICENSE_REQUIRES_REDIST=1

vpath %.c ../src
vpath %.c ${PROD_TREE_ROOT}/src/lib/libcommon
vpath %.c ${PROD_TREE_ROOT}/src/lib/libcommon/BIN_DIST/security/BIN_DIST

SRCS=   \
	md5.c \
	md5.h  \
	pam_radius_auth.c \
	pam_radius_auth.h \
	radius.h \
	escape_utils.c \


INCLUDES= \
	-I../src \
	-I../../include \

DEFINES=-D_GNU_SOURCE

ifeq ($(call CC_VERSION_ATLEAST,40000,1,0),1)
	WARNINGS+= \
		-Wno-pointer-sign \

endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk

#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=dhclient
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/sbin
#PROG_STRIP=1

vpath %.c ../../src/client

SRCS=	\
	clparse.c \
	dhc6.c \
	dhclient.c \

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/linux_common/dhcp/src/includes \

DEFINES_OVERRIDE=1
DEFINES= \
	-DHAVE_CONFIG_H \
	-DLOCALSTATEDIR=\"/var\" \
	-DCLIENT_PATH=\"/usr/sbin\" \
	-DLOCALSTATEDT \

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Werror \
	-Wall \

LDADD=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/linux_common/dhcp/lib/libdhcp/libdhcp.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/linux_common/dhcp/lib/libdhcpaux/libdhcpaux.a \
	-lcrypto \


ifeq (${PROD_TARGET_PLATFORM_FULL},LINUX_EL_EL6)
	LDADD+= \
		-lcap-ng \

endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk

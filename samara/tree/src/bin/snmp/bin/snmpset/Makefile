#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=snmpset
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/usr/bin
PROG_STRIP=1

vpath %.c ../../net-snmp/apps

SRCS=	\
	snmpset.c \

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent/mibgroup \

DEFINES= \
	-D_SVID_SOURCE \
	-D_GNU_SOURCE \
	-DUCD_COMPATIBLE

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Wall \
	-Wstrict-prototypes \
	-Wno-unused \

ifeq ($(call CC_VERSION_ATLEAST,40000,1,0),1)
	WARNINGS+= \
		-Wno-pointer-sign \

endif

LDADD=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/snmp/lib/libnetsnmp/libnetsnmp.a \

ifeq (${PROD_TARGET_OS},LINUX)
    DEFINES+= -Dlinux
    LDADD+= -lcrypto
endif

ifeq (${PROD_TARGET_OS},FREEBSD)
    CFLAGS+= -Dfreebsd6 -DNETSNMP_NO_INLINE
    LDADD+= -lcrypto
endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk

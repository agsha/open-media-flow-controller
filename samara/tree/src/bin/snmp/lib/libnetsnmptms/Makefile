#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=netsnmptms

vpath %.c ../../tms/
vpath %.c ../../tms/BIN_DIST

SRCS=   \
	sn_config.c \
	sn_main.c \
	sn_mgmt.c \
	sn_modules.c \
	sn_notifs.c \
	sn_registration.c \
	sn_scalars.c \
	sn_sets.c \
	sn_tables_new.c \
	sn_type_conv.c \
	sn_utils.c \

# These source files went with the old tables implementation, and are
# no longer used.
# SRCS+=  \
#	sn_cache.c \
#	sn_tables_old.c \

LIB_BUILD_DIR=${ILIB_DIR}/bin/snmp/lib/libnetsnmptms

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include/net-snmp/agent \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent/mibgroup \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/snmplib \
	-I${PROD_TREE_ROOT}/src/bin/snmp/tms \

DEFINES= \
	-D_GNU_SOURCE \


CFLAGS+= \
	-fno-strict-aliasing \

WARNINGS+= \
	-Wno-sign-compare \

# XXXX/EMT: can we get rid of this?
WARNINGS+= \
	-Wno-redundant-decls \

ifdef STATIC_MODULES
    DEFINES+=-DSTATIC_MODULES
endif

ifeq (${PROD_TARGET_OS},SUNOS)
    CFLAGS+= -std=gnu89
    DEFINES+= -Dsolaris2
    INCLUDES+= -I/usr/local/ssl/include
endif

ifeq (${PROD_TARGET_OS},FREEBSD)
	DEFINES+= -Dfreebsd6 -DNETSNMP_NO_INLINE
endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk

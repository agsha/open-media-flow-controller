#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

NAME=snmp_mods

ifdef STATIC_MODULES
    LIB=${NAME}
    LIB_BUILD_DIR=${ILIB_DIR}/bin/snmp/tms/modules
else
    SO_CLASS=snmp
endif

SRCS=                              \
	sn_mib_II.c                \

ifneq (${PROD_FEATURE_TMS_MIB},0)
SRCS+=                             \
	sn_mib_tms.c
endif

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include/net-snmp/agent \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent/mibgroup \
	-I${PROD_TREE_ROOT}/src/bin/snmp/tms \

DEFINES= \
	-D_SVID_SOURCE \
	-D_GNU_SOURCE \

WARNINGS+= \
	-Wno-redundant-decls \

# If we don't have any files to build / install the build system gets unhappy.
ifneq ($(strip ${SRCS}),)

ifdef STATIC_MODULES
    include ${PROD_TREE_ROOT}/src/mk/lib.mk
else
    include ${PROD_TREE_ROOT}/src/mk/so.mk
endif

endif

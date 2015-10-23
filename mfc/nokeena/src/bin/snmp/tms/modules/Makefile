# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

ifdef STATIC_MODULES
    LIB=snmp_generic_mods
else
    SO_CLASS=snmp
endif

SRCS=                              \
	sn_mib_mfd.c  \

INCLUDES= \
        -I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include \
        -I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include/net-snmp/agent \
        -I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent \
        -I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent/mibgroup \
        -I${PROD_TREE_ROOT}/src/bin/snmp/tms \
        -I${PROD_TREE_ROOT}/src/include/bin/snmp/net-snmp/include \
        -I${PROD_TREE_ROOT}/src/include/bin/snmp/net-snmp/include/net-snmp/agent \
        -I${PROD_TREE_ROOT}/src/include/bin/snmp/net-snmp/agent \
        -I${PROD_TREE_ROOT}/src/include/bin/snmp/net-snmp/agent/mibgroup \
        -I${PROD_TREE_ROOT}/src/include/bin/snmp/tms \

DEFINES= \
        -D_SVID_SOURCE \
        -D_GNU_SOURCE \

WARNINGS_OVERRIDE=1
WARNINGS= \
#        -Wall \
#        -Wstrict-prototypes \
#        -Wno-unused \

ifdef STATIC_MODULES
    include ${PROD_TREE_ROOT}/src/mk/lib.mk
else
    include ${PROD_TREE_ROOT}/src/mk/so.mk
endif

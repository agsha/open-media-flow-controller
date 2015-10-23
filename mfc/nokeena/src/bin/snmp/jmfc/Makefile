# Filename: bin/snmp/jmfc/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=jmfc-snmp
LIB_DYNAMIC=1
LIB_BUILD_DIR=${CUST_LIB_DIR}/lib/snmp/modules
LIB_DYNAMIC_REALNAME=
PROD_INSTALL_PREFIX=/opt/tms/lib/snmp/
LIB_INSTALL_DIR=modules

SRCS=							\
	snmp_util.c					\
	jmfc_snmp.c					\
	jmfcInterfaceTable.c				\
	jmfcInterfaceTable_data_access.c		\
	jmfcInterfaceTable_data_get.c			\
	jmfcInterfaceTable_data_set.c			\
	jmfcInterfaceTable_interface.c



INCLUDES= \
        -I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include \
        -I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include/net-snmp/agent \
        -I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent \
        -I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent/mibgroup \
        -I${PROD_TREE_ROOT}/src/include/bin/snmp/net-snmp/include \
        -I${PROD_TREE_ROOT}/src/include/bin/snmp/net-snmp/include/net-snmp/agent \
        -I${PROD_TREE_ROOT}/src/include/bin/snmp/net-snmp/agent \
        -I${PROD_TREE_ROOT}/src/include/bin/snmp/net-snmp/agent/mibgroup \

CFLAGS += \
	 -DNKN_BUILD_ID=\"${NKN_BUILD_ID}\" \
	 -DNKN_BUILD_PROD_RELEASE=\"${NKN_BUILD_PROD_RELEASE}\" \

LDFLAGS += -lcprops -lssl

LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncnt/libnkncnt.a \

DEFINES= \
        -D_SVID_SOURCE \
        -D_GNU_SOURCE \

WARNINGS_OVERRIDE=1
WARNINGS= \
        -Wall \
        -Wstrict-prototypes \
        -Wno-unused \

include ${PROD_TREE_ROOT}/src/mk/lib.mk

#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=snmpd

vpath %.c ../../net-snmp/agent

SRCS=	\
	snmpd.c \

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent/mibgroup \
	-I${PROD_TREE_ROOT}/src/bin/snmp/tms \

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

LDADD+=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/snmp/lib/libnetsnmpaux/libnetsnmpaux.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/snmp/lib/libnetsnmptms/libnetsnmptms.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/snmp/lib/libnetsnmp/libnetsnmp.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	-lcrypto \
	-lz \
	-lpthread \

PROG_PERM_MODE=750

# Graft point for customer-specific additions to build options for snmpd.
# e.g. a customer may want to make snmpd link with additional libraries
# by adding stuff to LDADD.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/bin/snmp/bin/snmpd/Makefile.inc

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# Make sure our symbols and libraries get dealt with correctly
USES_MODULES=1


ifdef STATIC_MODULES
    CFLAGS+=-DSTATIC_MODULES
    LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/snmp/tms/modules/libsnmp_mods.a \

endif

ifeq (${PROD_TARGET_OS},LINUX)
    DEFINES+= -Dlinux
endif

ifeq (${PROD_TARGET_OS},SUNOS)
    CFLAGS+= -std=gnu89
    DEFINES+= -Dsolaris2
    SRCS+= mibgroup/kernel_sunos5.c
    LDADD+= -L/usr/local/ssl/lib
    LDADD+= -lkstat
endif

ifeq (${PROD_TARGET_OS},FREEBSD)
    CFLAGS+= -Dfreebsd6 -DNETSNMP_NO_INLINE
    LDADD+= -lkvm
endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk

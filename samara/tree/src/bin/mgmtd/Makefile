#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=mgmtd

SRCS= \
	md_commit.c \
	md_audit.c \
	md_email_utils.c \
	md_message.c \
	md_ext.c \
	md_main.c \
	md_mod_reg.c \
	md_modules.c \
	md_mon.c \
	md_mon_map.c \
	md_net_utils.c \
	md_passthrough.c \
	md_upgrade.c \
	md_utils.c \
	md_utils_internal.c \
	mdm_acl.c \
	mdm_changes.c \
	mdm_db.c \
	mdm_events.c \
	mdm_license.c \
	mdm_license_events.c \
	mdm_license_mon.c \
	mdm_license_structs.c \
	mdm_adv_query.c \


ifneq (${PROD_FEATURE_GRAPHING},0)
	SRCS+=                      \
		md_graph_utils.c    \

endif

ifneq (${PROD_FEATURE_OLD_GRAPHING},0)
	SRCS+=                      \
		md_graph_utils_old.c    \

endif

SUBDIRS= \
	modules \
	templates

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/lib/libevent \
	-I${PROD_TREE_ROOT}/src/lib/libgcl \
	-I${PROD_TREE_ROOT}/src/lib/libmdb \
	-I${PROD_TREE_ROOT}/src/lib/libgd \

LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdb/libmdb.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtexpr/libtexpr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/liblicense/liblicense.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/liblicense_validation/liblicense_validation.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcgi/libcgi.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libweb/libweb.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtacl/libtacl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtcrypt/libtcrypt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	-lcrypt \


ifneq (${PROD_FEATURE_GRAPHING},0)
	LDADD+= \
		${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgraph/libgraph.a \
		${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgd/libgd.a \
		-lpng \
		-lfreetype \
		-lm \
		-lz \

endif

ifneq (${PROD_FEATURE_SECURE_NODES},0)
	SRCS+= md_crypt.c
	LDADD+= \
		-lcrypto \
		-lz \

endif

ifeq (${PROD_TARGET_ARCH_FAMILY},X86)

# Uncomment if you want to be able to use Valgrind's client API to
# control Valgrind from within the code.  If this is enabled, mgmtd
# will use this API to exempt the initial commit and shutdown from
# Callgrind runs, which can give more accurate results in many cases.
# This can be overridden using other constants documented below.
# 
# The generated binary works under Valgrind and not under Valgrind, so you
# could leave this enabled always.  This will mean that build system
# then needs to have the valgrind-devel RPM (or some other way of
# getting the Valgrind headers), although it does not mean that the
# target needs Valgrind installed, unless you want to run under
# Valgrind.
#
# The client request headers are included, so that mgmtd code may make
# client requests for base Valgrind, callgrind and memcheck.  Also, by
# default this will cause callgrind usage to not include the initial
# commit.

##	CFLAGS+= \
##		-DVALGRIND_ENABLE=1 \

# If you are using Valgrind's 'callgrind' tool to measure performance,
# should we include mgmtd's initial commit or not?  Defaults to not
# including the initial commit.
####	-DVALGRIND_CALLGRIND_MGMTD_INIT=1 \

# Similarly, should mgmtd's shutdown be included or not?  Defaults to
# not being included.
####	-DVALGRIND_CALLGRIND_MGMTD_DEINIT=1 \

endif

# Graft point for customer-specific additions to build options for mgmtd.
# e.g. a customer may want to make mgmtd link with additional libraries
# by adding stuff to LDADD.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/bin/mgmtd/Makefile.inc

# Make sure our symbols and libraries get dealt with correctly
USES_MODULES=1

ifdef STATIC_MODULES
    CFLAGS+=-DSTATIC_MODULES
    LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/mgmtd/modules/libmd_mods.a \

endif

ifeq (${PROD_TARGET_OS},FREEBSD)
	LDADD+= -L/usr/local/lib
endif

ifeq (${PROD_TARGET_OS},SUNOS)
    LDADD+= \
	-lm \
	-lz \

endif

PROG_PERM_MODE=750

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# WARNING: Only enable MD_PROVIDER_INET for testing, or in a very
# carefully constrained environment.  It causes mgmtd to listen on IPv4
# or IPv6 addresses.  You will need to specify the addresses to listen
# on in your customer.h using GCL_PROVIDER_INET_MGMTD_IPV4ADDR and
# GCL_PROVIDER_INET_MGMTD_IPV6ADDR .  You will also need to turn on
# GCL_PROVIDER_INET in your customer.h Please see DEMO's customer.h for
# more information.
#
# There is no security of any kind for these INET connections.

## DEFINES+=-DMD_PROVIDER_INET=1

include ${PROD_TREE_ROOT}/src/mk/prog.mk

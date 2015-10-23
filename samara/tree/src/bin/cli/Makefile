#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=cli

SRCS=                          \
	clish_completion.c     \
	clish_event.c          \
	clish_help.c           \
	clish_history.c        \
	clish_main.c           \
	clish_mgmt.c           \
	clish_output.c         \
	clish_progress.c       \
	clish_utils.c          \

SUBDIRS=                       \
	modules                \

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

CFLAGS= -I${PROD_TREE_ROOT}/src/lib/libtecla

# XXX these should be done in a more compact way
LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtecla/libtecla.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcli/libcli.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtacl/libtacl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtcrypt/libtcrypt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	-lcrypt \

# For libtecla
ifeq (${PROD_TARGET_PLATFORM_FULL},LINUX_EL_EL6)
	LDADD+= \
		-ltinfo
else
	# XXX/EMT: do we want lncursesw?
	LDADD+= \
		-lncurses
endif

# Graft point for customer-specific additions to build options for cli.
# e.g. a customer may want to make cli link with additional libraries
# by adding stuff to LDADD.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/bin/cli/Makefile.inc

# Make sure our symbols and libraries get dealt with correctly
USES_MODULES=1

ifdef STATIC_MODULES
    CFLAGS+=-DSTATIC_MODULES
    LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/cli/modules/libcli_mods.a \

endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk

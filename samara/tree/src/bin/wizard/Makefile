#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=wizard

SRCS=   \
	wiz_engine.c         \
	wiz_main.c           \
	wiz_mgmt.c           \
	wiz_modules.c        \
	wiz_navigation.c     \
	wiz_registration.c   \
	wiz_std_handlers.c   \
	wiz_utils.c          \

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
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtecla/libtecla.a \
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

# Graft point for customer-specific additions to build options for wizard.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/bin/wizard/Makefile.inc

# Make sure our symbols and libraries get dealt with correctly
USES_MODULES=1

ifdef STATIC_MODULES
    CFLAGS+=-DSTATIC_MODULES
    LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/wizard/modules/libwiz_mods.a \

endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk

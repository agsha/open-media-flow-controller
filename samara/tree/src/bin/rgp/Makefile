#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=rgp

SRCS= \
	rgp_actions.c          \
	rgp_config.c           \
	rgp_local.c            \
	rgp_main.c             \
	rgp_mdc_handlers.c     \
	rgp_outer.c            \
	rgp_proxy.c            \
	rgp_session.c          \


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

CFLAGS= -I${PROD_TREE_ROOT}/src/lib/libevent -I${PROD_TREE_ROOT}/src/lib/libgcl

# XXX these should be done in a more compact way
LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcli/libcli.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtacl/libtacl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libclish/libclish.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtcrypt/libtcrypt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libremgcl/libremgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	-lcrypt \

##PROG_PERM_MODE=750

# Graft point for customer-specific additions to build options for RGP.
# e.g. a customer may want to make RGP link with additional libraries
# by adding stuff to LDADD.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/bin/rgp/Makefile.inc

# Make sure our symbols and libraries get dealt with correctly
USES_MODULES=1

ifdef STATIC_MODULES
    CFLAGS+=-DSTATIC_MODULES
    LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/cli/modules/libcli_mods.a \

endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk

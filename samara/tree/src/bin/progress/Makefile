#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=progress

SRCS= \
	pro_main.c \
	pro_mod_api.c \
	pro_mod_internal.c \

INCLUDES= -I${PROD_TREE_ROOT}/src/lib/libevent

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \

# Graft point for customer-specific additions to build options for progress.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/bin/progress/Makefile.inc

# Make sure our symbols and libraries get dealt with correctly
USES_MODULES=1

ifdef STATIC_MODULES
    CFLAGS+=-DSTATIC_MODULES
    LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/progress/modules/libpro_mods.a \

endif

SUBDIRS= \
	modules \

include ${PROD_TREE_ROOT}/src/mk/prog.mk

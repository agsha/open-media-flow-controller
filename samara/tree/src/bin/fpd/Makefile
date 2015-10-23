#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=fpd

SRCS=   \
	fp_config.c            \
	fp_confirm.c           \
	fp_edit.c              \
	fp_io.c                \
	fp_leds.c              \
	fp_main.c              \
	fp_marquee.c           \
	fp_menu.c              \
	fp_mgmt.c              \
	fp_modules.c           \
	fp_reg.c               \
	fp_timers.c            \
	fp_ui.c                \

SUBDIRS=                       \
	modules                \
	utils                  \

CFLAGS=

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# XXX these should be done in a more compact way
LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \

# Graft point for customer-specific additions to build options for fpd.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/bin/fpd/Makefile.inc

# Make sure our symbols and libraries get dealt with correctly
USES_MODULES=1

ifdef STATIC_MODULES
    CFLAGS+=-DSTATIC_MODULES
    LDADD+=\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/fpd/modules/io/libfpd_io_mods.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/fpd/modules/ui/libfpd_ui_mods.a \

endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk

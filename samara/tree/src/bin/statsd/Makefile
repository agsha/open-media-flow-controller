#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=statsd

SRCS=                      \
	st_alarm.c         \
	st_chd.c           \
	st_config.c        \
	st_data.c          \
	st_file.c          \
	st_main.c          \
	st_mgmt.c          \
	st_modules.c       \
	st_monitor.c       \
	st_query.c         \
	st_registration.c  \
	st_report.c        \
	st_sample.c        \

SUBDIRS=                       \
	modules                \
	stats_fix_file         \

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# XXX these should be done in a more compact way
LDADD+=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/lib/libgcl \

# Graft point for customer-specific additions to build options for statsd.
# e.g. a customer may want to make statsd link with additional libraries
# by adding stuff to LDADD.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/bin/statsd/Makefile.inc

# Make sure our symbols and libraries get dealt with correctly
USES_MODULES=1

ifdef STATIC_MODULES
    CFLAGS+=-DSTATIC_MODULES
    LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/statsd/modules/libstats_mods.a \

endif

PROG_PERM_MODE=750

include ${PROD_TREE_ROOT}/src/mk/prog.mk

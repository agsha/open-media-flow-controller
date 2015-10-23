#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=rbmd

SRCS= \
	rbmd_main.c            \
	rbmd_remote.c          \
	rbmd_mgmt.c            \
	rbmd_mon.c             \
	rbmd_events.c	       \
	rbmd_config.c	       \
	rbmd_status.c          \
	rbmd_jobs.c            \
	rbmd_provider.c        \
	rbmd_proxy.c           \
	rbmd_types.c           \


CFLAGS= -I${PROD_TREE_ROOT}/src/lib/libgcl

# XXX these should be done in a more compact way
LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libremgcl/libremgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \

##PROG_PERM_MODE=750

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk


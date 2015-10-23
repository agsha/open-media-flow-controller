#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=pm

SRCS=                      \
	pm_actions.c       \
	pm_config.c        \
	pm_crash.c         \
	pm_defs.c          \
	pm_dynproc.c       \
	pm_global_state.c  \
	pm_liveness.c      \
	pm_main.c          \
	pm_mgmt.c          \
	pm_mon.c           \
	pm_mon_sys.c       \
	pm_proc_array.c    \
	pm_process.c       \
	pm_shutdown.c      \
	pm_signal.c        \
	pm_startup.c       \

SUBDIRS=                       \
	pm_create_snapshot     \

CFLAGS= -I${PROD_TREE_ROOT}/src/bin/mgmtd

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# XXX these should be done in a more compact way
LDADD=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdb/libmdb.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtexpr/libtexpr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtacl/libtacl.a \


PROG_PERM_MODE=750

include ${PROD_TREE_ROOT}/src/mk/prog.mk

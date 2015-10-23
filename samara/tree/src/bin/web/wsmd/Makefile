#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=wsmd

SRCS= \
	wsmd_auth.c             \
	wsmd_inner.c            \
	wsmd_main.c             \
	wsmd_outer.c            \
	wsmd_session.c          \
	wsmd_httpd_secure.c     \


ifneq (${PROD_FEATURE_CAPABS},0)
	SRCS+=                      \
		wsmd_capabs.c       \

endif

ifneq (${PROD_FEATURE_ACLS},0)
	SRCS+=                      \
		wsmd_acls.c         \

endif


CFLAGS= -I${PROD_TREE_ROOT}/src/lib/libevent -I${PROD_TREE_ROOT}/src/lib/libgcl

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# XXX these should be done in a more compact way
LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcgi/libcgi.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libweb/libweb.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcgi/libcgi.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtacl/libtacl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	-lpam \

PROG_PERM_MODE=750

include ${PROD_TREE_ROOT}/src/mk/prog.mk


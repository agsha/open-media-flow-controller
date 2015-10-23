#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=launch
PROG_INSTALL_DIR=/lib/web/cgi-bin

PROG_PERM_OWNER=root

# XXX/EMT: could we say "apache" here?  Presuming not since that
# user may not exist with same gid on build machine.
PROG_PERM_GROUP=48

PROG_PERM_MODE=4750

SRCS= \
	lau_auth.c \
	lau_debug.c \
	lau_main.c

CFLAGS= -I${PROD_TREE_ROOT}/src/lib/libevent -I${PROD_TREE_ROOT}/src/lib/libgcl

##CFLAGS+= -DLAU_DEBUG_LOOP
##CFLAGS+= -DCGI_DEBUG

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# XXX these should be done in a more compact way
LDADD=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcgi/libcgi.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \


include ${PROD_TREE_ROOT}/src/mk/prog.mk


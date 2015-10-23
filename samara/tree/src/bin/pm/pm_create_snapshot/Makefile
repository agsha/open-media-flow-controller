#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=pm_cs

SRCS=                      \
	pcs_main.c         \

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# XXX these should be done in a more compact way
LDADD=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \


PROG_PERM_MODE=750

include ${PROD_TREE_ROOT}/src/mk/prog.mk

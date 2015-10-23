#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=remgcl

SRCS= \
	remgcl.c \

CFLAGS=

# XXX these should be done in a more compact way
LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk

#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=clish

SRCS=                              \
	lcs_main.c                 \

CFLAGS=

# XXX these should be done in a more compact way
LDADD=

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk

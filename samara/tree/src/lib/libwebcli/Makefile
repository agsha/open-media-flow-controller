#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=webcli

SRCS= \
	web_cli.c

CFLAGS= -I${PROD_TREE_ROOT}/src/lib/libweb

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk

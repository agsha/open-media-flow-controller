#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=webtmpl

SRCS= \
	lwt_custom_tags.c \
	lwt_helper_tags.c \
	lwt_parser.c \
	lwt_webtmpl.c \
	lwt_writer.c \
	lwt_util.c

CFLAGS= 

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk

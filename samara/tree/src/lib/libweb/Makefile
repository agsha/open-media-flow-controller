#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=web

SRCS= \
	web_accessors.c \
	web_core.c \
	web_raw.c \
	web_session.c


ifneq (${PROD_FEATURE_CAPABS},0)
	SRCS+=                      \
		web_capabs.c        \

endif

ifneq (${PROD_FEATURE_ACLS},0)
	SRCS+=                      \
		web_acls.c          \

endif

CFLAGS= -I${PROD_TREE_ROOT}/src/lib/libevent -I${PROD_TREE_ROOT}/src/lib/libgcl

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk

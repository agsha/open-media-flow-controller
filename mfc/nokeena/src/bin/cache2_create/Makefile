# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=cache2_create

SRCS=                 \
	cache2_create.c \

CFLAGS=-fPIC -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include ${BUILD_EXTRA_CFLAGS} -Wall -Werror -Wno-missing-format-attribute

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif


# XXX these should be done in a more compact way
LDADD= ${BUILD_EXTRA_LDADD} \

include ${PROD_TREE_ROOT}/src/mk/prog.mk

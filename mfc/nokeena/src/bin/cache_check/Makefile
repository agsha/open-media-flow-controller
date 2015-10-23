# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=cache_check

SRCS=                 \
	diskcheck.c      \
	functions.c      \

CFLAGS=-fPIC -D_GNU_SOURCE -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -Wall -Werror ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDFLAGS += `pkg-config --cflags --libs glib-2.0 gthread-2.0` -lm

# XXX these should be done in a more compact way
LDADD= ${BUILD_EXTRA_LDADD} \


include ${PROD_TREE_ROOT}/src/mk/prog.mk

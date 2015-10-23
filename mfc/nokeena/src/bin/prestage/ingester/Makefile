#  Filename: nvsd/Makefile
#  (C) Copyright 2008-2009 Nokeena Networks, Inc.
#  All rights reserved.
#
include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=ingester

SRCS=                   \
	main.c          \
	ingest_request.c \

CFLAGS= \
 -Wall -Werror -Wno-missing-format-attribute \
 -fPIC \
 -I/usr/lib64/glib-2.0/include \
 -I/usr/include/glib-2.0 \
 -I/usr/lib/glib-2.0/include \
 -I${PROD_TREE_ROOT}/src/lib/libgcl \
 -I${PROD_TREE_ROOT}/src/include \
 ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
	CFLAGS += -fprofile-arcs -ftest-coverage
endif

CFLAGS += \
 -DNKN_BUILD_ID=\"${NKN_BUILD_ID}\" \
 -DNKN_BUILD_PROD_RELEASE=\"${NKN_BUILD_PROD_RELEASE}\" \
 -DNKN_BUILD_NUMBER=\"${NKN_BUILD_NUMBER}\" \
 -DNKN_BUILD_SCM_INFO_SHORT=\"${NKN_BUILD_SCM_INFO_SHORT}\" \
 -DNKN_BUILD_SCM_INFO=\"${NKN_BUILD_SCM_INFO}\" \
 -DEXTRA_CFLAGS_DEF=\"${BUILD_EXTRA_CFLAGS}\" \

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

THIRDPARTYLIBS= \

LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` `xml2-config --libs` -lm -lelf -ldl -lssl -luuid -L/usr/lib64/python2.6/ -lpython2.6 -lcares ${THIRDPARTYLIBS}

# XXX these should be done in a more compact way
LDADD= \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

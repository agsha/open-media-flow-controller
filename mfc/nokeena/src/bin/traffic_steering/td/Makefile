#  (C) Copyright 2011 Juniper Networks
#  All rights reserved.
#
include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/tms/lib/web
PROG_INSTALL_DIR=cgi-bin
PROG=td

SRCS=                    \
	td.c          \

CFLAGS= -Wall -Werror -Wno-missing-format-attribute \
 -fPIC \
 -I/usr/lib64/glib-2.0/include \
 -I/usr/include/glib-2.0 \
 -I/usr/lib/glib-2.0/include \
 ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
	CFLAGS += -fprofile-arcs -ftest-coverage
endif

LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` -lm -lelf -ldl -luuid

# XXX these should be done in a more compact way
LDADD= \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk


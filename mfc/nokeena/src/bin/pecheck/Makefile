# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=pecheck

SRCS=                 \
	pecheck.c     \


CFLAGS= -fPIC ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


LDFLAGS+= -ltcl8.5

LDADD = ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/prog.mk

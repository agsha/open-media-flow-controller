# Makefile - Ankeena
WARNINGS_OVERRIDE = 1
DEFINES_OVERRIDE=1
include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=ketama
LIB_BUILD_DIR=${THIRDPARTY_LIB_DIR}/ketama/libketama

PROD_INSTALL_PREFIX=/lib
LIB_INSTALL_DIR=nkn

LIB_DYNAMIC=1

SRCS=			\
	ketama.c	\
	md5.c		\

INCLUDES=

CFLAGS += -Wall -Werror ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


LDADD = ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk

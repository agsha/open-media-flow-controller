# Filename: lib/nvsd/memalloc/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBMAKEFILES+=Makefile-lite
SUBMAKEFILES+=Makefile-cmm

LIB=nvsd_memalloc
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/memalloc

PROD_INSTALL_PREFIX=/opt/nkn
LIB_INSTALL_DIR=lib

LIB_DYNAMIC=1
#LIB_DYNAMIC_SONAME_EMPTY=1
#LIB_DYNAMIC_REALNAME=



SRCS=                   \
	nkn_memalloc.c 		\

INCLUDES=

CFLAGS += -D_GNU_SOURCE
CFLAGS += -DENABLE_INSTRUMENTED_MALLOC=1
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif


LDADD = ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk


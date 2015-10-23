# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=nknmgmt
LIB_BUILD_DIR=${CUST_LIB_DIR}/libmgmt

SRCS= \
      libnknmgmt.c       \


CFLAGS += -D_GNU_SOURCE
CFLAGS += ${BUILD_EXTRA_CFLAGS}
ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif
 
LDADD= ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk


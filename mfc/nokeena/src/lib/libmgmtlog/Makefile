# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=mgmtlog
LIB_BUILD_DIR=${CUST_LIB_DIR}/libmgmtlog
PROD_INSTALL_PREFIX=/opt/nkn
LIB_INSTALL_DIR=lib
LIB_DYNAMIC=1


SRCS= log_helper.c 	\

CFLAGS += -D_GNU_SOURCE
CFLAGS += ${BUILD_EXTRA_CFLAGS}
ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif


LDADD = ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk

# Filename: lib/nvsd/htable/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=nvsd_htable
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/htable

#PROD_INSTALL_PREFIX=/opt/nkn
#LIB_INSTALL_DIR=lib

#LIB_DYNAMIC=1
#LIB_DYNAMIC_SONAME_EMPTY=1
#LIB_DYNAMIC_REALNAME=



SRCS=                       \
        nkn_hash.c          \
        nkn_chainhash.c     \
	bobhash.c	    \


CFLAGS += -D_GNU_SOURCE
CFLAGS += -include nkn_no_alloc.h
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif


LDADD = ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk


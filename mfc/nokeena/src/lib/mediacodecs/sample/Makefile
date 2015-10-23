# Filename: lib/mediacodecs/sample/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=mediacodecs_sample
LIB_BUILD_DIR=${CUST_LIB_DIR}/mediacodecs/sample

PROD_INSTALL_PREFIX=/lib
LIB_INSTALL_DIR=nkn
LIB_DYNAMIC_REALNAME=.1.0.1
LIB_DYNAMIC=1

CXXFLAGS= ${BUILD_EXTRA_CXXFLAGS}

ifeq (${BUILD_GCOV}, 1)
        CXXFLAGS += -fprofile-arcs -ftest-coverage
endif


INCLUDES= -I../common -I../../../include -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0

SRCS= \
  sample.cpp \
  ../common/nkn_vfs.cpp \

LDADD= ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk


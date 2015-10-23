# Filename: lib/mediacodecs/mpeg2ts_h221/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=mediacodecs_mpeg2ts_h221
LIB_BUILD_DIR=${CUST_LIB_DIR}/mediacodecs/mpeg2ts_h221

PROD_INSTALL_PREFIX=/lib
LIB_INSTALL_DIR=nkn
LIB_DYNAMIC_REALNAME=.1.0.1
LIB_DYNAMIC=1

CXXFLAGS= ${BUILD_EXTRA_CXXFLAGS}

ifeq (${BUILD_GCOV}, 1)
        CXXFLAGS += -fprofile-arcs -ftest-coverage
endif

DEFINES = -DMPEG_TS_TPLAY_PARSER
INCLUDES= -I../common -I../../../include -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0

LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/parser/libnvsd_parser.a \
 ${BUILD_EXTRA_LDADD}

SRCS= \
  mpeg2ts_h221.cpp \
  trickplay.cpp \
  ../common/nkn_vfs.cpp \


include ${PROD_TREE_ROOT}/src/mk/lib.mk


#  Makefile
#
#  Filename:  src/bin/utils/cmtest
#  Date:      2008-11-06
#  Author:    Ramanand Narayanan
# 
#  (C) Copyright 2008 Nokeena Networks, Inc.  
#  All rights reserved.
# 

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/nkn
PROG_INSTALL_DIR=bin
PROG=cmtest

SRCS=                 \
	cmtest.c  \

CFLAGS=-fPIC -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I${PROD_TREE_ROOT}/src/lib/libgcl  ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` -lm -lelf -ldl -lssl

# XXX these should be done in a more compact way
LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/sched/libnvsd_sched.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/bufmgr/libnvsd_bufmgr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/diskmgr2/libnvsd_diskmgr2.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/mediamgr/libnvsd_mediamgr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/mgmta/libnvsd_mgmta.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/am/libnvsd_am.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/common/libnvsd_common.a \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

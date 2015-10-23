#
#  @file src/bin/jpsd/Makefile
#  @brief
#  Makefile - Makefile for jpsd binary
#
#  Yuvaraja Mariappan, Dec 2014
#
#  Copyright (c) 2015, Juniper Networks, Inc.
#  All rights reserved.
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX = /opt/nkn
PROG_INSTALL_DIR = sbin
PROG = jpsd

SRCS = \
	jpsd_main.c \
	jpsd_timer.c \
	jpsd_network.c \
	jpsd_tdf.c \
	jpsd_mgmt.c \

CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -include nkn_no_alloc.h

CFLAGS += -Wall -Wno-unused -I./ -I/usr/include/glib-2.0 -I /usr/lib64/glib-2.0/include
CFLAGS += -fPIC ${BUILD_EXTRA_CFLAGS} -I/usr/include/libxml2 -std=gnu99
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/lib/jpsd/h
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/lib/jpsd/libdiameter-base/h
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/lib/jpsd/libdiameter-avp/h
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/lib/jpsd/libdiameter
CFLAGS += -I../../../thirdparty/lib/ketama/libketama
CFLAGS += -I../../../thirdparty/lib/stack/sys

CFLAGS += -DNKN_BUILD_ID=\"${NKN_BUILD_ID}\"
CFLAGS += -DNKN_BUILD_PROD_RELEASE=\"${NKN_BUILD_PROD_RELEASE}\"
CFLAGS += -DNKN_BUILD_NUMBER=\"${NKN_BUILD_NUMBER}\"
CFLAGS += -DNKN_BUILD_SCM_INFO_SHORT=\"${NKN_BUILD_SCM_INFO_SHORT}\"
CFLAGS += -DNKN_BUILD_SCM_INFO=\"${NKN_BUILD_SCM_INFO}\"
CFLAGS += -DEXTRA_CFLAGS_DEF=\"${BUILD_EXTRA_CFLAGS}\"

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif

LDFLAGS += -Wl,--no-as-needed -lpthread -lelf  -lglib-2.0 -lm -lcprops -ldl -lrt -lpcap

LDADD = \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
        ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
        ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0 \
        ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
        ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/jpsd/libdiameter/libdiameter.a \
        ${PROD_CUSTOMER_ROOT}/nokeena/src/lib/jpsd/libdiameter-base/libdiameter-base.a \
        ${PROD_CUSTOMER_ROOT}/nokeena/src/lib/jpsd/libdiameter-avp/libdiameter-avp.a \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

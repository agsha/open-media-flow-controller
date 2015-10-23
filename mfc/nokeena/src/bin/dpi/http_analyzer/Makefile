#  Makefile
#
#  Filename:  src/bin/dpi/http_analyzer
#
#  (C) Copyright 2014 Juniper Networks, Inc.
#  All rights reserved.
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=http_analyzer

SRCS=   \
	main.c \
	event_handler.c \
	utils.c \
	thread.c \
	pcap_mode.c \
	dpdk_mode.c 

CFLAGS = -Wall -Wno-unused -I./ -I/usr/include/glib-2.0 -I /usr/lib64/glib-2.0/include  -fPIC ${BUILD_EXTRA_CFLAGS} -I/usr/include/libxml2 -std=gnu99

CFLAGS += \
 -DNKN_BUILD_ID=\"${NKN_BUILD_ID}\" \
 -DNKN_BUILD_PROD_RELEASE=\"${NKN_BUILD_PROD_RELEASE}\" \
 -DNKN_BUILD_NUMBER=\"${NKN_BUILD_NUMBER}\" \
 -DNKN_BUILD_SCM_INFO_SHORT=\"${NKN_BUILD_SCM_INFO_SHORT}\" \
 -DNKN_BUILD_SCM_INFO=\"${NKN_BUILD_SCM_INFO}\" \
 -DEXTRA_CFLAGS_DEF=\"${BUILD_EXTRA_CFLAGS}\" \

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif

# Setup for Qosmos includes and libraries
DPI_SDK_ROOT=${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi

DEBUG=y
include ${DPI_SDK_ROOT}/http_analyzer/platform.mk

# CFLAGS extension for DPDK related files
CFLAGS += -I$(DPI_SDK_ROOT)/include/dpdk -msse4.2

LDFLAGS+= -Wl,--no-as-needed -lpthread -lelf  -lglib-2.0 -lm -lcprops -ldl -lrt -lpcap

LDADD= \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libapp.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libflow.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libplugins.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmengine.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmprotocols.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmfilter.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmctl.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmsecurity.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmdata.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi/lib/dpdk/librte_mbuf.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi/lib/dpdk/librte_eal.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi/lib/dpdk/librte_mempool.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi/lib/dpdk/librte_ring.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi/lib/dpdk/librte_malloc.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi/lib/dpdk/libethdev.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi/lib/dpdk/librte_pmd_e1000.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi/lib/dpdk/librte_pmd_ixgbe.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi/lib/dpdk/librte_timer.a \
    ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
    ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
	${BUILD_EXTRA_LDADD}

#	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
        ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
        ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
        ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0 \
        ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \

include ${PROD_TREE_ROOT}/src/mk/prog.mk

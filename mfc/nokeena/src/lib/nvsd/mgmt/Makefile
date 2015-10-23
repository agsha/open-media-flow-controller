# Filename: lib/nvsd/mgmt/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=nvsd_mgmt
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/mgmt

SRCS= \
	nvsd_mgmt_main.c       \
	nvsd_mgmt_monitor.c    \
	nvsd_mgmt_system.c     \
	nvsd_mgmt_network.c    \
	nvsd_mgmt_http.c       \
	nvsd_mgmt_buffermgr.c  \
	nvsd_mgmt_diskcache.c  \
	nvsd_mgmt_fmgr.c       \
	nvsd_mgmt_tfm.c       \
	nvsd_mgmt_am.c		\
	nvsd_mgmt_mm.c		\
	nvsd_mgmt_namespace.c       \
	nvsd_mgmt_server_map.c        \
	nvsd_mgmt_cluster.c        \
	nvsd_mgmt_virtual_player.c        \
	nvsd_mgmt_rtstream.c \
	nvsd_mgmt_pub_point.c \
	nvsd_mgmt_resource_mgr.c \
	nvsd_mgmt_acclog.c \
	nvsd_mgmt_errlog.c \
	nvsd_mgmt_strlog.c \
	nvsd_mgmt_disk_main.c \
	nvsd_mgmt_l4proxy.c \
	nvsd_mgmt_pe.c \
	nvsd_mgmt_url_filter.c \


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DPRELOAD_UF_DATA
CFLAGS += -include nkn_no_alloc.h
CFLAGS += -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif


LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmtlog/libmgmtlog.so.1.0.0 \
 ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

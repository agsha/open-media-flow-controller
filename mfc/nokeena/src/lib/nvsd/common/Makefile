# Filename: lib/nvsd/common/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=nvsd_common
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/common

SRCS= \
	nkn_locmgr_glib_hash.c    \
	nkn_memmgr_glib_hash.c    \
	nkn_diskio_glib_hash.c    \
	uri_mgr.c		  \
	nkn_trace.c		  \
	cpu_mgr.c		  \
	nkn_elf.c		  \
	nkn_debug.c		  \
	fqueue.c		  \
	http_header.c		  \
	rtsp_header.c		  \
	mime_header.c		  \
	nkn_attr.c		  \
	nkn_namespace.c		  \
	nkn_namespace_utils.c	  \
	nkn_namespace_stats.c	  \
	nkn_nodemap.c		  \
	nkn_nodemap_clustermap.c  \
	nkn_nodemap_clustermap_stubs.c  \
	nkn_nodemap_noload_clustermap.c  \
	nkn_nodemap_load_clustermap.c  \
	nkn_nodemap_originescalationmap.c  \
	nkn_slotapi.c		  \
	nkn_nodemap_originroundrobinmap.c \
	chunk_encoding_engine.c   \
	nkn_regex.c		  \
	nkn_lockstat.c		  \
	nkn_pseudo_con.c	  \
	nkn_cmm_request.c	  \
	nkn_trie.c		  \
	nkn_trie_stubs.c	  \
	nkn_time.c		  \
	nkn_nknexecd_common.c	  \


ifeq "${BUILD_USE_SAMARA_SSL}" ""
OPENSSL_INCLUDE=-I/usr/local/openssl/include
else
OPENSSL_INCLUDE=
endif

INCLUDES= \
 ${OPENSSL_INCLUDE} \
 -I/usr/lib64/glib-2.0/include  \
 -I/usr/include/glib-2.0  \
 -I/usr/lib/glib-2.0/include  \
 -I../../../thirdparty/lib/ketama/libketama \


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -include nkn_no_alloc.h
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
 ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

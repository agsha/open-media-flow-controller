# Filename: lib/nvsd/omgr/Makefile
include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=nvsd_omgr
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/omgr

SRCS= \
	om_api.c     \
	om_network.c \
	om_tunnel.c \
	om_dns.c \
	om_connpoll.c \
	om_transparent.c \
	om_rtsp_api.c \
	om_rtsp_network.c \
	om_rtsp_player.c \
	om_rtsp_network_wm.c \
	om_portmap.c \
	om_pe.c \



CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -include nkn_no_alloc.h
CFLAGS += -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
CFLAGS += -I${PROD_BUILD_OUTPUT_DIR}/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/image_kernel_smp/include
CFLAGS += -I../../../thirdparty/lib/ketama/libketama
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# XXX these should be done in a more compact way
LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a  \
 ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

# Filename: lib/nvsd/am/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=dpi_urif
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/dpi_urif

SRCS= \
	nkn_dpi.c            \
	nkn_dpi_netfilter.c       \
	nkn_dpi_eventhandler.c       \
	nkn_pf_lib.c		\


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DOBJECT_LEVEL_LOCK
CFLAGS += -include nkn_no_alloc.h
CFLAGS += -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I${PROD_CUSTOMER_ROOT}/nokeena/src/include/netfilter -I${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi/include/dpi_engine -I${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/pf_ring/ -I${PROD_CUSTOMER_ROOT}/nokeena/src/base_os/linux_common/kmods/pfring
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif

DPI_SDK_ROOT=${PROD_CUSTOMER_ROOT}/nokeena/src/bin/dpi

include ${DPI_SDK_ROOT}/http_analyzer/platform.mk

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
 ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

# Filename: lib/mfp/sync_serial/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=mfp_sync_serial
LIB_BUILD_DIR=${CUST_LIB_DIR}/mfp/sync_serial

SRCS= \
	mfp_sync_serial.c\


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I/usr/lib64/glib-2.0/include -I/usr/include/libxml2 \
-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/mfp/sync_serial
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
 -lxml2 ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk

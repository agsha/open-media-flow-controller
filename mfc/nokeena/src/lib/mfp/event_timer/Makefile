# Filename: lib/mfp/event_timer/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=mfp_event_timer
LIB_BUILD_DIR=${CUST_LIB_DIR}/mfp/event_timer

SRCS= \
	mfp_event_timer.c\


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I/usr/lib64/glib-2.0/include -I/usr/include/libxml2 \
-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/mfp/event_timer
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDADD = ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk

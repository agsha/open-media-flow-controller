# Filename: lib/mfp/io_hdlr/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=mfp_io_hdlr
LIB_BUILD_DIR=${CUST_LIB_DIR}/mfp/io_hdlr

SRCS= \
	mfp_io_hdlr.c\


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/include/mfp/io_hdlr
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

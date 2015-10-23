# Filename: lib/nvsd/common_opt/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=nvsd_common_opt
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/common_opt

SRCS= \
	nkn_util.c		  \
	nkn_opt.c		  \


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
CFLAGS += -g0 -O2
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

# XXX these should be done in a more compact way
LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon_opt.a \
 ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

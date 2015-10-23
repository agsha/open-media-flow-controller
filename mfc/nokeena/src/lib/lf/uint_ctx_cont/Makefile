# Filename: lib/lf/uint_ctx_cont/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=lf_uint_ctx_cont
LIB_BUILD_DIR=${CUST_LIB_DIR}/lf/uint_ctx_cont

SRCS= \
	lf_uint_ctx_cont.c\


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I/usr/lib64/glib-2.0/include \
-I/usr/include/libxml2 \
-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/lf \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/lf/uint_ctx_cont
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif


include ${PROD_TREE_ROOT}/src/mk/lib.mk

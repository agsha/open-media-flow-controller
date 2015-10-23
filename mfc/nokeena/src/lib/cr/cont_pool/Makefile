# Filename: lib/cr/cont_pool/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=cr_cont_pool
LIB_BUILD_DIR=${CUST_LIB_DIR}/cr/cont_pool

SRCS= \
	cont_pool.c\

CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I/usr/local/include/cprops/ \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr/cont_pool 
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

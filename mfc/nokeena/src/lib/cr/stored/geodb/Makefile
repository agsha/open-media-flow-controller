# Filename: lib/cr/stored/geodb/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=cr_stored_geodb
LIB_BUILD_DIR=${CUST_LIB_DIR}/cr/stored/geodb

SRCS= \
	${PROD_CUSTOMER_ROOT}/nokeena/src/bin/geodbd/nkn_geodbd.c\
	crst_geodb_mgr.c\


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr/ \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr/stored \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr/stored/proto_pr_bldr
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
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
 ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk

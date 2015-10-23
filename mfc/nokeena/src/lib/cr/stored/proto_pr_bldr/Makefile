# Filename: lib/cr/stored/proto_pr_bldr/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=cr_stored_proto_pr_bldr
LIB_BUILD_DIR=${CUST_LIB_DIR}/cr/stored/proto_pr_bldr

SRCS= \
	crst_prot_parser.c\
	crst_prot_bldr.c\

CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr/stored \
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

include ${PROD_TREE_ROOT}/src/mk/lib.mk

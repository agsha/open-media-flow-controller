# Filename: lib/cr/dns_pb/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=cr_dns_pb
LIB_BUILD_DIR=${CUST_LIB_DIR}/cr/dns_pb

SRCS= \
	dns_parser.c\
	dns_builder.c\

CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr/dns_pb \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr
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

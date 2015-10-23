# Makefile

WARNINGS_EXTRA_OVERRIDE = 1
include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=proto_HTTP
LIB_BUILD_DIR=${CUST_LIB_DIR}/proto_http

SRCS= \
	mime_header.c \
	http_header.c \
	parser_utils.c \
	http_parser.c \
	http_resp.c \
	http_request.c \
	proto_http.c \


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DPROTO_HTTP_LIB
CFLAGS += -I.
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDADD= ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

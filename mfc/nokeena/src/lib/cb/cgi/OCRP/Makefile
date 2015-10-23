# Filename: lib/cb/cgi/OCRP/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

# LIB=cgi_OCRP_client
LIB=cb_cgi_OCRP_client
LIB_BUILD_DIR=${CUST_LIB_DIR}/cb/cgi/OCRP

SRCS= \
      OCRP_post_cgi_client.c 	\

CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I/usr/include/libxml2
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

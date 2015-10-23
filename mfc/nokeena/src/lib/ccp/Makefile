# Makefile

WARNINGS_EXTRA_OVERRIDE = 1
include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=ccp
LIB_BUILD_DIR=${CUST_LIB_DIR}/ccp

SRCS= \
      ccp.c	\
      ccp_server.c	\
      ccp_client.c	\

CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DUSE_NEW_NKNLOG
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

ifeq "${BUILD_USE_SAMARA_SSL}" ""
LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` `xml2-config --libs` \
 -lm -lelf -ldl \
 -L${OPENSSL_LIBDIR}\
 -luuid \
 -L/usr/lib64/python2.4/ -lpython2.4 \
 -lfuse -ltcl -lcares -lcprops \
 -I/usr/include/libxml2
 ${THIRDPARTYLIBS}
else
LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` `xml2-config --libs` -lm -lelf -ldl -lssl -luuid -L/usr/lib64/python2.4/ -lpython2.4 -lfuse -ltcl -lcares -lcprops -I/usrinclude/libxml2 ${THIRDPARTYLIBS}
endif

LDADD= ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk

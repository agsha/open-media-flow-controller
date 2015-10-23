# Makefile

WARNINGS_EXTRA_OVERRIDE = 1
include ${PROD_TREE_ROOT}/src/mk/common.mk

ifeq "${ICCP_DEBUG}" "1"
PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=iccp
else
LIB=iccp
LIB_BUILD_DIR=${CUST_LIB_DIR}/iccp
endif

SRCS= \
      nkn_iccp.c	\
      nkn_iccp_srvr.c	\
      nkn_iccp_cli.c	\


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DUSE_NEW_NKNLOG
CFLAGS += -I/usr/include/libxml2
CFLAGS += ${BUILD_EXTRA_CFLAGS}
ifeq "${ICCP_DEBUG}" "1"
CFLAGS += -DICCP_DEBUG
endif

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` `xml2-config --libs` -lm -lelf -ldl -lssl -luuid -L/usr/lib64/python2.4/ -lpython2.4 -lfuse -ltcl -lcares -lcprops -I/usrinclude/libxml2

ifeq "${ICCP_DEBUG}" "1"
LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
 ${BUILD_EXTRA_LDADD}
else
LDADD= ${BUILD_EXTRA_LDADD}
endif

ifeq "${ICCP_DEBUG}" "1"
include ${PROD_TREE_ROOT}/src/mk/prog.mk
else
include ${PROD_TREE_ROOT}/src/mk/lib.mk
endif

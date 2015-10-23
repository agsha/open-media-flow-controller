# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=cse

SRCS=                 \
	cse_main.c      \
	cse_mgmt.c	\
	cse_lib_mgmt.c  \
	nkn_cue.c


CFLAGS= -Wall -I${PROD_TREE_ROOT}/src/lib/libgcl -I./ -I/usr/include/glib-2.0 -I /usr/lib64/glib-2.0/include  -fPIC ${BUILD_EXTRA_CFLAGS} -DUSE_NEW_NKNLOG -I/usr/include/libxml2

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif


LDFLAGS+= -lpthread -lelf  -lglib-2.0 -lm -lcprops -lssl ${THIRDPARTYLIBS} -lxml2 `pkg-config --cflags --libs glib-2.0 gthread-2.0`

LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_mt/libtc_mt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/iccp/libiccp.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/ccp/libccp.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmtlog/libmgmtlog.so.1.0.0 \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmt/libnknmgmt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0 \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

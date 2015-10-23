# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=gmgmthd

SRCS= \
	gmgmthd_config.c          \
	gmgmthd_main.c            \
	gmgmthd.c                 \
	gmgmthd_mgmt.c            \
	gmgmthd_mfp.c            \
	gmgmthd_mfp_mgmt.c            \

INCLUDES=\
 -I/usr/lib64/glib-2.0/include \
 -I/usr/include/glib-2.0 \
 -I/usr/lib/glib-2.0/include \

CFLAGS= -I${PROD_TREE_ROOT}/src/lib/libgcl ${BUILD_EXTRA_CFLAGS}\
	-I/usr/include/libxml2 -I/usr/lib/glib-2.0/include 


ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif

LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` \
-lm -lelf -ldl -lssl -luuid -lxml2 -L/usr/lib64/python2.6/ -lpython2.6 \
-lcares -lcprops\

# XXX these should be done in a more compact way
LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_mt/libtc_mt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncnt/libnkncnt.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncnt_client/libnkncnt_client.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmt/libnknmgmt.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0 \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmtlog/libmgmtlog.so.1.0.0 \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

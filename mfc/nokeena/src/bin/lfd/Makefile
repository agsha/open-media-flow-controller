# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk
include ${PROD_CUSTOMER_ROOT}/nokeena/src/mk/mfpcommon.mk


PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=lfd

SRCS= \
	lf_main.c \
	lf_nw_event_handler.c \
	lf_msg_handler.c \
	lf_metrics_monitor.c \
	lf_stats.c \
	lf_apps_metrics_intf.c \
	nkncnt_client.c

CFLAGS += -fPIC -Idisp -D_GNU_SOURCE\
-I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 \
-I/usr/include/libxml2 -I/usr/lib/glib-2.0/include \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/lf \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/lf/uint_ctx_cont \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/lf/sock_msg_buffer \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/lf/http_parser_builder \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/lf/connector \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/mfp/disp \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/mfp/thread_pool \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/mfp/event_timer \
-I${PROD_TREE_ROOT}/src/lib/libgcl  ${BUILD_EXTRA_CFLAGS}\

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
       GETTEXT_PACKAGE=${PROG}
       DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
       SUBMAKEFILES+=Makefile.i18n
endif

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif

LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` \
-lm -lelf -ldl -lssl -luuid -lxml2 -lvirt -L/usr/lib64/python2.6/ -lpython2.6 -lcprops\
-lcares


LDADD+= \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0 \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/disp/libmfp_disp.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/event_timer/libmfp_event_timer.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/thread_pool/libmfp_thread_pool.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/lf/http_parser_builder/liblf_http_parser_builder.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/lf/sock_msg_buffer/liblf_sock_msg_buffer.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/lf/uint_ctx_cont/liblf_uint_ctx_cont.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/lf/connector/liblf_connector.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/lf/common/liblf_common.a \
	   ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/common/libnvsd_common.a \
	   ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
	   ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk


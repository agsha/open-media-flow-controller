# Filename: bin/cr/stored/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk
include ${PROD_CUSTOMER_ROOT}/nokeena/src/mk/mfpcommon.mk

BUILD_MACHINE_ROOT_DIR=

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=cr_stored

SRCS= \
	crst_main.c\
	crst_nw_handler.c\
	crst_con_ctx.c\
	crst_request_proc.c\
	crst_mgmt.c\
	crst_mgmt_handler.c\
	ds_apis.c\
	ds_hash_table_mgmt.c\
	ds_main.c\

CFLAGS += -fPIC -D_GNU_SOURCE\
-I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 \
-I/usr/include/libxml2 -I/usr/lib/glib-2.0/include \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr/common \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr/stored \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr/stored/proto_pr_bldr \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr/event_disp \
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
-lm -lelf -ldl -lssl -luuid -lxml2 -L/usr/lib64/python2.4/ -lpython2.4 -lcprops\
-lcares


LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/cr/event_disp/libcr_event_disp.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/cr/common/libcr_common.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/cr/stored/proto_pr_bldr/libcr_stored_proto_pr_bldr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/cr/stored/geodb/libcr_stored_geodb.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/cr/de/libcr_de.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/common/libnvsd_common.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/common_opt/libnvsd_common_opt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0 \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/event_timer/libmfp_event_timer.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/cr/lfc/libcr_lfc.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/parser/libnvsd_parser.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
	${BUILD_MACHINE_ROOT_DIR}/usr/local/lib/libGeoIP.so \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmt/libnknmgmtclient.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_mt/libtc_mt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmtlog/libmgmtlog.so.1.0.0 \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmt/libnknmgmt.a \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

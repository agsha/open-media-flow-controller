# Filename: bin/mfp/live_pub/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk
include ${PROD_CUSTOMER_ROOT}/nokeena/src/mk/mfpcommon.mk


PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=live_mfpd

SRCS=                  \
	mfp_live_test_sess_id.c \
	../common/mfp_live_conf.c \
	../common/mfp_read_config.c \
	../common/mfp_pmf_tag_parsers.c \
	../common/mfp_pmf_parser_api.c \
	../common/mfp_publ_context.c \
	../common/mfp_pmf_processor.c \
	../common/mfp_mgmt_sess_id_map.c \
	../common/mfp_live_sess_id.c \
	../common/mfp_control_pmf_handler.c \
	../common/mfp_ref_count_mem.c\
	../common/mfp_sl_fmtr_conf.c \
	../common/mfp_video_header.c \
	../common/mfp_apple_io_hdlr.c \
	../common/mfp_data_pair.c \
	mfp_live_sess_mgr_api.c \
	mfp_live_listener.c \
	mfp_live_accum_ts_v2.c \
	mfp_live_mfu_merge.c \
	mfp_live_event_handler.c \
	mfu2iphone.c \
	mfp_publ_formatter_intf.c\
	mfp_live_file_pump.c\
	../nkn_vpe_mfp_ts.c \
	../nkn_vpe_ts_video.c \
	../common/mfp_cpu_affinity.c \
	mfp_live_accum_create.c \


#CFLAGS += -DUNIT_TEST=\"${UINT_TEST}\" -DLIVE_SESSID_TEST -fPIC -Idisp \
#-I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 \
#-I/usr/include/libxml2 -I/usr/lib/glib-2.0/include \
#-I${PROD_TREE_ROOT}/src/lib/libgcl \
#-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/mfp ${BUILD_EXTRA_CFLAGS}

CFLAGS += -fPIC -Idisp -D_GNU_SOURCE\
-I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 \
-I/usr/include/libxml2 -I/usr/lib/glib-2.0/include \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/mfp \
-I${PROD_TREE_ROOT}/src/lib/libgcl  ${BUILD_EXTRA_CFLAGS}\

ifdef MFP_FEATURE_ALIGNER
CFLAGS += -DINC_ALIGNER
endif

ifdef MFP_BUGFIX_8460
CFLAGS += -DMFP_BUGFIX_8460
endif

ifdef MFP_FEATURE_VAR_SEGD
CFLAGS += -DINC_SEGD
endif

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
       GETTEXT_PACKAGE=${PROG}
       DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
       SUBMAKEFILES+=Makefile.i18n
endif

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif

LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` \
-lm -lelf -ldl -lssl -luuid -lxml2 -L/usr/lib64/python2.6/ -lpython2.6 -lcprops\
-lcares


ifdef BUILD_ZVM
ZVM_LDADD=/usr/local/zvm-release/lib/linux_x86_64/libzvmfe.a
else
ZVM_LDADD=
endif

LDADD+= \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/sched/libnvsd_sched.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/rtsched/libnvsd_rtsched.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/common/libnvsd_common.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/common_opt/libnvsd_common_opt.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/parser/libnvsd_parser.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0 \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/authmgr/libnvsd_authmgr.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/encmgr/libnvsd_encmgr.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/disp/libmfp_disp.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/safeio/libmfp_safeio.a \
       /usr/lib/gcc/x86_64-redhat-linux/4.4.7/libstdc++.a\
       ${ZVM_LDADD}\
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/sync_serial/libmfp_sync_serial.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/event_timer/libmfp_event_timer.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/thread_pool/libmfp_thread_pool.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/io_hdlr/libmfp_io_hdlr.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/kms/libmfp_kms.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/aligner/libmfp_aligner.a \
	   ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/mfp/latm_parser/libmfp_latm_parser.a \
	 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/compression/libnvsd_compression.a \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk


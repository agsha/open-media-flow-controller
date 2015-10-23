# Filename: lib/nvsd/parser/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk
include ${PROD_CUSTOMER_ROOT}/nokeena/src/mk/mfpcommon.mk


LIB=nvsd_parser
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/parser

CXXFLAGS=\
	${BUILD_EXTRA_CXXFLAGS} \
	-D__cplusplus \
	-I/usr/lib64/glib-2.0/include\
	-I/usr/include/libxml2\
	-I/usr/include/glib-2.0\
	-I${PROD_CUSTOMER_ROOT}/nokeena/src/lib/mediacodecs/common\
	-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/mfp\
	-I${PROD_CUSTOMER_ROOT}/nokeena/src/lib/mfp/latm_parser\
	-I/usr/lib/glib-2.0/include\


ifeq (${BUILD_GCOV}, 1)
        CXXFLAGS += -fprofile-arcs -ftest-coverage
endif

SRCS=\
	nkn_vpe_asf_parser.c\
	nkn_vpe_webm_parser.c\
	nkn_vpe_mp4_parser.c\
	nkn_vpe_backend_io_plugin.c\
	nkn_vpe_bitio.c\
	nkn_vpe_bufio.c\
	nkn_vpe_mp4_feature.c\
	nkn_vpe_mp4_unhinted.c\
	rtp_avc_read.c\
	rtp_mpeg4_es.c\
	rtp_packetizer_common.c\
	rtp_avc_packetizer.c\
	rtp_m4a_read.c\
	rtp_mpeg4_packetizer.c\
	nkn_vpe_bitstream_io.c\
	nkn_vpe_mpeg2ps_parser.c \
	nkn_vpe_mpeg2ts_parser.c \
	nkn_vpe_mp4_seek_api.c\
	nkn_vpe_mp4_seek_parser.c\
	nkn_vpe_mp4_ismv_access_api.c\
	nkn_vpe_ism_read_api.c\
	nkn_vpe_ismc_read_api.c\
	nkn_vpe_flv_parser.c\
	nkn_vpe_media_props_api.c\
	nkn_vpe_ismv2ts_api.c \
	nkn_vpe_ismv2moof_api.c \
	nkn_vpe_ts2iphone_api.c \
	nkn_vpe_ts_segmenter_api.c \
	nkn_vpe_ismv_reader.c \
	nkn_vpe_h264_parser.c\
	nkn_vpe_mfp_ts2mfu.c\
	nkn_vpe_sync_http_read.c\
	nkn_vpe_f4v_frag_access_api.c\
	nkn_vpe_mfu_writer.c\
	nkn_vpe_sl_fmtr.c\
	nkn_vpe_moof2mfu.c\
	nkn_vpe_mfu2moof.c\
	nkn_vpe_utils.c\
	nkn_vpe_moof2mfu_api.c\
	nkn_vpe_mfu2ts_api.c\
	../../../bin/mfp/nkn_vpe_ismv2ts.c\
	../../../bin/mfp/live_pub/mfu2iphone.c\
	../../../bin/mfp/nkn_vpe_mfp_ts.c\
	../../../bin/mfp/nkn_vpe_mfu_aac.c\
	../../../bin/mfp/nkn_vpe_raw_h264.c\
	../../../bin/mfp/nkn_vpe_ts_video.c\
	nkn_vpe_mfu_parser.c\
	nkn_vpe_mfu_parse.c\
	nkn_vpe_mp42anyformat_api.c\
	nkn_vpe_mp42mfu.c\
	nkn_vpe_ts2anyformat_api.c\
	nkn_vpe_ts2mfu.c\
	nkn_vpe_pattern_search.c\


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I/usr/lib64/glib-2.0/include
CFLAGS += -I/usr/include/libxml2
CFLAGS += -I/usr/include/glib-2.0
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/lib/mediacodecs/common
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/include/mfp
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/lib/mfp/latm_parser
CFLAGS += -I/usr/lib/glib-2.0/include
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifdef MFP_FEATURE_ALIGNER
CFLAGS += -DINC_ALIGNER
endif

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
 -lxml2 ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

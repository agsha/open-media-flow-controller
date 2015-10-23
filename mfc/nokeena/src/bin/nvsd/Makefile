# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=nvsd

SRCS=                    \
	server.c          \
	network.c         \
	interface.c       \
	nkn_http.c        \
	parser_utils.c	  \
	http_parser.c     \
	http_resp.c       \
	nkn_timer.c       \
	nkn_accesslog.c   \
	nkn_streamlog.c   \
	file_manager.c    \
	rtsp_sched.c	  \
	rtsp_parser.c	  \
	rtsp_session.c	  \
	rtsp_vodserver.c  \
	rtsp_validate.c   \
	rtsp_response.c	  \
	rtcp_handler.c    \
	nkn_vfs.c	  \
	fuse_wrapper.c \
	server_init.c     \
	server_exit.c     \
	network_hres_timer.c \
	auth_helper.c \
	rtcp_util.c \
	http_pe.c \
	pe_common.c \
	conmsg_agent.c \
        nkn_recvfd.c \
        pe_helper.c \
        nkn_urlfilter.c \
        dpi_urlfilter.c \


ifeq "${BUILD_USE_SAMARA_SSL}" ""
OPENSSL_INCLUDE=-I/usr/local/openssl/include
OPENSSL_LIBDIR=/usr/local/openssl/lib
OPENSSL_SSL_LIB=/usr/local/openssl/lib/libssl.a
OPENSSL_CRYPTO_LIB=/usr/local/openssl/lib/libcrypto.a
else
OPENSSL_INCLUDE=
OPENSSL_SSL_LIB=
OPENSSL_CRYPTO_LIB=
endif

INCLUDES=\
 ${OPENSSL_INCLUDE} \
 -I/usr/lib64/glib-2.0/include \
 -I/usr/include/glib-2.0 \
 -I/usr/lib/glib-2.0/include \
 -I${PROD_TREE_ROOT}/src/lib/libgcl \
 -I/usr/local/include/fuse/ \


CFLAGS= -include nkn_no_alloc_gnu.h \
 -Wall -Werror -Wno-missing-format-attribute \
 -fPIC \
 -g `xml2-config --cflags` \
 ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
	CFLAGS += -fprofile-arcs -ftest-coverage
endif

CFLAGS += \
 -DNKN_BUILD_ID=\"${NKN_BUILD_ID}\" \
 -DNKN_BUILD_PROD_RELEASE=\"${NKN_BUILD_PROD_RELEASE}\" \
 -DNKN_BUILD_NUMBER=\"${NKN_BUILD_NUMBER}\" \
 -DNKN_BUILD_SCM_INFO_SHORT=\"${NKN_BUILD_SCM_INFO_SHORT}\" \
 -DNKN_BUILD_SCM_INFO=\"${NKN_BUILD_SCM_INFO}\" \
 -DEXTRA_CFLAGS_DEF=\"${BUILD_EXTRA_CFLAGS}\" \

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

ifeq "${BUILD_USE_SAMARA_SSL}" ""
LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` `xml2-config --libs` \
 -lm -lelf -ldl \
 -L${OPENSSL_LIBDIR}\
 -luuid \
-L ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/thirdparty/binary/tcl_84/ -ltcl8.4 \
-L/usr/lib64/python2.6/ -lpython2.6 \
-L ${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/pf_ring/ \
 -lfuse  -lcares -lcprops -lvirt -lpcap -lpfring
else
LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` `xml2-config --libs` \
 -lm -lelf -ldl 
 -lssl \
 -luuid \
 -L ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/thirdparty/binary/tcl_84/ -ltcl8.4 \
 -L/usr/lib64/python2.4/ -lpython2.4 \
 -L ${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/pf_ring/ \
 -lfuse  -lcares -lcprops -lvirt -lpcap -lpfring
endif


# XXX these should be done in a more compact way
LDADD= \
 ${OPENSSL_SSL_LIB} \
 ${OPENSSL_CRYPTO_LIB} \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${THIRDPARTY_LIB_DIR}/ketama/libketama/libketama.so.1.0.0  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/libsocket/libnvsd_libsocket.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/sched/libnvsd_sched.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/rtsched/libnvsd_rtsched.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/bufmgr/libnvsd_bufmgr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/bufmgr/bufmgropt/libnvsd_bufmgr_bufmgropt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/mediamgr/libnvsd_mediamgr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/diskmgr2/libnvsd_diskmgr2.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/omgr/libnvsd_omgr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/am/libnvsd_am.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/mgmt/libnvsd_mgmt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/mgmta/libnvsd_mgmta.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/nfsm/libnvsd_nfsm.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/ssp/libnvsd_ssp.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/cluster/libnvsd_cluster.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/common/libnvsd_common.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/common_opt/libnvsd_common_opt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/htable/libnvsd_htable.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncmm/libnkn_cmm.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_mt/libtc_mt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/pythonlib/libnvsd_pythonlib.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/vmgr/libnvsd_vmgr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/encmgr/libnvsd_encmgr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncnt_client/libnkncnt_client.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/authmgr/libnvsd_authmgr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/pemgr/libnvsd_pemgr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/parser/libnvsd_parser.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0 \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmtlog/libmgmtlog.so.1.0.0 \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmt/libnknmgmt.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/iccp/libiccp.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/virtcachesvr/libnvsd_virtcachesvr.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/compression/libnvsd_compression.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/dpi_urif/libdpi_urif.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libapp.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libflow.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libplugins.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmengine.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmprotocols.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmfilter.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmctl.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmsecurity.a \
	${PROD_CUSTOMER_ROOT}/nokeena/src/thirdparty/binary/qosmos_dpi/libqmdata.a \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

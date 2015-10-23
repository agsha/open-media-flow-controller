# Makefile

WARNINGS_EXTRA_OVERRIDE=1
include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=ssld

SRCS=                 \
	ssl_main.c      \
	ssl_cfg.c      \
	ssl_interface.c      \
	ssl_network.c     \
	ssl_server.c     \
	ssl_timer.c     \
	ssl_mgmt.c	\
	ssl_client.c	\
	ssl_connpoll.c	\
	ssl_server_proto.c \
	server_spdy.c	\
	server_http2.c	\
	server_common.c	\

ifeq "${BUILD_USE_SAMARA_SSL}" ""
#
# HTTP2_SUPPORT redefine the following to the SSL libs supporting HTTP2
#
OPENSSL_INCLUDE=-I/usr/local/openssl/include
OPENSSL_LIBDIR=/usr/local/openssl/lib
OPENSSL_SSL_LIB=/usr/local/openssl/lib/libssl.a
OPENSSL_CRYPTO_LIB=/usr/local/openssl/lib/libcrypto.a
OPENSSL_DEF=
else
OPENSSL_INCLUDE=
OPENSSL_SSL_LIB=
OPENSSL_CRYPTO_LIB=
OPENSSL_DEF=-DUSE_SAMARA_SSL
endif


#
# HTTP2_SUPPORT add the following to INCLUDES
# -I<Path to spdylay include directory>
# -I<Path to nghttp2 include directory>
#
INCLUDES=\
 ${OPENSSL_INCLUDE} \
 -I${PROD_TREE_ROOT}/src/lib/libgcl \



CFLAGS= -fPIC ${BUILD_EXTRA_CFLAGS} 
#
# HTTP2_SUPPORT enable the following
## CFLAGS+= -DHTTP2_SUPPORT

CFLAGS += \
 ${OPENSSL_DEF} \
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

ifeq (${BUILD_GCOV}, 1)
	CFLAGS += -fprofile-arcs -ftest-coverage
endif

ifeq "${BUILD_USE_SAMARA_SSL}" ""
#
# HTTP2_SUPPORT add the following to LDFLAGS
# -L<Path to spdylay lib directory> -lspdylay
# -L<Path to nghttp2 lib directory> -lnghttp2
#
LDFLAGS+= -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` \
 -lelf \
 -L/usr/lib64 \
 -L${OPENSSL_LIBDIR} -ldl \
 -lcprops
else
LDFLAGS+= -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` -lelf -L/usr/lib64 -lssl -lcrypto -lcprops
endif


# XXX these should be done in a more compact way
LDADD= \
 ${OPENSSL_SSL_LIB} \
 ${OPENSSL_CRYPTO_LIB} \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
	${BUILD_EXTRA_LDADD}

#
# HTTP2_SUPPORT enable the following
#
## LDADD+= ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/proto_http/libproto_HTTP.a


include ${PROD_TREE_ROOT}/src/mk/prog.mk

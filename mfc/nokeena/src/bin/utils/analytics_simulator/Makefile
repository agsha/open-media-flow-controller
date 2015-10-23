# Filename: bin/utils/analytics_simulator/Makefile

include ${PROD_TREE_ROOT}../../../../src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=analytics_sim

SRCS=                    \
	log_parser.c \
	config_parser.c \
	nkn_am_hotness.c \
	nkn_core_engine.c \
	nkn_bm.c \
	nkn_am.c \
	nkn_dm2.c \
#	../../../lib/nvsd/common/nkn_debug.c


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


CFLAGS= -Wall -Werror -Wno-missing-format-attribute \
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
 -luuid
else
LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` `xml2-config --libs` -lm -lelf -ldl 
endif


LDADD= \
 ${OPENSSL_SSL_LIB} \
 ${OPENSSL_CRYPTO_LIB} \
#	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/am/libnvsd_am.a \
#	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
#	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
#	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0 \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

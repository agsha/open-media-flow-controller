# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=nknlogd

SRCS=                 \
	log_cfg.c      \
	log_cfg_accesslog.c      \
	log_channel.c \
	log_file.c     \
	log_main.c     \
	log_network.c  \
	log_mgmt.c	\
	log_accesslog.c \
	log_upload.c	\
	al_hashtable.c	\
	log_streamlog.c \
	log_debuglog.c \
	log_uds_server.c \
	lgd_worker.c    \


CFLAGS=-I${PROD_TREE_ROOT}/src/lib/libgcl -fPIC ${BUILD_EXTRA_CFLAGS} -DUSE_NEW_NKNLOG #-DINCLUDE_GCL

CFLAGS += \
 -DNKN_BUILD_PROD_RELEASE=\"${NKN_BUILD_PROD_RELEASE}\" \

ifeq (${BUILD_GCOV}, 1)
        CFLAGS+= -fprofile-arcs -ftest-coverage
    LDFLAGS+= -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDFLAGS+= -lpthread `pkg-config ` -lelf -lcprops -lssl -lcurl -lm

# XXX these should be done in a more compact way
LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_mt/libtc_mt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

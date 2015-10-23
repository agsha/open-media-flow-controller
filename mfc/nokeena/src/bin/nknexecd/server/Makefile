#  Makefile nknexecd
#
#  Date:      24 February 2012
#  Author:    Raja Gopal Andra (randra@juniper.net)
# 
#  (C) Copyright 2012, Juniper Networks, Inc.
#  All rights reserved.
# 

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=nknexecd

SRCS=                 \
	nknexecd_main.c     \

CFLAGS=-std=gnu99 -I${PROD_TREE_ROOT}/src/lib/libgcl -fPIC ${BUILD_EXTRA_CFLAGS} -DUSE_NEW_NKNLOG #-DINCLUDE_GCL
ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDFLAGS+= -lpthread `pkg-config `

# XXX these should be done in a more compact way
LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/common/libnvsd_common.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

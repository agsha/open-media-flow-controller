# Filename: vpe_transcode/yt_flv_meta_mod/Makefile 
#
#
include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=yt_flv_meta_mod


SRCS = \
        yt_flv_meta_mod.c\
#        ${PROD_CUSTOMER_ROOT}/nokeena/src/lib/nvsd/memalloc/nkn_memalloc.c

#CFLAGS = -Wall -Werror -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE\
#-U_USE_NKN_VFS -g `xml2-config --cflags`  #-D_USE_INTERFACE_
CFLAGS += -Wall -Werror -fPIC -Idisp \
-I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 \
-I/usr/lib/glib-2.0/include \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include \
-I${PROD_TREE_ROOT}/src/lib/libgcl  ${BUILD_EXTRA_CFLAGS} \


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
       GETTEXT_PACKAGE=${PROG}
       DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
       SUBMAKEFILES+=Makefile.i18n
endif

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif

LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` \
-lm -lelf -ldl -lssl -luuid -lxml2 -L/usr/lib64/python2.6/ -lpython2.6 \
-lcares -lcprops\



LDADD= \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/rtsched/libnvsd_rtsched.a\
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/sched/libnvsd_sched.a\
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/common/libnvsd_common.a\
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nknlog/libnkn_log.a\
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/parser/libnvsd_parser.a\
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0\
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a\
       ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

# Filename: lib/nvsd/cluster/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=nvsd_cluster
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/cluster

SRCS= \
	cl.c \
	cl_cmm.c \
	cl_node_status.c \


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS +=-include nkn_no_alloc.h
CFLAGS +=-I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
CFLAGS +=-I${PROD_BUILD_OUTPUT_DIR}/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/image_kernel_smp/include
CFLAGS +=${BUILD_EXTRA_CFLAGS}

INCLUDES=\
 	-I${PROD_TREE_ROOT}/src/include     \
 	-I${PROD_TREE_ROOT}/src/lib/libevent     \
 	-I${PROD_TREE_ROOT}/src/bin/mgmtd     \
 	-I${PROD_TREE_ROOT}/src/lib/libgd     \

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
 ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

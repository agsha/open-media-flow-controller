# Filename: lib/nvsd/virtcachesvr/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=nvsd_virtcachesvr
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/virtcachesvr

SRCS= \
	vcs_fuse.c				\
	vcs_net.c				\
	vcs_virt.c				\
	vcs_workq.c				\
	nkn_virt_cache_server.c	\


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DNKN_DM2_GLOBAL_LOCK_DEBUG
CFLAGS += -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/local/include/fuse/
#CFLAGS += -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/fuse
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a 
 ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

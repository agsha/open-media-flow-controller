# Filename: lib/nvsd/diskmgr2/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk
# CUSTOMER_DEFINES+= -DBUILD_FAKE_PATCHED_GLIB2 

LIB=nvsd_diskmgr2
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/diskmgr2

SRCS= \
	diskmgr2_attr_read.c	\
	diskmgr2_common.c	\
	diskmgr2_util.c		\
	diskmgr2_locking.c	\
	diskmgr2_mgmt.c		\
	diskmgr2_namespace.c	\
	diskmgr2_preread.c	\
	diskmgr2_init.c		\
	nkn_diskmgr2.c		\
	nkn_diskmgr2_api.c	\
	diskmgr2_evict.c	\
	nkn_locmgr2_extent.c	\
	nkn_locmgr2_uri.c	\
	nkn_locmgr2_container.c \
	nkn_locmgr2_physid.c	\
	nkn_dm2_tester.c	\


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DCENTOS_6
CFLAGS += -DNKN_DM2_GLOBAL_LOCK_DEBUG
CFLAGS += -include nkn_no_alloc.h
CFLAGS += -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# XXX these should be done in a more compact way
LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
 ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

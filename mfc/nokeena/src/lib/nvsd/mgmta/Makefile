# Filename: lib/nvsd/mgmta/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=nvsd_mgmta
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/mgmta

SRCS= \
	nkn_cfg_params.c   \
	nkn_log_strings.c  \
	nkn_log_infra.c    \
	nkn_cfg_infra.c    \
	nkn_cfg_func.c     \


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
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

LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
 ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

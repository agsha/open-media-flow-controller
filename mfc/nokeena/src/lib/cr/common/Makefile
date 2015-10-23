# Filename: lib/cr/common/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=cr_common
LIB_BUILD_DIR=${CUST_LIB_DIR}/cr/common

SRCS= \
	cr_cfg_file_api.c\
	cr_utils.c\
	nkn_free_list.c\
	nkn_ref_count_mem.c\
	nkn_sync_http_client.c\
	rrecord_msg_utils.c\
	nkn_dir_watch.c\

CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I/usr/lib64/glib-2.0/include -I/usr/include/libxml2 \
-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr/common \
-I${PROD_CUSTOMER_ROOT}/nokeena/src/include/cr
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk

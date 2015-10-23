# Filename: lib/nvsd/ssp/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=nvsd_ssp
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/ssp

SRCS= \
	nkn_ssp_breakplayer.c \
	nkn_ssp_genericplayer.c \
	nkn_ssp_moveplayer.c \
	nkn_ssp_smoothflowplayer.c \
	nkn_ssp_yahooplayer.c \
	nkn_ssp_youtubeplayer.c \
	nkn_ssp_smoothstream_pub.c \
	nkn_ssp_flashstream_pub.c \
	ssp_authentication.c \
	ssp_queryparams.c \
	ssp.c \
	nkn_ssp_read_metadata.c \
	nkn_ssp_sessionbind.c \
	nkn_ssp_player_prop.c \


ifeq "${BUILD_USE_SAMARA_SSL}" ""
OPENSSL_INCLUDE=-I/usr/local/openssl/include
else
OPENSSL_INCLUDE=
endif

INCLUDES= \
 ${OPENSSL_INCLUDE} \
 -I/usr/lib64/glib-2.0/include \
 -I/usr/include/libxml2 \
 -I/usr/include/glib-2.0 \
 -I/usr/lib/glib-2.0/include \


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -include nkn_no_alloc.h
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif

LDFLAGS += -L/usr/lib64 -lm -lglib2.0

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
 ${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk

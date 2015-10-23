# Filename: lib/nvsd/pythonlib/Makefile

WARNINGS_OVERRIDE = 1
include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=nvsd_pythonlib
LIB_BUILD_DIR=${CUST_LIB_DIR}/nvsd/pythonlib

SRCS=				\
	ssp_python_plugin.c	\
	rtsp_python_videoingest.c	\


CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -include nkn_no_alloc.h
CFLAGS += -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/python2.7 -I/usr/include
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
  CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# This library will take more effort to make compile -Wall,-Werror
LDFLAGS += -L/usr/lib64 -lpthread -lpython2.4 -ldl -lutil -lm -L/usr/lib64/python2.4/ -lpython2.4 -L/usr/lib64

LDADD= \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
 ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk

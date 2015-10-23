#
#  @file src/lib/jpsd/libdiameter/Makefile
#  @brief
#  Makefile - Makefile for libdiameter library
#
#  Yuvaraja Mariappan, Dec 2014
#
#  Copyright (c) 2015, Juniper Networks, Inc.
#  All rights reserved.
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB = diameter
LIB_BUILD_DIR = ${CUST_LIB_DIR}/jpsd/libdiameter

SRCS = \
	diameter_system.c \
	diameter_timer.c \
	diameter_transport.c \
	diameter_peer_event.c \
	diameter_message_event.c \
	diameter_avp.c \

CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -include nkn_no_alloc.h
CFLAGS += -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
CFLAGS += -I${PROD_BUILD_OUTPUT_DIR}/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/image_kernel_smp/include
CFLAGS += -I../../../thirdparty/lib/ketama/libketama
CFLAGS += -I./../h/ -I./../libdiameter-base/h/ -I./../libdiameter-avp/h/ -I.
CFLAGS += -I${PROD_CUSTOMER_ROOT}/nokeena/src/bin/jpsd/
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
	CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDADD = \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a  \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/lib.mk

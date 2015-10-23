#  Filename: Makefile 
#  (C) Copyright 2014, Juniper Networks, Inc.
#  All rights reserved.
# 

WARNINGS_OVERRIDE = 1
include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=MapURL

CLEANFILES +=

CFLAGS += -g
CXXFLAGS +=
LDFLAGS +=
SRCS= \
	mapurl.cpp \
	URLfmtMap.cpp \
	IWFfmtMap.cpp \
	CALEAfmtMap.cpp \
	Utils.cpp

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk

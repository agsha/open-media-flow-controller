#  Filename: nvsd/Makefile
#  (C) Copyright 2008-2009 Nokeena Networks, Inc.
#  All rights reserved.
#
include ${PROD_TREE_ROOT}/src/mk/common.mk

#PROD_INSTALL_PREFIX=/opt/nkn
#PROG_INSTALL_DIR=sbin
PROD_INSTALL_PREFIX=/opt/tms/lib/web
PROG_INSTALL_DIR=cgi-bin
PROG=agentd_comm

PROG_PERM_OWNER=root

# XXX/EMT: could we say "apache" here?  Presuming not since that
# user may not exist with same gid on build machine.
PROG_PERM_GROUP=48

PROG_PERM_MODE=4777


SRCS=                   \
	agentd_comm.c          \

WARNINGS_ARE_ERRORS=0

CFLAGS= \
 -Wall -Werror -Wno-missing-format-attribute \
 -fPIC \
 -I${PROD_TREE_ROOT}/src/include \
 ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
	CFLAGS += -fprofile-arcs -ftest-coverage
endif

CFLAGS += \
 -DNKN_BUILD_ID=\"${NKN_BUILD_ID}\" \
 -DNKN_BUILD_PROD_RELEASE=\"${NKN_BUILD_PROD_RELEASE}\" \
 -DNKN_BUILD_NUMBER=\"${NKN_BUILD_NUMBER}\" \
 -DNKN_BUILD_SCM_INFO_SHORT=\"${NKN_BUILD_SCM_INFO_SHORT}\" \
 -DNKN_BUILD_SCM_INFO=\"${NKN_BUILD_SCM_INFO}\" \
 -DEXTRA_CFLAGS_DEF=\"${BUILD_EXTRA_CFLAGS}\" \

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

THIRDPARTYLIBS= \

LDADD= \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

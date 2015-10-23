#  Makefile
#
#  Filename:  src/bin/apps/nkncnt
#  Date:      2008-11-05
#  Author:    Ramanand Narayanan
# 
#  (C) Copyright 2008 Nokeena Networks, Inc.  
#  All rights reserved.
# 

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=ingest_req

SRCS=  ingest_req.c      \

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif

LDADD= \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk

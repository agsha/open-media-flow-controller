#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=crontab
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/usr/bin
PROG_STRIP=1

vpath %.c ../../src

LICENSE_REQUIRES_REDIST=1

SRCS=	\
	compat.c \
	crontab.c \
	entry.c \
	env.c \
	misc.c \

CFLAGS= \
	-std=gnu89

INCLUDES= \
	-I../../src

DEFINES= \
	-DPOSIX \
	-U_XOPEN_SOURCE

WARNINGS_OVERRIDE=1

include ${PROD_TREE_ROOT}/src/mk/prog.mk

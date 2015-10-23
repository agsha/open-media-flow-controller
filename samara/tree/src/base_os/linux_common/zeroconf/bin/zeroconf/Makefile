#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=zeroconf
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/usr/bin

LICENSE_REQUIRES_REDIST=1

vpath %.c ../../src

SRCS=	\
	zeroconf.c \
	delay.c \


INCLUDES= -I../../src
CFLAGS+= -D_GNU_SOURCE

LDADD=

DEFINES=

WARNINGS_OVERRIDE=1

include ${PROD_TREE_ROOT}/src/mk/prog.mk

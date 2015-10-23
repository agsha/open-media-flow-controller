#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=klogd
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/sbin
PROG_STRIP=1

LICENSE_REQUIRES_REDIST=1

vpath %.c ../../src

SRCS=	\
	klogd.c \
	syslog.c \
	pidfile.c \
	ksym.c \
	ksym_mod.c \
	zonechange_gpl.c \


CFLAGS= \
	-DSYSV \
	-D_GNU_SOURCE \
	-DALLOW_KERNEL_LOGGING \

INCLUDES= \

DEFINES= \
	-DFSSTND

WARNINGS_OVERRIDE=1

include ${PROD_TREE_ROOT}/src/mk/prog.mk

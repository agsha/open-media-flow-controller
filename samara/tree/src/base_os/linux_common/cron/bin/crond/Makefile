#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=crond
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/usr/sbin
PROG_STRIP=1

LICENSE_REQUIRES_REDIST=1

vpath %.c ../../src

SRCS=	\
	compat.c \
	cron.c \
	database.c \
	do_command.c \
	entry.c \
	env.c \
	job.c \
	misc.c \
	popen.c \
	user.c

CFLAGS= \
	-std=gnu89 

INCLUDES= \
	-I../../src

DEFINES= \
	-DPOSIX \
	-U_XOPEN_SOURCE

WARNINGS_OVERRIDE=1

ifeq ($(call CC_VERSION_ATLEAST,30404,1,0),1)
	WARNINGS+= \
		-Wno-deprecated-declarations \

endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk

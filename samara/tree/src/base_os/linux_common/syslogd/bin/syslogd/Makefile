#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=syslogd
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/sbin
PROG_STRIP=1

LICENSE_REQUIRES_REDIST=1

vpath %.c ../../src

SRCS=	\
	syslogd.c \
	pidfile.c \
	zonechange_gpl.c \

CFLAGS= \
	-DSYSV \
	-D_GNU_SOURCE

INCLUDES= \

# The following define establishes the name of the pid file for the
# syslogd daemon.  The library include file (paths.h) defines the
# name for the syslogd pid to be syslog.pid.  A number of people have
# suggested that this should be syslogd.pid.  You may cast your
# ballot below.
SYSLOGD_PIDNAME = -DSYSLOGD_PIDNAME=\"syslogd.pid\"

DEFINES= \
	-DSYSLOG_INET \
	-DSYSLOG_UNIXAF \
	-DNO_SCCS \
	-DFSSTND \
	${SYSLOGD_PIDNAME} \
	-DALLOW_KERNEL_LOGGING

WARNINGS_OVERRIDE=1

include ${PROD_TREE_ROOT}/src/mk/prog.mk

#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=openbsd-compat

vpath %.c ../../src/openbsd-compat

SRCS=   \
	base64.c \
	basename.c \
	bindresvport.c \
	bsd-arc4random.c \
	bsd-cray.c \
	bsd-cygwin_util.c \
	bsd-getpeereid.c \
	bsd-misc.c \
	bsd-nextstep.c \
	bsd-openpty.c \
	bsd-snprintf.c \
	bsd-waitpid.c \
	daemon.c \
	dirname.c \
	fake-rfc2553.c \
	getcwd.c \
	getgrouplist.c \
	getopt.c \
	getrrsetbyname.c \
	glob.c \
	inet_aton.c \
	inet_ntoa.c \
	inet_ntop.c \
	mktemp.c \
	port-aix.c \
	port-irix.c \
	readpassphrase.c \
	realpath.c \
	rresvport.c \
	setenv.c \
	setproctitle.c \
	sigact.c \
	strlcat.c \
	strlcpy.c \
	strmode.c \
	strsep.c \
	strtoul.c \
	vis.c \
	xcrypt.c \
	xmmap.c \


LIB_BUILD_DIR=${ILIB_DIR}/base_os/common/openssh/lib/libopenbsd-compat

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/common/openssh/src \
	-I${PROD_TREE_ROOT}/src/base_os/common/openssh/src/openbsd-compat

DEFINES_OVERRIDE=1
DEFINES= \
	-DHAVE_CONFIG_H \

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Wall \

include ${PROD_TREE_ROOT}/src/mk/lib.mk

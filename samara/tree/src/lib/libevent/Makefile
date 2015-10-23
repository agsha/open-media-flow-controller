#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=event

SRCS=                      \
	event.c            \
	libevent_wrapper.c \
	poll.c             \
	signal.c           \
	err.c		   \
	event_fdintrcb.c   \



DEFINES= \
	-DHAVE_CONFIG_H

ifeq (${PROD_TARGET_OS},LINUX)
	DEFINES+= \
	       -D_SVID_SOURCE \
	       -D_POSIX_C_SOURCE=199506L \

endif


CFLAGS= -Icompat

LDADD=

include ${PROD_TREE_ROOT}/src/mk/lib.mk

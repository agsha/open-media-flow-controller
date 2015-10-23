#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=zile

vpath %.c ../../src/lib

SRCS=   \
	alist.c \
	astr.c \
	err.c \
	htable.c \
	memcmp.c \
	xmalloc.c \
	xrealloc.c \
	xstrdup.c \


LIB_BUILD_DIR=${ILIB_DIR}/base_os/common/zile/lib/libzile

INCLUDES= -I../../src/src

DEFINES=-D_GNU_SOURCE

WARNINGS_EXTRA_OVERRIDE=1

include ${PROD_TREE_ROOT}/src/mk/lib.mk

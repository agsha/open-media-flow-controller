#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=lfi

INSTALL_TREE=rootflop
PROD_INSTALL_PREFIX=
PROG_STRIP=1
PROG_STRIP_FLAGS=--strip-unneeded -w -K 'rcsid*' -K 'build_*'

ifeq (${PROD_TARGET_PLATFORM_FULL}-${ROOTFLOP_OLD_BINARYS},LINUX_EL_EL5-1)
	export PROD_TARGET_CPU_WORDSIZE=32
endif

vpath %.c ../../src

SRCS= \
	lfi.c

INCLUDES=

LDADD= \

include ${PROD_TREE_ROOT}/src/mk/prog.mk

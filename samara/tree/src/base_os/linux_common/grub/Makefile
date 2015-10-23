# Makefile for grub
include ${PROD_TREE_ROOT}/src/mk/common.mk

# NOTE: Grub is only supported on x86 (i386 or x86_64)

ifeq (${PROD_TARGET_ARCH_FAMILY}-${PROD_TARGET_PLATFORM_FULL},X86-LINUX_EL_EL6)
    SUBMAKEFILES+= Makefile.el6
endif

include ${PROD_TREE_ROOT}/src/mk/packages.mk

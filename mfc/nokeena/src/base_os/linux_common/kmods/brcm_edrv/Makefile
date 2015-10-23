#  Makefile
#
#  Filename:  /trunk/nokeena/src/base_os/linux_common/kmods/brcm_edrv
#  Date:      2012-04-04
#  Author:    Ramanand  Narayanan
#
#  Purpose: This is a broadcom diag driver that is needed to run the 
#           diag utility lediag. 
#
#  (C) Copyright 2008-12 Juniper Networks, Inc.
#  All rights reserved.
#

#                                                                                                                                   
# Broadcom diag driver                                                                                                                 
#               

BUILDTOOLS_OVERRIDE=1

include ${PROD_TREE_ROOT}/src/mk/common.mk

# Build for the kernel(s) we're using, but not for the bootfloppy
KMOD_KERNEL_TARGETS=
ifneq (${PROD_FEATURE_KERNEL_IMAGE_UNI},0)
KMOD_KERNEL_TARGETS+=image_kernel_uni
endif
ifneq (${PROD_FEATURE_KERNEL_IMAGE_SMP},0)
KMOD_KERNEL_TARGETS+=image_kernel_smp
endif

obj-m   += edrv.o
edrv-objs = debug.o ediag_compat.o ediag_drv.o

EXTRA_CFLAGS+=-DLINUX -D__LINUX -D__KERNEL__ -O2 -pipe -DKPROBES_SYMBOL_NAME 
EXTRA_CFLAGS += -D_INLINE_LISTQ_CALLS=0 -DDBG=1 -DWOL_DBG_PRINT=0 -DLOG_REG_ACCESS=0 -DL2_RX_BUF_SIG=0

KMOD_ID=edrv

include ${PROD_TREE_ROOT}/src/mk/kmod.mk


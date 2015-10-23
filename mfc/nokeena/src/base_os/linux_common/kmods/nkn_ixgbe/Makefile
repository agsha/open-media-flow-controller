#  Makefile
#
#  Filename:  /trunk/nokeena/src/base_os/linux_common/kmods/nkn_igb
#  Date:      2008-11-05
#  Author:    Max He
#
#  (C) Copyright 2008 Nokeena Networks, Inc.
#  All rights reserved.
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
# ifneq (${PROD_FEATURE_KERNEL_BOOTFLOPPY_UNI},0)
# KMOD_KERNEL_TARGETS+=bootfloppy_kernel_uni
# endif

obj-m   += ixgbe.o
ixgbe-objs = ixgbe_main.o ixgbe_common.o ixgbe_api.o ixgbe_param.o \
           ixgbe_ethtool.o kcompat.o ixgbe_82598.o \
           ixgbe_82599.o \
           ixgbe_x540.o \
           ixgbe_sriov.o ixgbe_mbx.o \
           ixgbe_dcb.o ixgbe_dcb_82598.o \
           ixgbe_dcb_82599.o ixgbe_dcb_nl.o \
           ixgbe_phy.o ixgbe_fcoe.o ixgbe_sysfs.o 


ifeq (${PROD_TARGET_ARCH},X86_64)
	EXTRA_CFLAGS+=-mcmodel=kernel -mno-red-zone -DIXGBE_NO_LRO -DIXGBE_NO_NAPI\

endif

EXTRA_CFLAGS+=-DDRIVER_IXGBE -DDRIVER_NAME=ixgbe -DDRIVER_NAME_CAPS=IXGBE-DLINUX \
	-O2 -D__SMP__ -D__KERNEL__ -DMODULE -pipe -Wall \
	-I${PROD_CUSTOMER_ROOT}/nokeena/src/include
#	-DNKN_ENABLE

KMOD_ID=ixgbe

include ${PROD_TREE_ROOT}/src/mk/kmod.mk


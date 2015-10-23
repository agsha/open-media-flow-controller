#  Makefile
#
#  Filename:  /trunk/nokeena/src/base_os/linux_common/kmods/pfring
#  Date:      2012-04-09
#
#  (C) Copyright 2012 Juniper Networks
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

obj-m   += pf_ring.o


ifeq (${PROD_TARGET_ARCH},X86_64)
	EXTRA_CFLAGS+=-mcmodel=kernel -mno-red-zone \

endif

EXTRA_CFLAGS+= -O2 -D__SMP__ -D__KERNEL__ -DMODULE -pipe -Wall \
	-I${PROD_CUSTOMER_ROOT}/nokeena/src/include \
	-I${PROD_CUSTOMER_ROOT}/nokeena/src/base_os/linux_common/kmods/pfring
#	-DNKN_ENABLE

KMOD_ID=pf_ring

include ${PROD_TREE_ROOT}/src/mk/kmod.mk

#  Makefile
#
#

# This file is included by common.mk , so do not include anything here.

# To use this file, set PROD_TARGET_ID to ppc_6xx in your customer.mk .
# Do not attempt to directly include this file.

PROD_TARGET_OS=${PROD_HOST_OS}
PROD_TARGET_PLATFORM=${PROD_HOST_PLATFORM}

# This test is because all our PowerPC target is actually ELDK-related,
# and it historically has been called "LINUX_EL_EL5" .
ifeq (${PROD_HOST_PLATFORM_FULL},LINUX_EL_EL6)
    PROD_TARGET_PLATFORM_VERSION=EL5
    PROD_TARGET_PLATFORM_FULL=LINUX_EL_EL5
else
    PROD_TARGET_PLATFORM_VERSION=${PROD_HOST_PLATFORM_VERSION}
    PROD_TARGET_PLATFORM_FULL=${PROD_HOST_PLATFORM_FULL}
endif

PROD_TARGET_ARCH=PPC
PROD_TARGET_CPU=PPC6XX
PROD_TARGET_CPU_SUPPORTED_WORDSIZES=32
PROD_TARGET_CPU_DEFAULT_WORDSIZE=32

ELDK_DIR?=/opt/eldk/ppc-v42-1

PROD_TARGET_TOOLCHAIN_DIR=${ELDK_DIR}/usr/bin/
PROD_TARGET_TOOLCHAIN_PREFIX=ppc_6xx-
PROD_TARGET_TOOLCHAIN_EXTRA_DIR=${ELDK_DIR}/bin/
PROD_TARGET_TOOLCHAIN_TARGET_ROOT=${ELDK_DIR}/ppc_6xx/

PROD_TARGET_ATAG=ppc_6xx
PROD_TARGET_ATAG_DASHED=${PROD_TARGET_ATAG}-
PROD_TARGET_ATAG_PREUNDER=_${PROD_TARGET_ATAG}

PROD_TARGET_CPU_ENDIAN=BIG
PROD_TARGET_CPU_ALIGNMENT=8

PROD_TARGET_CC_VERSION=4.2.2
PROD_TARGET_COMPILE_FOR_TEXT=for ppc_6xx 
PROD_HOST_MAKE_VERSION=3.81

PROD_TARGET_HWNAME_REQUIRED=1
PROD_TARGET_ID_NAME=${PROD_TARGET_ARCH_LC}-${PROD_TARGET_HWNAME_LC}

# The ELDK does not work without this export'ed, also kernel build requires
export CROSS_COMPILE=ppc_6xx-

# The cross kernel build needs this.  Note we use the build server's depmod
export CROSS_PATH_PREADD:=${ELDK_DIR}/usr/bin:${ELDK_DIR}/bin

# The following three settings are suggested in the docs
# XXXX? are any or all of these required?
### export ARCH=ppc
### export PATH:=${PATH}:${ELDK_DIR}/usr/bin:${ELDK_DIR}/bin

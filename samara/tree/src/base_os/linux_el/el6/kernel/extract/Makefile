# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

# This makefile converts rpm's to tar's on 'all' and dumps the tar's into
# the image install tree on 'install'

#######
# NOTE: DIST_EL_EL6 is set in customer.inc via common.mk to "${PROD_CUSTOMER_ROOT}/dist/centos/6"
# This dist.inc file sets various DIST_EL_EL6_* settings.
include ${DIST_EL_EL6}/dist.inc

KERNEL_DIR=${PROD_TREE_ROOT}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel
FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/dist/kernel
CONFIG_FILES=

# Graft point to pick up SRPM pkg_config settings
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/arch_${PROD_TARGET_ARCH_LC}/image_packages/Makefile.inc

# Graft point to pick up kernel patches
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/extract/Makefile.inc


include ${PROD_TREE_ROOT}/src/mk/packages.mk

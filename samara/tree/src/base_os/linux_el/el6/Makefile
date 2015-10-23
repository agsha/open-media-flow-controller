#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

# The order of these matters, as the packages must be written before *_files
SUBDIRS=
ifneq (${PROD_FEATURE_DEV_ENV},1)
	SUBDIRS+=arch_${PROD_TARGET_ARCH_LC}
endif

SUBDIRS+=\
	image_files \
	initscripts_files \

ifneq (${PROD_FEATURE_DEV_ENV},1)
SUBDIRS+=\
	rootflop_files \
	pam \

endif

ifneq (${PROD_FEATURE_KERNELS_EXTERNAL},1)
	SUBDIRS+= kernel
endif

# Graft point for customer-specific additions to the base platform.
# e.g. a customer may want to build their own kernel or custom components.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/Makefile.inc \

# NOTE: These directories must be built and installed as 'root'

include ${PROD_TREE_ROOT}/src/mk/subdir.mk

#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

# The order of these matters, as the packages must be written before *_files
SUBDIRS=

ifneq (${PROD_FEATURE_DEV_ENV},1)
SUBDIRS+=\
	busybox \
	zile \

endif

ifneq (${PROD_TARGET_OS},FREEBSD)
SUBDIRS+=\
	less \
	openssh \
	ssmtp \

endif

SUBDIRS+=\
	apache \
	fonts \
	script_files \

ifeq (${PROD_FEATURE_GPT_PARTED}${PROD_FEATURE_DEV_ENV},10)
	SUBDIRS+=                      \
		parted \

endif

ifeq (${PROD_FEATURE_UBOOT}${PROD_FEATURE_DEV_ENV},10)
	SUBDIRS+=                      \
		u-boot \

endif

# NOTE: These directories must be built and installed as 'root'

include ${PROD_TREE_ROOT}/src/mk/subdir.mk

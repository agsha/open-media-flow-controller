#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

# The order of these matters, as the packages must be written before *_files

SUBDIRS=\
	${PROD_TARGET_PLATFORM_LC} \

ifeq (${PROD_TARGET_OS},LINUX)
SUBDIRS+=linux_common
endif

SUBDIRS+=common

# NOTE: These directories must be built and installed as 'root'

include ${PROD_TREE_ROOT}/src/mk/subdir.mk

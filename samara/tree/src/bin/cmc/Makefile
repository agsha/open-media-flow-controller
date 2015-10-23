#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

ifneq (${PROD_FEATURE_CMC_SERVER},0)
	INSTALL_DIRECTIVES=${SYS_CREATE_SYMLINK_rgp}
	SYS_CREATE_SYMLINK_rgp="symlink /opt/tms/bin/ /opt/tms/bin/rgp rgp_rendv"
	SUBDIRS+=                      \
		rbmd \

endif

ifneq (${PROD_FEATURE_CMC_CLIENT},0)
	SUBDIRS+=                      \
		rendv_client \

endif

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

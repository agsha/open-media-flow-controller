# Makefile

# Files listed in the FILES variable override the files of the same
# name in the Samara source $PROD_TREE_ROOT/src/bin/mgmtd/templates/

include ${PROD_TREE_ROOT}/src/mk/common.mk

FILES_INSTALL_DIR=/lib/md/templates

FILES= \
	httpd.conf            \

ifneq (${PROD_FEATURE_XINETD},0)
	FILES+= \
		xinetd.conf   \

endif

include ${PROD_TREE_ROOT}/src/mk/files.mk

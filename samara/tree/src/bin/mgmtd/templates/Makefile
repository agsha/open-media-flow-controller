#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

FILES_INSTALL_DIR=/lib/md/templates

FILES= \
	httpd.conf            \

ifneq (${PROD_FEATURE_XINETD},0)
	FILES+= \
		xinetd.conf   \

endif

ifneq (${PROD_FEATURE_VIRT},0)
	FILES+= \
		libvirtd.conf   \
		qemu.conf       \

endif

include ${PROD_TREE_ROOT}/src/mk/files.mk

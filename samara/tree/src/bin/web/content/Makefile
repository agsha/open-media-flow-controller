#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

FILES_INSTALL_DIR=/lib/web/content

FILES=                    \
	index.html        \
	tms-default.css   \
	tms-utils.js      \
	tms-graph-utils.js \
	favicon.ico       \

ifneq (${PROD_FEATURE_CLUSTER},0)
	FILES+=                    \
		cluster_upgrade.js \

endif

ifneq (${PROD_FEATURE_XML_GW},0)
	FILES+=               \
		xg-simple.xsl \

endif

SUBDIRS= \
	images \

include ${PROD_TREE_ROOT}/src/mk/files.mk

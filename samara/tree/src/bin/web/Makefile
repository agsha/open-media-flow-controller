#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS=\
	cgi_launcher        \
	conf                \
	request_handler     \
	template_compiler   \
	wsmd                \

ifneq (${PROD_FEATURE_WEB_REF_UI},0)
	SUBDIRS+=                   \
		content             \
		templates           \

endif

ifneq (${PROD_FEATURE_XML_GW},0)
	SUBDIRS+=           \
		xml_gw      \

endif

include ${PROD_TREE_ROOT}/src/mk/subdir.mk

#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS=\
	libcgi          \
	libcli          \
	libclish        \
	libcommon       \
	libevent        \
	libgcl          \
	liblicense      \
	liblicense_validation \
	libmdb          \
	libmdc_wrapper  \
	libremgcl 	\
	libtacl		\
	libtc_st        \
	libtcrypt       \
	libtecla        \
	libtexpr        \
	libwebcli       \
	libweb          \
	libwebtmpl      \


ifneq (${PROD_FEATURE_CLUSTER},0)
	SUBDIRS+=                      \
		libccl \

endif

ifneq (${PROD_FEATURE_GRAPHING},0)
	SUBDIRS+=                      \
		libgd \
		libgraph \

endif

ifneq (${PROD_FEATURE_MULTITHREADING},0)
	SUBDIRS+=                      \
		libtc_mt \

endif

ifneq (${PROD_FEATURE_JAVA},0)
	SUBDIRS+=                      \
		libtjava \

endif

ifneq (${PROD_FEATURE_XML_GW}${PROD_FEATURE_VIRT},00)
	SUBDIRS+=           \
		libtxml     \

endif

ifneq (${PROD_FEATURE_NVS},0)
	SUBDIRS+=           \
		libnvs      \

endif


include ${PROD_TREE_ROOT}/src/mk/subdir.mk

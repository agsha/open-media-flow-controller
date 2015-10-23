#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS=\
	cli \
	coreinfo \
	crypt \
	dumplicense \
	lfi \
	mddbreq \
	mdreq \
	mgmtd \
	genlicense \
	pm \
	progress \
	snmp \
	statsd \
	web \

ifneq (${PROD_FEATURE_ACCOUNTING},0)
	SUBDIRS+=                      \
		acctd \

endif

ifneq (${PROD_FEATURE_FRONT_PANEL},0)
	SUBDIRS+=                      \
		fpd \

endif

ifneq (${PROD_FEATURE_SCHED},0)
	SUBDIRS+=                      \
		sched \

endif

ifneq (${PROD_FEATURE_WIZARD},0)
	SUBDIRS+=                      \
		wizard \

endif

ifneq (${PROD_FEATURE_VIRT},0)
	SUBDIRS+=                      \
		tvirtd \
		tcons \

endif

ifneq (${PROD_FEATURE_CLUSTER},0)
	SUBDIRS+=                      \
		clusterd \

endif

ifneq (${PROD_FEATURE_CMC_CLIENT}${PROD_FEATURE_CMC_SERVER},00)
	SUBDIRS+=                      \
		cmc \

endif

ifneq (${PROD_FEATURE_CMC_CLIENT}${PROD_FEATURE_CMC_SERVER}${PROD_FEATURE_REMOTE_GCL},000)
	SUBDIRS+=                      \
		rgp \

endif

ifneq (${PROD_FEATURE_EETOOL},0)
	SUBDIRS+=                      \
		eetool \

endif

ifneq (${PROD_FEATURE_NVS},0)
	SUBDIRS+=                      \
		nvstool \

endif

include ${PROD_TREE_ROOT}/src/mk/subdir.mk

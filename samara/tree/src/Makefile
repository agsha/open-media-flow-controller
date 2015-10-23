#  Makefile

export VPATH=BIN_DIST
include ${PROD_TREE_ROOT}/src/mk/common.mk


SUBDIRS=            \
	lib         \
	bin         \
	base_os     \
	include	    \
        release     \

ifneq (${PROD_FEATURE_JAVA},0)
	SUBDIRS+=                      \
		java \

endif


include ${PROD_TREE_ROOT}/src/mk/subdir.mk

doxygen doxygen_install:
	${MAKE} -C ${PROD_TREE_ROOT}/doc/doxygen $@

javadoc javadoc_install:
	${MAKE} -C ${PROD_TREE_ROOT}/doc/javadoc $@

release:
	${MAKE} -C release release

.PHONY: release doxygen doxygen_install javadoc javadoc_install

# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS= \
  mflquery \
  mflweb \
  mfld \
  install_scripts \


include ${PROD_TREE_ROOT}/src/mk/subdir.mk

release:
	$(MAKE) -C mfld release

# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk
include ../Makefile.inc

FILES_INSTALL_DIR=cfg

FILES= \
  mfld.cfg \


SUBDIRS= \
  external \


include ${PROD_TREE_ROOT}/src/mk/files.mk

release:
	$(MAKE) -C external release

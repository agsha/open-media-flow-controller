#  Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_sbin} \
  ${SYS_INSTALL_FILE_lsiutil} \


CLEANFILES= \
    ${FULL_INSTALL_DIR}/sbin/lsiutil \


# Directory creation specs:
SYS_INSTALL_DIR_sbin="dir 0755 root root /sbin"

# File installation specs:

SYS_INSTALL_FILE_lsiutil="file lsiutil.32 0755 root root /sbin/lsiutil"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

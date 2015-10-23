#  Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_sbin} \
  ${SYS_INSTALL_FILE_sas2ircu} \


CLEANFILES= \
    ${FULL_INSTALL_DIR}/sbin/sas2ircu \


# Directory creation specs:
SYS_INSTALL_DIR_sbin="dir 0755 root root /sbin"

# File installation specs:

SYS_INSTALL_FILE_sas2ircu="file sas2ircu.32 0755 root root /sbin/sas2ircu"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

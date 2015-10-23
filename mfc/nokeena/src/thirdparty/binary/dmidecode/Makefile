#  Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_usr_sbin} \
  ${SYS_INSTALL_FILE_dmidecode} \


CLEANFILES= \
    ${FULL_INSTALL_DIR}/usr/sbin/dmidecode \


# Directory creation specs:
SYS_INSTALL_DIR_usr_sbin="dir 0755 root root /usr/sbin"

# File installation specs:

SYS_INSTALL_FILE_dmidecode="file 6/dmidecode 0755 root root /usr/sbin/dmidecode"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

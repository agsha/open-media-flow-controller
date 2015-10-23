#  Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_usr} \
  ${SYS_INSTALL_DIR_usr_bin} \
  ${SYS_INSTALL_FILE_ssh_43p2} \


CLEANFILES= \
    ${FULL_INSTALL_DIR}/usr/bin/ssh-43p2 \


# Directory creation specs:
SYS_INSTALL_DIR_usr="dir 0755 root root /usr"
SYS_INSTALL_DIR_usr_bin="dir 0755 root root /usr/bin"

# File installation specs:

SYS_INSTALL_FILE_ssh_43p2="file  6/ssh-43p2 0755 root root /usr/bin/ssh-43p2"


include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

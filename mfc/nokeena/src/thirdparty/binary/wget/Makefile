#  Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_opt_nkn} \
  ${SYS_INSTALL_DIR_opt_nkn_bin} \
  ${SYS_INSTALL_FILE_wget} \


CLEANFILES= \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/wget \


# Directory creation specs:
SYS_INSTALL_DIR_opt_nkn="dir 0755 root root /opt/nkn"
SYS_INSTALL_DIR_opt_nkn_bin="dir 0755 root root /opt/nkn/bin"

# File installation specs:

SYS_INSTALL_FILE_wget="file  wget  0755 root root /opt/nkn/bin/wget"


include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

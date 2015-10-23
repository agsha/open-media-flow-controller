# Makefile to put thirdparty binary files into the MFC package.  These binary files
# are NOT built from source each time MFC is built.
#
# Note: If thirdparty files are added or removed from the MFC package, and
#   if the files are GPL or have some other license that needs to have the
#   source made available, update nokeena/src/release/freedist/Makefile.inc

# License types:
#   tw_cli      ???

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_opt_nkn} \
  ${SYS_INSTALL_DIR_opt_nkn_bin} \
  ${SYS_INSTALL_FILE_tw_cli} \


CLEANFILES= \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/tw_cli \


# Directory creation specs:
SYS_INSTALL_DIR_opt_nkn="dir         0755 root root /opt/nkn"
SYS_INSTALL_DIR_opt_nkn_bin="dir     0755 root root /opt/nkn/bin"

# File installation specs:
SYS_INSTALL_FILE_tw_cli="file tw_cli 0755 root root /opt/nkn/bin/tw_cli"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

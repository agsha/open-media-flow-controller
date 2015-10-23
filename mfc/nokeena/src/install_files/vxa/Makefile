# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_opt_nkn} \
  ${SYS_INSTALL_DIR_opt_nkn_bin} \
  ${SYS_INSTALL_FILE_vxa_eth_setup_sh} \
  ${SYS_INSTALL_FILE_eth_test_sh} \

CLEANFILES= \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/vxa-eth-setup.sh

SYS_INSTALL_DIR_opt_nkn="dir 0755 root root /opt/nkn"
SYS_INSTALL_DIR_opt_nkn_bin="dir 0755 root root /opt/nkn/bin"
SYS_INSTALL_FILE_vxa_eth_setup_sh="file vxa-eth-setup.sh 0755 root root /opt/nkn/bin/vxa-eth-setup.sh"
SYS_INSTALL_FILE_eth_test_sh="file      eth-test.sh      0755 root root /opt/nkn/bin/eth-test.sh"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_opt_nkn} \
  ${SYS_INSTALL_DIR_opt_nkn_startup} \
  ${SYS_INSTALL_FILE_cpuperf_sh} \
  ${SYS_INSTALL_FILE_dmi_adapter_install_sh} \

CLEANFILES= \
    ${FULL_INSTALL_DIR}/opt/nkn/startup/cpuperf.sh

SYS_INSTALL_DIR_opt_nkn="dir 0755 root root /opt/nkn"
SYS_INSTALL_DIR_opt_nkn_startup="dir 0755 root root /opt/nkn/startup"
SYS_INSTALL_FILE_cpuperf_sh="file cpuperf.sh 0755 root root /opt/nkn/startup/cpuperf.sh"
SYS_INSTALL_FILE_dmi_adapter_install_sh="file dmi_adapter_install.sh 0755 root root /opt/nkn/startup/dmi_adapter_install.sh"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

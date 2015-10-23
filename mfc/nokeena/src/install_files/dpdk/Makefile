# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
    ${SYS_INSTALL_DIR_opt} \
    ${SYS_INSTALL_DIR_opt_dpdk} \
	${SYS_INSTALL_FILE_dpi_tproxy_sh} \
	${SYS_INSTALL_FILE_pci_unbind_py} \
	${SYS_INSTALL_FILE_switch_intf_dpdk_sh} \
	${SYS_INSTALL_FILE_igb_uio_ko} \
	${SYS_INSTALL_FILE_switch_intf_normal_sh} \


CLEANFILES= \
    ${FULL_INSTALL_DIR}/opt/dpdk/dpi_tproxy.sh \
    ${FULL_INSTALL_DIR}/opt/dpdk/pci_unbind.py \
    ${FULL_INSTALL_DIR}/opt/dpdk/switch_intf_dpdk.sh \
    ${FULL_INSTALL_DIR}/opt/dpdk/igb_uio.ko \
    ${FULL_INSTALL_DIR}/opt/dpdk/switch_intf_normal.sh \


SYS_INSTALL_DIR_opt="dir 0755 root root /opt"
SYS_INSTALL_DIR_opt_dpdk="dir 0755 root root /opt/dpdk"

SYS_INSTALL_FILE_dpi_tproxy_sh="file dpi_tproxy.sh 0755 root root /opt/dpdk/dpi_tproxy.sh"
SYS_INSTALL_FILE_pci_unbind_py="file pci_unbind.py 0755 root root /opt/dpdk/pci_unbind.py"
SYS_INSTALL_FILE_switch_intf_dpdk_sh="file switch_intf_dpdk.sh 0755 root root /opt/dpdk/switch_intf_dpdk.sh"
SYS_INSTALL_FILE_igb_uio_ko="file igb_uio.ko 644 root root /opt/dpdk/igb_uio.ko"
SYS_INSTALL_FILE_switch_intf_normal_sh="file switch_intf_normal.sh 0755 root root /opt/dpdk/switch_intf_normal.sh"


include ${PROD_TREE_ROOT}/src/mk/files_adv.mk


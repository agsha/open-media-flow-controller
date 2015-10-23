# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_opt_nkn} \
  ${SYS_INSTALL_DIR_opt_nkn_bin} \
  ${SYS_INSTALL_DIR_opt_nkn_pacifica} \
  ${SYS_INSTALL_DIR_opt_nkn_pacifica_initrd_etc} \
  ${SYS_INSTALL_DIR_opt_nkn_pacifica_initrd_usr_share_udhcpc} \
  ${SYS_INSTALL_FILE_elilo_efi} \
  ${SYS_INSTALL_FILE_elilo_conf_template} \
  ${SYS_INSTALL_FILE_initrd_img} \
  ${SYS_INSTALL_FILE_juniper_banner_msg} \
  ${SYS_INSTALL_FILE_rc_sysinit} \
  ${SYS_INSTALL_FILE_dhclient_conf} \
  ${SYS_INSTALL_FILE_dhclient_exit_hooks} \
  ${SYS_INSTALL_FILE_irq_smpaffinity_bnx2x_sh} \
  ${SYS_INSTALL_FILE_initrd_init} \
  ${SYS_INSTALL_FILE_initrd_etc_pacifica_hw} \
  ${SYS_INSTALL_FILE_initrd_udhcp_default_script} \

CLEANFILES= \
    ${FULL_INSTALL_DIR}/opt/nkn/pacifica/elilo.efi \
    ${FULL_INSTALL_DIR}/opt/nkn/pacifica/elilo.conf.template \
    ${FULL_INSTALL_DIR}/opt/nkn/pacifica/initrd.img \
    ${FULL_INSTALL_DIR}/opt/nkn/pacifica/juniper-banner.msg \
    ${FULL_INSTALL_DIR}/opt/nkn/pacifica/rc.sysinit \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/irq-smpaffinity-bnx2x.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/pacifica/initrd/init \
    ${FULL_INSTALL_DIR}/opt/nkn/pacifica/initrd/etc/pacifica_hw \
    ${FULL_INSTALL_DIR}/opt/nkn/pacifica/initrd/usr/share/udhcpc/default.script \

SYS_INSTALL_DIR_opt_nkn="dir 0755 root root /opt/nkn"
SYS_INSTALL_DIR_opt_nkn_bin="dir 0755 root root /opt/nkn/bin"
SYS_INSTALL_DIR_opt_nkn_pacifica="dir 0755 root root /opt/nkn/pacifica"
SYS_INSTALL_DIR_opt_nkn_pacifica_initrd_etc="dir 0755 root root /opt/nkn/pacifica/initrd/etc"
SYS_INSTALL_DIR_opt_nkn_pacifica_initrd_usr_share_udhcpc="dir 0755 root root /opt/nkn/pacifica/initrd/usr/share/udhcpc"
SYS_INSTALL_FILE_elilo_efi="file elilo.efi 0744 root root /opt/nkn/pacifica/elilo.efi"
SYS_INSTALL_FILE_elilo_conf_template="file elilo.conf.template 0744 root root /opt/nkn/pacifica/elilo.conf.template"
SYS_INSTALL_FILE_initrd_img="file initrd.img 0744 root root /opt/nkn/pacifica/initrd.img"
SYS_INSTALL_FILE_juniper_banner_msg="file juniper-banner.msg 0744 root root /opt/nkn/pacifica/juniper-banner.msg"
SYS_INSTALL_FILE_rc_sysinit="file rc.sysinit 0744 root root /opt/nkn/pacifica/rc.sysinit"
SYS_INSTALL_FILE_dhclient_conf="file dhclient.conf 0744 root root /opt/nkn/pacifica/dhclient.conf"
SYS_INSTALL_FILE_dhclient_exit_hooks="file dhclient-exit-hooks 0744 root root /opt/nkn/pacifica/dhclient-exit-hooks"
SYS_INSTALL_FILE_irq_smpaffinity_bnx2x_sh="file irq-smpaffinity-bnx2x.sh 0744 root root /opt/nkn/bin/irq-smpaffinity-bnx2x.sh"

# Install initrd specific files
SYS_INSTALL_FILE_initrd_init="file initrd/init 0744 root root /opt/nkn/pacifica/initrd/init"
SYS_INSTALL_FILE_initrd_etc_pacifica_hw="file initrd/etc/pacifica_hw 0744 root root /opt/nkn/pacifica/initrd/etc/pacifica_hw"
SYS_INSTALL_FILE_initrd_udhcp_default_script="file initrd/usr/share/udhcpc/default.script 0744 root root /opt/nkn/pacifica/initrd/usr/share/udhcpc/default.script"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

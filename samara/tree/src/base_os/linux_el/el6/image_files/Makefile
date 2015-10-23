#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

# So we can reference FULL_INSTALL_DIR below in a := rule
INSTALL_TREE=image
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}
FULL_INSTALL_DIR_OVERRIDE=1

#
# See src/mk/files_adv.mk for comments explaining the different types
# of directives available.
#

ifeq (${PROD_TARGET_ARCH_FAMILY},PPC)
    $(error PowerPC is not supported on EL6)
endif

# XXX-64 This would be an issue if we supported both 32 and 64 bit PAM at the same time

ifeq (${PROD_TARGET_CPU_WORDSIZE},64)
LIB_SECURITY_DIR=/lib64/security
SYSSTAT_FILE=sysstat.64
else
LIB_SECURITY_DIR=/lib/security
SYSSTAT_FILE=sysstat
endif

ifeq (${PROD_TARGET_ARCH_FAMILY},PPC)
SYSSTAT_FILE=
endif

# For files with deviations from X86:
XARCH=
XARCH_DOT=
NOHWRNG=
ifeq (${PROD_TARGET_ARCH_FAMILY},PPC)
	XARCH=_${PROD_TARGET_ARCH_LC}
	XARCH_DOT=.${PROD_TARGET_ARCH_LC}

# XXX This is a bit simplistic: we assume that all PowerPC that do not
# have a HW RNG need the /dev/random to /dev/urandom symlink, and that
# no x86 or other platforms ever need this.
ifneq (${PROD_FEATURE_HW_RNG},1)
	NOHWRNG=-nohwrng
endif

endif # PPC


INSTALL_DIRECTIVES= \
	${SYS_INSTALL_DIR_slash} \
	${SYS_INSTALL_DIR_bin} \
	${SYS_INSTALL_DIR_boot} \
	${SYS_INSTALL_DIR_bootmgr} \
	${SYS_INSTALL_DIR_bootmgr_boot} \
	${SYS_INSTALL_DIR_bootmgr_boot_grub} \
	${SYS_INSTALL_DIR_bootmgr_data} \
	${SYS_INSTALL_DIR_config} \
	${SYS_INSTALL_DIR_config_db} \
	${SYS_INSTALL_DIR_config_mfg} \
	${SYS_INSTALL_DIR_data} \
	${SYS_INSTALL_DIR_etc} \
	${SYS_INSTALL_DIR_etc_fskel} \
	${SYS_INSTALL_DIR_etc_cron_d} \
	${SYS_INSTALL_DIR_etc_rc_d} \
	${SYS_INSTALL_DIR_etc_rc_d_init_d} \
	${SYS_INSTALL_DIR_etc_init} \
	${SYS_INSTALL_DIR_etc_ssh} \
	${SYS_INSTALL_DIR_etc_skel} \
	${SYS_INSTALL_DIR_etc_skel_root} \
	${SYS_INSTALL_DIR_etc_cron_confd} \
	${SYS_INSTALL_DIR_etc_sysconfig} \
	${SYS_INSTALL_DIR_etc_pki} \
	${SYS_INSTALL_DIR_etc_pki_tpkg} \
	${SYS_INSTALL_DIR_etc_pki_tpkg_private} \
	${SYS_INSTALL_DIR_etc_pki_tmsgpg} \
	${SYS_INSTALL_DIR_etc_profile_d} \
	${SYS_INSTALL_DIR_etc_ld_so_conf_d} \
        ${SYS_INSTALL_DIR_etc_logrotate_d} \
	${SYS_INSTALL_DIR_etc_modprobe_d} \
	${SYS_INSTALL_DIR_etc_udev} \
	${SYS_INSTALL_DIR_etc_udev_rules_d} \
	${SYS_INSTALL_DIR_etc_dhcp} \
	${SYS_INSTALL_DIR_dev} \
	${SYS_INSTALL_DIR_initrd} \
	${SYS_INSTALL_DIR_lib} \
	${SYS_INSTALL_DIR_lib_modules} \
	${SYS_INSTALL_DIR_lib_security} \
	${SYS_INSTALL_DIR_mnt} \
	${SYS_INSTALL_DIR_mnt_cdrom} \
	${SYS_INSTALL_DIR_opt} \
	${SYS_INSTALL_DIR_opt_tms} \
	${SYS_INSTALL_DIR_opt_tms_bin} \
	${SYS_INSTALL_DIR_opt_tms_lib} \
	${SYS_INSTALL_DIR_opt_tms_lib_cli} \
	${SYS_INSTALL_DIR_opt_tms_lib_cli_modules} \
	${SYS_INSTALL_DIR_opt_tms_lib_fonts} \
	${SYS_INSTALL_DIR_opt_tms_lib_md} \
	${SYS_INSTALL_DIR_opt_tms_lib_md_modules} \
	${SYS_INSTALL_DIR_opt_tms_lib_md_templates} \
	${SYS_INSTALL_DIR_opt_tms_lib_snmp} \
	${SYS_INSTALL_DIR_opt_tms_lib_snmp_modules} \
	${SYS_INSTALL_DIR_opt_tms_lib_web} \
	${SYS_INSTALL_DIR_opt_tms_lib_web_cgi_bin} \
	${SYS_INSTALL_DIR_opt_tms_lib_web_conf} \
	${SYS_INSTALL_DIR_opt_tms_lib_web_conf_d} \
	${SYS_INSTALL_DIR_opt_tms_lib_web_content} \
	${SYS_INSTALL_DIR_opt_tms_lib_web_content_images} \
	${SYS_INSTALL_DIR_opt_tms_lib_web_fonts} \
	${SYS_INSTALL_DIR_opt_tms_lib_web_handlers} \
	${SYS_INSTALL_DIR_opt_tms_lib_web_modules} \
	${SYS_INSTALL_DIR_opt_tms_lib_web_templates} \
	${SYS_INSTALL_DIR_opt_tms_release} \
	${SYS_INSTALL_DIR_pam_d} \
	${SYS_INSTALL_DIR_raddb} \
	${SYS_INSTALL_DIR_proc} \
	${SYS_INSTALL_DIR_sbin} \
	${SYS_INSTALL_DIR_sys} \
	${SYS_INSTALL_DIR_usr} \
	${SYS_INSTALL_DIR_usr_bin} \
	${SYS_INSTALL_DIR_usr_lib} \
	${SYS_INSTALL_DIR_usr_local_bin} \
	${SYS_INSTALL_DIR_usr_local} \
	${SYS_INSTALL_DIR_usr_sbin} \
	${SYS_INSTALL_DIR_var_cache} \
	${SYS_INSTALL_DIR_var_cache_libvirt} \
	${SYS_INSTALL_DIR_var_empty} \
	${SYS_INSTALL_DIR_var_empty_sshd} \
	${SYS_INSTALL_DIR_var_home} \
	${SYS_INSTALL_DIR_var_home_monitor} \
	${SYS_INSTALL_DIR_var_home_root} \
	${SYS_INSTALL_DIR_var_lib} \
	${SYS_INSTALL_DIR_var_lib_ssh} \
	${SYS_INSTALL_DIR_var_lib_misc} \
	${SYS_INSTALL_DIR_var_lock_subsys} \
	${SYS_INSTALL_DIR_var_lock} \
	${SYS_INSTALL_DIR_var_log} \
	${SYS_INSTALL_DIR_var_log_bv} \
	${SYS_INSTALL_DIR_var_opt} \
	${SYS_INSTALL_DIR_var_root} \
	${SYS_INSTALL_DIR_var_racoon} \
	${SYS_INSTALL_DIR_var_root_tmp} \
	${SYS_INSTALL_DIR_var_opt_tms} \
	${SYS_INSTALL_DIR_var_opt_tms_cmc} \
	${SYS_INSTALL_DIR_var_opt_tms_cmc_ssh} \
	${SYS_INSTALL_DIR_var_opt_tms_cmc_ssh_keys} \
	${SYS_INSTALL_DIR_var_opt_tms_images} \
	${SYS_INSTALL_DIR_var_opt_tms_images_temp} \
	${SYS_INSTALL_DIR_var_opt_tms_output} \
	${SYS_INSTALL_DIR_var_opt_tms_output_cron_d} \
	${SYS_INSTALL_DIR_var_opt_tms_output_web} \
	${SYS_INSTALL_DIR_var_opt_tms_output_web_conf} \
	${SYS_INSTALL_DIR_var_opt_tms_output_libvirt} \
	${SYS_INSTALL_DIR_var_opt_tms_output_libvirt_qemu} \
	${SYS_INSTALL_DIR_var_opt_tms_output_libvirt_qemu_networks} \
	${SYS_INSTALL_DIR_var_opt_tms_output_libvirt_qemu_networks_autostart} \
	${SYS_INSTALL_DIR_var_opt_tms_output_libvirt_qemu_autostart} \
	${SYS_INSTALL_DIR_var_opt_tms_sched} \
	${SYS_INSTALL_DIR_var_opt_tms_snapshots} \
	${SYS_INSTALL_DIR_var_opt_tms_stats} \
	${SYS_INSTALL_DIR_var_opt_tms_stats_reports} \
	${SYS_INSTALL_DIR_var_opt_tms_sysdumps} \
	${SYS_INSTALL_DIR_var_opt_tms_tcpdumps} \
	${SYS_INSTALL_DIR_var_opt_tms_text_configs} \
	${SYS_INSTALL_DIR_var_opt_tms_web} \
	${SYS_INSTALL_DIR_var_opt_tms_web_conf} \
	${SYS_INSTALL_DIR_var_opt_tms_web_graphs} \
	${SYS_INSTALL_DIR_var_opt_tms_output_certs} \
        ${SYS_INSTALL_DIR_var_opt_tms_output_certs_ca} \
	${SYS_INSTALL_DIR_var_opt_tms_output_pluto} \
	${SYS_INSTALL_DIR_var_opt_tms_output_pluto_ipsec_d} \
	${SYS_INSTALL_DIR_var_opt_tms_output_pluto_ipsec_d_aacerts} \
	${SYS_INSTALL_DIR_var_opt_tms_output_pluto_ipsec_d_cacerts} \
	${SYS_INSTALL_DIR_var_opt_tms_output_pluto_ipsec_d_crls} \
	${SYS_INSTALL_DIR_var_opt_tms_output_pluto_ipsec_d_ocspcerts} \
	${SYS_INSTALL_DIR_var_opt_tms_output_racoon} \
	${SYS_INSTALL_DIR_var_opt_tms_output_racoon_certs} \
	${SYS_INSTALL_DIR_var} \
	${SYS_INSTALL_DIR_var_run} \
	${SYS_INSTALL_DIR_var_spool} \
	${SYS_INSTALL_DIR_var_spool_cron} \
	${SYS_INSTALL_DIR_var_tmp} \
	${SYS_INSTALL_DIR_var_stmp} \
	${SYS_INSTALL_DIR_var_opt_tms_web_sysimages} \
	${SYS_INSTALL_DIR_vtmp} \
	${SYS_INSTALL_DIR_var_opt_tms_virt} \
	${SYS_INSTALL_DIR_var_opt_tms_virt_pools} \
	${SYS_INSTALL_DIR_var_opt_tms_virt_vm_save} \
	${SYS_INSTALL_DIR_var_opt_tms_virt_vm_save_temp} \
	${SYS_INSTALL_FILE_hosts} \
	${SYS_INSTALL_FILE_inittab${XARCH}} \
	${SYS_INSTALL_FILE_adjtime} \
	${SYS_INSTALL_FILE_syslog} \
	${SYS_INSTALL_FILE_fstab} \
	${SYS_INSTALL_FILE_rngd} \
	${SYS_INSTALL_FILE_smartd} \
	\
	${SYS_INSTALL_FILE_internal_startup} \
	${SYS_INSTALL_FILE_mdinit} \
	${SYS_INSTALL_FILE_pm} \
	${SYS_INSTALL_FILE_rename_ifs} \
	${SYS_INSTALL_FILE_tun} \
	${SYS_INSTALL_FILE_shutdown_check} \
	\
	${SYS_INSTALL_FILE_sysctl_conf} \
	${SYS_INSTALL_FILE_profile} \
	${SYS_INSTALL_FILE_login} \
	${SYS_INSTALL_FILE_nsswitch_conf} \
	${SYS_INSTALL_FILE_csh_cshrc} \
	${SYS_INSTALL_FILE_passwd} \
	${SYS_INSTALL_FILE_host_conf} \
	${SYS_INSTALL_FILE_group} \
	${SYS_INSTALL_FILE_securetty} \
	${SYS_INSTALL_FILE_shells} \
	${SYS_INSTALL_FILE_syslog_conf} \
	${SYS_INSTALL_FILE_ldso_conf} \
	${SYS_INSTALL_FILE_grub_conf_template} \
	${SYS_INSTALL_FILE_grub_conf_template_new} \
	${SYS_INSTALL_FILE_grub_conf} \
	${SYS_INSTALL_FILE_smartd_conf} \
	${SYS_INSTALL_FILE_smartd_conf_template} \
	${SYS_INSTALL_FILE_sshd_config} \
	${SYS_INSTALL_FILE_ssh_config} \
	${SYS_INSTALL_FILE_system_auth} \
	${SYS_INSTALL_FILE_wsmd_auth} \
	${SYS_INSTALL_FILE_sshd_auth} \
	${SYS_INSTALL_FILE_login_auth${XARCH}} \
	${SYS_INSTALL_FILE_remote_auth${XARCH}} \
	${SYS_INSTALL_FILE_bashrc} \
	${SYS_INSTALL_FILE_moduli} \
	${SYS_INSTALL_FILE_etc_profile_d_limits_sh} \
	${SYS_INSTALL_FILE_etc_profile_d_limits_csh} \
	${SYS_INSTALL_FILE_etc_profile_d_less_opts_sh} \
	${SYS_INSTALL_FILE_etc_profile_d_less_opts_csh} \
	${SYS_INSTALL_FILE_var_var_version_sh} \
	${SYS_INSTALL_FILE_etc_var_version_sh} \
	${SYS_INSTALL_FILE_rootfiles_bash_logout} \
	${SYS_INSTALL_FILE_rootfiles_bash_profile} \
	${SYS_INSTALL_FILE_rootfiles_bashrc} \
	${SYS_INSTALL_FILE_rootfiles_cshrc} \
	${SYS_INSTALL_FILE_rootfiles_gdbinit} \
	${SYS_INSTALL_FILE_rootfiles_tcshrc} \
	${SYS_INSTALL_FILE_monfiles_bash_logout} \
	${SYS_INSTALL_FILE_monfiles_bash_profile} \
	${SYS_INSTALL_FILE_monfiles_bashrc} \
	${SYS_INSTALL_FILE_etc_skel_bash_logout} \
	${SYS_INSTALL_FILE_etc_skel_bash_profile} \
	${SYS_INSTALL_FILE_etc_skel_bashrc} \
	${SYS_INSTALL_FILE_skel_root_bash_logout} \
	${SYS_INSTALL_FILE_skel_root_bash_profile} \
	${SYS_INSTALL_FILE_skel_root_bashrc} \
	${SYS_INSTALL_FILE_skel_root_cshrc} \
	${SYS_INSTALL_FILE_skel_root_gdbinit} \
	${SYS_INSTALL_FILE_skel_root_tcshrc} \
	${SYS_INSTALL_FILE_etc_gdbinit} \
	${SYS_INSTALL_FILE_etc_ipv6_disable_conf} \
	${SYS_INSTALL_FILE_udev_20_base_rules} \
	${SYS_INSTALL_FILE_udev_70_base_rules} \
	${SYS_TOUCH_FILE_dot_firstboot} \
	${SYS_TOUCH_FILE_etc_mtab} \
	${SYS_TOUCH_FILE_lastlog} \
	${SYS_TOUCH_FILE_systemlog} \
	${SYS_TOUCH_FILE_messages} \
	${SYS_TOUCH_FILE_web_access_log} \
	${SYS_TOUCH_FILE_web_error_log} \
	${SYS_TOUCH_FILE_utmp} \
	${SYS_TOUCH_FILE_wtmp} \
	${SYS_TOUCH_FILE_tallynamelog} \
	${SYS_TOUCH_FILE_etc_sysconfig_udev} \
	${SYS_CREATE_SYMLINK_tmp} \
	${SYS_CREATE_SYMLINK_etc_cron_d} \
	${SYS_CREATE_SYMLINK_hosts} \
	${SYS_CREATE_SYMLINK_resolv_conf} \
	${SYS_CREATE_SYMLINK_etc_clirc} \
	${SYS_CREATE_SYMLINK_etc_snmpd_conf} \
	${SYS_CREATE_SYMLINK_usr_bin_snmpinform} \
	${SYS_CREATE_SYMLINK_ntp_conf} \
	${SYS_CREATE_SYMLINK_localtime} \
	${SYS_CREATE_SYMLINK_passwd} \
	${SYS_CREATE_SYMLINK_group} \
	${SYS_CREATE_SYMLINK_smartd_conf} \
	${SYS_CREATE_SYMLINK_shadow} \
	${SYS_CREATE_SYMLINK_sshd_config} \
	${SYS_CREATE_SYMLINK_ssh_config} \
	${SYS_CREATE_SYMLINK_ssh_known_hosts} \
	${SYS_CREATE_SYMLINK_motd} \
	${SYS_CREATE_SYMLINK_issue} \
	${SYS_CREATE_SYMLINK_issue_net} \
	${SYS_CREATE_SYMLINK_logrotate_conf} \
	${SYS_CREATE_SYMLINK_logrotate_crontab} \
	${SYS_CREATE_SYMLINK_vpartsync_crontab} \
	${SYS_CREATE_SYMLINK_syslog} \
	${SYS_CREATE_SYMLINK_syslog_conf} \
	${SYS_CREATE_SYMLINK_ssmtp_conf} \
	${SYS_CREATE_SYMLINK_revaliases_conf} \
	${SYS_CREATE_SYMLINK_ssmtp_auto_conf} \
	${SYS_CREATE_SYMLINK_sendmail_usrsbin} \
	${SYS_CREATE_SYMLINK_sendmail_usrlib} \
	${SYS_CREATE_SYMLINK_fscktmpfs_usrsbin} \
	${SYS_CREATE_SYMLINK_pam_radius_auth} \
	${SYS_CREATE_SYMLINK_pam_radius_auth_conf} \
	${SYS_CREATE_SYMLINK_pam_tacplus} \
	${SYS_CREATE_SYMLINK_pam_unix} \
	${SYS_CREATE_SYMLINK_pam_disabled} \
	${SYS_CREATE_SYMLINK_pam_ldap} \
	${SYS_CREATE_SYMLINK_pam_tallybyname} \
	${SYS_CREATE_SYMLINK_system_auth} \
	${SYS_CREATE_SYMLINK_ssh_rsa1_priv} \
	${SYS_CREATE_SYMLINK_ssh_rsa1_pub} \
	${SYS_CREATE_SYMLINK_ssh_rsa2_priv} \
	${SYS_CREATE_SYMLINK_ssh_rsa2_pub} \
	${SYS_CREATE_SYMLINK_ssh_dsa2_priv} \
	${SYS_CREATE_SYMLINK_ssh_dsa2_pub} \
	${SYS_CREATE_SYMLINK_web_graphs} \
	${SYS_CREATE_SYMLINK_web_sysimages} \
	${SYS_CREATE_SYMLINK_root_home} \
	${SYS_CREATE_SYMLINK_pluto_ipsec_d} \
	${SYS_CREATE_SYMLINK_pluto_ipsec_conf} \
	${SYS_CREATE_SYMLINK_racoon} \
	${SYS_CREATE_SYMLINK_config_text} \
	${SYS_CREATE_SYMLINK_busybox} \


# Per-platform ld.so config
ifeq (${PROD_TARGET_ARCH_FAMILY},X86)
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_ldso_tms_i386_conf} \
	${SYS_INSTALL_FILE_ldso_tms_x86_64_conf} \

else ifeq (${PROD_TARGET_ARCH_FAMILY},PPC)

# We use the i386 one, as it is just the default
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_ldso_tms_i386_conf} \

else
# We use the i386 one, as it is just the default
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_ldso_tms_i386_conf} \

endif

ifneq ($(strip ${SYSSTAT_FILE}),)
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_etc_cron_confd_sysstat} \
	${SYS_CREATE_SYMLINK_sysstat_crontab} \

endif

ifeq (${PROD_FEATURE_FTPD},1)
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_DIR_etc_vsftpd} \
	${SYS_INSTALL_FILE_vsftpd_auth} \
	${SYS_INSTALL_FILE_etc_vsftpd_vsftpd_conf} \
	${SYS_INSTALL_FILE_etc_vsftpd_ftpusers} \
	${SYS_INSTALL_FILE_etc_vsftpd_user_list} \

endif

ifeq (${PROD_FEATURE_JAVA},1)
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_etc_profile_d_java_sh} \
	${SYS_INSTALL_FILE_etc_profile_d_java_csh} \

endif

ifeq (${PROD_FEATURE_XML_GW},1)
INSTALL_DIRECTIVES+=\
	${SYS_CREATE_SYMLINK_xg} \

endif

ifeq (${PROD_FEATURE_REMOTE_GCL},1)
INSTALL_DIRECTIVES+=\
	${SYS_CREATE_SYMLINK_rgp_raw} \

endif

ifeq (${PROD_FEATURE_VIRT},1)
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_TAR_iptables_tar} \
	${SYS_INSTALL_FILE_iptables} \
	${SYS_INSTALL_FILE_ip6tables} \
	${SYS_CREATE_SYMLINK_etc_libvirt} \

endif

ifeq (${PROD_FEATURE_VIRT_KVM},1)
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_kvm} \

endif # VIRT_KVM

ifeq (${PROD_FEATURE_DHCP_CLIENT},1)
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_dhclient_conf} \

	ifeq (${PROD_FEATURE_IPV6},1)
	INSTALL_DIRECTIVES+=\
		${SYS_INSTALL_FILE_dhclient6_conf} \

	endif
endif

ifeq (${PROD_FEATURE_EETOOL},1)
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_ee_conf_template} \

endif

ifeq (${PROD_FEATURE_NVS},1)
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_nvs_conf_template} \

endif

ifeq (${PROD_TARGET_ARCH_FAMILY},PPC)
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_DIR_bootmgr_uboot} \

endif

ifneq (${PROD_FEATURE_DEV_ENV},1)
INSTALL_DIRECTIVES+=\
	${SYS_CREATE_SPECIAL_dev_null} \
	${SYS_CREATE_SPECIAL_dev_zero} \
	${SYS_CREATE_SPECIAL_dev_console} \
	${SYS_CREATE_SPECIAL_dev_ttyS0} \


ifeq (${PROD_TARGET_ARCH_FAMILY},X86)
INSTALL_DIRECTIVES+=\
	${SYS_CREATE_SPECIAL_dev_tty1} \
	${SYS_CREATE_SPECIAL_dev_tty2} \
	${SYS_CREATE_SPECIAL_dev_tty3} \
	${SYS_CREATE_SPECIAL_dev_tty4} \
	${SYS_CREATE_SPECIAL_dev_tty5} \
	${SYS_CREATE_SPECIAL_dev_tty6} \
	${SYS_CREATE_SPECIAL_dev_tty7} \
	${SYS_CREATE_SPECIAL_dev_tty8} \

endif

endif

ifneq (${PROD_FEATURE_IPV6},1)
# This is in case PROD_FEATURE_IPV6=0, but the kernel has IPv6 enabled
# as a module
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_modprobe_ipv6_disable_conf} \

endif

ifneq ($(strip ${VIRT_DIR}),)
	ifneq ($(strip ${VIRT_DIR}),/var/opt/tms/virt)
		INSTALL_DIRECTIVES+=${SYS_CREATE_SYMLINK_virt}
	endif
else
	VIRT_DIR=/var/opt/tms/virt
endif

# First, all the directories we need

SYS_INSTALL_DIR_slash="dir 0755 root root /"
SYS_INSTALL_DIR_bin="dir 0755 root root /bin"
SYS_INSTALL_DIR_boot="dir 0755 root root /boot"
SYS_INSTALL_DIR_bootmgr="dir 0755 root root /bootmgr"
SYS_INSTALL_DIR_bootmgr_boot="dir 0755 root root /bootmgr/boot"
SYS_INSTALL_DIR_bootmgr_boot_grub="dir 0755 root root /bootmgr/boot/grub"
SYS_INSTALL_DIR_bootmgr_uboot="dir 0755 root root /bootmgr/u-boot"
SYS_INSTALL_DIR_bootmgr_data="dir 0755 root root /bootmgr/data"
SYS_INSTALL_DIR_config="dir 0755 root root /config"
SYS_INSTALL_DIR_config_db="dir 0755 root root /config/db"
SYS_INSTALL_DIR_config_mfg="dir 0755 root root /config/mfg"
SYS_INSTALL_DIR_data="dir 0755 root root /data"
SYS_INSTALL_DIR_etc="dir 0755 root root /etc"
SYS_INSTALL_DIR_etc_fskel="dir 0755 root root /etc/fskel"
SYS_INSTALL_DIR_etc_rc_d="dir 0755 root root /etc/rc.d"
SYS_INSTALL_DIR_etc_rc_d_init_d="dir 0755 root root /etc/rc.d/init.d"
SYS_INSTALL_DIR_etc_init="dir 0755 root root /etc/init"
SYS_INSTALL_DIR_etc_ssh="dir 0755 root root /etc/ssh"
SYS_INSTALL_DIR_etc_skel="dir 0755 root root /etc/skel"
SYS_INSTALL_DIR_etc_skel_root="dir 0755 root root /etc/skel-root"
SYS_INSTALL_DIR_etc_vsftpd="dir 0755 root root /etc/vsftpd"
SYS_INSTALL_DIR_etc_cron_confd="dir 0755 root root /etc/cron_conf.d"
SYS_INSTALL_DIR_etc_sysconfig="dir 0755 root root /etc/sysconfig"
SYS_INSTALL_DIR_etc_pki="dir 0755 root root /etc/pki"
SYS_INSTALL_DIR_etc_pki_tpkg="dir 0755 root root /etc/pki/tpkg"
SYS_INSTALL_DIR_etc_pki_tpkg_private="dir 0700 root root /etc/pki/tpkg/private"
SYS_INSTALL_DIR_etc_pki_tmsgpg="dir 0755 root root /etc/pki/tms-gpg"
SYS_INSTALL_DIR_etc_profile_d="dir 0755 root root /etc/profile.d"
SYS_INSTALL_DIR_etc_ld_so_conf_d="dir 0755 root root /etc/ld.so.conf.d"
SYS_INSTALL_DIR_etc_logrotate_d="dir 0755 root root /etc/logrotate.d"
SYS_INSTALL_DIR_etc_modprobe_d="dir 0755 root root /etc/modprobe.d"
SYS_INSTALL_DIR_etc_udev="dir 0755 root root /etc/udev"
SYS_INSTALL_DIR_etc_udev_rules_d="dir 0755 root root /etc/udev/rules.d"
SYS_INSTALL_DIR_etc_dhcp="dir 0755 root root /etc/dhcp"
SYS_INSTALL_DIR_dev="dir 0755 root root /dev"
SYS_INSTALL_DIR_initrd="dir 0755 root root /initrd"
SYS_INSTALL_DIR_lib="dir 0755 root root /lib"
SYS_INSTALL_DIR_lib_modules="dir 0755 root root /lib/modules"
SYS_INSTALL_DIR_lib_security="dir 0755 root root ${LIB_SECURITY_DIR}"
SYS_INSTALL_DIR_mnt="dir 0755 root root /mnt"
SYS_INSTALL_DIR_mnt_cdrom="dir 0755 root root /mnt/cdrom"
SYS_INSTALL_DIR_opt="dir 0755 root root /opt"
SYS_INSTALL_DIR_opt_tms="dir 0755 root root /opt/tms"
SYS_INSTALL_DIR_opt_tms_bin="dir 0755 root root /opt/tms/bin"
SYS_INSTALL_DIR_opt_tms_lib="dir 0755 root root /opt/tms/lib"
SYS_INSTALL_DIR_opt_tms_lib_cli="dir 0755 root root /opt/tms/lib/cli"
SYS_INSTALL_DIR_opt_tms_lib_cli_modules="dir 0755 root root /opt/tms/lib/cli/modules"
SYS_INSTALL_DIR_opt_tms_lib_fonts="dir 0755 root root /opt/tms/lib/fonts"
SYS_INSTALL_DIR_opt_tms_lib_md="dir 0755 root root /opt/tms/lib/md"
SYS_INSTALL_DIR_opt_tms_lib_md_modules="dir 0755 root root /opt/tms/lib/md/modules"
SYS_INSTALL_DIR_opt_tms_lib_md_templates="dir 0755 root root /opt/tms/lib/md/templates"
SYS_INSTALL_DIR_opt_tms_lib_snmp="dir 0755 root root /opt/tms/lib/snmp"
SYS_INSTALL_DIR_opt_tms_lib_snmp_modules="dir 0755 root root /opt/tms/lib/snmp/modules"
SYS_INSTALL_DIR_opt_tms_lib_web="dir 0755 root root /opt/tms/lib/web"
SYS_INSTALL_DIR_opt_tms_lib_web_cgi_bin="dir 0755 root root /opt/tms/lib/web/cgi-bin"
SYS_INSTALL_DIR_opt_tms_lib_web_conf="dir 0755 root root /opt/tms/lib/web/conf"
SYS_INSTALL_DIR_opt_tms_lib_web_conf_d="dir 0755 root root /opt/tms/lib/web/conf.d"
SYS_INSTALL_DIR_opt_tms_lib_web_content="dir 0755 root root /opt/tms/lib/web/content"
SYS_INSTALL_DIR_opt_tms_lib_web_content_images="dir 0755 root root /opt/tms/lib/web/content/images"
SYS_INSTALL_DIR_opt_tms_lib_web_fonts="dir 0755 root root /opt/tms/lib/web/fonts"
SYS_INSTALL_DIR_opt_tms_lib_web_handlers="dir 0755 root root /opt/tms/lib/web/handlers"
SYS_INSTALL_DIR_opt_tms_lib_web_modules="dir 0755 root root /opt/tms/lib/web/modules"
SYS_INSTALL_DIR_opt_tms_lib_web_templates="dir 0755 root root /opt/tms/lib/web/templates"
SYS_INSTALL_DIR_opt_tms_release="dir 0755 root root /opt/tms/release"
SYS_INSTALL_DIR_pam_d="dir 0755 root root /etc/pam.d"
SYS_INSTALL_DIR_raddb="dir 0700 root root /etc/raddb"
SYS_INSTALL_DIR_proc="dir 0755 root root /proc"
SYS_INSTALL_DIR_sys="dir 0755 root root /sys"
SYS_INSTALL_DIR_sbin="dir 0755 root root /sbin"
SYS_INSTALL_DIR_usr="dir 0755 root root /usr"
SYS_INSTALL_DIR_usr_bin="dir 0755 root root /usr/bin"
SYS_INSTALL_DIR_usr_lib="dir 0755 root root /usr/lib"
SYS_INSTALL_DIR_usr_local="dir 0755 root root /usr/local"
SYS_INSTALL_DIR_usr_local_bin="dir 0755 root root /usr/local/bin"
SYS_INSTALL_DIR_usr_sbin="dir 0755 root root /usr/sbin"
SYS_INSTALL_DIR_var="dir 0755 root root /var"
SYS_INSTALL_DIR_var_cache="dir 0755 root root /var/cache"
SYS_INSTALL_DIR_var_cache_libvirt="dir 0700 root root /var/cache/libvirt"
SYS_INSTALL_DIR_var_empty="dir 0755 root root /var/empty"
SYS_INSTALL_DIR_var_empty_sshd="dir 0111 root root /var/empty/sshd"
SYS_INSTALL_DIR_var_home="dir 0755 root root /var/home"
SYS_INSTALL_DIR_var_home_monitor="dir 0755 1001 1001 /var/home/monitor"
SYS_INSTALL_DIR_var_home_root="dir 0755 root root /var/home/root"
SYS_INSTALL_DIR_var_lib="dir 0755 root root /var/lib"
SYS_INSTALL_DIR_var_lib_ssh="dir 0755 root root /var/lib/ssh"
SYS_INSTALL_DIR_var_lib_misc="dir 0755 root root /var/lib/misc"
SYS_INSTALL_DIR_var_lock="dir 0755 root root /var/lock"
SYS_INSTALL_DIR_var_lock_subsys="dir 0755 root root /var/lock/subsys"
SYS_INSTALL_DIR_var_log="dir 0755 root root /var/log"
SYS_INSTALL_DIR_var_log_bv="dir 0700 root root /var/log/build_versions"
SYS_INSTALL_DIR_var_opt="dir 0755 root root /var/opt"
SYS_INSTALL_DIR_var_root="dir 0755 root root /var/root"
SYS_INSTALL_DIR_var_racoon="dir 0755 root root /var/racoon"
SYS_INSTALL_DIR_var_root_tmp="dir 1777 root root /var/root/tmp"
SYS_INSTALL_DIR_var_opt_tms="dir 0755 root root /var/opt/tms"
SYS_INSTALL_DIR_var_opt_tms_cmc="dir 0755 root root /var/opt/tms/cmc"
SYS_INSTALL_DIR_var_opt_tms_cmc_ssh="dir 0700 root root /var/opt/tms/cmc/ssh"
SYS_INSTALL_DIR_var_opt_tms_cmc_ssh_keys="dir 0700 root root /var/opt/tms/cmc/ssh/keys"
SYS_INSTALL_DIR_var_opt_tms_output="dir 0755 root root /var/opt/tms/output"
SYS_INSTALL_DIR_var_opt_tms_output_cron_d="dir 0755 root root /var/opt/tms/output/cron.d"
SYS_INSTALL_DIR_var_opt_tms_output_web="dir 0755 root root /var/opt/tms/output/web"
SYS_INSTALL_DIR_var_opt_tms_output_web_conf="dir 0755 root root /var/opt/tms/output/web/conf"
SYS_INSTALL_DIR_var_opt_tms_output_libvirt="dir 0700 root root /var/opt/tms/output/libvirt"
SYS_INSTALL_DIR_var_opt_tms_output_libvirt_qemu="dir 0700 root root /var/opt/tms/output/libvirt/qemu"
SYS_INSTALL_DIR_var_opt_tms_output_libvirt_qemu_networks="dir 0700 root root /var/opt/tms/output/libvirt/qemu/networks"
SYS_INSTALL_DIR_var_opt_tms_output_libvirt_qemu_networks_autostart="dir 0700 root root /var/opt/tms/output/libvirt/qemu/networks/autostart"
SYS_INSTALL_DIR_var_opt_tms_output_libvirt_qemu_autostart="dir 0700 root root /var/opt/tms/output/libvirt/qemu/autostart"

#
# 0500 is md_image_dir_mode_serve_all_disabled; see comment above its
# definition in tpaths.h.  48 is apache; 3000 is mgt_diag from md_client.h.
# The temp dir doesn't need any of this, as we never serve out of it.
#
SYS_INSTALL_DIR_var_opt_tms_images="dir 0500 48 3000 /var/opt/tms/images"
SYS_INSTALL_DIR_var_opt_tms_images_temp="dir 0700 0 0 /var/opt/tms/images/.temp"

SYS_INSTALL_DIR_var_opt_tms_sched="dir 0755 root root /var/opt/tms/sched"

# We don't particularly care for "other" users to be snooping around in here,
# but processes which don't run as root (like the CLI and RGP) need to be able
# to get through here on the way to their working directories.
SYS_INSTALL_DIR_var_opt_tms_snapshots="dir 0775 root root /var/opt/tms/snapshots"

SYS_INSTALL_DIR_var_opt_tms_stats="dir 0750 root 3001 /var/opt/tms/stats"
SYS_INSTALL_DIR_var_opt_tms_stats_reports="dir 0750 root 3001 /var/opt/tms/stats/reports"
SYS_INSTALL_DIR_var_opt_tms_sysdumps="dir 0770 root 3000 /var/opt/tms/sysdumps"
SYS_INSTALL_DIR_var_opt_tms_tcpdumps="dir 0770 root 3000 /var/opt/tms/tcpdumps"
SYS_INSTALL_DIR_var_opt_tms_text_configs="dir 0700 root root /var/opt/tms/text_configs"
SYS_INSTALL_DIR_var_opt_tms_web="dir 0755 root root /var/opt/tms/web"
SYS_INSTALL_DIR_var_opt_tms_web_conf="dir 0755 root root /var/opt/tms/web/conf"
SYS_INSTALL_DIR_var_opt_tms_web_graphs="dir 1777 root root /var/opt/tms/web/graphs"
SYS_INSTALL_DIR_var_opt_tms_output_certs="dir 0750 root root /var/opt/tms/output/certs"
SYS_INSTALL_DIR_var_opt_tms_output_certs_ca="dir 0750 root root /var/opt/tms/output/cacerts"
SYS_INSTALL_DIR_var_opt_tms_output_pluto="dir 0700 root root /var/opt/tms/output/pluto"
SYS_INSTALL_DIR_var_opt_tms_output_pluto_ipsec_d="dir 0700 root root /var/opt/tms/output/pluto/ipsec.d"
SYS_INSTALL_DIR_var_opt_tms_output_pluto_ipsec_d_aacerts="dir 0700 root root /var/opt/tms/output/pluto/ipsec.d/aacerts"
SYS_INSTALL_DIR_var_opt_tms_output_pluto_ipsec_d_cacerts="dir 0700 root root /var/opt/tms/output/pluto/ipsec.d/cacerts"
SYS_INSTALL_DIR_var_opt_tms_output_pluto_ipsec_d_crls="dir 0700 root root /var/opt/tms/output/pluto/ipsec.d/crls"
SYS_INSTALL_DIR_var_opt_tms_output_pluto_ipsec_d_ocspcerts="dir 0700 root root /var/opt/tms/output/pluto/ipsec.d/ocspcerts"
SYS_INSTALL_DIR_var_opt_tms_output_racoon="dir 0755 root root /var/opt/tms/output/racoon"
SYS_INSTALL_DIR_var_opt_tms_output_racoon_certs="dir 0700 root root /var/opt/tms/output/racoon/certs"
SYS_INSTALL_DIR_var_run="dir 0755 root root /var/run"
SYS_INSTALL_DIR_var_spool="dir 0755 root root /var/spool"
SYS_INSTALL_DIR_var_spool_cron="dir 0755 root root /var/spool/cron"
SYS_INSTALL_DIR_var_tmp="dir 1777 root root /var/tmp"
SYS_INSTALL_DIR_var_stmp="dir 0700 root root /var/stmp"
SYS_INSTALL_DIR_var_opt_tms_web_sysimages="dir 0755 root root /var/opt/tms/web/sysimages"
SYS_INSTALL_DIR_vtmp="dir 1777 root root /vtmp"
SYS_INSTALL_DIR_var_opt_tms_virt="dir 0770 root 3002 ${VIRT_DIR}"
SYS_INSTALL_DIR_var_opt_tms_virt_pools="dir 0770 root 3002 ${VIRT_DIR}/pools"
SYS_INSTALL_DIR_var_opt_tms_virt_vm_save="dir 0770 root 3002 ${VIRT_DIR}/vm_save"
SYS_INSTALL_DIR_var_opt_tms_virt_vm_save_temp="dir 0770 root 3002 ${VIRT_DIR}/vm_save/.temp"
SYS_CREATE_SYMLINK_virt="symlink /var/opt/tms ${VIRT_DIR} virt"


# All the files.  Some of these overwrite stuff coming from the base os

# This is needed so that localhost can be looked up before we first write it
SYS_INSTALL_FILE_hosts="file hosts 0644 root root /var/opt/tms/output/hosts"
SYS_INSTALL_FILE_inittab="file inittab 0644 root root /etc/inittab"
SYS_INSTALL_FILE_inittab_ppc="file inittab.ppc 0644 root root /etc/inittab"
SYS_INSTALL_FILE_syslog="file syslog 0755 root root /etc/rc.d/init.d/syslog"
SYS_INSTALL_FILE_fstab="file fstab 0644 root root /etc/fstab"
SYS_INSTALL_FILE_rngd="file rngd 0755 root root /etc/rc.d/init.d/rngd"
SYS_INSTALL_FILE_smartd="file smartd 0755 root root /etc/rc.d/init.d/smartd"

SYS_INSTALL_FILE_adjtime="file adjtime 0644 root root /etc/adjtime"

SYS_INSTALL_FILE_dhclient_conf="file dhclient.conf 0644 root root /etc/dhcp/dhclient.conf"
SYS_INSTALL_FILE_dhclient6_conf="file dhclient6.conf 0644 root root /etc/dhcp/dhclient6.conf"

# TMS startup scripts
SYS_INSTALL_FILE_internal_startup="file internal_startup 0755 root root /etc/rc.d/init.d/internal_startup"
SYS_INSTALL_FILE_mdinit="file mdinit 0755 root root /etc/rc.d/init.d/mdinit"
SYS_INSTALL_FILE_pm="file pm 0755 root root /etc/rc.d/init.d/pm"
SYS_INSTALL_FILE_rename_ifs="file rename_ifs 0755 root root /etc/rc.d/init.d/rename_ifs"
SYS_INSTALL_FILE_tun="file tun 0755 root root /etc/rc.d/init.d/tun"
SYS_INSTALL_TAR_iptables_tar="tar iptables-1.4.12.2.tar /"
SYS_INSTALL_FILE_iptables="file iptables 0755 root root /etc/rc.d/init.d/iptables"
SYS_INSTALL_FILE_ip6tables="file ip6tables 0755 root root /etc/rc.d/init.d/ip6tables"
SYS_INSTALL_FILE_shutdown_check="file shutdown_check 0755 root root /etc/rc.d/init.d/shutdown_check"

SYS_INSTALL_FILE_sysctl_conf="file sysctl.conf 0644 root root /etc/sysctl.conf"
SYS_INSTALL_FILE_profile="file profile 0644 root root /etc/profile"
SYS_INSTALL_FILE_login="file csh.login 0644 root root /etc/csh.login"

# NOTE: on EL6, termcap is not supported, only terminfo
## SYS_INSTALL_FILE_termcap="file termcap 0644 root root /etc/termcap"

SYS_INSTALL_FILE_nsswitch_conf="file nsswitch.conf 0644 root root \
	/etc/nsswitch.conf"
SYS_INSTALL_FILE_csh_cshrc="file csh.cshrc 0644 root root /etc/csh.cshrc"
SYS_INSTALL_FILE_passwd="file passwd 0644 root root /var/opt/tms/output/passwd"
SYS_INSTALL_FILE_host_conf="file host.conf 0644 root root /etc/host.conf"
SYS_INSTALL_FILE_group="file group 0644 root root /var/opt/tms/output/group"
SYS_INSTALL_FILE_var_var_version_sh="file var_version.sh 0644 root root /var/var_version.sh"
SYS_INSTALL_FILE_etc_var_version_sh="file var_version.sh 0644 root root /etc/var_version.sh"
SYS_INSTALL_FILE_etc_cron_confd_sysstat="file ${SYSSTAT_FILE} 0644 root root /etc/cron_conf.d/sysstat"
SYS_INSTALL_FILE_rootfiles_bash_logout="file dot_bash_logout 0644 root root /var/home/root/.bash_logout"
SYS_INSTALL_FILE_rootfiles_bash_profile="file admin_dot_bash_profile 0644 root root /var/home/root/.bash_profile"
SYS_INSTALL_FILE_rootfiles_bashrc="file dot_bashrc 0644 root root /var/home/root/.bashrc"
SYS_INSTALL_FILE_rootfiles_cshrc="file admin_dot_cshrc 0644 root root /var/home/root/.cshrc"
SYS_INSTALL_FILE_rootfiles_gdbinit="file admin_dot_gdbinit 0644 root root /var/home/root/.gdbinit"
SYS_INSTALL_FILE_rootfiles_tcshrc="file admin_dot_tcshrc 0644 root root /var/home/root/.tcshrc"
SYS_INSTALL_FILE_monfiles_bash_logout="file dot_bash_logout 0644 1001 1001 /var/home/monitor/.bash_logout"
SYS_INSTALL_FILE_monfiles_bash_profile="file dot_bash_profile 0644 1001 1001 /var/home/monitor/.bash_profile"
SYS_INSTALL_FILE_monfiles_bashrc="file dot_bashrc 0644 1001 1001 /var/home/monitor/.bashrc"
SYS_INSTALL_FILE_etc_skel_bash_logout="file dot_bash_logout 0644 root root /etc/skel/.bash_logout"
SYS_INSTALL_FILE_etc_skel_bash_profile="file dot_bash_profile 0644 root root /etc/skel/.bash_profile"
SYS_INSTALL_FILE_etc_skel_bashrc="file dot_bashrc 0644 root root /etc/skel/.bashrc"
SYS_INSTALL_FILE_skel_root_bash_logout="file dot_bash_logout 0644 root root /etc/skel-root/.bash_logout"
SYS_INSTALL_FILE_skel_root_bash_profile="file admin_dot_bash_profile 0644 root root /etc/skel-root/.bash_profile"
SYS_INSTALL_FILE_skel_root_bashrc="file dot_bashrc 0644 root root /etc/skel-root/.bashrc"
SYS_INSTALL_FILE_skel_root_cshrc="file admin_dot_cshrc 0644 root root /etc/skel-root/.cshrc"
SYS_INSTALL_FILE_skel_root_gdbinit="file admin_dot_gdbinit 0644 root root /etc/skel-root/.gdbinit"
SYS_INSTALL_FILE_skel_root_tcshrc="file admin_dot_tcshrc 0644 root root /etc/skel-root/.tcshrc"
SYS_INSTALL_FILE_etc_vsftpd_vsftpd_conf="file vsftpd.conf 0644 root root /etc/vsftpd/vsftpd.conf"
SYS_INSTALL_FILE_etc_vsftpd_ftpusers="file vsftpd.ftpusers 0644 root root /etc/vsftpd/ftpusers"
SYS_INSTALL_FILE_etc_vsftpd_user_list="file vsftpd.user_list 0644 root root /etc/vsftpd/user_list"
SYS_INSTALL_FILE_securetty="file securetty 0644 root root /etc/securetty"
SYS_INSTALL_FILE_shells="file shells 0644 root root /etc/shells"
SYS_INSTALL_FILE_syslog_conf="file syslog.conf 0644 root root /var/opt/tms/output/syslog.conf"
SYS_INSTALL_FILE_ldso_conf="file ld.so.conf 0644 root root /etc/ld.so.conf"
SYS_INSTALL_FILE_ldso_tms_i386_conf="file tms-i386.conf 0644 root root /etc/ld.so.conf.d/tms-i386.conf"
SYS_INSTALL_FILE_ldso_tms_x86_64_conf="file tms-x86_64.conf 0644 root root /etc/ld.so.conf.d/tms-x86_64.conf"
SYS_INSTALL_FILE_grub_conf_template="file grub.conf.template 0600 root root \
	/etc/grub.conf.template"
SYS_INSTALL_FILE_grub_conf_template_new="file grub.conf.template.new 0600 \
	root root /etc/grub.conf.template.new"
SYS_INSTALL_FILE_grub_conf="file grub.conf 0600 root root \
	/bootmgr/boot/grub/grub.conf"
SYS_INSTALL_FILE_ee_conf_template="file ee.conf.template 0600 root root \
	/etc/ee.conf.template"
SYS_INSTALL_FILE_nvs_conf_template="file nvs.conf.template 0600 root root \
	/etc/nvs.conf.template"
SYS_INSTALL_FILE_smartd_conf="file smartd.conf 0644 root root /var/opt/tms/output/smartd.conf"
SYS_INSTALL_FILE_smartd_conf_template="file smartd.conf.template 0644 root root /etc/smartd.conf.template"
SYS_INSTALL_FILE_sshd_config="file sshd_config 0644 root root /var/opt/tms/output/sshd_config"
SYS_INSTALL_FILE_ssh_config="file ssh_config 0644 root root /var/opt/tms/output/ssh_config"
SYS_INSTALL_FILE_rt_tables="file rt_tables 0644 root root /etc/iproute2/rt_tables"
SYS_INSTALL_FILE_system_auth="file system-auth 0644 root root /var/opt/tms/output/system-auth"
SYS_INSTALL_FILE_wsmd_auth="file wsmd 0644 root root /etc/pam.d/wsmd"
SYS_INSTALL_FILE_sshd_auth="file sshd 0600 root root /etc/pam.d/sshd"
SYS_INSTALL_FILE_login_auth="file login 0600 root root /etc/pam.d/login"
SYS_INSTALL_FILE_login_auth_ppc="file login.ppc 0600 root root /etc/pam.d/login"
SYS_INSTALL_FILE_vsftpd_auth="file vsftpd 0600 root root /etc/pam.d/vsftpd"
SYS_INSTALL_FILE_remote_auth="file remote 0600 root root /etc/pam.d/remote"
SYS_INSTALL_FILE_remote_auth_ppc="file remote.ppc 0600 root root /etc/pam.d/remote"
SYS_INSTALL_FILE_bashrc="file bashrc 0644 root root /etc/bashrc"
SYS_INSTALL_FILE_moduli="file moduli 0600 root root /etc/ssh/moduli"
SYS_INSTALL_FILE_etc_profile_d_limits_csh="file limits.csh 0755 root root /etc/profile.d/limits.csh"
SYS_INSTALL_FILE_etc_profile_d_limits_sh="file limits.sh 0755 root root /etc/profile.d/limits.sh"
SYS_INSTALL_FILE_etc_profile_d_less_opts_csh="file less-opts.csh 0755 root root /etc/profile.d/less-opts.csh"
SYS_INSTALL_FILE_etc_profile_d_less_opts_sh="file less-opts.sh 0755 root root /etc/profile.d/less-opts.sh"
SYS_INSTALL_FILE_etc_ipv6_disable_conf="file ipv6-disable.conf 0644 root root /etc/ipv6-disable.conf"
SYS_INSTALL_FILE_etc_gdbinit="file gdbinit 0644 root root /etc/gdbinit"
SYS_INSTALL_FILE_modprobe_ipv6_disable_conf="file ipv6-disable.conf 0644 root root /etc/modprobe.d/ipv6-disable.conf"

# NOTE: on EL6 the PowerPC files implicitly referenced here are not present
SYS_INSTALL_FILE_udev_20_base_rules="file 20-base.rules${XARCH_DOT}${NOHWRNG} 0644 root root /etc/udev/rules.d/20-base.rules"
SYS_INSTALL_FILE_udev_70_base_rules="file 70-base.rules${XARCH_DOT} 0644 root root /etc/udev/rules.d/70-base.rules"

# Java stuff

SYS_INSTALL_FILE_etc_profile_d_java_csh="file java.csh 0755 root root /etc/profile.d/java.csh"
SYS_INSTALL_FILE_etc_profile_d_java_sh="file java.sh 0755 root root /etc/profile.d/java.sh"

# Files to create empty

SYS_TOUCH_FILE_dot_firstboot="touch 0644 root root /etc/.firstboot"
SYS_TOUCH_FILE_etc_mtab="touch 0644 root root /etc/mtab"
SYS_TOUCH_FILE_lastlog="touch 0600 root root /var/log/lastlog"
SYS_TOUCH_FILE_systemlog="touch 0600 root root /var/log/systemlog"
SYS_TOUCH_FILE_messages="touch 0640 root 3000 /var/log/messages"
SYS_TOUCH_FILE_web_access_log="touch 0600 root root /var/log/web_access_log"
SYS_TOUCH_FILE_web_error_log="touch 0600 root root /var/log/web_error_log"
SYS_TOUCH_FILE_utmp="touch 0664 root utmp /var/run/utmp"
SYS_TOUCH_FILE_wtmp="touch 0664 root utmp /var/log/wtmp"
SYS_TOUCH_FILE_tallynamelog="touch 0600 root root /var/log/tallynamelog"
SYS_TOUCH_FILE_etc_sysconfig_udev="touch 0644 root root /etc/sysconfig/udev"

# Symlinks

SYS_CREATE_SYMLINK_tmp="symlink / /var/root/tmp tmp"
SYS_CREATE_SYMLINK_etc_cron_d="symlink /etc /var/opt/tms/output/cron.d cron.d"
SYS_CREATE_SYMLINK_hosts="symlink /etc /var/opt/tms/output/hosts hosts"
SYS_CREATE_SYMLINK_resolv_conf="symlink /etc /var/opt/tms/output/resolv.conf \
	resolv.conf"
SYS_CREATE_SYMLINK_etc_clirc="symlink /etc /var/opt/tms/output/clirc clirc"
SYS_CREATE_SYMLINK_etc_snmpd_conf="symlink /etc /var/opt/tms/output/snmpd.conf snmpd.conf"
SYS_CREATE_SYMLINK_usr_snmpd_conf="symlink /usr/share/snmp/conf /var/opt/tms/output/snmpd.conf snmpd.conf"
SYS_CREATE_SYMLINK_usr_bin_snmpinform="symlink /usr/bin /usr/bin/snmptrap snmpinform"
SYS_CREATE_SYMLINK_ntp_conf="symlink /etc /var/opt/tms/output/ntp.conf ntp.conf"
SYS_CREATE_SYMLINK_localtime="symlink /etc /var/opt/tms/output/localtime \
	localtime"

SYS_CREATE_SYMLINK_passwd="symlink /etc /var/opt/tms/output/passwd passwd"
SYS_CREATE_SYMLINK_shadow="symlink /etc /var/opt/tms/output/shadow shadow"
SYS_CREATE_SYMLINK_group="symlink /etc /var/opt/tms/output/group group"
SYS_CREATE_SYMLINK_smartd_conf="symlink /etc /var/opt/tms/output/smartd.conf smartd.conf"
SYS_CREATE_SYMLINK_sshd_config="symlink /etc/ssh /var/opt/tms/output/sshd_config sshd_config"
SYS_CREATE_SYMLINK_ssh_config="symlink /etc/ssh /var/opt/tms/output/ssh_config ssh_config"
SYS_CREATE_SYMLINK_ssh_known_hosts="symlink /etc/ssh /var/opt/tms/output/ssh_known_hosts ssh_known_hosts"
SYS_CREATE_SYMLINK_motd="symlink /etc /var/opt/tms/output/motd motd"
SYS_CREATE_SYMLINK_issue="symlink /etc /var/opt/tms/output/issue issue"
SYS_CREATE_SYMLINK_issue_net="symlink /etc /var/opt/tms/output/issue.net issue.net"
SYS_CREATE_SYMLINK_logrotate_conf="symlink /etc /var/opt/tms/output/logrotate.conf logrotate.conf"
SYS_CREATE_SYMLINK_logrotate_crontab="symlink /var/opt/tms/output/cron.d ../logrotate logrotate"
SYS_CREATE_SYMLINK_vpartsync_crontab="symlink /var/opt/tms/output/cron.d ../vpart_sync vpart_sync"
SYS_CREATE_SYMLINK_sysstat_crontab="symlink /var/opt/tms/output/cron.d /etc/cron_conf.d/sysstat sysstat"
SYS_CREATE_SYMLINK_syslog="symlink /etc/sysconfig /var/opt/tms/output/syslog syslog"
SYS_CREATE_SYMLINK_syslog_conf="symlink /etc /var/opt/tms/output/syslog.conf syslog.conf"
SYS_CREATE_SYMLINK_ssmtp_conf="symlink /etc /var/opt/tms/output/ssmtp.conf ssmtp.conf"
SYS_CREATE_SYMLINK_revaliases_conf="symlink /etc /var/opt/tms/output/revaliases revaliases"
SYS_CREATE_SYMLINK_ssmtp_auto_conf="symlink /etc /var/opt/tms/output/ssmtp-auto.conf ssmtp-auto.conf"
SYS_CREATE_SYMLINK_sendmail_usrsbin="symlink /usr/sbin ../../sbin/ssmtp sendmail"
SYS_CREATE_SYMLINK_sendmail_usrlib="symlink /usr/lib ../../sbin/ssmtp sendmail"
SYS_CREATE_SYMLINK_fscktmpfs_usrsbin="symlink /usr/sbin ../../bin/true fsck.tmpfs"
SYS_CREATE_SYMLINK_pam_radius_auth="symlink ${LIB_SECURITY_DIR} /opt/tms/lib/security/pam_radius_auth.so pam_radius_auth.so"
SYS_CREATE_SYMLINK_pam_radius_auth_conf="symlink /etc/raddb /var/opt/tms/output/pam_radius_auth.conf pam_radius_auth.conf"
SYS_CREATE_SYMLINK_pam_tacplus="symlink ${LIB_SECURITY_DIR} /opt/tms/lib/security/pam_tacplus.so pam_tacplus.so"
SYS_CREATE_SYMLINK_pam_unix="symlink ${LIB_SECURITY_DIR} /opt/tms/lib/security/pam_unix.so pam_unix.so"
SYS_CREATE_SYMLINK_pam_disabled="symlink ${LIB_SECURITY_DIR} /opt/tms/lib/security/pam_disabled.so pam_disabled.so"
SYS_CREATE_SYMLINK_pam_ldap="symlink ${LIB_SECURITY_DIR} /opt/tms/lib/security/pam_ldap.so pam_ldap.so"
SYS_CREATE_SYMLINK_pam_tallybyname="symlink ${LIB_SECURITY_DIR} /opt/tms/lib/security/pam_tallybyname.so pam_tallybyname.so"
SYS_CREATE_SYMLINK_system_auth="symlink /etc/pam.d /var/opt/tms/output/system-auth system-auth"
SYS_CREATE_SYMLINK_ssh_rsa1_priv="symlink /etc/ssh /var/lib/ssh/ssh_host_key         ssh_host_key"
SYS_CREATE_SYMLINK_ssh_rsa1_pub= "symlink /etc/ssh /var/lib/ssh/ssh_host_key.pub     ssh_host_key.pub"
SYS_CREATE_SYMLINK_ssh_rsa2_priv="symlink /etc/ssh /var/lib/ssh/ssh_host_rsa_key     ssh_host_rsa_key"
SYS_CREATE_SYMLINK_ssh_rsa2_pub= "symlink /etc/ssh /var/lib/ssh/ssh_host_rsa_key.pub ssh_host_rsa_key.pub"
SYS_CREATE_SYMLINK_ssh_dsa2_priv="symlink /etc/ssh /var/lib/ssh/ssh_host_dsa_key     ssh_host_dsa_key"
SYS_CREATE_SYMLINK_ssh_dsa2_pub= "symlink /etc/ssh /var/lib/ssh/ssh_host_dsa_key.pub ssh_host_dsa_key.pub"
SYS_CREATE_SYMLINK_web_graphs= "symlink /opt/tms/lib/web/content /var/opt/tms/web/graphs graphs"
SYS_CREATE_SYMLINK_root_home="symlink / /var/home/root root"
SYS_CREATE_SYMLINK_pluto_ipsec_d="symlink /etc /var/opt/tms/output/pluto/ipsec.d ipsec.d"
SYS_CREATE_SYMLINK_pluto_ipsec_conf="symlink /etc /var/opt/tms/output/pluto/ipsec.conf ipsec.conf"
SYS_CREATE_SYMLINK_racoon="symlink /etc /var/opt/tms/output/racoon racoon"
SYS_CREATE_SYMLINK_web_sysimages= "symlink /opt/tms/lib/web/content /var/opt/tms/web/sysimages sysimages"
SYS_CREATE_SYMLINK_xg="symlink /opt/tms/bin /opt/tms/lib/web/handlers/xg xg"
SYS_CREATE_SYMLINK_rgp_raw="symlink /opt/tms/bin/ /opt/tms/bin/rgp rgp_raw"
SYS_CREATE_SYMLINK_config_text="symlink /config /var/opt/tms/text_configs text"

#
# This is a legacy.  Busybox belongs in /sbin, and that's where it now lives.
# But for a while, it was in /bin, so keep a symlink for anyone who is still
# looking for it there.
#
SYS_CREATE_SYMLINK_busybox="symlink /bin /sbin/busybox busybox"

SYS_INSTALL_FILE_kvm="file kvm 0755 root root /etc/rc.d/init.d/kvm"

SYS_CREATE_SYMLINK_etc_libvirt="symlink /etc /var/opt/tms/output/libvirt libvirt"

# Special (device) files

SYS_CREATE_SPECIAL_dev_null="special c 1 3 0666 root root /dev/null"
SYS_CREATE_SPECIAL_dev_zero="special c 1 5 0666 root root /dev/zero"
SYS_CREATE_SPECIAL_dev_console="special c 5 1 0600 root root /dev/console"
SYS_CREATE_SPECIAL_dev_tty1="special c 4 1 0600 root root /dev/tty1"
SYS_CREATE_SPECIAL_dev_tty2="special c 4 1 0600 root root /dev/tty2"
SYS_CREATE_SPECIAL_dev_tty3="special c 4 1 0600 root root /dev/tty3"
SYS_CREATE_SPECIAL_dev_tty4="special c 4 1 0600 root root /dev/tty4"
SYS_CREATE_SPECIAL_dev_tty5="special c 4 1 0600 root root /dev/tty5"
SYS_CREATE_SPECIAL_dev_tty6="special c 4 1 0600 root root /dev/tty6"
SYS_CREATE_SPECIAL_dev_tty7="special c 4 1 0600 root root /dev/tty7"
SYS_CREATE_SPECIAL_dev_tty8="special c 4 1 0600 root root /dev/tty8"
SYS_CREATE_SPECIAL_dev_ttyS0="special c 4 64 0660 root uucp /dev/ttyS0"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

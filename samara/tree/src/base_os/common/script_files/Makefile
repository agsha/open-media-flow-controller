#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

#
# See src/mk/files_adv.mk for comments explaining the different types
# of directives available.
#

INSTALL_DIRECTIVES= \
	${SYS_INSTALL_FILE_firstboot_sh} \
	${SYS_INSTALL_FILE_var_upgrade_sh} \
	${SYS_INSTALL_FILE_customer_ssl_vars_sh} \
	${SYS_INSTALL_FILE_smartev_sh} \
	${SYS_INSTALL_FILE_dhclient_script} \
	${SYS_INSTALL_FILE_dhclient6_script} \
	${SYS_INSTALL_FILE_writeimage_sh} \
	${SYS_INSTALL_FILE_imgq_sh} \
	${SYS_INSTALL_FILE_aiset_sh} \
	${SYS_INSTALL_FILE_aiget_sh} \
	${SYS_INSTALL_FILE_layout_settings_sh} \
	${SYS_INSTALL_FILE_makemail_sh} \
	${SYS_INSTALL_FILE_sysdump_sh} \
	${SYS_INSTALL_FILE_cleanse_db_sh} \
	${SYS_INSTALL_FILE_logrotate_wrapper_sh} \
	${SYS_INSTALL_FILE_resetpw_sh} \
	${SYS_INSTALL_FILE_resetpw_shell_sh} \
	${SYS_INSTALL_FILE_afail_sh} \
	${SYS_INSTALL_FILE_webpost_sh} \
	${SYS_INSTALL_FILE_imgverify_sh} \
	${SYS_INSTALL_FILE_scrub_sh} \
	${SYS_INSTALL_FILE_manufacture_sh} \
	${SYS_INSTALL_FILE_opt_feature_rootflop_sh} \
	${SYS_INSTALL_FILE_vpart_sh} \
	${SYS_INSTALL_FILE_zeroconf_sh} \
	${SYS_INSTALL_FILE_tailgrep_sh} \
	${SYS_INSTALL_FILE_remanufacture_sh} \
	${SYS_INSTALL_FILE_rkn_sh} \
	${SYS_INSTALL_FILE_stats_defrag_sh} \
	${SYS_INSTALL_FILE_change_kern_sh} \
	${SYS_INSTALL_FILE_web_fetch_sh} \
	${SYS_INSTALL_FILE_tpkg_create_sh} \
	${SYS_INSTALL_FILE_tpkg_extract_sh} \
	${SYS_INSTALL_FILE_tpkg_query_sh} \
	${SYS_INSTALL_FILE_tpkg_secure_sh} \
	${SYS_INSTALL_FILE_timeout_sh} \
	${SYS_INSTALL_FILE_imgtoiso_sh} \
	${SYS_INSTALL_FILE_virt_sync_pools_sh} \
	${SYS_INSTALL_FILE_gstack_sh} \
	${SYS_INSTALL_FILE_racoon_startup_sh} \
	${SYS_INSTALL_FILE_ipsec_startup_sh} \
	${SYS_INSTALL_FILE_lesspipe_sh} \
	${SYS_INSTALL_FILE_get_layout_disks_sh} \


ifeq (${PROD_TARGET_ARCH_FAMILY},X86)
	INSTALL_DIRECTIVES+= \
		${SYS_INSTALL_FILE_dmidump_sh} \

endif


ifneq (${PROD_FEATURE_CLUSTER},0)
	INSTALL_DIRECTIVES+= \
		${SYS_INSTALL_FILE_cluster_upgrade_tcl} \
		${SYS_INSTALL_FILE_cluster_upgrade_wrapper_sh} \
		${SYS_INSTALL_FILE_cluster_wide_tcl} \
		${SYS_INSTALL_FILE_cluster_rootflop_sh} \

endif

ifneq (${PROD_FEATURE_CMC_CLIENT}${PROD_FEATURE_CMC_SERVER},00)
	INSTALL_DIRECTIVES+= \
		${SYS_INSTALL_FILE_cmc_rootflop_sh} \

endif

# All the files.  Some of these overwrite stuff coming from the base os

SYS_INSTALL_FILE_firstboot_sh="file firstboot.sh 0755 root root \
	/sbin/firstboot.sh"
SYS_INSTALL_FILE_var_upgrade_sh="file var_upgrade.sh 0755 root root \
	/sbin/var_upgrade.sh"
SYS_INSTALL_FILE_smartev_sh="file smartev.sh 0755 root root /sbin/smartev.sh"
SYS_INSTALL_FILE_dhclient_script="file dhclient-script 0755 root root \
	/sbin/dhclient-script"
SYS_INSTALL_FILE_dhclient6_script="file dhclient6-script 0755 root root \
	/sbin/dhclient6-script"
SYS_INSTALL_FILE_writeimage_sh="file writeimage.sh 0755 root root /sbin/writeimage.sh"
SYS_INSTALL_FILE_imgq_sh="file imgq.sh 0755 root root /sbin/imgq.sh"
SYS_INSTALL_FILE_aiset_sh="file aiset.sh 0755 root root /sbin/aiset.sh"
SYS_INSTALL_FILE_aiget_sh="file aiget.sh 0755 root root /sbin/aiget.sh"
SYS_INSTALL_FILE_layout_settings_sh="file layout_settings.sh 0755 root root /etc/layout_settings.sh"
SYS_INSTALL_FILE_get_layout_disks_sh="file get_layout_disks.sh 0755 root root /sbin/get_layout_disks.sh"
SYS_INSTALL_FILE_makemail_sh="file makemail.sh 0755 root root /sbin/makemail.sh"
SYS_INSTALL_FILE_sysdump_sh="file sysdump.sh 0755 root root /sbin/sysdump.sh"
SYS_INSTALL_FILE_cleanse_db_sh="file cleanse_db.sh 0755 root root /sbin/cleanse_db.sh"
SYS_INSTALL_FILE_logrotate_wrapper_sh="file logrotate_wrapper.sh 0755 root root /usr/sbin/logrotate_wrapper.sh"
SYS_INSTALL_FILE_resetpw_sh="file resetpw.sh 0755 root root /sbin/resetpw.sh"
SYS_INSTALL_FILE_resetpw_shell_sh="file resetpw_shell.sh 0755 root root /sbin/resetpw_shell.sh"
SYS_INSTALL_FILE_afail_sh="file afail.sh 0755 root root /sbin/afail.sh"
SYS_INSTALL_FILE_webpost_sh="file webpost.sh 0755 root root /sbin/webpost.sh"
SYS_INSTALL_FILE_imgverify_sh="file imgverify.sh 0755 root root /sbin/imgverify.sh"
SYS_INSTALL_FILE_scrub_sh="file scrub.sh 0755 root root /sbin/scrub.sh"
SYS_INSTALL_FILE_manufacture_sh="file manufacture.sh 0755 root root /sbin/manufacture.sh"
SYS_INSTALL_FILE_opt_feature_rootflop_sh="file opt_feature_rootflop.sh 0755 root root /sbin/opt_feature_rootflop.sh"
SYS_INSTALL_FILE_cmc_rootflop_sh="file cmc_rootflop.sh 0755 root root /sbin/cmc_rootflop.sh"
SYS_INSTALL_FILE_ntpd_wrapper_sh="file ntpd_wrapper.sh 0755 root root /usr/sbin/ntpd_wrapper.sh"
SYS_INSTALL_FILE_vpart_sh="file vpart.sh 0755 root root /sbin/vpart.sh"
SYS_INSTALL_FILE_zeroconf_sh="file zeroconf 0755 root root /etc/zeroconf"
SYS_INSTALL_FILE_tailgrep_sh="file tailgrep.sh 0755 root root /sbin/tailgrep.sh"
SYS_INSTALL_FILE_remanufacture_sh="file remanufacture.sh 0755 root root /sbin/remanufacture.sh"
SYS_INSTALL_FILE_rkn_sh="file rkn.sh 0755 root root /sbin/rkn.sh"
SYS_INSTALL_FILE_stats_defrag_sh="file stats_defrag.sh 0755 root root /sbin/stats_defrag.sh"
SYS_INSTALL_FILE_cluster_upgrade_tcl="file cluster_upgrade.tcl 0755 root root /sbin/cluster_upgrade.tcl"
SYS_INSTALL_FILE_cluster_upgrade_wrapper_sh="file cluster_upgrade_wrapper.sh 0755 root root /sbin/cluster_upgrade_wrapper.sh"
SYS_INSTALL_FILE_cluster_wide_tcl="file cluster_wide.tcl 0755 root root /sbin/cluster_wide.tcl"
SYS_INSTALL_FILE_cluster_rootflop_sh="file cluster_rootflop.sh 0755 root root /sbin/cluster_rootflop.sh"
SYS_INSTALL_FILE_change_kern_sh="file change_kern.sh 0755 root root /sbin/change_kern.sh"
SYS_INSTALL_FILE_web_fetch_sh="file web_fetch.sh 0755 root root /bin/web_fetch.sh"
SYS_INSTALL_FILE_tpkg_create_sh="file tpkg_create.sh 0755 root root /sbin/tpkg_create.sh"
SYS_INSTALL_FILE_tpkg_extract_sh="file tpkg_extract.sh 0755 root root /sbin/tpkg_extract.sh"
SYS_INSTALL_FILE_tpkg_query_sh="file tpkg_query.sh 0755 root root /sbin/tpkg_query.sh"
SYS_INSTALL_FILE_tpkg_secure_sh="file tpkg_secure.sh 0755 root root /sbin/tpkg_secure.sh"
SYS_INSTALL_FILE_timeout_sh="file timeout.sh 0755 root root /bin/timeout.sh"
SYS_INSTALL_FILE_imgtoiso_sh="file imgtoiso.sh 0755 root root /sbin/imgtoiso.sh"
SYS_INSTALL_FILE_virt_sync_pools_sh="file virt_sync_pools.sh 0755 root root /sbin/virt_sync_pools.sh"
SYS_INSTALL_FILE_gstack_sh="file gstack.sh 0755 root root /usr/bin/gstack"
SYS_INSTALL_FILE_racoon_startup_sh="file racoon_startup.sh 0755 root root /usr/sbin/racoon_startup.sh"
SYS_INSTALL_FILE_ipsec_startup_sh="file ipsec_startup.sh 0755 root root /usr/sbin/ipsec_startup.sh"
SYS_INSTALL_FILE_lesspipe_sh="file lesspipe.sh 0755 root root /usr/bin/lesspipe.sh"
SYS_INSTALL_FILE_dmidump_sh="file dmidump.sh 0755 root root /sbin/dmidump.sh"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

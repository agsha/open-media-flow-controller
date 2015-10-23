# Makefile
include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS= \
  startup \
  vxa \
  pacifica \
  dpi-analyzer \
  dpdk \


INSTALL_DIRECTIVES= \
	${SYS_INSTALL_DIR_coredump} \
	${SYS_INSTALL_DIR_log} \
	${SYS_INSTALL_DIR_nkn} \
	${SYS_INSTALL_DIR_opt_nkn} \
	${SYS_INSTALL_DIR_opt_nkn_bin} \
	${SYS_INSTALL_DIR_opt_nkn_bin_policy} \
	${SYS_INSTALL_DIR_opt_nkn_lib} \
	${SYS_INSTALL_DIR_opt_nkn_lib_lite} \
	${SYS_INSTALL_DIR_opt_nkn_debugbin} \
	${SYS_INSTALL_DIR_opt_nkn_generic_cli_bin} \
	${SYS_INSTALL_DIR_usr} \
	${SYS_INSTALL_DIR_usr_bin} \
	${SYS_INSTALL_DIR_etc_sysconfig} \
	${SYS_INSTALL_DIR_etc_rc_d_init_d} \
	${SYS_INSTALL_DIR_etc_pam_d} \
	${SYS_INSTALL_DIR_etc_ld_so_conf_d} \
	${SYS_INSTALL_DIR_etc_profile_d} \
	${SYS_INSTALL_DIR_etc_modprobe_d} \
	${SYS_INSTALL_DIR_usr_local_bin} \
	${SYS_INSTALL_DIR_home} \
	${SYS_INSTALL_DIR_home2} \
	${SYS_INSTALL_DIR_homes} \
	${SYS_INSTALL_DIR_bugbits} \
	${SYS_INSTALL_DIR_nokeena_utils} \
	${SYS_INSTALL_DIR_eng} \
	${SYS_INSTALL_DIR_boot} \
	${SYS_INSTALL_DIR_opt_nkn_support} \
	${SYS_INSTALL_DIR_opt_nkn_installthis} \
	${SYS_INSTALL_DIR_opt_nkn_installthis_config} \
	${SYS_INSTALL_DIR_opt_nkn_installthis_config_nkn} \
	${SYS_INSTALL_DIR_opt_nkn_installthis_config_nkn_cert} \
	${SYS_INSTALL_DIR_opt_nkn_installthis_config_nkn_ca} \
	${SYS_INSTALL_DIR_opt_nkn_installthis_config_nkn_key} \
	${SYS_INSTALL_DIR_opt_nkn_installthis_config_nkn_csr} \
	${SYS_INSTALL_DIR_opt_nkn_installthis_nkn} \
	${SYS_INSTALL_DIR_opt_nkn_installthis_nkn_plugins} \
	${SYS_INSTALL_DIR_var_named} \
	${SYS_INSTALL_DIR_var_named_chroot} \
	${SYS_INSTALL_DIR_var_named_chroot_etc} \
	${SYS_INSTALL_DIR_var_logexport} \
	${SYS_INSTALL_DIR_var_logexport_chroot} \
	${SYS_INSTALL_DIR_nkn_cb} \
	${SYS_INSTALL_DIR_nkn_tmp_ocrp} \
	${SYS_INSTALL_DIR_opt_tms_lib_web_cgi_bin_protocol} \
	${SYS_INSTALL_DIR_cgi_bin_protocol_ocrp} \
	${SYS_INSTALL_DIR_cgi_bin_ocrp_asset_map} \
	${SYS_INSTALL_DIR_var_opt} \
	${SYS_INSTALL_DIR_var_opt_tms} \
	${SYS_INSTALL_DIR_var_opt_tms_output} \
	${SYS_INSTALL_DIR_var_opt_tms_output_cron_d} \
	${SYS_INSTALL_FILE_etc_sysconfig_network} \
	${SYS_INSTALL_FILE_eth_reorder} \
	${SYS_INSTALL_FILE_firstboot_action} \
	${SYS_INSTALL_FILE_fixups} \
	${SYS_INSTALL_FILE_fixups_sh} \
        ${SYS_INSTALL_FILE_delete_unused_drives_from_TMdb} \
        ${SYS_INSTALL_FILE_delete_unused_drives_from_TMdb_sh} \
	${SYS_INSTALL_FILE_fuse} \
	${SYS_INSTALL_FILE_initdisks} \
	${SYS_INSTALL_FILE_raid_monitor} \
	${SYS_INSTALL_FILE_mfd_setup} \
	${SYS_INSTALL_FILE_irqpin_ixgbe_py} \
	${SYS_INSTALL_FILE_set_irq_affinity_sh} \
	${SYS_INSTALL_FILE_eth_reorder_sh} \
	${SYS_INSTALL_FILE_eth_device_from_http_sh} \
	${SYS_INSTALL_FILE_ethflash} \
	${SYS_INSTALL_FILE_empty_diskinfo_from_db_sh} \
	${SYS_INSTALL_FILE_initdisks_sh} \
	${SYS_INSTALL_FILE_nvsdmon} \
	${SYS_INSTALL_FILE_nvsdmon_sh} \
	${SYS_INSTALL_FILE_raid_monitor_sh} \
	${SYS_INSTALL_FILE_nvsd_init_sh} \
	${SYS_INSTALL_FILE_disks_common_sh} \
	${SYS_INSTALL_FILE_reinit_config_disks_sh} \
	${SYS_INSTALL_FILE_write_diskinfo_to_db_sh} \
	${SYS_INSTALL_FILE_get_smart_data_sh} \
	${SYS_INSTALL_FILE_mfd_setup_sh} \
	${SYS_INSTALL_FILE_IC_dm2_ext3_sh} \
	${SYS_INSTALL_FILE_IRC_dm2_ext3_sh} \
	${SYS_INSTALL_FILE_IC_ps_xfs_sh} \
	${SYS_INSTALL_FILE_IC_del_ps_disk_sh} \
	${SYS_INSTALL_FILE_upgrade_ssd_sh} \
	${SYS_INSTALL_FILE_scrub_mfd_sh} \
	${SYS_INSTALL_FILE_dump_uri_list_sh} \
	${SYS_INSTALL_FILE_cache_add_sh} \
	${SYS_INSTALL_FILE_cache_add_vpe_sh} \
	${SYS_INSTALL_FILE_cache_convert_sh} \
	${SYS_INSTALL_FILE_cache_del_sh} \
	${SYS_INSTALL_FILE_cache_du_sh} \
	${SYS_INSTALL_FILE_cache_evict_sh} \
	${SYS_INSTALL_FILE_disk_auto_enable_sh} \
	${SYS_INSTALL_FILE_evictd_common_sh} \
	${SYS_INSTALL_FILE_cache_evictd_ssd_sh} \
	${SYS_INSTALL_FILE_cache_evictd_sata_sh} \
	${SYS_INSTALL_FILE_cache_evictd_sas_sh} \
	${SYS_INSTALL_FILE_evict_metadata_py} \
	${SYS_INSTALL_FILE_cache_meta_evictd_sh} \
	${SYS_INSTALL_FILE_cache_ls_sh} \
	${SYS_INSTALL_FILE_cache_mgmt_ls_py} \
	${SYS_INSTALL_FILE_cache_mgmt_del_py} \
	${SYS_INSTALL_FILE_cache_slow_del_py} \
	${SYS_INSTALL_FILE_cache_v1_to_v2_sh} \
	${SYS_INSTALL_FILE_cache_disk_lib_sh} \
	${SYS_INSTALL_FILE_cache_disk_lib_py} \
	${SYS_INSTALL_FILE_cache_mgmt_lib_py} \
	${SYS_INSTALL_FILE_mount_cache_sh} \
	${SYS_INSTALL_FILE_update_grub_console_sh} \
	${SYS_INSTALL_FILE_umount_cache_sh} \
	${SYS_INSTALL_FILE_max_arp_entry_sh} \
	${SYS_INSTALL_FILE_cmm_op_sh} \
	${SYS_INSTALL_FILE_mgmt_build_ns_object_list_py} \
	${SYS_INSTALL_FILE_nkn_sys_mon_sh} \
	${SYS_INSTALL_FILE_nkn_sys_mon_cron} \
	${SYS_INSTALL_FILE_ingest} \
	${SYS_INSTALL_FILE_ftpusers} \
	${SYS_INSTALL_FILE_pureftpd} \
	${SYS_INSTALL_FILE_storman_sh} \
	${SYS_INSTALL_FILE_df_all} \
	${SYS_INSTALL_FILE_prestage} \
	${SYS_INSTALL_FILE_tech_support_sh} \
	${SYS_INSTALL_FILE_cli_log_sh} \
	${SYS_INSTALL_FILE_disk_diagnosis_sh} \
	${SYS_INSTALL_FILE_lsiutil_debug_info_sh} \
	${SYS_INSTALL_FILE_dm_ext_sh} \
	${SYS_INSTALL_FILE_dmidecode_info_sh} \
	${SYS_INSTALL_FILE_eth_info_sh} \
	${SYS_INSTALL_FILE_container_dump_py} \
	${SYS_INSTALL_FILE_container_bkp_py} \
	${SYS_INSTALL_FILE_nkn_x86_64_conf} \
	${SYS_CREATE_SYMLINK_nvsd_memalloc} \
	${SYS_CREATE_SYMLINK_nvsd_memalloc_lite} \
	${SYS_CREATE_SYMLINK_nvsd_memalloc_cmm} \
	${SYS_CREATE_SYMLINK_nvsd_memalloc_lib_lite} \
	${SYS_INSTALL_FILE_intfstats} \
	${SYS_INSTALL_FILE_mem_sh} \
	${SYS_INSTALL_FILE_installthis_key_pem} \
	${SYS_INSTALL_FILE_installthis_root_pem} \
	${SYS_INSTALL_FILE_installthis_nkn_conf_default} \
	${SYS_INSTALL_FILE_installthis_nkn_ucflt_conf} \
	${SYS_INSTALL_FILE_installthis_nkn_nknlogd_cfg} \
	${SYS_INSTALL_FILE_installthis_log_analyzer_conf} \
	${SYS_INSTALL_FILE_installthis_generate_change_lists} \
	$(SYS_INSTALL_FILE_installthis_generate_aws_file_list) \
	${SYS_INSTALL_FILE_installthis_defaule_rule_tcl} \
	${SYS_INSTALL_FILE_installthis_tech_support_cfg} \
	${SYS_INSTALL_FILE_installthis_pyHashCheck} \
	${SYS_INSTALL_FILE_installthis_pyVpePublish} \
        ${SYS_INSTALL_FILE_installthis_pyGenericTranscode} \
        ${SYS_INSTALL_FILE_installthis_pyYoutubeTranscode} \
	${SYS_INSTALL_FILE_installthis_mfp_pmf_schema_xsd} \
	${SYS_INSTALL_FILE_installthis_mfp_filed_conf_default} \
	${SYS_INSTALL_FILE_installthis_mfp_lived_conf_default} \
	${SYS_INSTALL_FILE_installthis_lf_conf_default} \
	${SYS_INSTALL_FILE_installthis_lf_metrics_presentation_xsd} \
	${SYS_INSTALL_FILE_mgmt_namespace_cleanup_sh} \
	${SYS_INSTALL_FILE_fetch_smap} \
	${SYS_INSTALL_FILE_fetch_uf_list} \
	${SYS_INSTALL_FILE_update_cachetype_sh} \
	${SYS_INSTALL_FILE_amrank_dump_sh} \
	${SYS_INSTALL_FILE_tproxy_setup} \
	${SYS_INSTALL_FILE_debugbin_debug_sh} \
	${SYS_INSTALL_FILE_debugbin_glob_memalloc_sh} \
	${SYS_INSTALL_FILE_debugbin_mdreq_showdisk_sh} \
	${SYS_INSTALL_FILE_debugbin_pacifica_ndiag_sh} \
	${SYS_INSTALL_FILE_debugbin_pacifica_satadimm_table_16} \
	${SYS_INSTALL_FILE_debugbin_sandforce_smartctl_sh} \
	${SYS_INSTALL_FILE_debugbin_slabinfo_sh} \
	${SYS_INSTALL_FILE_debugbin_delete_one_drive_from_TMdb_sh} \
	${SYS_INSTALL_FILE_genericbin_verify_config_sh} \
	${SYS_INSTALL_FILE_genericbin_getdisk_for_container_sh} \
	${SYS_INSTALL_FILE_genericbin_findslottodcno_sh} \
	${SYS_INSTALL_FILE_genericbin_detect_hdd_sh} \
	${SYS_INSTALL_FILE_postaccesslog_sh} \
	${SYS_INSTALL_FILE_ankeena_probe_py} \
	${SYS_INSTALL_FILE_proc_config_sh} \
	${SYS_INSTALL_FILE_collect_nkncnt_data_sh} \
	${SYS_INSTALL_FILE_initrd_img} \
	${SYS_INSTALL_FILE_top5_proc_sh} \
	${SYS_INSTALL_FILE_load_ct_values_sh} \
	${SYS_INSTALL_FILE_disk_metadata_proc_sh} \
	${SYS_INSTALL_FILE_dump_del_py} \
	${SYS_INSTALL_FILE_get_interface_txqueuelen_sh} \
	${SYS_INSTALL_FILE_set_interface_txqueuelen_sh} \
	${SYS_INSTALL_FILE_remove_iptables_sh} \
	${SYS_INSTALL_FILE_raid_events_sh} \
	${SYS_INSTALL_FILE_mirror_resync_sh} \
	${SYS_INSTALL_FILE_mirror_status_sh} \
	${SYS_INSTALL_FILE_set_syncookie_sh} \
	${SYS_INSTALL_FILE_jmfc_sample_tcl}\
	${SYS_INSTALL_FILE_pe_new_sh}\
	${SYS_INSTALL_FILE_pe_edit_sh}\
	${SYS_INSTALL_FILE_pe_del_sh}\
	${SYS_INSTALL_FILE_pe_com_sh}\
	${SYS_INSTALL_FILE_pe_revert_sh}\
	${SYS_INSTALL_FILE_pe_file_sh}\
        ${SYS_INSTALL_FILE_fix_fallback_sh} \
        ${SYS_INSTALL_FILE_tcp_params_sh} \
        ${SYS_INSTALL_FILE_sftp_sh} \
	${SYS_INSTALL_FILE_prefetch_py} \
	${SYS_INSTALL_FILE_mgmt_nfs_mount_sh} \
	${SYS_INSTALL_FILE_mgmt_nfs_umount_sh} \
	${SYS_INSTALL_FILE_mgmt_nfs_mountstat_sh} \
	${SYS_INSTALL_FILE_whitelist} \
	${SYS_INSTALL_FILE_generic_shell_cmd_py} \
	${SYS_INSTALL_FILE_tproxy_tcp_timewait_sh} \
	${SYS_INSTALL_FILE_postencode_sh} \
	${SYS_INSTALL_FILE_drop_cache_sh} \
	${SYS_INSTALL_FILE_drop_cache_awk} \
	${SYS_INSTALL_FILE_nkncntdump_sh} \
	${SYS_INSTALL_FILE_wd_gdb_sh} \
	${SYS_INSTALL_FILE_pxy_tgl_sh} \
	${SYS_INSTALL_FILE_etc_modprobe_d_nkn_conf} \
	${SYS_INSTALL_FILE_etc_modprobe_urif_d_nkn_conf} \
	${SYS_INSTALL_FILE_startdmi_sh} \
	${SYS_INSTALL_FILE_ssld_verify_cert_sh} \
	${SYS_INSTALL_FILE_etc_rc_d_init_d_rpcbind} \
	${SYS_INSTALL_FILE_etc_rc_d_init_d_named} \
	${SYS_INSTALL_FILE_etc_sysconfig_named} \
	${SYS_INSTALL_FILE_var_named_chroot_etc_named_conf} \
	${SYS_INSTALL_FILE_var_named_chroot_etc_named_forwarders_conf} \
	${SYS_INSTALL_FILE_var_named_chroot_etc_rndc_key} \
	${SYS_CREATE_SYMLINK_etc_named_conf} \
	${SYS_CREATE_SYMLINK_etc_named_forwarders_conf} \
	${SYS_CREATE_SYMLINK_etc_rndc_key} \
	${SYS_INSTALL_FILE_clear_ftp} \
	${SYS_INSTALL_FILE_clear_ftp_file} \
	${SYS_INSTALL_FILE_adaptec_log_sh} \
	${SYS_INSTALL_FILE_etc_rc_d_init_d_ipmi} \
	${SYS_INSTALL_FILE_etc_rc_d_init_d_ipmievd} \
	${SYS_INSTALL_FILE_etc_rc_d_init_d_irqbalance} \
	${SYS_INSTALL_FILE_etc_rc_d_init_d_kdump} \
	${SYS_INSTALL_FILE_etc_sysconfig_kdump} \
	${SYS_INSTALL_FILE_etc_kdump_conf} \
	${SYS_INSTALL_FILE_etc_sysconfig_clock} \
	${SYS_INSTALL_FILE_etc_rssh_conf} \
	${SYS_INSTALL_FILE_opt_nkn_bin_mkchroot_sh} \
	${SYS_INSTALL_FILE_logCleanup_py} \
	${SYS_INSTALL_FILE_mdreq_py} \
	${SYS_CREATE_SYMLINK_logCleanup_crontab} \
	${SYS_INSTALL_FILE_rc_d_rc_local} \
	${SYS_INSTALL_FILE_logPurge_py} \
	${SYS_INSTALL_FILE_config_hdlr_py} \
	${SYS_INSTALL_FILE_OCRP_asset_map_load} \
	${SYS_INSTALL_FILE_OCRP_asset_map_update} \
	${SYS_INSTALL_FILE_OCRP_get_asset_map} \
	${SYS_INSTALL_FILE_OCRP_get_status} \
	${SYS_INSTALL_FILE_upgrade_firmware_generic} \
	${SYS_INSTALL_FILE_s3origin_auth_py} \
	${SYS_INSTALL_FILE_pacifica_reboot} \
	${SYS_INSTALL_FILE_pacifica_shutdown} \
	${SYS_INSTALL_FILE_pacifica_resiliency} \


CLEANFILES= \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/cert/root.pem \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/cert/key.pem \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/nkn.conf.default \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/nkn.ucflt.conf \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/nknlogd.cfg \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/log_analyzer.conf \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/default_rule.tcl \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/tech-support.cfg \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/nkn/plugins/pyVpePublish.py \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/nkn/plugins/pyGenericTranscode.py \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/nkn/plugins/pyYoutubeTranscode.py \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/nkn/plugins/pyHashCheck.py \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/mfp_pmf_schema.xsd \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/mfp.filed.conf.default \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/mfp.lived.conf.default \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/lfd.conf.default \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/lf_metrics_presentation.xsd \
    ${FULL_INSTALL_DIR}/etc/sysconfig/network \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/eth-reorders \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/firstboot_action \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/fixups \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/fixups.sh \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/delete_unused_drives_from_TMdb \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/delete_unused_drives_from_TMdb.sh \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/fuse \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/initdisks \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/raid_monitor \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/mfd_setup \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/dmidecode-info.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/eth-info.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/eth-reorder.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/eth-device-from-http.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/ethflash \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/empty_diskinfo_from_db.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/initdisks.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/raid_monitor.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/nvsd_init.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/disks_common.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/reinit_config_disks.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/write_diskinfo_to_db.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mfd_setup.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/irqpin_ixgbe.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/initcache_dm2_ext3.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/initcache_ps_xfs.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/delete_ps_disk.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/initrootcache_dm2_ext3.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/upgrade_ssd.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/scrub_mfd.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/dump_uri_list.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_add.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_add_vpe.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_convert.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_del.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_du.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_evict.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/disk_auto_enable.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/evictd_common.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_evictd_ssd.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_evictd_sata.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_evictd_sas.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_meta_evict.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/evict_metadata.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_ls.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_mgmt_ls.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_mgmt_del.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_slow_del.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_v1_to_v2.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_disk_lib.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cache_disk_lib.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mgmt_lib.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mount_cache.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/update_grub_console.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/umount_cache.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/cmm_op.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mgmt_build_ns_object_list.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/nkn_sys_mon.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/nkn_sys_mon.cron \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/ingest \
    ${FULL_INSTALL_DIR}/etc/ftpusers \
    ${FULL_INSTALL_DIR}/etc/pam.d/pure-ftpd \
    ${FULL_INSTALL_DIR}/etc/profile.d/StorMan.sh \
    ${FULL_INSTALL_DIR}/usr/local/bin/df-all \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/prestage \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/tech-support.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/support/* \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/intfstats \
    ${FULL_INSTALL_DIR}/etc/ld.so.conf.d/nkn-x86_64.conf \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mem.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/lib/libnvsd_memalloc.so.1 \
    ${FULL_INSTALL_DIR}/opt/nkn/lib/libnvsd_memalloc_lite.so.1 \
    ${FULL_INSTALL_DIR}/opt/nkn/lib/libnvsd_memalloc_cmm.so.1 \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/container_dump.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mgmt_namespace_cleanup.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/fetch_smap \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/fetch_uf_list \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/update_cachetype.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/amrank_dump.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/max_arp_entry.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/tproxy_setup \
    ${FULL_INSTALL_DIR}/opt/nkn/debugbin/debug.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/debugbin/glob_memalloc.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/debugbin/mdreq_showdisk.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/debugbin/pacifica_ndiag.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/debugbin/Pacifica_SATADIMM_table_16 \
    ${FULL_INSTALL_DIR}/opt/nkn/debugbin/sandforce_smartctl.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/debugbin/slabinfo.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/debugbin/delete_one_drive_from_TMdb.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/generic_cli_bin/verify_config.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/generic_cli_bin/getdisk_for_container.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/generic_cli_bin/findslottodcno.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/generic_cli_bin/detect_hdd.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/postaccesslog.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/pyAnkeenaProbe.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/proc_config.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/collect_nkncnt_data.sh \
    ${FULL_INSTALL_DIR}/boot/initrd.img \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/top5_proc.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/load_ct_values.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/disk_metadata.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/dump_del.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/get_interface_txqueuelen.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/set_interface_txqueuelen.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/remove_iptables.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/raid_events.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mirror_resync.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mirror_status.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/set_syncookie.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/policy/jmfc_sample.tcl \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/policy/pe_new.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/policy/pe_edit.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/policy/pe_com.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/policy/pe_del.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/policy/pe_revert.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/policy/pe_files.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/fix-fallback.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/tcp_params.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/sftp.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/prefetch.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mgmt_nfs_mount.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mgmt_nfs_umount.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mgmt_nfs_mountstat.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/whitelist \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/generic_shell_cmd.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/tproxy_tcp_timewait.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/postencode.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/drop_cache.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/drop_cache.awk  \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/nkncntdump.sh \
    ${FULL_INSTALL_DIR}/etc/modprobe.d/nkn.conf \
    ${FULL_INSTALL_DIR}/etc/modprobe_urif.conf \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/startdmi.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/ssld_verify_cert.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/cert/default \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/rpcbind \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/named \
    ${FULL_INSTALL_DIR}/etc/sysconfig/named \
    ${FULL_INSTALL_DIR}/var/named/chroot/etc/named.conf \
    ${FULL_INSTALL_DIR}/var/named/chroot/etc/named-forwarders.conf \
    ${FULL_INSTALL_DIR}/var/named/chroot/etc/rndc.key \
    ${FULL_INSTALL_DIR}/etc/named.conf \
    ${FULL_INSTALL_DIR}/etc/named-forwarders.conf \
    ${FULL_INSTALL_DIR}/etc/rndc.key \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/clear_ftp.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/clear_ftp_file.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/adaptec_log.sh \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/ipmi \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/ipmievd \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/irqbalance \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/pxy_toggle.sh \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/kdump \
    ${FULL_INSTALL_DIR}/etc/sysconfig/kdump \
    ${FULL_INSTALL_DIR}/etc/kdump.conf \
    ${FULL_INSTALL_DIR}/etc/sysconfig/clock \
    ${FULL_INSTALL_DIR}/etc/rssh.conf \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mkchroot.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/logCleanup.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/mdreq.py \
    ${FULL_INSTALL_DIR}/var/opt/tms/output/cron.d/logCleanup \
    ${FULL_INSTALL_DIR}/etc/rc.d/rc.local \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/logPurge.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/config_file_hdlr.py \
    ${FULL_INSTALL_DIR}/etc/rc.d/init.d/nvsdmon \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/nvsdmon.sh \
    ${FULL_INSTALL_DIR}/opt/tms/lib/web/cgi-bin/protocol/ocrp/asset-map/load \
    ${FULL_INSTALL_DIR}/opt/tms/lib/web/cgi-bin/protocol/ocrp/asset-map/update \
    ${FULL_INSTALL_DIR}/opt/tms/lib/web/cgi-bin/protocol/ocrp/get-asset-map \
    ${FULL_INSTALL_DIR}/opt/tms/lib/web/cgi-bin/protocol/ocrp/get-status \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/upgrade-firmware-generic.sh \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/s3origin_auth.py \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/reboot.pacifica \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/shutdown.pacifica \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/pacifica_resiliency.py \


# Mount point for our custom filesystems.  If these are not
# in the image file the Samara system will not mount them!!!
SYS_INSTALL_DIR_coredump="dir 0755 root root /coredump"
SYS_INSTALL_DIR_log="dir 0755 root root /log"
SYS_INSTALL_DIR_nkn="dir 0755 root root /nkn"
# Directories under /nkn /log and /config are created as needed
# every time the system is booted by init.d/fixups.sh or by the
# script that runs on first-boot after installation/upgrade.
# E.g. /nkn/mnt/ /nkn/plugins/ /nkn/tmp/ /nkn/vpe/
# E.g. /config/nkn/ /config/mnts/
# E.g. /log/varlog/nkn /log/varlog/nkn/cache/


SYS_INSTALL_DIR_usr="dir 0755 root root /usr"
SYS_INSTALL_DIR_usr_bin="dir 0755 root root /usr/bin"
SYS_INSTALL_DIR_opt_nkn="dir 0755 root root /opt/nkn"
SYS_INSTALL_DIR_opt_nkn_bin="dir 0755 root root /opt/nkn/bin"
SYS_INSTALL_DIR_opt_nkn_bin_policy="dir 0755 root root /opt/nkn/bin/policy"
SYS_INSTALL_DIR_opt_nkn_lib="dir 0755 root root /opt/nkn/lib"
SYS_INSTALL_DIR_opt_nkn_lib_lite="dir 0755 root root /opt/nkn/lib-lite"
SYS_INSTALL_DIR_opt_nkn_debugbin="dir 0755 root root /opt/nkn/debugbin"
SYS_INSTALL_DIR_opt_nkn_generic_cli_bin="dir 0755 root root /opt/nkn/generic_cli_bin"

# Files under /opt/nkn/installthis get installed into other filesystems
# at first boot time by customer.sh firstboot_graft_1()
# E.g.  /config/nkn/*.cfg  /nkn/plugins/*.py
SYS_INSTALL_DIR_opt_nkn_installthis="dir 0755 root root /opt/nkn/installthis"
SYS_INSTALL_DIR_opt_nkn_installthis_config="dir 0755 root root /opt/nkn/installthis/config"
SYS_INSTALL_DIR_opt_nkn_installthis_config_nkn="dir 0755 root root /opt/nkn/installthis/config/nkn"
SYS_INSTALL_DIR_opt_nkn_installthis_config_nkn_cert="dir 0755 root root /opt/nkn/installthis/config/nkn/cert"
SYS_INSTALL_DIR_opt_nkn_installthis_config_nkn_ca="dir 0755 root root /opt/nkn/installthis/config/nkn/ca"
SYS_INSTALL_DIR_opt_nkn_installthis_config_nkn_key="dir 0755 root root /opt/nkn/installthis/config/nkn/key"
SYS_INSTALL_DIR_opt_nkn_installthis_config_nkn_csr="dir 0755 root root /opt/nkn/installthis/config/nkn/csr"

SYS_INSTALL_DIR_opt_nkn_installthis_nkn="dir 0755 root root /opt/nkn/installthis/nkn"
SYS_INSTALL_DIR_opt_nkn_installthis_nkn_plugins="dir 0755 root root /opt/nkn/installthis/nkn/plugins"

SYS_INSTALL_DIR_etc_sysconfig="dir 0755 root root /etc/sysconfig"
SYS_INSTALL_DIR_etc_rc_d_init_d="dir 0755 root root /etc/rc.d/init.d"
SYS_INSTALL_DIR_etc_pam_d="dir 0755 root root /etc/pam.d"
SYS_INSTALL_DIR_etc_ld_so_conf_d="dir 0755 root root /etc/ld.so.conf.d"
SYS_INSTALL_DIR_etc_profile_d="dir 0755 root root /etc/profile.d"
SYS_INSTALL_DIR_etc_modprobe_d="dir 0755 root root /etc/modprobe.d"

SYS_INSTALL_DIR_usr_local_bin="dir 0755 root root /usr/local/bin"

# The /home* directories are for developers as an nfs mount points.
SYS_INSTALL_DIR_home="dir 0755 root root /home"
SYS_INSTALL_DIR_home2="dir 0755 root root /home2"
SYS_INSTALL_DIR_homes="dir 0755 root root /homes"
# The /bugbits directory is for developers as an nfs mount point.
SYS_INSTALL_DIR_bugbits="dir 0755 root root /bugbits"
# The /nokeena-utils directory is for developers as an nfs mount point.
SYS_INSTALL_DIR_nokeena_utils="dir 0755 root root /nokeena-utils"
# The /eng directory is for developers as an nfs mount point.
SYS_INSTALL_DIR_eng="dir 0755 root root /eng"
# The /boot directory
SYS_INSTALL_DIR_boot="dir 0755 root root /boot"

# The /opt/nkn/support directory is for scripts that generate/append log
# when a tech-support command is called.
SYS_INSTALL_DIR_opt_nkn_support="dir 0755 root root /opt/nkn/support"

# cgi_bin protocol directories 
SYS_INSTALL_DIR_opt_tms_lib_web_cgi_bin_protocol="dir 0755 root root /opt/tms/lib/web/cgi-bin/protocol"

# cgi_bin OCRP protocol directories and files
SYS_INSTALL_DIR_nkn_cb="dir 0755 root root /nkn/cb"
SYS_INSTALL_DIR_nkn_tmp_ocrp="dir 0755 root root /nkn/tmp/ocrp"
SYS_INSTALL_DIR_cgi_bin_ocrp_asset_map="dir 0755 root root /opt/tms/lib/web/cgi-bin/protocol/ocrp/asset-map"

# cron.d dirs where run-time generated crontab files are created as well as symlinks during build
SYS_INSTALL_DIR_var_opt="dir 0755 root root /var/opt"
SYS_INSTALL_DIR_var_opt_tms="dir 0755 root root /var/opt/tms"
SYS_INSTALL_DIR_var_opt_tms_output="dir 0755 root root /var/opt/tms/output"
SYS_INSTALL_DIR_var_opt_tms_output_cron_d="dir 0755 root root /var/opt/tms/output/cron.d"

SYS_INSTALL_FILE_OCRP_asset_map_load="file ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/bin/OCRP_cgi/OCRP_cgi 06755 root root /opt/tms/lib/web/cgi-bin/protocol/ocrp/asset-map/load"

SYS_INSTALL_FILE_OCRP_asset_map_update="file ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/bin/OCRP_cgi/OCRP_cgi 06755 root root /opt/tms/lib/web/cgi-bin/protocol/ocrp/asset-map/update"

SYS_INSTALL_FILE_OCRP_get_asset_map="file ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/bin/OCRP_cgi/OCRP_cgi 06755 root root /opt/tms/lib/web/cgi-bin/protocol/ocrp/get-asset-map"

SYS_INSTALL_FILE_OCRP_get_status="file ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/bin/OCRP_cgi/OCRP_cgi 06755 root root /opt/tms/lib/web/cgi-bin/protocol/ocrp/get-status"

SYS_INSTALL_FILE_installthis_key_pem="file key.pem 0755 root root /opt/nkn/installthis/config/nkn/cert/key.pem"
SYS_INSTALL_FILE_installthis_root_pem="file root.pem 0755 root root /opt/nkn/installthis/config/nkn/cert/root.pem"
# These two configuration files will eventually be going away, as the functionality
# is moved into the standard Samara configuration DB.
SYS_INSTALL_FILE_installthis_nkn_conf_default="file nkn.conf.default 0644 root root /opt/nkn/installthis/config/nkn/nkn.conf.default"
SYS_INSTALL_FILE_installthis_nkn_ucflt_conf="file nkn.ucflt.conf 0644 root root /opt/nkn/installthis/config/nkn/nkn.ucflt.conf"
SYS_INSTALL_FILE_installthis_nkn_nknlogd_cfg="file  nknlogd.cfg      0644 root root /opt/nkn/installthis/config/nkn/nknlogd.cfg"
SYS_INSTALL_FILE_installthis_log_analyzer_conf="file log_analyzer.conf 0644 root root /opt/nkn/installthis/config/nkn/log_analyzer.conf"

SYS_INSTALL_FILE_installthis_defaule_rule_tcl="file  default_rule.tcl      0644 root root /opt/nkn/installthis/config/nkn/default_rule.tcl"

SYS_INSTALL_FILE_etc_sysconfig_network="file network  0644 root root /etc/sysconfig/network"

# To activate the files in init.d, edit base_os/common/script_files/customer.sh to add "chkconfig --add <filename>"
SYS_INSTALL_FILE_eth_reorder="file    init.d/eth-reorder    0755 root root /etc/rc.d/init.d/eth-reorder"
SYS_INSTALL_FILE_eth_reorder_sh="file init.d/eth-reorder.sh 0755 root root /opt/nkn/bin/eth-reorder.sh"
SYS_INSTALL_FILE_eth_device_from_http_sh="file init.d/eth-device-from-http.sh 0755 root root /opt/nkn/bin/eth-device-from-http.sh"
SYS_INSTALL_FILE_ethflash="file       ethflash              0755 root root /opt/nkn/bin/ethflash"
SYS_INSTALL_FILE_firstboot_action="file init.d/firstboot_action 0755 root root /etc/rc.d/init.d/firstboot_action"
SYS_INSTALL_FILE_fixups="file         init.d/fixups         0755 root root /etc/rc.d/init.d/fixups"
SYS_INSTALL_FILE_fixups_sh="file      init.d/fixups.sh      0755 root root /etc/rc.d/init.d/fixups.sh"
SYS_INSTALL_FILE_delete_unused_drives_from_TMdb="file      init.d/delete_unused_drives_from_TMdb      0755 root root /etc/rc.d/init.d/delete_unused_drives_from_TMdb"
SYS_INSTALL_FILE_delete_unused_drives_from_TMdb_sh="file      init.d/delete_unused_drives_from_TMdb.sh      0755 root root /etc/rc.d/init.d/delete_unused_drives_from_TMdb.sh"
SYS_INSTALL_FILE_fuse="file           init.d/fuse           0755 root root /etc/rc.d/init.d/fuse"
SYS_INSTALL_FILE_initdisks="file      init.d/initdisks      0755 root root /etc/rc.d/init.d/initdisks"
SYS_INSTALL_FILE_initdisks_sh="file   init.d/initdisks.sh   0755 root root /opt/nkn/bin/initdisks.sh"
SYS_INSTALL_FILE_raid_monitor="file      init.d/raid_monitor      0755 root root /etc/rc.d/init.d/raid_monitor"
SYS_INSTALL_FILE_raid_monitor_sh="file   init.d/raid_monitor.sh   0755 root root /opt/nkn/bin/raid_monitor.sh"
SYS_INSTALL_FILE_nvsd_init_sh="file   nvsd_init.sh   0755 root root /opt/nkn/bin/nvsd_init.sh"
SYS_INSTALL_FILE_mfd_setup="file      init.d/mfd_setup      0755 root root /etc/rc.d/init.d/mfd_setup"
SYS_INSTALL_FILE_mfd_setup_sh="file   init.d/mfd_setup.sh   0755 root root /opt/nkn/bin/mfd_setup.sh"
SYS_INSTALL_FILE_irqpin_ixgbe_py="file init.d/irqpin_ixgbe.py 0755 root root /opt/nkn/bin/irqpin_ixgbe.py"
SYS_INSTALL_FILE_set_irq_affinity_sh="file init.d/set_irq_affinity.sh 0755 root root /opt/nkn/bin/set_irq_affinity.sh"
SYS_INSTALL_FILE_nvsdmon="file      init.d/nvsdmon      0755 root root /etc/rc.d/init.d/nvsdmon"
SYS_INSTALL_FILE_nvsdmon_sh="file   init.d/nvsdmon.sh   0755 root root /opt/nkn/bin/nvsdmon.sh"

SYS_INSTALL_FILE_dmidecode_info_sh="file dmidecode-info.sh 0755 root root /opt/nkn/bin/dmidecode-info.sh"
SYS_INSTALL_FILE_eth_info_sh="file       eth-info.sh       0755 root root /opt/nkn/bin/eth-info.sh"

SYS_INSTALL_FILE_installthis_generate_change_lists="file generate_change_lists.pl 0755 root root /opt/nkn/bin/generate_change_lists.pl"
SYS_INSTALL_FILE_installthis_generate_aws_file_list="file generate_aws_file_list.py 0755 root root /opt/nkn/bin/generate_aws_file_list.py"

SYS_INSTALL_FILE_empty_diskinfo_from_db_sh="file empty_diskinfo_from_db.sh 0755 root root /opt/nkn/bin/empty_diskinfo_from_db.sh"
SYS_INSTALL_FILE_reinit_config_disks_sh="file    reinit_config_disks.sh    0755 root root /opt/nkn/bin/reinit_config_disks.sh"
SYS_INSTALL_FILE_write_diskinfo_to_db_sh="file   write_diskinfo_to_db.sh   0755 root root /opt/nkn/bin/write_diskinfo_to_db.sh"
SYS_INSTALL_FILE_get_smart_data_sh="file   get_smart_data.sh   0755 root root /opt/nkn/bin/get_smart_data.sh"
SYS_INSTALL_FILE_disks_common_sh="file   disks_common.sh   0755 root root /opt/nkn/bin/disks_common.sh"

SYS_INSTALL_FILE_IC_dm2_ext3_sh="file  initcache_dm2_ext3.sh     0755 root root /opt/nkn/bin/initcache_dm2_ext3.sh"
SYS_INSTALL_FILE_IRC_dm2_ext3_sh="file initrootcache_dm2_ext3.sh 0755 root root /opt/nkn/bin/initrootcache_dm2_ext3.sh"
SYS_INSTALL_FILE_IC_ps_xfs_sh="file  initcache_ps_xfs.sh     0755 root root /opt/nkn/bin/initcache_ps_xfs.sh"
SYS_INSTALL_FILE_IC_del_ps_disk_sh="file  delete_ps_disk.sh     0755 root root /opt/nkn/bin/delete_ps_disk.sh"
SYS_INSTALL_FILE_upgrade_ssd_sh="file upgrade_ssd.sh 0755 root root /opt/nkn/bin/upgrade_ssd.sh"
SYS_INSTALL_FILE_scrub_mfd_sh="file    scrub_mfd.sh              0755 root root /opt/nkn/bin/scrub_mfd.sh"

SYS_INSTALL_FILE_dump_uri_list_sh="file  dump_uri_list.sh    0755 root root /opt/nkn/bin/dump_uri_list.sh"
SYS_INSTALL_FILE_cache_add_sh="file      cache_add.sh        0755 root root /opt/nkn/bin/cache_add.sh"
SYS_INSTALL_FILE_cache_add_vpe_sh="file  cache_add_vpe.sh    0755 root root /opt/nkn/bin/cache_add_vpe.sh"
SYS_INSTALL_FILE_cache_convert_sh="file  cache_convert.sh    0755 root root /opt/nkn/bin/cache_convert.sh"
SYS_INSTALL_FILE_cache_del_sh="file      cache_del.sh        0755 root root /opt/nkn/bin/cache_del.sh"
SYS_INSTALL_FILE_cache_du_sh="file       cache_du.sh         0755 root root /opt/nkn/bin/cache_du.sh"
SYS_INSTALL_FILE_cache_evict_sh="file    cache_evict.sh      0755 root root /opt/nkn/bin/cache_evict.sh"
SYS_INSTALL_FILE_disk_auto_enable_sh="file   disk_auto_enable.sh     0755 root root /opt/nkn/bin/disk_auto_enable.sh"
SYS_INSTALL_FILE_evictd_common_sh="file   evictd_common.sh     0755 root root /opt/nkn/bin/evictd_common.sh"
SYS_INSTALL_FILE_cache_evictd_ssd_sh="file   cache_evictd_ssd.sh     0755 root root /opt/nkn/bin/cache_evictd_ssd.sh"
SYS_INSTALL_FILE_cache_evictd_sata_sh="file   cache_evictd_sata.sh     0755 root root /opt/nkn/bin/cache_evictd_sata.sh"
SYS_INSTALL_FILE_cache_evictd_sas_sh="file   cache_evictd_sas.sh     0755 root root /opt/nkn/bin/cache_evictd_sas.sh"
SYS_INSTALL_FILE_cache_meta_evictd_sh="file cache_meta_evictd.sh    0755    root root /opt/nkn/bin/cache_meta_evictd.sh"
SYS_INSTALL_FILE_evict_metadata_py="file evict_metadata.py   0755 root root /opt/nkn/bin/evict_metadata.py"
SYS_INSTALL_FILE_cache_ls_sh="file       cache_ls.sh         0755 root root /opt/nkn/bin/cache_ls.sh"
SYS_INSTALL_FILE_cache_mgmt_ls_py="file  cache_mgmt_ls.py    0755 root root /opt/nkn/bin/cache_mgmt_ls.py"
SYS_INSTALL_FILE_cache_mgmt_del_py="file cache_mgmt_del.py   0755 root root /opt/nkn/bin/cache_mgmt_del.py"
SYS_INSTALL_FILE_cache_slow_del_py="file cache_slow_del.py   0755 root root /opt/nkn/bin/cache_slow_del.py"
SYS_INSTALL_FILE_cache_v1_to_v2_sh="file cache_v1_to_v2.sh   0755 root root /opt/nkn/bin/cache_v1_to_v2.sh"
SYS_INSTALL_FILE_cache_disk_lib_sh="file cache_disk_lib.sh   0755 root root /opt/nkn/bin/cache_disk_lib.sh"
SYS_INSTALL_FILE_cache_disk_lib_sh="file cache_disk_lib.py   0755 root root /opt/nkn/bin/cache_disk_lib.py"
SYS_INSTALL_FILE_cache_mgmt_lib_py="file mgmt_lib.py   0755 root root /opt/nkn/bin/mgmt_lib.py"
SYS_INSTALL_FILE_mount_cache_sh="file mount_cache.sh   0755 root root /opt/nkn/bin/mount_cache.sh"
SYS_INSTALL_FILE_update_grub_console_sh="file init.d/update_grub_console.sh   0755 root root /opt/nkn/bin/update_grub_console.sh"
SYS_INSTALL_FILE_umount_cache_sh="file umount_cache.sh   0755 root root /opt/nkn/bin/umount_cache.sh"
SYS_INSTALL_FILE_max_arp_entry_sh="file	 max_arp_entry.sh	0755 root root /opt/nkn/bin/max_arp_entry.sh"
SYS_INSTALL_FILE_cmm_op_sh="file cmm_op.sh   0755 root root /opt/nkn/bin/cmm_op.sh"
SYS_INSTALL_FILE_proc_config_sh="file	 proc_config.sh	0755 root root /opt/nkn/bin/proc_config.sh"
SYS_INSTALL_FILE_top5_proc_sh="file top5_proc.sh 0755 root root /opt/nkn/bin/top5_proc.sh"
SYS_INSTALL_FILE_disk_metadata_proc_sh="file disk_metadata.sh 0755 root root /opt/nkn/bin/disk_metadata.sh"

SYS_INSTALL_FILE_mgmt_build_ns_object_list_py="file mgmt_build_ns_object_list.py   0755 root root /opt/nkn/bin/mgmt_build_ns_object_list.py"

SYS_INSTALL_FILE_nkn_sys_mon_sh="file nkn_sys_mon.sh 0755 root root /opt/nkn/bin/nkn_sys_mon.sh"
SYS_INSTALL_FILE_nkn_sys_mon_cron="file nkn_sys_mon.cron 0644 root root /opt/nkn/bin/nkn_sys_mon.cron"

SYS_INSTALL_FILE_ingest="file          ingest          0755 root root /opt/nkn/bin/ingest"
SYS_INSTALL_FILE_ftpusers="file        ftpusers        0644 root root /etc/ftpusers"
SYS_INSTALL_FILE_pureftpd="file        pure-ftpd       0755 root root /etc/pam.d/pure-ftpd"
SYS_INSTALL_FILE_storman_sh="file StorMan.sh   0644 root root /etc/profile.d/StorMan.sh"

#Policy Engine
SYS_INSTALL_FILE_jmfc_sample_tcl="file jmfc_sample.tcl   0755 root root /opt/nkn/bin/policy/jmfc_sample.tcl"
SYS_INSTALL_FILE_pe_new_sh="file pe_new.sh   0755 root root /opt/nkn/bin/policy/pe_new.sh"
SYS_INSTALL_FILE_pe_edit_sh="file pe_edit.sh   0755 root root /opt/nkn/bin/policy/pe_edit.sh"
SYS_INSTALL_FILE_pe_com_sh="file pe_com.sh   0755 root root /opt/nkn/bin/policy/pe_com.sh"
SYS_INSTALL_FILE_pe_del_sh="file pe_del.sh   0755 root root /opt/nkn/bin/policy/pe_del.sh"
SYS_INSTALL_FILE_pe_revert_sh="file pe_revert.sh   0755 root root /opt/nkn/bin/policy/pe_revert.sh"
SYS_INSTALL_FILE_pe_file_sh="file pe_files.sh   0755 root root /opt/nkn/bin/policy/pe_files.sh"


SYS_INSTALL_FILE_df_all="file          df-all          0755 root root /usr/local/bin/df-all"

SYS_INSTALL_FILE_installthis_pyHashCheck="file     pyHashCheck.py  0755 root root /opt/nkn/installthis/nkn/plugins/pyHashCheck.py"
SYS_INSTALL_FILE_installthis_pyVpePublish="file    pyVpePublish.py 0755 root root /opt/nkn/installthis/nkn/plugins/pyVpePublish.py"
SYS_INSTALL_FILE_installthis_pyGenericTranscode="file    pyGenericTranscode.py 0755 root root /opt/nkn/installthis/nkn/plugins/pyGenericTranscode.py"
SYS_INSTALL_FILE_installthis_pyYoutubeTranscode="file    pyYoutubeTranscode.py 0755 root root /opt/nkn/installthis/nkn/plugins/pyYoutubeTranscode.py"


SYS_INSTALL_FILE_prestage="file        prestage        0755 root root /opt/nkn/bin/prestage"

SYS_INSTALL_FILE_intfstats="file        intfstats        0755 root root /opt/nkn/bin/intfstats"

# For supporting module level logs in tech-support command
SYS_INSTALL_FILE_installthis_tech_support_cfg="file    tech-support.cfg  0644 root root /opt/nkn/installthis/config/nkn/tech-support.cfg"
SYS_INSTALL_FILE_tech_support_sh="file     tech-support.sh   0755 root root /opt/nkn/bin/tech-support.sh"

# Add new tech-support scripts (module level) here
SYS_INSTALL_FILE_cli_log_sh="file    support-scripts/cli-log.sh   0755 root root /opt/nkn/support/cli-log.sh"

SYS_INSTALL_FILE_disk_diagnosis_sh="file    support-scripts/disk_diagnosis.sh   0755 root root /opt/nkn/support/disk_diagnosis.sh"

SYS_INSTALL_FILE_lsiutil_debug_info_sh="file    support-scripts/lsiutil_debug_info.sh   0755 root root /opt/nkn/support/lsiutil_debug_info.sh"

# Add the file to be added to ld.so.conf.d
SYS_INSTALL_FILE_nkn_x86_64_conf="file    nkn-x86_64.conf   0644 root root /etc/ld.so.conf.d/nkn-x86_64.conf"

SYS_CREATE_SYMLINK_nvsd_memalloc="symlink /opt/nkn/lib /opt/nkn/lib/libnvsd_memalloc.so.1.0.0 libnvsd_memalloc.so.1"

SYS_CREATE_SYMLINK_nvsd_memalloc_lite="symlink /opt/nkn/lib /opt/nkn/lib/libnvsd_memalloc_lite.so.1.0.0 libnvsd_memalloc_lite.so.1"

SYS_CREATE_SYMLINK_nvsd_memalloc_cmm="symlink /opt/nkn/lib /opt/nkn/lib/libnvsd_memalloc_cmm.so.1.0.0 libnvsd_memalloc_cmm.so.1"

SYS_CREATE_SYMLINK_nvsd_memalloc_lib_lite="symlink /opt/nkn/lib-lite /opt/nkn/lib/libnvsd_memalloc_lite.so.1.0.0 libnvsd_memalloc.so.1"



SYS_INSTALL_FILE_mem_sh="file       mem.sh 0755 root root /opt/nkn/bin/mem.sh"

SYS_INSTALL_FILE_dm_ext_sh="file    support-scripts/dm_ext.sh   0755 root root /opt/nkn/support/dm_ext.sh"
SYS_INSTALL_FILE_container_dump_py="file container_dump.py 0755 root root /opt/nkn/bin/container_dump.py"
SYS_INSTALL_FILE_container_bkp_py="file container_bkp.py 0755 root root /opt/nkn/bin/container_bkp.py"

SYS_INSTALL_FILE_mgmt_namespace_cleanup_sh="file     mgmt_namespace_cleanup.sh     0755 root root /opt/nkn/bin/mgmt_namespace_cleanup.sh"
SYS_INSTALL_FILE_fetch_smap="file     fetch_smap     0755 root root /opt/nkn/bin/fetch_smap"
SYS_INSTALL_FILE_fetch_uf_list="file     fetch_uf_list 0755 root root /opt/nkn/bin/fetch_uf_list"
SYS_INSTALL_FILE_update_cachetype_sh="file     update_cachetype.sh     0755 root root /opt/nkn/bin/update_cachetype.sh"
SYS_INSTALL_FILE_amrank_dump_sh="file amrank_dump.sh 0755 root root /opt/nkn/bin/amrank_dump.sh"
SYS_INSTALL_FILE_tproxy_setup="file     tproxy_setup 0755 root root /opt/nkn/bin/tproxy_setup"
SYS_INSTALL_FILE_postaccesslog_sh="file postaccesslog.sh 0755 root root /opt/nkn/bin/postaccesslog.sh"
SYS_INSTALL_FILE_ankeena_probe_py="file pyAnkeenaProbe.py 0755 root root /opt/nkn/bin/pyAnkeenaProbe.py"
SYS_INSTALL_FILE_collect_nkncnt_data_sh="file  collect_nkncnt_data.sh    0755 root root /opt/nkn/bin/collect_nkncnt_data.sh"

ifeq (${KERNEL_USE_VER},2.6.32-el6)
	SYS_INSTALL_FILE_initrd_img="file initrd.img-2.6.32-el6 0644 root root /boot/initrd.img"
else
	SYS_INSTALL_FILE_initrd_img="file initrd.img 0644 root root /boot/initrd.img"
endif

SYS_INSTALL_FILE_dump_del_py="file dump_del.py 0755 root root /opt/nkn/bin/dump_del.py"
SYS_INSTALL_FILE_set_syncookie_sh="file set_syncookie.sh 0755 root root /opt/nkn/bin/set_syncookie.sh"
SYS_INSTALL_FILE_fix_fallback_sh="file fix-fallback.sh 0755 root root /opt/nkn/bin/fix-fallback.sh"
SYS_INSTALL_FILE_tcp_params_sh="file tcp_params.sh 0755 root root /opt/nkn/bin/tcp_params.sh"
SYS_INSTALL_FILE_sftp_sh="file sftp.sh 0755 root root /opt/nkn/bin/sftp.sh"
SYS_INSTALL_FILE_prefetch_py="file prefetch.py 0755 root root /opt/nkn/bin/prefetch.py"
SYS_INSTALL_FILE_mgmt_nfs_mount_sh="file mgmt_nfs_mount.sh 0755 root root /opt/nkn/bin/mgmt_nfs_mount.sh"
SYS_INSTALL_FILE_mgmt_nfs_umount_sh="file mgmt_nfs_umount.sh 0755 root root /opt/nkn/bin/mgmt_nfs_umount.sh"
SYS_INSTALL_FILE_mgmt_nfs_mountstat_sh="file mgmt_nfs_mountstat.sh 0755 root root /opt/nkn/bin/mgmt_nfs_mountstat.sh"
SYS_INSTALL_FILE_whitelist="file whitelist 0755 root root /opt/nkn/bin/whitelist"
SYS_INSTALL_FILE_generic_shell_cmd_py="file generic_shell_cmd.py 0755 root root /opt/nkn/bin/generic_shell_cmd.py"

SYS_INSTALL_FILE_debugbin_debug_sh="file debugbin/debug.sh 0755 root root /opt/nkn/debugbin/debug.sh"
SYS_INSTALL_FILE_debugbin_glob_memalloc_sh="file debugbin/glob_memalloc.sh 0755 root root /opt/nkn/debugbin/glob_memalloc.sh"
SYS_INSTALL_FILE_debugbin_mdreq_showdisk_sh="file debugbin/mdreq_showdisk.sh 0755 root root /opt/nkn/debugbin/mdreq_showdisk.sh"
SYS_INSTALL_FILE_debugbin_pacifica_ndiag_sh="file debugbin/pacifica_ndiag.sh 0755 root root /opt/nkn/debugbin/pacifica_ndiag.sh"
SYS_INSTALL_FILE_debugbin_pacifica_satadimm_table_16="file debugbin/Pacifica_SATADIMM_table_16 0444 root root /opt/nkn/debugbin/Pacifica_SATADIMM_table_16"
SYS_INSTALL_FILE_debugbin_sandforce_smartctl_sh="file debugbin/sandforce_smartctl.sh 0755 root root /opt/nkn/debugbin/sandforce_smartctl.sh"
SYS_INSTALL_FILE_debugbin_slabinfo_sh="file debugbin/slabinfo.sh 0755 root root /opt/nkn/debugbin/slabinfo.sh"
SYS_INSTALL_FILE_debugbin_delete_one_drive_from_TMdb_sh="file debugbin/delete_one_drive_from_TMdb.sh 0755 root root /opt/nkn/debugbin/delete_one_drive_from_TMdb.sh"

SYS_INSTALL_FILE_genericbin_verify_config_sh="file generic_cli_bin/verify_config.sh 0755 root root /opt/nkn/generic_cli_bin/verify_config.sh"
SYS_INSTALL_FILE_genericbin_getdisk_for_container_sh="file generic_cli_bin/getdisk_for_container.sh 0755 root root /opt/nkn/generic_cli_bin/getdisk_for_container.sh"
SYS_INSTALL_FILE_genericbin_findslottodcno_sh="file generic_cli_bin/findslottodcno.sh 0755 root root /opt/nkn/generic_cli_bin/findslottodcno.sh"
SYS_INSTALL_FILE_genericbin_detect_hdd_sh="file generic_cli_bin/detect_hdd.sh 0755 root root /opt/nkn/generic_cli_bin/detect_hdd.sh"

SYS_INSTALL_FILE_load_ct_values_sh="file 	load_ct_values.sh 0755 root root /opt/nkn/bin/load_ct_values.sh"
SYS_INSTALL_FILE_get_interface_txqueuelen_sh="file  get_interface_txqueuelen.sh    0755 root root /opt/nkn/bin/get_interface_txqueuelen.sh"
SYS_INSTALL_FILE_set_interface_txqueuelen_sh="file  set_interface_txqueuelen.sh    0755 root root /opt/nkn/bin/set_interface_txqueuelen.sh"
SYS_INSTALL_FILE_remove_iptables_sh="file  remove_iptables.sh    0755 root root /opt/nkn/bin/remove_iptables.sh"
SYS_INSTALL_FILE_raid_events_sh="file  raid_events.sh    0755 root root /opt/nkn/bin/raid_events.sh"
SYS_INSTALL_FILE_mirror_resync_sh="file  mirror_resync.sh    0755 root root /opt/nkn/bin/mirror_resync.sh"
SYS_INSTALL_FILE_mirror_status_sh="file  mirror_status.sh    0755 root root /opt/nkn/bin/mirror_status.sh"
SYS_INSTALL_FILE_ssld_verify_cert_sh="file ssld_verify_cert.sh    0755 root root /opt/nkn/bin/ssld_verify_cert.sh"

# xsd for MFP manifest validation
SYS_INSTALL_FILE_installthis_mfp_pmf_schema_xsd="file  mfp_pmf_schema.xsd      0644 root root /opt/nkn/installthis/config/nkn/mfp_pmf_schema.xsd"
SYS_INSTALL_FILE_installthis_mfp_filed_conf_default="file mfp.filed.conf.default 0644 root root /opt/nkn/installthis/config/nkn/mfp.filed.conf.default"
SYS_INSTALL_FILE_installthis_mfp_lived_conf_default="file mfp.lived.conf.default 0644 root root /opt/nkn/installthis/config/nkn/mfp.lived.conf.default"
SYS_INSTALL_FILE_installthis_lf_conf_default="file lfd.conf.default 0644 root root /opt/nkn/installthis/config/nkn/lfd.conf.default"
SYS_INSTALL_FILE_installthis_lf_metrics_presentation_xsd="file  lf_metrics_presentation.xsd      0644 root root /opt/nkn/installthis/config/nkn/lf_metrics_presentation.xsd"
SYS_INSTALL_FILE_tproxy_tcp_timewait_sh="file tproxy_tcp_timewait.sh 0755 root root /opt/nkn/bin/tproxy_tcp_timewait.sh"
SYS_INSTALL_FILE_postencode_sh="file postencode.sh 0755 root root /opt/nkn/bin/postencode.sh"
SYS_INSTALL_FILE_drop_cache_sh="file drop_cache.sh 0755 root root /opt/nkn/bin/drop_cache.sh"
SYS_INSTALL_FILE_drop_cache_awk="file drop_cache.awk 0755 root root /opt/nkn/bin/drop_cache.awk"
SYS_INSTALL_FILE_nkncntdump_sh="file nkncntdump.sh 0755 root root /opt/nkn/bin/nkncntdump.sh"
SYS_INSTALL_FILE_etc_modprobe_d_nkn_conf="file modprobe.conf 0644 root root /etc/modprobe.d/nkn.conf"
SYS_INSTALL_FILE_etc_modprobe_urif_d_nkn_conf="file modprobe_urif.conf 0644 root root /etc/modprobe_urif.conf"
SYS_INSTALL_FILE_wd_gdb_sh="file wd_gdb.sh 0755 root root /opt/nkn/bin/wd_gdb.sh"
SYS_INSTALL_FILE_pxy_tgl_sh="file pxy_toggle.sh 0755 root root /opt/nkn/bin/pxy_toggle.sh"
SYS_INSTALL_FILE_startdmi_sh="file startdmi.sh 0755 root root /opt/nkn/bin/startdmi.sh"

SYS_INSTALL_FILE_clear_ftp="file clear_ftp.sh 0755 root root /opt/nkn/bin/clear_ftp.sh"
SYS_INSTALL_FILE_clear_ftp_file="file clear_ftp_file.sh 0755 root root /opt/nkn/bin/clear_ftp_file.sh"

SYS_INSTALL_FILE_etc_rc_d_init_d_rpcbind="file init.d/rpcbind 0755 root root /etc/rc.d/init.d/rpcbind"

SYS_INSTALL_DIR_var_named="dir 0755 root root /var/named"
SYS_INSTALL_DIR_var_named_chroot="dir 0755 25 25 /var/named/chroot"
SYS_INSTALL_DIR_var_named_chroot_etc="dir 0755 25 25 /var/named/chroot/etc"


SYS_INSTALL_FILE_etc_rc_d_init_d_named="file init.d/named 0755 root root /etc/rc.d/init.d/named"
SYS_INSTALL_FILE_etc_sysconfig_named="file named 644 root root /etc/sysconfig/named"
SYS_INSTALL_FILE_var_named_chroot_etc_named_conf="file named.conf 644 25 25 /var/named/chroot/etc/named.conf"
SYS_INSTALL_FILE_var_named_chroot_etc_named_forwarders_conf="file named-forwarders.conf 644 25 25 /var/named/chroot/etc/named-forwarders.conf"
SYS_INSTALL_FILE_var_named_chroot_etc_rndc_key="file rndc.key 644 25 25 /var/named/chroot/etc/rndc.key"
SYS_CREATE_SYMLINK_etc_named_conf="symlink /etc /var/named/chroot/etc/named.conf named.conf"
SYS_CREATE_SYMLINK_etc_named_forwarders_conf="symlink /etc /var/named/chroot/etc/named-forwarders.conf named-forwarders.conf"
SYS_CREATE_SYMLINK_etc_rndc_key="symlink /etc /var/named/chroot/etc/rndc.key rndc.key"

SYS_INSTALL_FILE_adaptec_log_sh="file adaptec_log.sh 0755 root root /opt/nkn/bin/adaptec_log.sh"

SYS_INSTALL_FILE_etc_rc_d_init_d_ipmi="file init.d/ipmi 0755 root root /etc/rc.d/init.d/ipmi"
SYS_INSTALL_FILE_etc_rc_d_init_d_ipmievd="file init.d/ipmievd 0755 root root /etc/rc.d/init.d/ipmievd"
SYS_INSTALL_FILE_etc_rc_d_init_d_irqbalance="file init.d/irqbalance 0755 root root /etc/rc.d/init.d/irqbalance"

SYS_INSTALL_FILE_etc_sysconfig_kdump="file kdump 644 root root /etc/sysconfig/kdump"
SYS_INSTALL_FILE_etc_kdump_conf="file kdump.conf 644 root root /etc/kdump.conf"

SYS_INSTALL_FILE_etc_sysconfig_clock="file clock 644 root root /etc/sysconfig/clock"

SYS_INSTALL_FILE_etc_rssh_conf="file etc/rssh.conf 644 root root /etc/rssh.conf"

SYS_INSTALL_FILE_opt_nkn_bin_mkchroot_sh="file mkchroot.sh 755 root root /opt/nkn/bin/mkchroot.sh"
SYS_INSTALL_DIR_var_logexport="dir 0755 root root /var/logexport"
SYS_INSTALL_DIR_var_logexport_chroot="dir 0755 root root /var/logexport/chroot"

SYS_INSTALL_FILE_logCleanup_py="file logCleanup.py 0755 root root /opt/nkn/bin/logCleanup.py"
SYS_INSTALL_FILE_mdreq_py="file mdreq.py 0755 root root /opt/nkn/bin/mdreq.py"
SYS_CREATE_SYMLINK_logCleanup_crontab="symlink /var/opt/tms/output/cron.d /var/opt/tms/output/logCleanup logCleanup"
SYS_INSTALL_FILE_rc_d_rc_local="file rc.local 0755 root root /etc/rc.d/rc.local"
SYS_INSTALL_FILE_logPurge_py="file logPurge.py 0755 root root /opt/nkn/bin/logPurge.py"
SYS_INSTALL_FILE_config_hdlr_py="file config_file_hdlr.py 0755 root root /opt/nkn/bin/config_file_hdlr.py"
SYS_INSTALL_FILE_upgrade_firmware_generic="file upgrade-firmware-generic.sh 0755 root root /opt/nkn/bin/upgrade-firmware-generic.sh"
SYS_INSTALL_FILE_s3origin_auth_py="file s3origin_auth.py 0755 root root /opt/nkn/bin/s3origin_auth.py"
SYS_INSTALL_FILE_pacifica_reboot="file reboot.pacifica 0755 root root /opt/nkn/bin/reboot.pacifica"
SYS_INSTALL_FILE_pacifica_shutdown="file shutdown.pacifica 0755 root root /opt/nkn/bin/shutdown.pacifica"
SYS_INSTALL_FILE_pacifica_resiliency="file pacifica_resiliency.py 0755 root root /opt/nkn/bin/pacifica_resiliency.py"


include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

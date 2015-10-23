/*
 *
 * Filename:  md_modules.inc.c
 * Date:      2008/10/22 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */


#ifndef __MD_MODULES_INC_C_
#define __MD_MODULES_INC_C_
#endif /* __MD_MODULES_INC_C_ */


/* 
 * This file is included by src/bin/mgmtd/md_modules.c .  The different
 * blocks are chosen by the caller #define'ing MD_MOD_INC_GRAFT_POINT to the
 * numbers seen below.
*/

/* Graft point 1: declarations */
#if MD_MOD_INC_GRAFT_POINT == 1

int md_nvsd_init(const lc_dso_info *info, void *data);
int md_nvsd_pm_init(const lc_dso_info *info, void *data);
int md_nvsd_l4proxy_init(const lc_dso_info *info, void *data);
int md_proxyd_pm_init(const lc_dso_info *info, void *data);
int md_tmd_pm_init(const lc_dso_info *info, void *data);
int md_ucfltd_pm_init(const lc_dso_info *info, void *data);
int md_bgpd_pm_init(const lc_dso_info *info, void *data);
int md_adnsd_pm_init(const lc_dso_info *info, void *data);
int md_ssld_pm_init(const lc_dso_info *info, void *data);
int md_acclog_pm_init(const lc_dso_info *info, void *data);
int md_errlog_pm_init(const lc_dso_info *info, void *data);
int md_oomgr_pm_init(const lc_dso_info *info, void *data);
int md_nkn_cmm_pm_init(const lc_dso_info *info, void *data);
int md_cache_evictd_pm_init(const lc_dso_info *info, void *data);
int md_mfd_license_init(const lc_dso_info *info, void *data);
int md_nvsd_interface_init(const lc_dso_info *info, void *data);
int md_dashboard_pm_init(const lc_dso_info *info, void *data);
int md_irqpin_ixgbed_pm_init(const lc_dso_info *info, void *data);
int md_junos_re_init(const lc_dso_info *info, void *data);
int md_apply_first_init(const lc_dso_info *info, void *data);
int md_apply_last_init(const lc_dso_info *info, void *data);
#endif /* GRAFT_POINT 1 */

/* Graft point 2: init */
#if MD_MOD_INC_GRAFT_POINT == 2

    err = md_nkn_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_buffermgr_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_mediamgr_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_http_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_diskcache_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_originmgr_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_scheduler_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_fmgr_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_am_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_namespace_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_uri_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_virtual_player_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_origin_fetch_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_service_policy_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_interface_init(NULL, NULL);
    complain_error(err);

    err = md_acclog_init(NULL, NULL);
    complain_error(err);

    err = md_oomgr_init(NULL, NULL);
    complain_error(err);

    err = md_nkn_cmm_init(NULL, NULL);
    complain_error(err);

    err = md_fmgr_init(NULL, NULL);
    complain_error(err);

    err = md_oomgr_pm_init(NULL, NULL);
    complain_error(err);

    err = md_nkn_cmm_pm_init(NULL, NULL);
    complain_error(err);

    err = md_vpemgr_pm_init(NULL, NULL);
    complain_error(err);

    err = md_proxyd_pm_init(NULL, NULL);
    complain_error(err);

    err = md_tmd_pm_init(NULL, NULL);
    complain_error(err);

    err = md_ucfltd_pm_init(NULL, NULL);
    complain_error(err);

    err = md_bgpd_pm_init(NULL, NULL);
    complain_error(err);

    err = md_zebra_pm_init(NULL, NULL);
    complain_error(err);

    err = md_bgp_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_pm_init(NULL, NULL);
    complain_error(err);

    err = md_acclog_pm_init(NULL, NULL);
    complain_error(err);

    err = md_errlog_pm_init(NULL, NULL);
    complain_error(err);

    err = md_cache_evictd_pm_init(NULL, NULL);
    complain_error(err);

    err = md_nkn_stats_init(NULL, NULL);
    complain_error(err);

    err = md_mfd_license_init(NULL, NULL);
    complain_error(err);

    err = md_dashboard_pm_init(NULL, NULL);
    complain_error(err);

    err = md_nvsd_l4proxy_init(NULL, NULL);
    complain_error(err);

    err = md_adnsd_pm_init(NULL, NULL);
    complain_error(err);

    err = md_ssld_pm_init(NULL, NULL);
    complain_error(err);

    err = md_irqpin_ixgbed_pm_init(NULL, NULL);
    complain_error(err);

    err = md_junos_re_init(NULL, NULL);
    complain_error(err);

    err = md_pacifica_resiliency_pm_init(NULL, NULL);
    complain_error(err);

    err = md_apply_first_init(NULL, NULL);
    complain_error(err);

    err = md_apply_last_init(NULL, NULL);
    complain_error(err);

#endif /* GRAFT_POINT 2 */

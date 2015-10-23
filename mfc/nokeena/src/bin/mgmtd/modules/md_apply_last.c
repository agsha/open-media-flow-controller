/*
 *
 * Filename:  md_apply_last.c
 * Date:      2011/12/12
 * Author:    mvengata@juniper.net
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <openssl/md5.h>
#include "tcrypt.h"
#include <ctype.h>
#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "mdb_dbbe.h"
#include "nkn_defs.h"
#include "tpaths.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "regex.h"
#include "nkn_mgmt_defs.h"
#include "nkn_cntr_api.h"
#include "nkn_common_config.h"


#define APPLY_LAST_ORDER	32000
static md_upgrade_rule_array *md_apply_last_upgrade_rules = NULL;

int md_apply_last_init(const lc_dso_info *info, void *data);

static int
md_apply_last_commit_apply(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg);

static int
md_apply_last_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg);

static int
md_apply_last_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg);

static int
md_apply_last_add_initial(md_commit *commit, mdb_db *db, void *arg);

int
md_nvsd_ns_cron(md_commit *commit, const mdb_db *db,
		const mdb_db_change *change);

static int
md_nvsd_prestage_install_cron_job( const char *user, uint16 file_cron, uint16 file_age, const char *name);

static const bn_str_value md_nkn_snmp_upgrade_adds_1_to_2[] = {
        /*Root mirror disk broken*/
        {"/snmp/traps/events/"
	         "\\/nkn\\/nvsd\\/events\\/root_disk_mirror_broken",
		 bt_name, "/nkn/nvsd/events/root_disk_mirror_broken"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/nvsd\\/events\\/root_disk_mirror_broken/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	         "\\/nkn\\/nvsd\\/events\\/root_disk_mirror_complete",
		 bt_name, "/nkn/nvsd/events/root_disk_mirror_complete"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/nvsd\\/events\\/root_disk_mirror_complete/enable",
	    bt_bool, "true"},

	/*Connection rate high -ok*/
        {"/snmp/traps/events/"
	         "\\/stats\\/events\\/connection_rate\\/rising\\/error",
		 bt_name, "/stats/events/connection_rate/rising/error"},
	{"/snmp/traps/events/"
		"\\/stats\\/events\\/connection_rate\\/rising\\/error/enable",
	    bt_bool, "true"},

        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/connection_rate\\/rising\\/clear",
		 bt_name, "/stats/events/connection_rate/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/connection_rate\\/rising\\/clear/enable",
	    bt_bool, "true"},
	/*cache byte rate  high-ok*/

        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/cache_byte_rate\\/rising\\/clear",
		 bt_name, "/stats/events/cache_byte_rate/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/cache_byte_rate\\/rising\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/cache_byte_rate\\/rising\\/error",
		 bt_name, "/stats/events/cache_byte_rate/rising/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/cache_byte_rate\\/rising\\/error/enable",
	    bt_bool, "true"},

	/*origin-byte-rate high-ok*/
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/origin_byte_rate\\/rising\\/clear",
		 bt_name, "/stats/events/origin_byte_rate/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/origin_byte_rate\\/rising\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/origin_byte_rate\\/rising\\/error",
		 bt_name, "/stats/events/origin_byte_rate/rising/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/origin_byte_rate\\/rising\\/error/enable",
	    bt_bool, "true"},
	/*Http transaction rate high -ok */
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/http_transaction_rate\\/rising\\/clear",
		 bt_name, "/stats/events/http_transaction_rate/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/http_transaction_rate\\/rising\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/http_transaction_rate\\/rising\\/error",
		 bt_name, "/stats/events/http_transaction_rate/rising/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/http_transaction_rate\\/rising\\/error/enable",
	    bt_bool, "true"},
	/*Aggr cpu util high -ok*/

        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/aggr_cpu_util\\/rising\\/clear",
		 bt_name, "/stats/events/aggr_cpu_util/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/aggr_cpu_util\\/rising\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/aggr_cpu_util\\/rising\\/error",
		 bt_name, "/stats/events/aggr_cpu_util/rising/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/aggr_cpu_util\\/rising\\/error/enable",
	    bt_bool, "true"},

	/*Appl max cpu util high-ok*/

        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/appl_cpu_util\\/rising\\/clear",
		 bt_name, "/stats/events/appl_cpu_util/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/appl_cpu_util\\/rising\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/appl_cpu_util\\/rising\\/error",
		 bt_name, "/stats/events/appl_cpu_util/rising/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/appl_cpu_util\\/rising\\/error/enable",
	    bt_bool, "true"},

	/*MFC fan-failure - ok*/
        {"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/fanfailure",
		 bt_name, "/nkn/ipmi/events/fanfailure"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/fanfailure/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/fanok",
		 bt_name, "/nkn/ipmi/events/fanok"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/fanok/enable",
	    bt_bool, "true"},

	/*MFC powersupply -failure - ok*/
        {"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/powerfailure",
		 bt_name, "/nkn/ipmi/events/powerfailure"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/powerfailure/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/powerok",
		 bt_name, "/nkn/ipmi/events/powerok"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/powerok/enable",
	    bt_bool, "true"},

	/*Log export failed*/
        {"/snmp/traps/events/"
	    "\\/nkn\\/nknlogd\\/events\\/logexportfailed",
		 bt_name, "/nkn/nknlogd/events/logexportfailed"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/nknlogd\\/events\\/logexportfailed/enable",
	    bt_bool, "true"},

	/*JBOD shelf unreachable*/
        {"/snmp/traps/events/"
	    "\\/nkn\\/nvsd\\/events\\/jbod_shelf_unreachable",
		 bt_name, "/nkn/nvsd/events/jbod_shelf_unreachable"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/nvsd\\/events\\/jbod_shelf_unreachable/enable",
	    bt_bool, "true"},

	/*Temperature high/ok*/
        {"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/temp_high",
		 bt_name, "/nkn/ipmi/events/temp_high"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/temp_high/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/temp_ok",
		 bt_name, "/nkn/ipmi/events/temp_ok"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/ipmi\\/events\\/temp_ok/enable",
	    bt_bool, "true"},

	/*Net interface utilization high-ok*/
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/intf_util\\/rising\\/clear",
		 bt_name, "/stats/events/intf_util/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/intf_util\\/rising\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/intf_util\\/rising\\/error",
		 bt_name, "/stats/events/intf_util/rising/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/intf_util\\/rising\\/error/enable",
	    bt_bool, "true"},

	/*Paging ok*/
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/paging\\/rising\\/clear",
		 bt_name, "/stats/events/paging/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/paging\\/rising\\/clear/enable",
	    bt_bool, "true"},

	/*CPU indiv util ok*/
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/cpu_util_indiv\\/rising\\/clear",
		 bt_name, "/stats/events/cpu_util_indiv/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/cpu_util_indiv\\/rising\\/clear/enable",
	    bt_bool, "true"},

	/*Mem-util high/ok */
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_mem_util\\/rising\\/clear",
		 bt_name, "/stats/events/nkn_mem_util/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_mem_util\\/rising\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_mem_util\\/rising\\/error",
		 bt_name, "/stats/events/nkn_mem_util/rising/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_mem_util\\/rising\\/error/enable",
	    bt_bool, "true"},

	/*Cache hit ratio low/ok*/
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/cache_hit_ratio\\/falling\\/clear",
		bt_name, "/stats/events/cache_hit_ratio/falling/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/cache_hit_ratio\\/falling\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/cache_hit_ratio\\/falling\\/error",
		 bt_name, "/stats/events/cache_hit_ratio/falling/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/cache_hit_ratio\\/falling\\/error/enable",
	    bt_bool, "true"},

	/*cluster node up /down*/

        {"/snmp/traps/events/"
	         "\\/nkn\\/nvsd\\/cluster\\/node_down",
		 bt_name, "/nkn/nvsd/cluster/node_down"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/nvsd\\/cluster\\/node_down/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	         "\\/nkn\\/nvsd\\/cluster\\/node_up",
		 bt_name, "/nkn/nvsd/cluster/node_up"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/nvsd\\/cluster\\/node_up/enable",
	    bt_bool, "true"},

	/*Disk failure*/
        {"/snmp/traps/events/"
	         "\\/nkn\\/nvsd\\/diskcache\\/events\\/disk_failure",
		 bt_name, "/nkn/nvsd/diskcache/events/disk_failure"},
	{"/snmp/traps/events/"
	    "\\/nkn\\/nvsd\\/diskcache\\/events\\/disk_failure/enable",
	    bt_bool, "true"},

	/*Disk space low ok*/
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_space\\/falling\\/clear",
		 bt_name, "/stats/events/nkn_disk_space/falling/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_space\\/falling\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_space\\/falling\\/error",
		 bt_name, "/stats/events/nkn_disk_space/falling/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_space\\/falling\\/error/enable",
	    bt_bool, "true"},

	/*Disk io high /ok*/
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_io\\/rising\\/clear",
		 bt_name, "/stats/events/nkn_disk_io/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_io\\/rising\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_io\\/rising\\/error",
		 bt_name, "/stats/events/nkn_disk_io/rising/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_io\\/rising\\/error/enable",
	    bt_bool, "true"},

	/*Disk bw high /ok*/
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_bw\\/rising\\/clear",
		 bt_name, "/stats/events/nkn_disk_bw/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_bw\\/rising\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_bw\\/rising\\/error",
		 bt_name, "/stats/events/nkn_disk_bw/rising/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/nkn_disk_bw\\/rising\\/error/enable",
	    bt_bool, "true"},


	/*Resource pool usage low /ok*/
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/resource_pool_event\\/falling\\/clear",
		 bt_name, "/stats/events/resource_pool_event/falling/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/resource_pool_event\\/falling\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/resource_pool_event\\/falling\\/error",
		 bt_name, "/stats/events/resource_pool_event/falling/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/resource_pool_event\\/falling\\/error/enable",
	    bt_bool, "true"},

	/*Resource pool usage high /ok*/
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/resource_pool_event\\/rising\\/clear",
		 bt_name, "/stats/events/resource_pool_event/rising/clear"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/resource_pool_event\\/rising\\/clear/enable",
	    bt_bool, "true"},
        {"/snmp/traps/events/"
	    "\\/stats\\/events\\/resource_pool_event\\/rising\\/error",
		 bt_name, "/stats/events/resource_pool_event/rising/error"},
	{"/snmp/traps/events/"
	    "\\/stats\\/events\\/resource_pool_event\\/rising\\/error/enable",
	    bt_bool, "true"},







};
///snmp/traps/events/\/nkn\/nvsd\/events\/root_disk_mirror_broken

int md_apply_last_init(
        const lc_dso_info *info,
        void *data)
{
    int err = 0;

    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule_array *ra = NULL;


    err = mdm_add_module(
            "apply_last",
            2,
            "/nkn/apply-last", "/nkn/apply-last/config",
            modrf_all_changes,
            md_apply_last_commit_side_effects, NULL,
	    md_apply_last_commit_check, NULL,
	    md_apply_last_commit_apply, NULL,
	    0, APPLY_LAST_ORDER,
            md_generic_upgrade_downgrade, &md_apply_last_upgrade_rules,
	    md_apply_last_add_initial, NULL,
            NULL, NULL, NULL, NULL, &module);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_apply_last_upgrade_rules);
    bail_error(err);
    ra = md_apply_last_upgrade_rules;

    err = md_upgrade_add_add_rules
	(ra, md_nkn_snmp_upgrade_adds_1_to_2,
	 sizeof(md_nkn_snmp_upgrade_adds_1_to_2) / sizeof(bn_str_value), 1, 2);
    bail_error(err);




bail:
    return err;
}

static int
md_apply_last_commit_apply(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
    int err = 0;
    int	i = 0, num_changes = 0;
    const mdb_db_change *change = NULL;
    tstring *val_str = NULL;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	/* Check if user sets up a password */
	if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 8,"nkn", "nvsd","namespace", "*", "prestage", "user",
		    "*", "password") && (mdct_delete != change->mdc_change_type)) {
	    const bn_attrib *new_attrib = NULL;

	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);

	    ts_free(&val_str);
	    err = bn_attrib_get_tstr(new_attrib, NULL, 0, NULL, &val_str);
	    bail_error(err);

	    if (val_str && (false == ts_equal_str(val_str, "", false))) {
		/* don't install process if no password is set */
		err = md_nvsd_ns_cron(commit, new_db, change);
		bail_error(err);
	    }
	} else if (((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			    0, 6,"nkn","nvsd","namespace","*", "prestage",
			    ",cron_time")) ||
		    (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			    0, 6,"nkn","nvsd","namespace","*","prestage","file_age")))
		&& (mdct_modify == change->mdc_change_type)) {
	    /*
	     * in case of addition this this not going to work
	     * as username doesn't exists
	     */
	    err = md_nvsd_ns_cron(commit, new_db, change);
	    bail_error(err);
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 5, "nkn", "nvsd", "system", "config", "mod_dmi")) {
	    const bn_attrib *new_attrib = NULL;
	    tbool mod_dmi_status = false;

	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);

	    err = bn_attrib_get_tbool(new_attrib, NULL, NULL,
		    &mod_dmi_status);
	    bail_error(err);

	    if (mod_dmi_status) {
		/* following is informational message only */
		err = md_commit_set_return_status_msg(commit, 0,
			"Enabled required web services as mod-dmi is enabled");
		bail_error(err);
	    }
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 6, "nkn", "nvsd", "http", "config", "filter", "mode")) {
	    const bn_attrib *new_attrib = NULL;

	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);

	    ts_free(&val_str);
	    err = bn_attrib_get_tstr(new_attrib, NULL, 0, NULL, &val_str);
	    bail_error(err);

	    if (!strcmp(ts_str(val_str), "packet")) {
		err = lf_touch_file("/config/nkn/packet_mode_filter", 0666);
		bail_error(err);
		err = md_commit_set_return_status_msg(commit, 0,
			"WARNING: please save configuration and reboot the SYSTEM, for the change to take effect");
		bail_error(err);
	    } else {
		err = lf_remove_file("/config/nkn/packet_mode_filter");
		bail_error(err);
		err = md_commit_set_return_status_msg(commit, 0,
			"WARNING: please save configuration and reboot the SYSTEM, for the change to take effect");
		bail_error(err);
	    }
	}
    }
bail:
    ts_free(&val_str);
    return err;
}



int
md_nvsd_ns_cron(md_commit *commit, const mdb_db *db,
		const mdb_db_change *change)
{
    int err = 0;
    uint16 file_cron = 0, file_age = 0;
    const char *ns_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
    char file_cron_node[256] = {0}, file_age_node[256] = {0}, ftp_username[64] = {0};

    /* XXX - assuming the ftp username */
    snprintf(ftp_username, sizeof(ftp_username), "%s_ftpuser",ns_name);

    snprintf(file_cron_node, sizeof(file_cron_node),
		    "/nkn/nvsd/namespace/%s/prestage/cron_time",ns_name);
    snprintf(file_age_node, sizeof(file_age_node),
		    "/nkn/nvsd/namespace/%s/prestage/file_age", ns_name);

    err = mdb_get_node_value_uint16(commit, db, file_cron_node, 0, NULL, &file_cron);
    bail_error(err);

    err = mdb_get_node_value_uint16(commit, db, file_age_node, 0, NULL, &file_age);
    bail_error(err);

    // Setup a CRON task for the user.
    err = md_nvsd_prestage_install_cron_job(ftp_username, file_cron, file_age, ns_name);
    bail_error(err);

bail:
    return err;
}

static int
md_nvsd_prestage_install_cron_job( const char *user, uint16 file_cron, uint16 file_age, const char *name)
{
    int err = 0;
    const char *dom = NULL;
    const char *month = NULL, *doy = NULL, *minute = NULL;
    const char *hour = NULL;
    const char *command = NULL;
    char *crontab = NULL;
    tstring *filename_real = NULL;
    char *filename_temp = NULL;
    int fd_temp = -1;
    char *user_output_path = NULL;
    const char *user_cron_file = "user_cron";
    char clean_ftp_cron[1024] = {0};
    char clean_file_cron[1024] = {0};
    int code = 0;
    lc_launch_params *lp = NULL;

    minute =    "*/1";  /* Check every 5 minutes */
    hour =      "*";
    dom =       "*";
    month =     "*";
    doy =       "*";

    command = "/opt/nkn/bin/prestage";

    crontab = smprintf("MAILTO=\"\"\n%s %s %s %s %s %s %s %s\n",
	    minute, hour, dom, month, doy, command, user, name);
    bail_null(crontab);

    clean_ftp_cron[0] = '\0';
    snprintf(clean_ftp_cron, sizeof(clean_ftp_cron),
		    "MAILTO=\"\"\n0 * * * * /opt/nkn/bin/clear_ftp.sh %s > /tmp/clear_ftp.op\n", user);

    clean_file_cron[0] = '\0';
    snprintf(clean_file_cron, sizeof(clean_file_cron),
		    "MAILTO=\"\"\n*/%d * * * * /opt/nkn/bin/clear_ftp_file.sh $HOME %d > /tmp/clear_file_ftp.op\n",
		    file_cron, file_age);
    /*-----------------------------------------------------------------
     * Get the output file setup
     */
    err = lf_path_from_va_str(&filename_real, true, 2,
            md_gen_output_path, user_cron_file);
    bail_error(err);

    err = lf_temp_file_get_fd(ts_str(filename_real), &filename_temp, &fd_temp);
    bail_error(err);

    /*-----------------------------------------------------------------
     * Write the file
     */
    err = lf_write_bytes(fd_temp, crontab, strlen(crontab), NULL);
    bail_error(err);

    err = lf_write_bytes(fd_temp, clean_ftp_cron, strlen(clean_ftp_cron), NULL);
    bail_error(err);

    err = lf_write_bytes(fd_temp, clean_file_cron, strlen(clean_file_cron), NULL);
    bail_error(err);

    close(fd_temp);


    /*-----------------------------------------------------------------
     * Move the file into place
     */
    err = lf_temp_file_activate(ts_str(filename_real), filename_temp,
                                md_gen_conf_file_owner,
                                md_gen_conf_file_group,
                                md_gen_conf_file_mode, true);
    bail_error(err);

#define PROG_CRONTAB_PATH   "/usr/bin/crontab"

    /* Launch crontab -u user file
     */

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);
    err = ts_new_str(&(lp->lp_path), PROG_CRONTAB_PATH);
    bail_error(err);

    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 4,
            PROG_CRONTAB_PATH, "-u", user, ts_str(filename_real));
    bail_error(err);

    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_devnull;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_devnull;

    err = lc_launch(lp, NULL);
    bail_error(err);

bail:
    lc_launch_params_free(&lp);
    ts_free(&filename_real);
    safe_free(filename_temp);
    return err;
}


static int
md_apply_last_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
    int err = 0;
    int i, num_changes = 0;
    const 	bn_attrib *new_val = NULL;
    tstring *dmi_state = NULL;
    const mdb_db_change *change = NULL;
    tstring *val_str = NULL;


    num_changes = mdb_db_change_array_length_quick (change_list);

    for (i = 0; i < num_changes; i++) {

	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);
	if (bn_binding_name_parts_pattern_match_va
		(change->mdc_name_parts, 0, 5,
		 "nkn", "nvsd", "system", "config", "mod_dmi")) {

	    if(change->mdc_new_attribs) {

		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);
		bail_null(new_val);

		ts_free(&dmi_state);
		err = bn_attrib_get_tstr(new_val, NULL, bt_bool,
			NULL, &dmi_state);
		bail_error_null(err, dmi_state);

		if(ts_equal_str(dmi_state, "true", false)) {
		    err = mdb_delete_child_nodes(commit, inout_new_db, 0,
			    "/web/httpd/listen/interface");
		    bail_error(err);

		    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			    0, bt_bool, "true", "/web/enable");
		    bail_error(err);

		    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			    0, bt_uint16, "8080", "/web/httpd/http/port");
		    bail_error(err);

		    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			    0, bt_bool, "false", "/web/httpd/http/sredirect");
		    bail_error(err);

		    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			    0, bt_bool, "true", "/web/httpd/http/enable");
		    bail_error(err);

		    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			    0, bt_bool, "true", "/web/httpd/listen/enable");
		    bail_error(err);

		    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			    0, bt_bool, "true", "/pm/process/httpd/auto_launch");
		    bail_error(err);

		    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			    0, bt_bool, "true", "/pm/process/agentd/auto_launch");
		    bail_error(err);

		    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			    0, bt_bool, "true", "/pm/process/ssh_tunnel/auto_launch");
		    bail_error(err);
		}
	    }
	} else if (bn_binding_name_parts_pattern_match_va
		(change->mdc_name_parts, 0, 5,
		 "nkn", "nvsd", "system", "config", "mod_ssl")) {

	    if(change->mdc_new_attribs) {
		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);
		bail_null(new_val);

		ts_free(&dmi_state);
		err = bn_attrib_get_tstr(new_val, NULL, bt_bool,
			NULL, &dmi_state);
		bail_error_null(err, dmi_state);

		err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			0, bt_bool, ts_str(dmi_state),
			"/pm/process/ssld/auto_launch");
		bail_error(err);
	    }
	} else if (bn_binding_name_parts_pattern_match_va
		(change->mdc_name_parts, 0, 5,
		 "nkn", "nvsd", "system", "config", "mod_ftp")) {
	    if(change->mdc_new_attribs) {

		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);
		bail_null(new_val);

		ts_free(&dmi_state);
		err = bn_attrib_get_tstr(new_val, NULL, bt_bool,
			NULL, &dmi_state);
		bail_error_null(err, dmi_state);

		err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			0, bt_bool, ts_str(dmi_state),
			"/pm/process/ftpd/auto_launch");
		bail_error(err);
	    }
	} else if (bn_binding_name_parts_pattern_match_va
		(change->mdc_name_parts, 0, 5,
		 "nkn", "nvsd", "system", "config", "mod_crawler")) {

	    if(change->mdc_new_attribs) {
		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);
		bail_null(new_val);

		ts_free(&dmi_state);
		err = bn_attrib_get_tstr(new_val, NULL, bt_bool,
			NULL, &dmi_state);
		bail_error_null(err, dmi_state);

		err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			0, bt_bool, ts_str(dmi_state),
			"/pm/process/crawler/auto_launch");
		bail_error(err);
	    }
	} else if (bn_binding_name_parts_pattern_match_va(
		    change->mdc_name_parts,
		    0, 6, "nkn", "nvsd", "http", "config", "filter", "mode")) {
	    const bn_attrib *new_attrib = NULL;

	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);

	    err = bn_attrib_get_tstr(new_attrib, NULL, 0, NULL, &val_str);
	    bail_error(err);

	    if (!strcmp(ts_str(val_str), "packet")) {
		err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			0, bt_string, "/opt/nkn/lib-lite:/opt/nkn/lib:/lib/nkn",
			"/pm/process/nvsd/environment/set/LD_LIBRARY_PATH/value");
		bail_error(err);
	    } else {
		err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
			0, bt_string, "/opt/nkn/lib:/lib/nkn",
			"/pm/process/nvsd/environment/set/LD_LIBRARY_PATH/value");
		bail_error(err);
	    }
	}
    }

bail:
    ts_free(&val_str);
    ts_free(&dmi_state);
    return err;
}



static int
md_apply_last_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg)
{
    int err = 0;
    int i, num_changes = 0;
    tstring *mod_dmi = NULL;
    tbool mod_dmi_enabled = false;
    tstring *new_val = NULL;
    const bn_attrib *new_attrib = NULL;
    const mdb_db_change *change = NULL;
    uint64 thresh = 0;

    err = mdb_get_node_value_tstr(commit, new_db,
	    "/nkn/nvsd/system/config/mod_dmi",
	    0,
	    NULL,
	    &mod_dmi);
    bail_null(mod_dmi);

    mod_dmi_enabled = ts_equal_str(mod_dmi, "1", false);

    num_changes = mdb_db_change_array_length_quick (change_list);

    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	if (bn_binding_name_parts_pattern_match_va
		(change->mdc_name_parts, 0, 2, "web", "enable") &&
		mod_dmi_enabled &&
		change->mdc_new_attribs) {

	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);
	    bail_null(new_attrib);

	    err = bn_attrib_get_tstr(new_attrib, NULL, bt_bool,
		    NULL, &new_val);
	    bail_error_null(err, new_val);

	    if(ts_equal_str(new_val, "false", false)) {
		err = md_commit_set_return_status_msg(commit, 1,
			"Cannot disable web ,when mod-dmi is enabled");
		bail_error(err);
		goto bail;
	    }
	} else if (bn_binding_name_parts_pattern_match_va
		(change->mdc_name_parts, 0, 4, "web", "httpd", "http", "enable") &&
		mod_dmi_enabled && change->mdc_new_attribs) {

	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);
	    bail_null(new_attrib);

	    err = bn_attrib_get_tstr(new_attrib, NULL, bt_bool,
		    NULL, &new_val);
	    bail_error_null(err, new_val);

	    if(ts_equal_str(new_val, "false", false)) {
		err = md_commit_set_return_status_msg(commit, 1,
			"Cannot disable HTTP,when mod-dmi is enabled");
		bail_error(err);
		goto bail;
	    }
	}  else if (bn_binding_name_parts_pattern_match_va
		(change->mdc_name_parts, 0, 4, "web", "httpd", "http", "port") &&
		mod_dmi_enabled &&
		change->mdc_new_attribs) {
	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);
	    bail_null(new_attrib);

	    err = bn_attrib_get_tstr(new_attrib, NULL, bt_uint16,
		    NULL, &new_val);
	    bail_error_null(err, new_val);

	    if(!ts_equal_str(new_val, "8080", false)) {
		err = md_commit_set_return_status_msg(commit, 1,
			"HTTP port should be 8080, when mod-dmi is enabled");
		bail_error(err);
		goto bail;
	    }
	}  else if (bn_binding_name_parts_pattern_match_va
		(change->mdc_name_parts, 0, 4, "web", "httpd", "listen", "enable") &&
		mod_dmi_enabled &&
		change->mdc_new_attribs) {
	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);
	    bail_null(new_attrib);

	    err = bn_attrib_get_tstr(new_attrib, NULL, bt_bool,
		    NULL, &new_val);
	    bail_error_null(err, new_val);

	    if(ts_equal_str(new_val, "false", false)) {
		err = md_commit_set_return_status_msg(commit, 1,
			"Cannot disable http listen, when mod-dmi is enabled");
		bail_error(err);
		goto bail;
	    }
	}  else if (bn_binding_name_parts_pattern_match_va
		(change->mdc_name_parts, 0, 5, "web", "httpd", "listen", "interface", "*") &&
		mod_dmi_enabled &&
		change->mdc_new_attribs) {
	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);
	    bail_null(new_attrib);

	    ts_free(&new_val);
	    err = bn_attrib_get_tstr(new_attrib, NULL, bt_string,
		    NULL, &new_val);
	    bail_error(err);

	    if(new_val) {
		err = md_commit_set_return_status_msg(commit, 1,
			"Cannot add http listen interface, when mod-dmi is enabled");
		bail_error(err);
		goto bail;
	    }
	}  else if (bn_binding_name_parts_pattern_match_va
		(change->mdc_name_parts, 0, 4, "web", "httpd", "http", "sredirect") &&
		mod_dmi_enabled &&
		change->mdc_new_attribs) {
	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);
	    bail_null(new_attrib);

	    ts_free(&new_val);
	    err = bn_attrib_get_tstr(new_attrib, NULL, bt_bool,
		    NULL, &new_val);
	    bail_error_null(err, new_val);

	    if(ts_equal_str(new_val, "true", false)) {
		err = md_commit_set_return_status_msg(commit, 1,
			"Cannot redirect http to https, when mod-dmi is enabled");
		bail_error(err);
		goto bail;
	    }
	} else if (change->mdc_new_attribs &&
		(bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
               "stats", "config", "alarm", "*", "rising", "error_threshold") ||
          bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
               "stats", "config", "alarm", "*", "rising", "clear_threshold") ||
          bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
               "stats", "config", "alarm", "*", "falling", "error_threshold") ||
          bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
               "stats", "config", "alarm", "*", "falling", "clear_threshold"))){
            const tstring *alarmstr = NULL;
            err = tstr_array_get(change->mdc_name_parts, 3, (tstring **)&alarmstr);
            bail_error_null(err,alarmstr);
            if(ts_has_prefix_str(alarmstr,"rp",false)&&
              (ts_has_suffix_str(alarmstr,"client_sessions",false)||ts_has_suffix_str(alarmstr,"max_bandwidth",false))){
                new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
                    ba_value);
                bail_null(new_attrib);

                err = bn_attrib_get_uint64(new_attrib, NULL, NULL, &thresh);
                bail_error(err);

                if((thresh > 100)){
                    err = md_commit_set_return_status_msg(commit, 1,
                        "Threshold beyond range of 0-100");
                    bail_error(err);
                    goto bail;
                }
            }

        }
#if !defined(__JUNIPER_DEV_BUILD__)
	else if (bn_binding_name_parts_pattern_match_va
		    (change->mdc_name_parts, 0, 5, "auth", "passwd", "user",
		     "root", "password")
		&& (change->mdc_change_type != mdct_delete)
		&& change->mdc_new_attribs) {
	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
						   ba_value);
	    bail_null(new_attrib);

	    ts_free(&new_val);
	    err = bn_attrib_get_tstr(new_attrib, NULL, bt_string, NULL,
				     &new_val);
	    bail_error_null(err, new_val);

	    if (ts_equal_str(new_val, "", false)) {
		err = md_commit_set_return_status_msg(commit, 1,
			"root password cannot be set empty");
		bail_error(err);
		goto bail;
	    }
	}
#endif
	ts_free(&new_val);
    }

bail:
    ts_free(&new_val);
    ts_free(&mod_dmi);
    return err;

}

static int
md_apply_last_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings
	(commit, db, md_nkn_snmp_upgrade_adds_1_to_2,
	 sizeof(md_nkn_snmp_upgrade_adds_1_to_2) / sizeof(bn_str_value));
    bail_error(err);
bail:
    return(err);
}


/*
 *
 * Filename:  md_apply_first.c
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
#include "nkn_defs.h"

#define APPLY_FIRST_ORDER	-9000

int md_apply_first_init(const lc_dso_info *info, void *data);

static int
md_apply_first_commit_apply(md_commit *commit,
	const mdb_db *old_db,
	const mdb_db *new_db,
	mdb_db_change_array *change_list,
	void *arg);

static int
md_apply_first_commit_side_effects(
	md_commit *commit,
	const mdb_db *old_db,
	mdb_db *inout_new_db,
	mdb_db_change_array *change_list,
	void *arg);

static int
md_apply_first_setup_pacifica(
	md_commit *commit,
	const mdb_db *old_db,
	mdb_db *inout_new_db,
	mdb_db_change_array *change_list,
	void *arg);


int
md_nvsd_ns_cron_del(const mdb_db_change *change);

int md_apply_first_init(
	const lc_dso_info *info,
	void *data)
{
    int err = 0;

    md_module *module = NULL;
    md_reg_node *node = NULL;

    err = mdm_add_module(
	    "apply_first",
	    1,
	    "/nkn/apply-first", "/nkn/apply-first/config",
	    modrf_all_changes,
	    md_apply_first_commit_side_effects, NULL,
	    NULL, NULL,
	    md_apply_first_commit_apply, NULL,
	    0, APPLY_FIRST_ORDER,
	    NULL, NULL, NULL, NULL,
	    NULL, NULL, NULL, NULL, &module);
    bail_error(err);

bail:
    return err;
}

static int
md_apply_first_commit_side_effects(
	md_commit *commit,
	const mdb_db *old_db,
	mdb_db *inout_new_db,
	mdb_db_change_array *change_list,
	void *arg)
{
    int err = 0;
    uint8_t hw_type = 0;


    //Check if its pacifica
    err = mdb_get_node_value_uint8(commit, inout_new_db,
	    "/nkn/nvsd/services/hw_type",0, NULL, &hw_type);
    bail_error(err);

    lc_log_basic(LOG_INFO, "hw type = %d", hw_type);

    /* Appyl Initial changes for pacifica */
    if(hw_type == HW_TYPE_PACIFICA) {

	err = md_apply_first_setup_pacifica(commit, old_db,
		inout_new_db,
		change_list,
		arg);
	bail_error(err);

    } else {
	//for others hw types
    }

bail:
    return err;

}

static int
md_apply_first_setup_pacifica(
	md_commit *commit,
	const mdb_db *old_db,
	mdb_db *inout_new_db,
	mdb_db_change_array *change_list,
	void *arg)
{
    int err = 0;
    int i, num_changes = 0;
    const mdb_db_change *change = NULL;
    tbool initial = false, one_shot = false;
    uint8_t hw_type = 0;
    static int intf_detail_slapped = 0;

    const char *mgmt_if_lst [] = {"eth0", "eth1", NULL};

    err = md_commit_get_commit_initial(commit, &initial);
    bail_error(err);

    err = md_commit_get_commit_mode(commit, &one_shot);
    bail_error(err);

    lc_log_basic(LOG_INFO, "commit mode,  intial = %d", initial);

    if(one_shot) {
	const char **if_lst = mgmt_if_lst;
	const char *intf_name = *if_lst;

	while(intf_name) {

	    lc_log_basic(LOG_NOTICE, "Resetting IP address of interface %s", intf_name);
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_ipv4addr, "0.0.0.0", "/net/interface/config/%s/addr/ipv4/static/1/ip", intf_name);
	    bail_error(err);

	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_uint8, "0", "/net/interface/config/%s/addr/ipv4/static/1/mask_len", intf_name);
	    bail_error(err);

	    intf_name = *(++if_lst);
	}
    }

    if(!intf_detail_slapped && initial && !one_shot) {

	/*Set the interface details*/
	const char **if_lst = mgmt_if_lst;
	const char *intf_name = *if_lst;
	while(intf_name) {
	    lc_log_basic(LOG_NOTICE, "Setting MGMT Interface %s", intf_name);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_create,
		    0, bt_bool, "true",
		    "/net/interface/config/%s/addr/ipv4/dhcp",
		    intf_name);
	    bail_error(err);

	    intf_detail_slapped = 1;

	    intf_name = *(++if_lst);
	}

	/* Set syslog rotation to file size based(10 MB),
	 * As in pacifica, we have 2 GB disk space and if
	 * syslog takes over the space in one day,
	 * we will run out of logging space
	 */

	err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
		0, bt_uint64, "10485760", //10MB
		"/logging/rotation/global/criteria/threshold_size");
	bail_error(err);

	err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
		0, bt_string, "size",
		"/logging/rotation/global/criteria/type");
	bail_error(err);


    }


bail:
    return err;
}

static int
md_apply_first_commit_apply(
	md_commit *commit,
	const mdb_db *old_db,
	const mdb_db *new_db,
	mdb_db_change_array *change_list,
	void *arg)
{
    int err = 0;
    int	i = 0, num_changes = 0;
    const mdb_db_change *change = NULL;
    lc_launch_params *lp = NULL;
    tstring *value = NULL;
    num_changes = mdb_db_change_array_length_quick (change_list);

    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (bn_binding_name_pattern_match(ts_str(change->mdc_name),
		    "/nkn/nvsd/namespace/*")
		&& (mdct_delete == change->mdc_change_type)) {

	    err = md_nvsd_ns_cron_del(change);
	    bail_error(err);
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 2, "system", "hostname") &&
		(mdct_add == change->mdc_change_type ||
		 mdct_modify == change->mdc_change_type)) {

	    const bn_attrib *new_val = NULL;

	    ts_free(&value);
	    lc_launch_params_free(&lp);

	    str_value_t cmd_line = {0};

	    new_val =
		bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);
	    bail_null(new_val);

	    err = bn_attrib_get_tstr(new_val, NULL,
		    bt_hostname, NULL, &value);
	    bail_error_null(err, value);

	    snprintf(cmd_line, sizeof(cmd_line),
		    "edit; set system host-name %s;commit", ts_str(value));

	    err = lc_launch_params_get_defaults(&lp);
	    bail_error(err);

	    err = ts_new_str(&(lp->lp_path),"/usr/bin/ssh");
	    bail_error(err);


	    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 5,
		    "/usr/bin/ssh", "-i",
		    "/config/nkn/id_dsa",
		    "admin@10.84.77.10",
		    cmd_line);

	    lp->lp_log_level = LOG_INFO;
	    lp->lp_wait = false;

	    err = lc_launch(lp, NULL);
	    bail_error(err);

	}
    }
bail:
    ts_free(&value);
    lc_launch_params_free(&lp);
    return err;
}



    int
md_nvsd_ns_cron_del(const mdb_db_change *change)
{
    int err = 0;
    char ftp_username[64] = {0};
    const char *ns_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
    int32 ret_status = 0;
    tstring *ret_output = NULL;

    snprintf(ftp_username, sizeof(ftp_username), "%s_ftpuser",ns_name);
#define PROG_CRONTAB_PATH   "/usr/bin/crontab"

    err = lc_launch_quick_status(&ret_status, &ret_output, false, 4,
	    PROG_CRONTAB_PATH, "-r", "-u", ftp_username);
    bail_error(err);

    lc_log_basic(LOG_INFO, "status of crontab: %d, output: %s",
	    ret_status, ts_str_maybe_empty(ret_output));

bail:
    return err;
}


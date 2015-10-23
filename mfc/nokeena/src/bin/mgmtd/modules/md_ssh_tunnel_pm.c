/*
 *
 * Filename:    md_ssh_tunnel_pm.c
 * Date:        2011-08-26
 * Author:      Thilak Raj S
 *
 * (C) Copyright 2008-2010 Juniper Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "dso.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "file_utils.h"
#include "md_utils.h"
#include "tpaths.h"
#include "nkn_mgmt_common.h"
#include "nkn_mgmt_defs.h"

/*----------------------------------------------------------------------------
 * PREPROCESSOR MACROS/CONSTANTS
 *---------------------------------------------------------------------------*/
#define NKN_SSH_TUNNEL_PATH             "/opt/nkn/sbin/sshtd"
#define NKN_SSH_TUNNEL_WORKING_DIR      "/coredump/ssh_tunnel"
#define SSH_LOGIN_CRED                  "admin@10.84.77.10"
#define SSH_TUN_PRIV_KEY                "/config/nkn/id_dsa"





static int
md_ssh_tunnel_commit_apply(md_commit *commit, const mdb_db *old_db,
                    const mdb_db *new_db,
                    mdb_db_change_array *change_list, void *arg);

static int
md_interface_ok_for_listen(md_commit *commit, const mdb_db *db,
                           const char *ifname, tbool *ret_ok);
int
md_lib_get_mgmt_ip(md_commit *commit, const mdb_db *db, tstring **ip_address);
/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_ssh_tunnel_pm_ug_rules = NULL;


/*ssh -i /config/nkn/id_dsa -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -N -g -L -q
22:10.84.77.10:22 root@10.84.77.10 */

static const bn_str_value md_ssh_tunnel_pm_initial_values[] = {
    /*
     * ssh_tunnel : basic launch configuration.
     */
    {"/pm/process/ssh_tunnel/description", bt_string,
    				"Juniper space-agent"},
    {"/pm/process/ssh_tunnel/launch_path", bt_string,
    				NKN_SSH_TUNNEL_PATH},
    /* create a tunnel,  Do not execute a remote command.
     * This is useful for just forwarding ports
     */
    {"/pm/process/ssh_tunnel/launch_params/1/param", bt_string, "-v6"},
    /* verbose-ness */
    {"/pm/process/ssh_tunnel/working_dir", bt_string,
    				NKN_SSH_TUNNEL_WORKING_DIR},
    {"/pm/process/ssh_tunnel/launch_enabled", bt_bool, "true"},
    {"/pm/process/ssh_tunnel/auto_launch", bt_bool, "false"},
    {"/pm/process/ssh_tunnel/auto_relaunch", bt_bool, "true"},
    {"/pm/process/ssh_tunnel/launch_uid", bt_uint16, "0"},
    {"/pm/process/ssh_tunnel/launch_gid", bt_uint16, "0"},
    {"/pm/process/ssh_tunnel/term_signals/force/1", bt_string, "SIGTERM"},
    {"/pm/process/ssh_tunnel/term_signals/normal/1", bt_string, "SIGTERM"},

    /*------------------ Launch Order ---------------*/
    {"/pm/process/ssh_tunnel/initial_launch_order", bt_int32,
    				NKN_LAUNCH_ORDER_SSH_TUNNEL},
    {"/pm/process/ssh_tunnel/initial_launch_timeout", bt_duration_ms,
    				NKN_LAUNCH_TIMEOUT_SSH_TUNNEL}

    /*--------------- Params -------------------*/
};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_ssh_tunnel_pm_init(const lc_dso_info *info, void *data);


static int md_nkn_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings(
	    commit,
	    db,
	    md_ssh_tunnel_pm_initial_values,
	    sizeof(md_ssh_tunnel_pm_initial_values)/ sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

int
md_ssh_tunnel_pm_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Commit order of 200 is greater than the 0 used by most modules,
     * including md_pm.c.  This is to ensure that we can override some
     * of PM's global configuration, such as liveness grace period.
     *
     * We need modrf_namespace_unrestricted to allow us to set nodes
     * from our initial values function that are not underneath our
     * module root.
     */
    err = mdm_add_module(
	    "ssh_tunnel-pm",
	    3,
	    "/pm/nokeena/ssh_tunnel",
	    NULL,
	    0,
	    NULL, NULL,
	    NULL, NULL,
	    md_ssh_tunnel_commit_apply, NULL,
	    200, 0,
	    md_generic_upgrade_downgrade,
	    &md_ssh_tunnel_pm_ug_rules,
	    md_nkn_pm_add_initial,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    &module);
    bail_error(err);

    err = mdm_set_interest_subtrees(module,
	    "/net/interface",
	    "/nkn/mgmt-if/config");
    bail_error(err);

    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_ssh_tunnel_pm_ug_rules);
    bail_error(err);
    ra = md_ssh_tunnel_pm_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_MODIFY;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_path";
    rule->mur_new_value_type = bt_string;
    /* new ssh binary ssh-43p2 */
    rule->mur_new_value_str = NKN_SSH_TUNNEL_PATH;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_MODIFY;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/1/param";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "-N";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_MODIFY;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/2/param";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "-q";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_MODIFY;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/3/param";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "-g";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_MODIFY;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/4/param";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "-F";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_MODIFY;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/5/param";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "/var/opt/tms/output/ssh_43p2_config";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_MODIFY;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/6/param";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = SSH_LOGIN_CRED;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/7";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/7/param";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/8";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/8/param";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/9";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/9/param";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/10";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/10/param";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/11";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/11/param";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/12";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/12/param";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_MODIFY;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_path";
    rule->mur_new_value_type = bt_string;
    /* new ssh binary ssh-43p2 */
    rule->mur_new_value_str = NKN_SSH_TUNNEL_PATH;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_MODIFY;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/1/param";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "-v6";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/2";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/2/param";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/3";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/3/param";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/4";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/4/param";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/5";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/5/param";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/6";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_DELETE;
    rule->mur_name = "/pm/process/ssh_tunnel/launch_params/6/param";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_MODIFY;
    rule->mur_name = "/pm/process/ssh_tunnel/term_signals/force/1";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "SIGTERM";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_MODIFY;
    rule->mur_name = "/pm/process/ssh_tunnel/term_signals/normal/1";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "SIGTERM";
    rule->mur_cond_name_exists = true;
    MD_ADD_RULE(ra);

bail:
    return err;
}

int
md_lib_get_mgmt_ip(md_commit *commit, const mdb_db *db, tstring **ip_address)
{
    int err = 0;
    tstring * t_ifname = NULL, *t_ipaddr = NULL;
    tbool listen_ok = false;
    node_name_t node_name = {0};

    bail_null(ip_address);
    *ip_address = NULL;

    err = mdb_get_node_value_tstr(commit, db, "/nkn/mgmt-if/config/if-name",
	    0, NULL, &t_ifname);
    bail_error(err);

    if (t_ifname == NULL) {
	/* we don't have the interface name, */
	goto bail;
    }

    /* get ip-address */
    err = md_interface_ok_for_listen(commit, db, ts_str(t_ifname),&listen_ok);
    bail_error(err);

    lc_log_debug(LOG_DEBUG,
	    "ifname: %s, listen: %d", ts_str(t_ifname), listen_ok);

    if (listen_ok == false) {
	/* current interface is not good to listen, return NULL */
	goto bail;
    }
    /* if ip address is not there, return NULL (don't listen) */
    snprintf(node_name, sizeof(node_name),
	    "/net/interface/state/%s/addr/ipv4/1/ip", ts_str(t_ifname));

    err = mdb_get_node_value_tstr(commit, db, node_name,
	    0, NULL, &t_ipaddr);
    bail_error(err);

    *ip_address = t_ipaddr;
    t_ipaddr = NULL;

bail:
    ts_free(&t_ifname);
    ts_free(&t_ipaddr);
    return err;
}

static int
md_ssh_tunnel_commit_apply(md_commit *commit, const mdb_db *old_db,
                    const mdb_db *new_db,
                    mdb_db_change_array *change_list, void *arg)
{
    int err = 0, num_changes = 0, i = 0;
    const mdb_db_change *change = NULL;
    tstring *t_old_ipaddr = NULL, *t_new_ipaddr = NULL;
    uint32_t  dmi_port = 0;
    tstring *filename_real = NULL;
    char *filename_temp = NULL;
    int fd_temp = -1;
    tbool match = false;

    num_changes = mdb_db_change_array_length_quick (change_list);
    lc_log_debug(LOG_DEBUG, "num -> %d",num_changes );

    /* get if ip-address */
    err = md_lib_get_mgmt_ip(commit, old_db, &t_old_ipaddr);
    bail_error(err);

    err = md_lib_get_mgmt_ip(commit, new_db, &t_new_ipaddr);
    bail_error(err);

    /* get junos-agent port */
    err = mdb_get_node_value_uint32(commit, new_db,
	    "/nkn/mgmt-if/config/dmi-port",
	    0, NULL, &dmi_port);
    bail_error(err);

    /* generate config file */
    err = lf_path_from_va_str(&filename_real, true, 2,
	    md_gen_output_path, "ssh_43p2_config");
    bail_error(err);

    err = lf_temp_file_get_fd(ts_str(filename_real),
	    &filename_temp, &fd_temp);
    bail_error(err);

    err = md_conf_file_write_header_opts(fd_temp, "md_ssh_43p2", "##",
	    mchf_no_timestamp);
    bail_error(err);

    err = lf_write_bytes_printf
	(fd_temp, NULL,
	 /* weather to use strict host checking or not */
	 "StrictHostKeyChecking=no\n"
	 /* where to save VJX key files */
	 "UserKnownHostsFile=/dev/null\n"
	 /* which file to use to present to ssh server */
	 "IdentityFile="SSH_TUN_PRIV_KEY"\n");
    bail_error(err);

    if (t_new_ipaddr && !ts_equal_str(t_new_ipaddr, "0.0.0.0", true)) {
	err = lf_write_bytes_printf(fd_temp, NULL,
		/* allows ssh-tunnel to use which  port and IP */
		"LocalForward=%s:%d 10.84.77.10:22\n", ts_str(t_new_ipaddr),
		dmi_port);
	bail_error(err);
    }
    err = lf_compare_file_fd(ts_str(filename_real), fd_temp, &match);
    if (err == lc_err_file_not_found) {
	/*
	 * If the other file doesn't exist, that's no problem;
	 * we just treat it as no match.
	 */
	err = 0;
	match = false;
    }
    bail_error(err);
    if (match) {
	/*
	 * The new file matches the existing one.  So we don't need to
	 * call lf_temp_file_activate_fd() and poke sshd; we can just
	 * delete it.  The main point here is to save poking sshd.
	 */
	lc_log_basic(LOG_DEBUG, "md_ssh: regenerated sshd config file, "
		"but it was the same as the old one, so leaving the "
		"old one and not poking sshd");
	safe_close(&fd_temp);
	err = lf_remove_file(filename_temp);
	complain_error_errno(err, "Deleting %s", filename_temp);
	err = 0;
	goto bail;
    }
    err = lf_temp_file_activate_fd(ts_str(filename_real), filename_temp,
	    &fd_temp,
	    md_gen_conf_file_owner,
	    md_gen_conf_file_group,
	    md_gen_conf_file_mode, lao_backup_orig);
    bail_error(err);

    /* send SIGHUP to ssh-tunnel */
    err = md_send_signal_to_process(commit, SIGHUP, "ssh_tunnel", false);
    bail_error(err);

bail:
    safe_close(&fd_temp);
    lf_remove_file(filename_temp);
    ts_free(&t_new_ipaddr);
    ts_free(&t_old_ipaddr);
    return err;
}

/* Copied from md_utils_internal.c, not checking for IPv6 */
static int
md_interface_ok_for_listen(md_commit *commit, const mdb_db *db,
                           const char *ifname, tbool *ret_ok)
{
    int err = 0;
    tbool ok = false, found = false, enabled = false;
    tbool dhcp = false, zeroconf = false;
    tbool has_addr = false;
    char *nodename = NULL;
    bn_binding *binding = NULL;
    bn_ipv4addr ipv4addr = lia_ipv4addr_any;

    bail_null(db);
    bail_null(ifname);
    bail_null(ret_ok);

    /* ........................................................................
     * Make sure the interface exists and is enabled.
     */
    nodename = smprintf("/net/interface/config/%s/enable", ifname);
    bail_null(nodename);
    err = mdb_get_node_value_tbool(commit, db, nodename, 0, &found, &enabled);
    bail_error(err);
    if (!found || !enabled) {
        ok = false;
        goto bail;
    }

    /* ........................................................................
     * Make sure the interface has neither DHCP nor Zeroconf enabled.
     */
    safe_free(nodename);
    nodename = smprintf("/net/interface/config/%s/addr/ipv4/dhcp", ifname);
    bail_null(nodename);
    err = mdb_get_node_value_tbool(commit, db, nodename, 0, &found, &dhcp);
    bail_error(err);
    bail_require(found);

    safe_free(nodename);
    nodename = smprintf("/net/interface/config/%s/addr/ipv4/zeroconf", ifname);
    bail_null(nodename);
    err = mdb_get_node_value_tbool(commit, db, nodename, 0, &found, &zeroconf);
    bail_error(err);
    bail_require(found);

    if (dhcp || zeroconf) {
        ok = false;
        goto bail;
    }

    /* ........................................................................
     * See if the interface has an active static nonzero IPv4 address.
     * If so, that's all we need, the interface is eligible.
     *
     * XXX/EMT: currently we assume there's only one slot for IPv4 addrs.
     */
    has_addr = false;
    safe_free(nodename);
    nodename = smprintf("/net/interface/config/%s/addr/ipv4/static/1/ip",
                        ifname);
    bail_null(nodename);
    err = mdb_get_node_binding(commit, db, nodename, 0, &binding);
    bail_error(err);
    if (binding) {
        err = bn_binding_get_ipv4addr_bnipv4(binding, ba_value, NULL,
                                             &ipv4addr);
        bail_error(err);
        if (!lia_ipv4addr_is_zero_quick(&ipv4addr)) {
            has_addr = true;
        }
    }

    if (has_addr) {
        ok = true;
        goto bail;
    }


    /* ........................................................................
     * We failed the third requirement: either usable IPv4 or IPv6
     * addresses present.  So the interface is not eligible.
     */
    ok = false;

bail:
    if (ret_ok) {
        *ret_ok = ok;
    }
    safe_free(nodename);
    bn_binding_free(&binding);
    return(err);
}

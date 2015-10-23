/*
 *
 * Filename:  md_nvsd_pm.c
 * Date:      2008-11-13
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/* ------------------------------------------------------------------------- */
/* md_nvsd_pm.c: shows how to add new instances of wildcard nodes to
 * the initial database, and how to override defaults for nodes
 * created by Samara base components.  Also shows how to add a process
 * of yours to be launched and monitored by Process Manager.
 *
 * This module does not register any nodes of its own: it's sole
 * purpose is to add/change nodes in the initial database.  But there
 * would be no problem if we did want to register nodes here.
 */

#include "common.h"
#include "dso.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "nkn_mgmt_common.h"

#define	NVSD_PATH	 "/opt/nkn/sbin/nvsd"
#define NVSD_WORKING_DIR "/coredump/nvsd"
#define NVSD_CONFIG_FILE "/config/nkn/nkn.conf.default"
#define	DEFAULT_WEBADMIN_PORT	"8080"
#define	DEFAULT_SECURE_WEBADMIN_PORT	"8443"

int md_nvsd_pm_init(const lc_dso_info *info, void *data);
static md_upgrade_rule_array *md_nvsd_pm_ug_rules = NULL;

/* ------------------------------------------------------------------------- */
/* In initial values, we are not required to specify values for nodes
 * underneath wildcards for which we want to accept the default.
 * Here we are just specifying the ones where the default is not what
 * we want, or where we don't want to rely on the default not changing.
 */
const bn_str_value md_nvsd_pm_initial_values[] = {
    /* .....................................................................
     * nvsd daemon: basic launch configuration.
     */
    {"/pm/process/nvsd/launch_enabled", bt_bool, "true"},
    {"/pm/process/nvsd/auto_launch", bt_bool, "true"},
    {"/pm/process/nvsd/working_dir", bt_string, NVSD_WORKING_DIR},
    {"/pm/process/nvsd/launch_path", bt_string, NVSD_PATH},
    {"/pm/process/nvsd/launch_uid", bt_uint16, "0"},
    {"/pm/process/nvsd/launch_gid", bt_uint16, "0"},
    {"/pm/process/nvsd/kill_timeout", bt_duration_ms, "12000"},
    {"/pm/process/nvsd/term_signals/force/1", bt_string, "SIGKILL"},
    {"/pm/process/nvsd/term_signals/normal/1", bt_string, "SIGKILL"},
    {"/pm/process/nvsd/launch_params/1/param", bt_string, "-f"},
    {"/pm/process/nvsd/launch_params/2/param", bt_string, NVSD_CONFIG_FILE},
    {"/pm/process/nvsd/launch_params/3/param", bt_string, "-D"},
    {"/pm/process/nvsd/environment/set/PYTHONPATH", bt_string, "PYTHONPATH"},
    {"/pm/process/nvsd/environment/set/PYTHONPATH/value", bt_string, "${PYTHONPATH}:/nkn/plugins"},
    {"/pm/process/nvsd/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_NVSD},
    {"/pm/process/nvsd/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_NVSD},
    /* .....................................................................
     * Other samara node overrides
     */
    {"/web/httpd/http/port", bt_uint16, DEFAULT_WEBADMIN_PORT},
    {"/web/httpd/https/port", bt_uint16, DEFAULT_SECURE_WEBADMIN_PORT},
    {"/cli/prefix_modes/enable", bt_bool, "true"},
    {"/cli/prompt/confirm_reload", bt_bool, "true"},
    {"/web/httpd/listen/interface/eth0", bt_string, "eth0"},
    {"/ssh/server/listen/interface/eth0", bt_string, "eth0"},
    {"/email/autosupport/mailhub", bt_hostname, "mail.juniper.net"},
    {"/email/autosupport/recipient", bt_string, "autosupport@juniper.net"},
    {"/email/autosupport/auth/username", bt_string, "autosupport@juniper.net"},
    {"/email/autosupport/auth/password", bt_string, "UgmB9x6s"},
    {"/email/autosupport/auth/enable", bt_bool, "true"},

    /* Set open files limit to a large value */
    {"/pm/process/nvsd/fd_limit", bt_uint32, "64000"},
    {"/pm/process/nvsd/description", bt_string, "MFC"},
    {"/pm/process/nvsd/environment/set/LD_LIBRARY_PATH", bt_string, "LD_LIBRARY_PATH"},
    {"/pm/process/nvsd/environment/set/LD_LIBRARY_PATH/value", bt_string, "/opt/nkn/lib:/lib/nkn"},

    /* Set additional library paths that need to be saved in case of a core dump */
    {"/pm/process/nvsd/save_paths/1", bt_uint8, "1"},
    {"/pm/process/nvsd/save_paths/1/path", bt_string, "/lib/nkn"},
    {"/pm/process/nvsd/save_paths/2", bt_uint8, "2"},
    {"/pm/process/nvsd/save_paths/2/path", bt_string, "/opt/nkn/lib"},

    /* .....................................................................
     * Echo daemon: liveness configuration (optional).
     */
    /* ---- NVSD Liveness ------*/
    {"/pm/process/nvsd/liveness/enable", bt_bool, "true"},
    {"/pm/process/nvsd/liveness/type", bt_string, "gcl_client"},
    {"/pm/process/nvsd/liveness/data", bt_string,
	"/nkn/nvsd/state/alive"},
    {"/pm/process/nvsd/liveness/hung_count", bt_uint32, "3"},
    /* ---- NVSD Liveness ------  */

    /* ---- NVSD Liveness term signals ----- */
    {"/pm/process/nvsd/term_signals/liveness/1", bt_string, "SIGQUIT"},
    {"/pm/process/nvsd/term_signals/liveness/2", bt_string, "SIGTERM"},
    {"/pm/process/nvsd/term_signals/liveness/3", bt_string, "SIGKILL"},

    /* .....................................................................
     * Global PM config overrides.  Let's pretend nvsd needs 90 seconds
     * to get going before it's safe to test it for liveness, but that it
     * should be tested every 30 seconds thereafter.
     */
    /* ---- Commented out for later ------*/
    {"/pm/failure/liveness/grace_period", bt_duration_sec, "120"},
    {"/pm/failure/liveness/poll_freq", bt_duration_sec, "5"},
    /* ---- Commented out for later ------  */

    /* Set the Max disk use absolute to a large value and
     * set the percetage usage to 90
     * since PM takes the most restrictive one
     * 90% coredump partition will always be chosen
     */
    {"/pm/failure/snapshot_max_disk_use", bt_uint64, "104857600000"},
    {"/pm/failure/snapshot_max_disk_use_pct", bt_int8, "90"}
};


const bn_str_value md_initial_overrides[] = {
    {"/virt/config/vnet/nat-all",                    bt_string, "nat-all"},
    {"/virt/config/vnet/nat-all/vbridge/name",       bt_string,
	md_virt_vbridge_prefix "2"},
    {"/virt/config/vnet/nat-all/forward/mode",       bt_string, "nat"},
    {"/virt/config/vnet/nat-all/forward/interface",  bt_string, ""},
    {"/virt/config/vnet/nat-all/ip/address",
	bt_ipv4addr, "10.116.109.1"},
    {"/virt/config/vnet/nat-all/ip/mask_len",        bt_uint8,  "24"},
    {"/virt/config/vnet/nat-all/dhcp/enable",        bt_bool, "true"},
    {"/virt/config/vnet/nat-all/dhcp/range/low",
	bt_ipv4addr, "10.116.109.100"},
    {"/virt/config/vnet/nat-all/dhcp/range/high",
	bt_ipv4addr, "10.116.109.200"},
};

/* ------------------------------------------------------------------------- */
static int
md_nvsd_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    uint32 i = 0;
    bn_binding *binding = NULL;
    bn_binding *tmp_binding = NULL;

    err = mdb_create_node_bindings
        (commit, db, md_nvsd_pm_initial_values,
         sizeof(md_nvsd_pm_initial_values) / sizeof(bn_str_value));
    bail_error(err);

    err = mdb_get_node_binding(commit, db, "/virt/config/vnet/nat-all", 0, &tmp_binding);
    bail_error(err);

    if(tmp_binding) {
    /* Delete /virt/config/vnet/nat-all  nodes */
        err = bn_binding_new_named(&binding, md_initial_overrides[0].bv_name);
	bail_error_null(err, binding);

	err = mdb_set_node_binding(commit, db, bsso_delete, 0, binding);

    }

 bail:
    bn_binding_free(&tmp_binding);
    bn_binding_free(&binding);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_nvsd_pm_init(const lc_dso_info *info, void *data)
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
    err = mdm_add_module
        ("nvsd_pm", 14, "/pm/nokeena/nvsd", NULL,
         modrf_namespace_unrestricted,
         NULL, NULL, NULL, NULL, NULL, NULL, 300, 0,
         md_generic_upgrade_downgrade,
	 &md_nvsd_pm_ug_rules,
         md_nvsd_pm_add_initial, NULL,
         NULL, NULL, NULL, NULL, &module);
    bail_error(err);

    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_nvsd_pm_ug_rules);
    bail_error(err);
    ra = md_nvsd_pm_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_MODIFY;
    rule->mur_name =           "/pm/process/nvsd/kill_timeout";
    rule->mur_new_value_type = bt_duration_ms;
    rule->mur_new_value_str =  "12000";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/nvsd/environment/set/LD_LIBRARY_PATH";
    rule->mur_new_value_str =   "LD_LIBRARY_PATH";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/nvsd/environment/set/LD_LIBRARY_PATH/value";
    rule->mur_new_value_str =   "/opt/nkn/lib";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/term_signals/force/1";
    rule->mur_new_value_str =   "SIGKILL";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/term_signals/normal/1";
    rule->mur_new_value_str =   "SIGKILL";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/initial_launch_order";
    rule->mur_new_value_type =  bt_int32;
    rule->mur_new_value_str =   NKN_LAUNCH_ORDER_NVSD;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/initial_launch_timeout";
    rule->mur_new_value_type =  bt_duration_ms;
    rule->mur_new_value_str =   NKN_LAUNCH_TIMEOUT_NVSD;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/nvsd/save_paths/1";
    rule->mur_new_value_type =  bt_uint8;
    rule->mur_new_value_str =   "1";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/nvsd/save_paths/1/path";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "/lib/nkn";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/nvsd/save_paths/2";
    rule->mur_new_value_type =  bt_uint8;
    rule->mur_new_value_str =   "2";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/nvsd/save_paths/2/path";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "/opt/nkn/lib";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/liveness/enable";
    rule->mur_new_value_str =   "true";
    rule->mur_new_value_type =  bt_bool;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/liveness/type";
    rule->mur_new_value_str =   "gcl_client";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/liveness/data";
    rule->mur_new_value_str =   "/nkn/nvsd/state/alive";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/liveness/hung_count";
    rule->mur_new_value_str =   "3";
    rule->mur_new_value_type =  bt_uint32;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/term_signals/liveness/1";
    rule->mur_new_value_str =   "SIGQUIT";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/term_signals/liveness/2";
    rule->mur_new_value_str =   "SIGKILL";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/term_signals/liveness/3";
    rule->mur_new_value_str =   "SIGTERM";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/failure/liveness/poll_freq";
    rule->mur_new_value_str =   "20";
    rule->mur_new_value_type =  bt_duration_sec;
    MD_ADD_RULE(ra);

    /* upgrade rule to disbale MFC-Probe by default */
    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/liveness/enable";
    rule->mur_new_value_str =   "false";
    rule->mur_new_value_type =  bt_bool;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/failure/liveness/poll_freq";
    rule->mur_new_value_str =   "5";
    rule->mur_new_value_type =  bt_duration_sec;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/liveness/enable";
    rule->mur_new_value_str =   "true";
    rule->mur_new_value_type =  bt_bool;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/description";
    rule->mur_new_value_str =   "MFC";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/web/httpd/https/port";
    rule->mur_new_value_str =   DEFAULT_SECURE_WEBADMIN_PORT;
    rule->mur_new_value_type =  bt_uint16;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/term_signals/liveness/2";
    rule->mur_new_value_str =   "SIGTERM";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nvsd/term_signals/liveness/3";
    rule->mur_new_value_str =   "SIGKILL";
    rule->mur_new_value_type =  bt_string;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/failure/snapshot_max_disk_use_pct";
    rule->mur_new_value_str =   "90";
    rule->mur_new_value_type =  bt_int8;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/failure/snapshot_max_disk_use";
    rule->mur_new_value_str =   "104857600000";
    rule->mur_new_value_type =  bt_uint64;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/failure/liveness/grace_period";
    rule->mur_new_value_str =   "120";
    rule->mur_new_value_type =  bt_duration_sec;
    MD_ADD_RULE(ra);



 bail:
    return(err);
}

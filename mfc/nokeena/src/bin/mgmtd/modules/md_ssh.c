/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

static const char rcsid[] = "$Id: md_ssh.c,v 1.130 2012/09/05 21:55:32 slanser Exp $";

#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "mdb_dbbe.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "mdm_events.h"
#include "tacl.h"
#include "proc_utils.h"
#include "md_utils.h"
#include "md_utils_internal.h"
#include "mdmod_common.h"
#include "tpaths.h"
#include "file_utils.h"
#include "key_utils.h"
#include "md_utils.h"
#include "md_upgrade.h"
#include "libevent_wrapper.h"
#include "md_main.h" /* XXX/EMT: for md_lwc */

#define md_ssh_user_rsa_key_type  "rsa2"
#define md_ssh_user_dsa_key_type  "dsa2"

#define md_ssh_user_key_type_choices "," md_ssh_user_rsa_key_type "," \
                                   md_ssh_user_dsa_key_type  ","


#define md_ssh_client_hostkey_check_yes "yes"
#define md_ssh_client_hostkey_check_no "no"
#define md_ssh_client_hostkey_check_ask "ask"

#define md_ssh_client_hostkey_check_choices "," \
    md_ssh_client_hostkey_check_yes "," \
    md_ssh_client_hostkey_check_no "," \
    md_ssh_client_hostkey_check_ask "," \

static const char *md_ssh_mfg_bindings[] = {
    "/ssh/server/hostkey/public/rsa1",
    "/ssh/server/hostkey/private/rsa1",
    "/ssh/server/hostkey/public/rsa2",
    "/ssh/server/hostkey/private/rsa2",
    "/ssh/server/hostkey/public/dsa2",
    "/ssh/server/hostkey/private/dsa2"
};

/*
 * How long will we wait after getting an interface state change event
 * before reconfiguring sshd?  If we get another, we'll reset this
 * timer, to see if more arrive.
 */
static const lt_dur_ms md_ssh_reconfig_event_short_ms = 750;
static lew_event *md_ssh_reconfig_event_short = NULL;

/*
 * What's the maximum time after getting an interface state change
 * event that we'll wait before reconfiguring?  This is just a
 * failsafe to make sure the reconfig does happen, even if we get a
 * steady stream of events which keep making us reset the other timer.
 */
static const lt_dur_ms md_ssh_reconfig_event_long_ms = 3000;
static lew_event *md_ssh_reconfig_event_long = NULL;

/* ------------------------------------------------------------------------- */

static md_upgrade_rule_array *md_ssh_ug_rules = NULL;

/* ------------------------------------------------------------------------- */

int md_ssh_init(const lc_dso_info *info, void *data);

int md_ssh_deinit(const lc_dso_info *info, void *data);

static int md_ssh_hostkey_bindings(const char *priv_key_path,
                                   const char *pub_key_path,
                                   const char *key_type, mdb_db *new_db);

static int md_ssh_upgrade_downgrade(const mdb_db *old_db,
                                    mdb_db *inout_new_db,
                                    uint32 from_module_version,
                                    uint32 to_module_version, void *arg);

static int md_ssh_add_initial(md_commit *commit, mdb_db *db, void *arg);

static int md_ssh_commit_check(md_commit *commit, const mdb_db *old_db, 
                               const mdb_db *new_db, 
                               mdb_db_change_array *change_list, void *arg);

static int md_ssh_commit_apply(md_commit *commit, const mdb_db *old_db, 
                               const mdb_db *new_db, 
                               mdb_db_change_array *change_list, void *arg);

static int md_ssh_write_sshd_conf_file(tstr_array *ip_addrs,
                                       bn_binding_array *port_bindings,
                                       tbool listen_active,
                                       tbool aaa_tally_enabled,
                                       tbool x11_forwarding_enabled,
                                       uint32 min_version,
                                       tbool *ret_changed_file);

static int md_ssh_write_known_hosts_file(bn_binding_array *host_bindings,
                                         md_commit *commit, const mdb_db *db);

static int md_ssh_write_ssh_conf_file(tstring *hostkeycheck);

static int md_ssh_iterate_users(md_commit *commit, const mdb_db *db,
                                const char *parent_node_name,
                                const uint32_array *node_attrib_ids,
                                int32 max_num_nodes, int32 start_child_num,
                                const char *get_next_child,
                                mdm_mon_iterate_names_cb_func iterate_cb,
                                void *iterate_cb_arg, void *arg);

static int md_ssh_get_user(md_commit *commit, const mdb_db *db,
                           const char *node_name, 
                           const bn_attrib_array *node_attribs,
                           bn_binding **ret_binding, uint32 *ret_node_flags,
                           void *arg);

static int md_ssh_iterate_known_host(md_commit *commit, const mdb_db *db,
                                     const char *parent_node_name,
                                     const uint32_array *node_attrib_ids,
                                     int32 max_num_nodes,
                                     int32 start_child_num,
                                     const char *get_next_child,
                                     mdm_mon_iterate_names_cb_func iterate_cb,
                                     void *iterate_cb_arg, void *arg);

static int md_ssh_get_known_host(md_commit *commit, const mdb_db *db,
                                 const char *node_name, 
                                 const bn_attrib_array *node_attribs,
                                 bn_binding **ret_binding,
                                 uint32 *ret_node_flags,
                                 void *arg);

static int md_ssh_iterate_global_known_host(md_commit *commit,
                                      const mdb_db *db,
                                      const char *parent_node_name,
                                      const uint32_array *node_attrib_ids,
                                      int32 max_num_nodes,
                                      int32 start_child_num,
                                      const char *get_next_child,
                                      mdm_mon_iterate_names_cb_func iterate_cb,
                                      void *iterate_cb_arg, void *arg);

static int md_ssh_get_global_known_host(md_commit *commit, const mdb_db *db,
                                        const char *node_name, 
                                        const bn_attrib_array *node_attribs,
                                        bn_binding **ret_binding,
                                        uint32 *ret_node_flags,
                                        void *arg);

static int md_ssh_get_global_known_host_fp(md_commit *commit, const mdb_db *db,
                                        const char *node_name, 
                                        const bn_attrib_array *node_attribs,
                                        bn_binding **ret_binding,
                                        uint32 *ret_node_flags,
                                        void *arg);

static int md_ssh_action_gen_hostkey(md_commit *commit, mdb_db **db,
                                     const char *action_name,
                                     bn_binding_array *params,
                                     void *cb_arg);

static int md_ssh_action_gen_keypair(md_commit *commit, mdb_db **db,
                                     const char *action_name,
                                     bn_binding_array *params,
                                     void *cb_arg);

static int md_ssh_action_remove_known_host(md_commit *commit,
                                           const mdb_db *db,
                                           const char *action_name,
                                           bn_binding_array *params,
                                           void *cb_arg);

static int md_ssh_action_remove_known_host_safe(md_commit *commit, 
                                                const mdb_db *db,
                                                const char *action_name,
                                                bn_binding_array *params,
                                                void *cb_arg);

static int md_write_hostkey_file(md_commit *commit, const mdb_db *db,
                                 const char *priv_key_path,
                                 const char *pub_key_path,
                                 const char *key_type);

static int md_ssh_hostkey_fp_get(md_commit *commit, const mdb_db *db,
                                 const char *node_name,
                                 const bn_attrib_array *node_attribs,
                                 bn_binding **ret_binding,
                                 uint32 *ret_node_flags,
                                 void *arg);

static int md_ssh_commit_side_effects(md_commit *commit, 
                                      const mdb_db *old_db, 
                                      mdb_db *inout_new_db, 
                                      mdb_db_change_array *change_list,
                                      void *arg);


/* ------------------------------------------------------------------------- */
static int
md_ssh_key_gen_audit(md_commit *commit, const mdb_db *db, 
                     const char *action_name,
                     const bn_binding_array *bindings,
                     uint32 node_reg_flags,
                     const mdm_action_audit_directives *audit_dir,
                     const mdm_reg_action **param_regs,
                     uint32 param_reg_count, void *arg,
                     mdm_action_audit_context *ctxt)
{
    int err = 0;
    tstring *key_type = NULL, *bn_prefix = NULL;
    tstr_array *parts = NULL;
    const char *str = NULL;

    bail_null(audit_dir);
    bail_null(ctxt);
    err = md_audit_action_add_line(ctxt, maalt_description, maalf_none,
                                   audit_dir->maad_action_descr);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "key_type", 
                                                  NULL, &key_type);
    bail_error_null(err, key_type);

    err = md_audit_action_add_line_fmt(ctxt, maalt_param, maalf_none,
                                       _("key type: %s"),
                                       ts_str(key_type));
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "bn_prefix", 
                                                  NULL, &bn_prefix);
    bail_error_null(err, bn_prefix);

    err = bn_binding_name_to_parts(ts_str(bn_prefix), &parts, NULL);
    bail_error(err);

    /*
     * What we log depends on what we find in here.  Unfortunately, we
     * have to be familiar with how everyone uses it to log something
     * meaningful.
     *
     * XXX/EMT: could have a graft point here, in case a customer
     * used this action for their own purposes...
     */
    if (bn_binding_name_parts_pattern_match_va
        (parts, 0, 7, "cmc", "common", "config", "auth", "*", "identity",
         "*")) {
        str = tstr_array_get_str_quick(parts, 6);
        bail_null(str);
        err = md_audit_action_add_line_fmt(ctxt, maalt_param, maalf_none,
                                           _("CMC identity: '%s'"), str);
        bail_error(err);
    }
    else if (bn_binding_name_parts_pattern_match_va
             (parts, 0, 6, "ssh", "client", "username", "*", "keytype", "*")) {
        str = tstr_array_get_str_quick(parts, 3);
        bail_null(str);
        err = md_audit_action_add_line_fmt(ctxt, maalt_param, maalf_none,
                                           _("identity for user: '%s'"), str);
        bail_error(err);
    }

 bail:
    ts_free(&key_type);
    ts_free(&bn_prefix);
    tstr_array_free(&parts);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_kh_remove_audit(md_commit *commit, const mdb_db *db, 
                       const char *action_name,
                       const bn_binding_array *bindings,
                       uint32 node_reg_flags,
                       const mdm_action_audit_directives *audit_dir,
                       const mdm_reg_action **param_regs,
                       uint32 param_reg_count, void *arg,
                       mdm_action_audit_context *ctxt)
{
    int err = 0;
    tstring *user = NULL, *host = NULL, *path = NULL;
    int param_num = 1;

    /*
     * We use the "goto next" approach below, rather than nesting
     * if/else, in order to make it easier to have tests that are
     * conditional on PROD_FEATUREs.
     */

    bail_null(audit_dir);

    /*
     * We ignore mrn_action_audit_descr because we log a different 
     * description depending on what parameters are passed to us.
     */

    err = bn_binding_array_get_value_tstr_by_name(bindings, "user", 
                                                  NULL, &user);
    bail_error(err);
    if (user) {
        err = md_audit_action_add_line(ctxt, maalt_description, maalf_none,
                                       _("remove SSH known host"));
        bail_error(err);
        err = md_audit_action_add_line_fmt(ctxt, maalt_param, maalf_none,
                                           _("user: \"%s\""),
                                           ts_str(user));
        bail_error(err);
        goto next;
    }

    err = bn_binding_array_get_value_tstr_by_name(bindings, 
                                                  "known_hosts_path", NULL,
                                                  &path);
    bail_error(err);

#if defined(PROD_FEATURE_CMC_CLIENT) || defined(PROD_FEATURE_CMC_SERVER) || \
    defined(PROD_FEATURE_REMOTE_GCL)
    if (path && ts_equal_str(path, cmc_ssh_known_hosts_path, false)) {
        err = md_audit_action_add_line(ctxt, maalt_description, maalf_none,
                                       _("remove SSH known host for CMC"));
        bail_error(err);
        goto next;
    }
#endif /* PROD_FEATURE_CMC_CLIENT || PROD_FEATURE_CMC_SERVER ||
          PROD_FEATURE_REMOTE_GCL */

    /*
     * Fallback case: they didn't provide a user, and they
     * provided a path we didn't recognize.  We have to log
     * something anyway, but it will be missing any indication
     * of where the known host was removed from, since we
     * cannot expose the raw path.
     */
    err = md_audit_action_add_line(ctxt, maalt_description, maalf_none,
                                   _("remove SSH known host"));
    bail_error(err);

 next:
    err = bn_binding_array_get_value_tstr_by_name(bindings, "host", 
                                                  NULL, &host);
    bail_error_null(err, host);
    err = md_audit_action_add_line_fmt(ctxt, maalt_param, maalf_none,
                                       _("host: \"%s\""),
                                       ts_str(host));
    bail_error(err);

 bail:
    ts_free(&user);
    ts_free(&host);
    ts_free(&path);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_user_acl(md_commit *commit, const mdb_db *old_db, const mdb_db *new_db,
                const char *node_name, const tstr_array *node_name_parts, 
                const char *reg_name, tbool permitted, 
                const mdm_node_acl_request *req, void *data,
                mdm_node_acl_response *ret_response)
{
    int err = 0;
    const char *username = NULL;
    tbool have_superset = false;
    uint32_array *my_auth_groups = NULL;
    uint32 idx = 0;

    bail_null(node_name);
    bail_null(node_name_parts);
    bail_null(ret_response);

    if (!lc_is_prefix("/ssh/server/username/", node_name, false) &&
        !lc_is_prefix("/ssh/client/username/", node_name, false)) {
        goto bail;
    }

    username = tstr_array_get_str_quick(node_name_parts, 3);
    bail_null(username);
    err = md_utils_acl_check_user_commit(commit, old_db, new_db, username,
                                         taag_auth_set_9, ret_response);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_reconfig_sshd(md_commit *commit, const mdb_db *old_db,
                     const mdb_db *new_db, tbool force_restart,
                     tbool listen_active, tstr_array *listen_intf_addrs)
{
    int err = 0;
    tbool restart_sshd = false;
    bn_binding_array *port_bindings = NULL;
    uint32 min_version = 0;
    tbool x11_forwarding_enabled = false;
    tbool aaa_tally_enabled = false;
    tbool found = false, was_enabled = false, is_enabled = false;
    tbool changed_file = false;

    err = mdb_iterate_binding(commit, new_db, "/ssh/server/port",
                              mdqf_sel_iterate_include_self,
                              &port_bindings);
    bail_error(err);
    
    err = mdb_get_node_value_uint32(commit, new_db, 
                                    "/ssh/server/min-version", 0,
                                    NULL, &min_version);
    bail_error(err);
    
    /*
     * NOTE: md_ssh_commit_apply() also mentions this node name.
     * And our call to mdm_set_interest_subtrees() sets us up to
     * be called for it too.
     */
    err = mdb_get_node_value_tbool(commit, new_db,
                                   "/aaa/tally/config/enable", 0,
                                   NULL, &aaa_tally_enabled);
    bail_error(err);

    err = mdb_get_node_value_tbool(commit, new_db,
                                   "/ssh/server/x11_forwarding/enable", 0,
                                   NULL, &x11_forwarding_enabled);
    bail_error(err);

    err = md_ssh_write_sshd_conf_file(listen_intf_addrs, port_bindings,
                                      listen_active, aaa_tally_enabled,
                                      x11_forwarding_enabled, min_version,
                                      &changed_file);
    bail_error(err);

    /* ........................................................................
     * Now send sshd a SIGHUP if necessary.
     */

    if (!changed_file) {
        goto bail;
    }

    err = mdb_get_node_value_tbool(commit, new_db,
                                   "/ssh/server/enable", 0,
                                   &found, &is_enabled);
    bail_error(err);
    if (!found) { /* not sure why this would happen... */
        is_enabled = false;
    }
    
    if (old_db) {
        err = mdb_get_node_value_tbool(commit, old_db, 
                                       "/ssh/server/enable", 0,
                                       &found, &was_enabled);
        bail_error(err);
        if (!found) { /* e.g. during initial apply */
            was_enabled = false;
        }
    }
    else {
        /*
         * If we didn't have an old_db, this is not a commit, and we
         * have to assume that the 'enabled' flag did not change.
         */
        was_enabled = is_enabled;
    }
        
    if (was_enabled && is_enabled) {
        restart_sshd = true;
    }

    if (restart_sshd || force_restart) {
        /*
         * If sshd was already running, send it a SIGHUP (via PM)
         * to cause it to reload its configuration.
         *
         * Just because it was set to auto-launch doesn't mean it was
         * already running; someone could have sent a terminate action
         * request.  But an extraneous SIGHUP should not hurt.  Will
         * we miss a SIGHUP if we needed it?  Generally not, because
         * PM uses the same heuristic to know whether it should
         * restart a process: if auto-launch changes to true, we'll
         * always restart it even if it was already "unofficially"
         * running.
         *
         * The one case where we might have wanted a SIGHUP is when
         * sshd was and still is set to not automatically launch, but
         * something has manually launched it.  Here it is running
         * somewhat unofficially... unclear whether a SIGHUP is even
         * desired here.  The same goes for config changes handled
         * by various other modules, for ntpd, httpd, and xinetd.
         */
        lc_log_basic(LOG_INFO, "SSH configuration updated; signalling sshd");
        err = md_send_signal_to_process(commit, SIGHUP, "sshd", false);
        if (err) {
            err = md_commit_set_return_status_msg
                (commit, 1, _("Unable to communicate changes to SSH "
                           "server. Your changes will not take effect until "
                           " another change to its configuration is made."));
            bail_error(err);
        }
    }

 bail:
    bn_binding_array_free(&port_bindings);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_reconfig_sshd_event(int fd, short event_type, void *data,
                           lew_context *ctxt)
{
    int err = 0;
    md_commit *commit = NULL;
    mdb_db **dbp = NULL;
    tbool listen_active = false;
    tstr_array *listen_intf_addrs = NULL;

    err = lew_event_cancel(md_lwc, &md_ssh_reconfig_event_short);
    bail_error(err);

    err = lew_event_cancel(md_lwc, &md_ssh_reconfig_event_long);
    bail_error(err);

    err = md_commit_create_commit(&commit);
    bail_error_null(err, commit);

    err = md_commit_get_main_db(commit, &dbp);
    bail_error(err);
    bail_null(dbp);

    lc_log_basic(LOG_DEBUG, "md_ssh: reconfiguring due to interface state "
                 "change event");

    err = md_net_utils_get_listen_config(commit, NULL, *dbp,
                                         "/ssh/server/listen/enable",
                                         "/ssh/server/listen/interface", true,
                                         &listen_active, NULL, NULL,
                                         &listen_intf_addrs);
    bail_error(err);

    err = md_ssh_reconfig_sshd(commit, NULL, *dbp, false, listen_active,
                               listen_intf_addrs);
    bail_error(err);

 bail:
    md_commit_commit_state_free(&commit);
    tstr_array_free(&listen_intf_addrs);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_handle_ifstate_change(md_commit *commit, const mdb_db *db,
                             const char *event_name, 
                             const bn_binding_array *bindings,
                             void *cb_reg_arg, void *cb_arg)
{
    int err = 0;
    tbool short_pending = false, long_pending = false;
    lt_time_ms long_time_ms = 0;

    /*
     * We basically just want to call md_ssh_reconfig_sshd().
     * But we're concerned that in some cases, there might be several
     * interface state change events in quick succession, and we don't
     * want to HUP sshd too much.  So we'll set a timer, and only
     * reconfig sshd after we've gone a certain time without an
     * event.  And to protect against starvation (in the unlikely
     * event of a continuous stream of events), we'll also reconfig
     * no matter what after a longer interval.
     *
     * XXX/EMT: perf: could look inside state change event, and in
     * conjunction with reading some of our config, might sometimes
     * decide that the event isn't relevant to us, and save the effort
     * of regenerating the file, which may involve reading some more
     * expensive interface state nodes.
     */
    err = lew_event_is_pending
        (md_lwc, (const lew_event **)&md_ssh_reconfig_event_short,
         &short_pending, NULL);
    bail_error(err);
    err = lew_event_is_pending
        (md_lwc, (const lew_event **)&md_ssh_reconfig_event_long,
         &long_pending, &long_time_ms);
    bail_error(err);

    err = lew_event_reg_timer_full(md_lwc, &md_ssh_reconfig_event_short,
                                   "md_ssh_reconfig_event_short",
                                   "md_ssh_reconfig_sshd_event",
                                   md_ssh_reconfig_sshd_event, NULL,
                                   md_ssh_reconfig_event_short_ms);
    bail_error(err);

    complain_require(short_pending == long_pending);

    if (short_pending) {
        lc_log_basic(LOG_DEBUG, "md_ssh intf state change: postponing "
                     "sshd reconfig to be %" PRId64 " ms out.  "
                     "Definite reconfig now %" PRId64 " ms out.",
                     md_ssh_reconfig_event_short_ms,
                     long_time_ms - lt_curr_time_ms());
    }
    else {
        err = lew_event_reg_timer_full(md_lwc, &md_ssh_reconfig_event_long,
                                       "md_ssh_reconfig_event_long",
                                       "md_ssh_reconfig_sshd_event",
                                       md_ssh_reconfig_sshd_event, NULL,
                                       md_ssh_reconfig_event_long_ms);
        bail_error(err);

        lc_log_basic(LOG_DEBUG, "md_ssh intf state change: scheduling "
                     "sshd reconfig for %" PRId64 " ms out.  "
                     "Even if it gets postponed, it will never be more "
                     "that %" PRId64 " ms out.",
                     md_ssh_reconfig_event_short_ms,
                     md_ssh_reconfig_event_long_ms);
    }
    
 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/* The ACL on this command only permits access to tacl_sbm_ssh.  But if
 * this is a request to remove CMC known hosts, we want to also permit
 * access to taag_cmc_action_7.
 */
static int
md_ssh_remove_known_host_acl(md_commit *commit, const mdb_db *old_db,
                             const mdb_db *new_db, const char *node_name,
                             const tstr_array *node_name_parts,
                             const char *reg_name, tbool permitted,
                             const mdm_node_acl_request *req,
                             void *data, mdm_node_acl_response *ret_response)
{
    int err = 0;
    tbool found = false, is_cmc = false;
    uint32_array *auth_groups = NULL;
    uint32 idx = 0;

    bail_require(req->mnaq_oper == tao_action);
    bail_null(req->mnaq_action_bindings);

    err = bn_binding_array_get_value_tbool_by_name
        (req->mnaq_action_bindings, "cmc_known_hosts", &found, &is_cmc);
    bail_error(err);
    if (found && is_cmc) {
        err = md_commit_get_auth_groups(commit, &auth_groups);
        bail_error(err);
        err = uint32_array_linear_search(auth_groups, taag_cmc_action_7, 0,
                                         &idx);
        if (err == lc_err_not_found) {
            err = 0;
        }
        else {
            bail_error(err);
            ret_response->mnar_ok = true;
        }
    }

 bail:
    uint32_array_free(&auth_groups);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_ssh_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    err = mdm_add_module("ssh", 10,
                         "/ssh", NULL,
                         0,
                         md_ssh_commit_side_effects, NULL,
                         md_ssh_commit_check, NULL,
                         md_ssh_commit_apply, NULL,
                         0, md_ssh_apply_order,
                         md_ssh_upgrade_downgrade, &md_ssh_ug_rules,
                         md_ssh_add_initial, NULL, 
                         NULL,  NULL, NULL, NULL, 
                         &module);
    bail_error(err);

    /*
     * We watch the passwd nodes because some of our nodes reference
     * usernames which must exist locally.
     *
     * Our interface config watch is derived from
     * md_net_utils_relevant_for_listen_quick().  We don't have to watch
     * the rest of interface config because we listen for interface state
     * change events.
     *
     * We watch the auto-launch nodes for sshd.
     */
    err = mdm_set_interest_subtrees(module,
                                    "/auth/passwd/user",
                                    "/aaa/tally/config",
                                    "/net/interface/config/*/addr/ipv4",
                                    "/pm/process/*/auto_launch");
    bail_error(err);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = mdm_module_set_gettext_domain(module, GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    /*
     * For "listen" behavior, we need to be notified whenever interface
     * state changes (see bug 13901).
     */
    err = md_events_handle_int_interest_add
        ("/net/interface/events/state_change", md_ssh_handle_ifstate_change,
         NULL);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh";
    node->mrn_node_reg_flags =     mrf_acl_inheritable;
    mdm_node_acl_add(node,         tacl_sbm_ssh);
    node->mrn_description =        md_acl_dummy_node_descr;
    err = mdm_override_node_flags_acls(module, &node, 0);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "SSH server enabled";
    node->mrn_audit_descr =        N_("SSH server");
    node->mrn_audit_flags =        mnaf_enabled_flag;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/min-version";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "2";
    node->mrn_limit_num_min =      "1";
    node->mrn_limit_num_max =      "2";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Minimum protocol version supported";
    node->mrn_audit_descr =       N_("SSH minimum required protocol "
                                      "version");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/username/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_node_reg_flags |=    mrf_acl_inheritable;
    mdm_node_acl_add(node,         tacl_sbm_aaa_lo);
    node->mrn_acl_func =           md_ssh_user_acl;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "User for key";
    node->mrn_audit_descr =       N_("SSH server record for user '$v$'");
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * NOTE: md_passwd.c has a dependency on ssh keys, because it will
     * only allow password authentication to be disabled if there is
     * an ssh key installed.  So if we add any other key types, it
     * needs to be modified to check for them too. 
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =
        "/ssh/server/username/*/auth-key/sshv2/*";
    node->mrn_value_type =         bt_uint32;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Authorized public keys for user";
    node->mrn_audit_descr =       N_("SSH server record for user '$4$': "
                                      "authorized key $v$");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               
        "/ssh/server/username/*/auth-key/sshv2/*/public";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_initial_value =      "";
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "An authorized public key for user";
    node->mrn_audit_descr =       N_("public key");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/listen/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Enable the listen interface list";
    node->mrn_audit_descr =       N_("SSH 'listen' interface restrictions");
    node->mrn_audit_flags =       mnaf_enabled_flag;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/listen/interface/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Interface names to listen on";
    node->mrn_audit_descr =       N_("SSH server 'listen' interface $v$");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/port/*";
    node->mrn_value_type =         bt_uint32;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_bad_value_msg =
        N_("Ssh port numbers must be between 1 and 65535, inclusive");
    node->mrn_description =        "Ports to run SSH server on";
    node->mrn_audit_descr =       N_("SSH server port $v$");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/x11_forwarding/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_audit_descr =        N_("X11 forwarding");
    node->mrn_audit_flags =        mnaf_enabled_flag;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/hostkey/public/rsa1";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_initial_value =      "";
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Public RSAv1 host key for system";
    node->mrn_audit_descr =       N_("SSH public RSA v1 host key");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/hostkey/private/rsa1";
    node->mrn_value_type =         bt_binary;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal_secure;
    node->mrn_initial_value =      "";
    mdm_node_acl_add(node,         tacl_sbm_ssh_hi);
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Private RSAv1 host key for system";
    node->mrn_audit_descr =       N_("SSH private RSA v1 host key");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/hostkey/finger_print/rsa1";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Finger print of RSAv1 host key for system";
    node->mrn_mon_get_func =       md_ssh_hostkey_fp_get;
    node->mrn_mon_get_arg =        (void *) 1;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/hostkey/public/rsa2";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_initial_value =      "";
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Public RSAv2 host key for system";
    node->mrn_audit_descr =       N_("SSH public RSA v2 host key");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/hostkey/private/rsa2";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal_secure;
    node->mrn_initial_value =      "";
    mdm_node_acl_add(node,         tacl_sbm_ssh_hi);
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Private RSAv2 host key for system";
    node->mrn_audit_descr =       N_("SSH private RSA v2 host key");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/hostkey/finger_print/rsa2";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Finger print of RSAv2 host key for system";
    node->mrn_mon_get_func =       md_ssh_hostkey_fp_get;
    node->mrn_mon_get_arg =        (void *) 2;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/hostkey/public/dsa2";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_initial_value =      "";
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Public DSAv2 host key for system";
    node->mrn_audit_descr =       N_("SSH public DSA v2 host key");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/hostkey/private/dsa2";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal_secure;
    node->mrn_initial_value =      "";
    mdm_node_acl_add(node,         tacl_sbm_ssh_hi);
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Private DSAv2 host key for system";
    node->mrn_audit_descr =       N_("SSH private DSA v2 host key");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/server/hostkey/finger_print/dsa2";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Finger print of DSAv2 host key for system";
    node->mrn_mon_get_func =       md_ssh_hostkey_fp_get;
    node->mrn_mon_get_arg =        (void *) 3;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/client/username/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_node_reg_flags |=    mrf_acl_inheritable;
    mdm_node_acl_add(node,         tacl_sbm_aaa_lo);
    node->mrn_acl_func =           md_ssh_user_acl;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "User identity for key";
    node->mrn_audit_descr =       N_("SSH client record for user '$v$'");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/client/username/*/keytype/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_choices_str = md_ssh_user_key_type_choices;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_initial_value =      "";
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Key type for user identity";
    node->mrn_audit_descr =       N_("SSH client record for user '$4$': "
                                      "key type $v$");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/client/username/*/keytype/*/public";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_initial_value =      "";
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "A Public key for user identity";
    node->mrn_audit_descr =       N_("public key");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/client/username/*/keytype/*/private";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal_secure;
    node->mrn_initial_value =      "";
    /*
     * We are currently the only node using this ACL.
     * tacl_sbm_aaa is not right because its query privileges are
     * too lax for a private key.  tacl_sbm_aaa_hi is not right because
     * its set privileges are too restrictive.  sys_set_7 should be OK,
     * because we have a custom ACL function to limit whose you can set
     * to users with a subset of your auth groups.
     */
    mdm_node_acl_add(node,         tacl_sbm_aaa_medhi);
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "A Private key for user identity";
    node->mrn_audit_descr =       N_("private key");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/client/username/*/keytype/*/passphrase";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal_secure;
    node->mrn_initial_value =      "";
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "A Passphrase for user identity";
    node->mrn_audit_descr =       N_("passphrase");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/client/state/username/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_wildcard;
    node->mrn_initial_value =      "";
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "A user identity";
    node->mrn_mon_iterate_func =   md_ssh_iterate_users;
    node->mrn_mon_get_func =       md_ssh_get_user;
    node->mrn_mon_get_arg =     NULL;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/client/state/username/*/known-host/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_wildcard;
    node->mrn_initial_value =      "";
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "A known host for user identity";
    node->mrn_mon_iterate_func =   md_ssh_iterate_known_host;
    node->mrn_mon_get_func =       md_ssh_get_known_host;
    node->mrn_mon_get_arg =     NULL;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/client/global/hostkey-check";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_choices_str = md_ssh_client_hostkey_check_choices;
    node->mrn_initial_value =      "ask";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Global setting for hostkey checking";
    node->mrn_audit_descr =       N_("SSH global host key check policy");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/client/global/known-host/*";
    node->mrn_value_type =         bt_uint16;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Global known hosts entries";
    node->mrn_audit_descr =       N_("SSH global known host entry for $v$");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/client/global/known-host/*/entry";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Global known host entry";
    node->mrn_audit_descr =       N_("key");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/ssh/client/state/global/known-host/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_wildcard;
    node->mrn_initial_value =      "";
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "A global known host";
    node->mrn_mon_iterate_func =   md_ssh_iterate_global_known_host;
    node->mrn_mon_get_func =       md_ssh_get_global_known_host;
    node->mrn_mon_get_arg =        NULL;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               
        "/ssh/client/state/global/known-host/*/finger_print";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_privileged;
    node->mrn_description =        "Finger print of a global known host";
    node->mrn_mon_get_func =       md_ssh_get_global_known_host_fp;
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Actions */

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name = "/ssh/hostkey/generate";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    mdm_node_acl_add(node, tacl_sbm_ssh_hi_sa);
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_config_func = md_ssh_action_gen_hostkey;
    node->mrn_action_arg = NULL;
    node->mrn_action_audit_descr =
        N_("regenerate SSH host keys");
    node->mrn_actions[0]->mra_param_name = "key_type";
    node->mrn_actions[0]->mra_param_audit_descr = N_("key type");
    /* XXX possible values: rsa1, rsa2, dsa2, all */
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_default_value_str = "all";
    node->mrn_description = "Generate new host keys";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name = "/ssh/keypair/generate";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    mdm_node_acl_add(node, tacl_sbm_aaa_lo);
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_config_func = md_ssh_action_gen_keypair;
    node->mrn_action_arg = NULL;
    node->mrn_action_audit_descr = N_("generate SSH key pair");
    /*
     * We have to log something different for the bn_prefix binding,
     * including a different description, depending on what we find there.
     */
    node->mrn_action_audit_func = md_ssh_key_gen_audit;
    node->mrn_actions[0]->mra_param_name = "key_type";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_required = true;
    node->mrn_actions[1]->mra_param_name = "bn_prefix";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_required = true;
    node->mrn_description = "Generate new key pair of type";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 3);
    bail_error(err);
    node->mrn_name = "/ssh/known_host/remove";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    mdm_node_acl_add(node, tacl_sbm_ssh_hi);
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_func = md_ssh_action_remove_known_host;
    node->mrn_action_arg = NULL;
    /*
     * We have to log something different depending on whether
     * known_hosts_path is provided.
     */
    node->mrn_action_audit_func = md_ssh_kh_remove_audit;
    /* Either user or known_hosts_path must be present, but not both */
    node->mrn_actions[0]->mra_param_name = "user";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "host";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_required = true;
    node->mrn_actions[2]->mra_param_name = "known_hosts_path";
    node->mrn_actions[2]->mra_param_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 3);
    bail_error(err);
    node->mrn_name = "/ssh/known_host/remove_safe";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    mdm_node_acl_add(node, tacl_sbm_ssh);
    /* Custom func also allows taag_cmc_action_7 if cmc_known_hosts set */
    node->mrn_acl_func = md_ssh_remove_known_host_acl;
    node->mrn_cap_mask = mcf_cap_action_restricted;
    node->mrn_action_func = md_ssh_action_remove_known_host_safe;
    /* Either user or cmc_known_hosts must be present, but not both */
    node->mrn_action_audit_descr =
        N_("remove SSH known host entry");
    node->mrn_actions[0]->mra_param_name = "user";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_audit_descr = _("username");
    node->mrn_actions[1]->mra_param_name = "host";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_required = true;
    node->mrn_actions[1]->mra_param_audit_descr = _("hostname");
    node->mrn_actions[2]->mra_param_name = "cmc_known_hosts";
    node->mrn_actions[2]->mra_param_type = bt_bool;
    node->mrn_actions[2]->mra_param_audit_descr = _("CMC known host entry");
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Upgrade processing nodes */

    err = md_upgrade_rule_array_new(&md_ssh_ug_rules);
    bail_error(err);
    ra = md_ssh_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name = "/ssh/server/listen/enable";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str = "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =     MUTT_REPARENT;
    rule->mur_name =
        "/ssh/client/username/*/private";
    rule->mur_reparent_graft_under_name =
        "/ssh/client/username/*/keytype/dsa2";
    rule->mur_reparent_new_self_name = NULL;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =     MUTT_REPARENT;
    rule->mur_name =
        "/ssh/client/username/*/public";
    rule->mur_reparent_graft_under_name =
        "/ssh/client/username/*/keytype/dsa2";
    rule->mur_reparent_new_self_name = NULL;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =     MUTT_REPARENT;
    rule->mur_name =
        "/ssh/client/username/*/passphrase";
    rule->mur_reparent_graft_under_name =
        "/ssh/client/username/*/keytype/dsa2";
    rule->mur_reparent_new_self_name = NULL;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =     MUTT_REPARENT;
    rule->mur_name =
        "/ssh/server/username/*/auth-key/rsa/sshv2/*";
    rule->mur_reparent_graft_under_name =
        "/ssh/server/username/*/auth-key/sshv2";
    rule->mur_reparent_new_self_name = NULL;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name = "/ssh/server/min-version";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "2";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name = "/ssh/client/global/hostkey-check";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "ask";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name = "/ssh/server/port/22";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "22";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =        MUTT_ADD;
    rule->mur_name = "/ssh/server/x11_forwarding/enable";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str = "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =             "/ssh/server/enable";
    rule->mur_new_value_type =   bt_bool;
    rule->mur_copy_from_node =   "/pm/process/sshd/auto_launch";
    MD_ADD_RULE(ra);

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
int
md_ssh_deinit(const lc_dso_info *info, void *data)
{
    int err = 0;

    md_upgrade_rule_array_free(&md_ssh_ug_rules);

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_hostkey_bindings(const char *priv_key_path, const char *pub_key_path,
                        const char *key_type, mdb_db *new_db)
{
    int err = 0;
    tbool priv_file_exists = false, pub_file_exists = false;
    tbool do_generate = true;
    char *priv_key_data = NULL, *pub_key_data = NULL;
    tbuf *priv_key_buf = NULL;
    uint32 priv_key_len = 0, pub_key_len = 0;
    char *bn_name = NULL;
    bn_binding *new_binding = NULL;
    tbool priv_key_binary = false;

    bail_null(new_db);
    bail_null(key_type);

    priv_key_binary = strcmp(key_type, "rsa1") == 0 ? true : false;

    if (priv_key_path && pub_key_path) {
        /*
         * If given private and public key locations, try to set the
         * associated bindings from the contents of these files.
         */

        err = lf_test_exists(priv_key_path, &priv_file_exists);
        bail_error(err);

        err = lf_test_exists(pub_key_path, &pub_file_exists);
        bail_error(err);

        if (priv_file_exists && pub_file_exists) {
            if (priv_key_binary) {
                err = lf_read_file_rbuf(priv_key_path, -1, &priv_key_data,
                                        &priv_key_len);
                bail_error(err);
            }
            else {
                err = lf_read_file_str(priv_key_path, -1, &priv_key_data,
                                       NULL);
                bail_error(err);
            }

            err = lf_read_file_str(pub_key_path, -1, &pub_key_data, NULL);
            bail_error(err);

            if (priv_key_data && pub_key_data) {
                do_generate = false;
            }
        }
    }

    if (do_generate) {
        lc_log_basic(LOG_NOTICE, "Generating new hostkey of type %s",
                     key_type);

        /* 
         * Either couldn't read the information or it is desired
         * to create new random keys.
         */
        if (strcmp(key_type, "rsa1") == 0) {
            err = lc_ssh_util_keygen("rsa1", NULL, &pub_key_data, &pub_key_len,
                                     &priv_key_data, &priv_key_len);
            bail_error(err);
        }
        else if (strcmp(key_type, "rsa2") == 0) {
            err = lc_ssh_util_keygen("rsa", NULL, &pub_key_data, &pub_key_len,
                                     &priv_key_data, &priv_key_len);
            bail_error(err);
        }
        else if (strcmp(key_type, "dsa2") == 0) {
            err = lc_ssh_util_keygen("dsa", NULL, &pub_key_data, &pub_key_len,
                                     &priv_key_data, &priv_key_len);
            bail_error(err);
        }
    }

    /* Must have these by now */
    bail_require(priv_key_data);
    bail_null(pub_key_data);

    bn_name = smprintf("/ssh/server/hostkey/private/%s", key_type);
    bail_null(bn_name);

    if (priv_key_binary) {
        err = tb_new_buf(&priv_key_buf, (const uint8 *) priv_key_data,
                         (int32) priv_key_len);
        bail_error(err);

        err = bn_binding_new_binary(&new_binding, bn_name, ba_value,
                                    0, priv_key_buf);
        bail_error_null(err, new_binding);
    }
    else {
        err = bn_binding_new_str(&new_binding, bn_name, ba_value,
                                 bt_string, 0, priv_key_data);
        bail_error_null(err, new_binding);
    }

    err = mdb_set_node_binding(NULL, new_db, bsso_modify, 0, new_binding);
    bail_error(err);

    safe_free(bn_name);
    bn_binding_free(&new_binding);

    bn_name = smprintf("/ssh/server/hostkey/public/%s", key_type);
    bail_null(bn_name);

    err = bn_binding_new_str(&new_binding, bn_name, ba_value,
                             bt_string, 0, pub_key_data);
    bail_error_null(err, new_binding);

    err = mdb_set_node_binding(NULL, new_db, bsso_modify, 0, new_binding);
    bail_error(err);

    safe_free(bn_name);
    bn_binding_free(&new_binding);

 bail:
    safe_free(priv_key_data);
    safe_free(pub_key_data);
    safe_free(bn_name);
    tb_free(&priv_key_buf);
    bn_binding_free(&new_binding);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_upgrade_downgrade(const mdb_db *old_db,
                         mdb_db *inout_new_db,
                         uint32 from_module_version,
                         uint32 to_module_version, void *arg)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    uint32 port_num = 0;
    const tstring *bname = NULL;
    bn_binding *del_binding = NULL;
    int num_bindings = 0, i = 0;
    const bn_binding *binding = NULL;

    /*
     * For most upgrade/downgrade scenarios, we just want the basic
     * behavior...
     */
    err = md_generic_upgrade_downgrade
        (old_db, inout_new_db, from_module_version, to_module_version, arg);
    bail_error(err);

    if (from_module_version == 2 && to_module_version == 3) {
        err = md_ssh_hostkey_bindings(rsa1_host_key_path,
                                      rsa1_host_pub_key_path,
                                      "rsa1", inout_new_db);
        bail_error(err);

        err = md_ssh_hostkey_bindings(rsa2_host_key_path,
                                      rsa2_host_pub_key_path,
                                      "rsa2", inout_new_db);
        bail_error(err);

        err = md_ssh_hostkey_bindings(dsa2_host_key_path,
                                      dsa2_host_pub_key_path,
                                      "dsa2", inout_new_db);
        bail_error(err);
    }

    if (from_module_version == 7 && to_module_version == 8) {
        /*
         * Can't use mdb_iterate_binding_cb(), since we need to be able
         * to delete bindings, and it only passes us a const db pointer.
         */
        err = mdb_iterate_binding(NULL, inout_new_db, "/ssh/server/port", 0,
                                  &bindings);
        bail_error(err);
        num_bindings = bn_binding_array_length_quick(bindings);
        for (i = 0; i < num_bindings; ++i) {
            binding = bn_binding_array_get_quick(bindings, i);
            bail_null(binding);
            err = bn_binding_get_uint32(binding, ba_value, NULL, &port_num);
            bail_error(err);
            if (port_num == 0 || port_num > UINT16_MAX) {
                lc_log_basic(LOG_NOTICE, "Deleting illegal ssh port "
                             "number %" PRId32, port_num);
                err = bn_binding_get_name(binding, &bname);
                bail_error(err);
                bn_binding_free(&del_binding);
                err = bn_binding_new_named(&del_binding, ts_str(bname));
                bail_error(err);
                err = mdb_set_node_binding(NULL, inout_new_db, bsso_delete, 0,
                                           del_binding);
                bail_error(err);
            }
        }
    }   
#if 0
    /*
     * XXX/GOPU: This upgrade would be necessary later, hence commenting
     */
    else if (from_module_version == 8 && to_module_version == 9) {
        err = mdb_iterate_binding(NULL, inout_new_db,
                    "/ssh/server/listen/interface", 0, &bindings);
        bail_error(err);

        num_bindings = bn_binding_array_length_quick(bindings);
        for (i = 0; i < num_bindings; i++) {
            binding = bn_binding_array_get_quick(bindings, i);
            bail_null(binding);

            err = bn_binding_get_name(binding, &bname);
            bail_error(err);

            err = md_remove_node_iff_bad_interface(NULL, inout_new_db,
                        ts_str(bname), NULL);
            bail_error(err);
        }
    }
#endif /* if 0 */
    
 bail:
    bn_binding_free(&del_binding);
    bn_binding_array_free(&bindings);
    return(err);
}

/* ------------------------------------------------------------------------- */
static const bn_str_value md_ssh_initial_values[] = {
    {"/ssh/server/port/22", bt_uint32, "22"}
};

/* ------------------------------------------------------------------------- */
static int
md_ssh_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    char *key_data = NULL;
    bn_binding *binding = NULL;
    bn_binding *mfg_binding = NULL;
    mdb_db *md_mfdb = NULL;
    const char *hostkey_bn_name = NULL;
    uint32 i = 0, num_bns = 0;

    err = mdb_create_node_bindings
        (commit, db, md_ssh_initial_values, 
         sizeof(md_ssh_initial_values) / sizeof(bn_str_value));
    bail_error(err);

    /*
     * REM/GS XXX believe this 'exporting' of the mfdb hostkey
     * bindings should not be necessary if other infrastructure
     * work is later done to make this more automatic, but for
     * now work around getting the values created at firstboot
     * time to be used as the basis for the configuration and
     * avoid generating new hostkey bindings each time a new
     * configuration is created.
     *
     * XXXX/EMT: this is not documented in mfdb-nodes.txt!
     * And it violates our convention that all nodes in the mfdb
     * are under /mfg/mfdb.
     */
    err = mdb_dbbe_open(&md_mfdb, NULL, md_mfdb_path);
    if (err == 0) {
        /*
         * Host key binding names are identical in the mfdb and
         * config db.
         */ 
        num_bns = sizeof(md_ssh_mfg_bindings) / sizeof(md_ssh_mfg_bindings[0]);
        for (i = 0; i < num_bns; ++i) {
            hostkey_bn_name = md_ssh_mfg_bindings[i];

            bn_binding_free(&mfg_binding);
            err = mdb_get_node_binding(NULL, md_mfdb, hostkey_bn_name,
                                       0, &mfg_binding);
            complain_error(err);

            if (mfg_binding) {
                err = mdb_set_node_binding(NULL, db, bsso_modify, 0,
                                           mfg_binding);
                bail_error(err);
            }
        }
    }
    else {
        /* Don't complain if no mfdb, but note it otherwise */
        if (err != lc_err_file_not_found) {
            lc_log_basic(LOG_WARNING, I_("Problem getting values from the "
                         "manufacture db"));
        }
        err = 0;
    }

    /*
     * Based on the existence of the public, decide if a new key
     * pair for a particular key type needs to be generated.
     */

    bn_binding_free(&binding);
    err = mdb_get_node_binding(commit, db, "/ssh/server/hostkey/public/rsa2",
                               0, &binding);
    bail_error(err);

    if (binding) {
        err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
                                 &key_data);
        bail_error(err);

        if (!key_data || (strlen(key_data) == 0)) {
            err = md_ssh_hostkey_bindings(NULL, NULL, "rsa2", db);
            bail_error(err);
        }
        safe_free(key_data);
    }

    bn_binding_free(&binding);
    err = mdb_get_node_binding(commit, db, "/ssh/server/hostkey/public/rsa1",
                               0, &binding);
    bail_error(err);

    if (binding) {
        err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
                                 &key_data);
        bail_error(err);

        if (!key_data || (strlen(key_data) == 0)) {
            err = md_ssh_hostkey_bindings(NULL, NULL, "rsa1", db);
            bail_error(err);
        }
        safe_free(key_data);
    }

    bn_binding_free(&binding);
    err = mdb_get_node_binding(commit, db, "/ssh/server/hostkey/public/dsa2",
                               0, &binding);
    bail_error(err);

    if (binding) {
        err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
                                 &key_data);
        bail_error(err);

        if (!key_data || (strlen(key_data) == 0)) {
            err = md_ssh_hostkey_bindings(NULL, NULL, "dsa2", db);
            bail_error(err);
        }
        safe_free(key_data);
    }

 bail:
    safe_free(key_data);
    bn_binding_free(&mfg_binding);
    bn_binding_free(&binding);
    mdb_dbbe_close(&md_mfdb);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_commit_check(md_commit *commit, const mdb_db *old_db, 
                    const mdb_db *new_db, mdb_db_change_array *change_list,
                    void *arg)
{
    int err = 0;
    uint32 num_changes = 0, i = 0;
    mdb_db_change *change = NULL;
    const char *username = NULL, *intf = NULL;
    const char *key_type = NULL, *key_kind = NULL;
    tstring *home_dir = NULL, *hostkey_value = NULL;
    tbool intf_ok = false, is_public = false;
    bn_binding_array *port_bindings = NULL;
    char *type = NULL, *key = NULL;
    char host_str[255], ip_str[255];
    bn_attrib *attrib = NULL;
    const bn_attrib *attrib_const = NULL;
    uint32 port_num = 0;

    if (!lc_devel_is_production()) {
        lc_log_basic(LOG_NOTICE, "Devenv: skipping ssh checks");
        goto bail;
    }
    
    /* XXX need to check that all the keys are unique?? */

    /* ........................................................................
     * Check #1: must have at least one SSH server port
     */
    err = mdb_iterate_binding(commit, new_db, "/ssh/server/port",
                              mdqf_sel_iterate_include_self,
                              &port_bindings);
    bail_error(err);

    if (!port_bindings ||
        (bn_binding_array_length_quick(port_bindings) == 0)) {
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Must specify at least one SSH server port"));
        bail_error(err);
        goto bail;
    }

    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; ++i) {
        change = mdb_db_change_array_get_quick(change_list, i);
        bail_null(change);

        if (!ts_has_prefix_str(change->mdc_name, "/ssh/", false)) {
            continue;
        }

        /* ....................................................................
         * Check #2: ssh server port numbers must be within range
         */
        if (change->mdc_change_type == mdct_add &&
            bn_binding_name_parts_pattern_match_va
            (change->mdc_name_parts, 0, 4, "ssh", "server", "port", "*")) {
            bail_null(change->mdc_new_attribs);
            attrib_const = bn_attrib_array_get_quick(change->mdc_new_attribs, 
                                                     ba_value);
            complain_null(attrib_const);
            if (attrib_const) {
                err = bn_attrib_get_uint32(attrib_const, NULL, NULL,
                                           &port_num);
                bail_error(err);
                if (port_num == 0 || port_num > UINT16_MAX) {
                    err = md_commit_set_return_status_msg_fmt
                        (commit, 1, 
                         _("Ssh port number must be between 1 and 65535, "
                           "inclusive"));
                    bail_error(err);
                }
            }
        }

        if (!ts_has_prefix_str(change->mdc_name, "/ssh/server/username/",
                               false)
            && !ts_has_prefix_str(change->mdc_name, "/ssh/client/username/",
                                  false)) {

            /* ................................................................
             * Check #3: listen interfaces must be valid names
             */
            if (bn_binding_name_parts_pattern_match
                (change->mdc_name_parts, true,
                 "/ssh/server/listen/interface/*")) {
                if (change->mdc_change_type != mdct_delete) {
                    intf = tstr_array_get_str_quick(change->mdc_name_parts, 4);
                    bail_null(intf);

                    err = lc_ifname_validate(intf, &intf_ok);
                    bail_error(err);
                    if (!intf_ok) {
                        err = md_commit_set_return_status_msg_fmt
                            (commit, 1,
                             _("The interface name '%s' is not valid."), intf);
                        bail_error(err);
                        goto bail;
                    }
                }
            }

            /* ................................................................
             * Check #4: known host entry validity.
             *
             * (XXX/EMT: bug 11521)
             */
            if ((change->mdc_change_type != mdct_delete) &&
                (bn_binding_name_parts_pattern_match(change->mdc_name_parts,
                                   true,
                                   "/ssh/client/global/known-host/*/entry"))) {
                ts_free(&hostkey_value);
                err = mdb_get_node_value_tstr(commit, new_db,
                                              ts_str(change->mdc_name), 0,
                                              NULL, &hostkey_value);
                bail_error(err);

                if (strchr(ts_str(hostkey_value), '\n')) {
                    err = md_commit_set_return_status_msg_fmt
                        (commit, 1, _("Invalid host key entry, contains a "
                                      "newline character."));
                    bail_error(err);
                    goto bail;
                }

                safe_free(type);
                safe_free(key);

                err = lc_ssh_util_parse_hostkey_entry(hostkey_value, host_str,
                                                      sizeof(host_str), ip_str,
                                                      sizeof(ip_str),
                                                      &type, &key);
                bail_error(err);

                if (!host_str[0] || !type || !key) {
                    err = md_commit_set_return_status_msg_fmt
                        (commit, 1, _("Invalid host key entry (%s)."),
                         host_str);
                    bail_error(err);
                    goto bail;
                }

                if (strlen(host_str) > 0) {
                    bn_attrib_free(&attrib);
                    err = bn_attrib_new_str(&attrib, ba_value,
                                            bt_hostname, 0, host_str);
                    if (err) {
                        err = md_commit_set_return_status_msg_fmt
                            (commit, 1, _("Invalid host (%s) for host key."),
                             host_str);
                        bail_error(err);
                        goto bail;
                    }
                }

                if (strlen(ip_str) > 0) {
                    bn_attrib_free(&attrib);
                    err = bn_attrib_new_str(&attrib, ba_value,
                                            bt_ipv4addr, 0, ip_str);
                    if (err) {
                        err = md_commit_set_return_status_msg_fmt
                            (commit, 1, _("Invalid IP address (%s) for "
                                          "host key."), ip_str);
                        bail_error(err);
                        goto bail;
                    }
                }

                if (!lc_is_prefix("ssh-", type, false)) {
                    err = md_commit_set_return_status_msg_fmt
                        (commit, 1, _("Invalid key type (%s) for "
                                      "host key."), type);
                    bail_error(err);
                    goto bail;
                }
            }
           
            /* ................................................................
             * Check #5: host key validity
             */
            if ((change->mdc_change_type != mdct_delete) &&
                ts_has_prefix_str(change->mdc_name, "/ssh/server/hostkey/",
                                  false)) {
                key_kind = tstr_array_get_str_quick(change->mdc_name_parts, 3);
                bail_null(key_kind);

                is_public = (safe_strcmp(key_kind, "public") == 0)
                    ? true : false;

                key_type = tstr_array_get_str_quick(change->mdc_name_parts, 4);
                bail_null(key_type);

                ts_free(&hostkey_value);
                err = mdb_get_node_value_tstr(commit, new_db,
                                              ts_str(change->mdc_name), 0,
                                              NULL, &hostkey_value);
                bail_error(err);

                /*
                 * Permit any of these known host keys to be empty.
                 * This is not a desirable state to be in, but they
                 * are the initial values, and we could get here if we
                 * failed to generate the known host keys in the side
                 * effects function.  And we don't want that to cause
                 * the initial commit to fail.
                 */
                if (hostkey_value == NULL || 
                    ts_num_chars(hostkey_value) <= 0) {
                    continue;
                }

                if (safe_strcmp(key_type, "rsa1") == 0) {
                    /* XXX what can we verify? */
                }
                else if (safe_strcmp(key_type, "rsa2") == 0) {
                    if (is_public) {
                        if (!ts_has_prefix_str(hostkey_value, "ssh-rsa",
                                               false)) {
                            err = md_commit_set_return_status_msg
                                (commit, 1, _("Invalid rsa2 public key."));
                            bail_error(err);
                            goto bail;
                        }
                    }
                    else {
                        if (!ts_has_prefix_str(hostkey_value,
                                         "-----BEGIN RSA PRIVATE KEY-----",
                                               false)) {
                            err = md_commit_set_return_status_msg
                                (commit, 1, _("Invalid rsa2 private key."));
                            bail_error(err);
                            goto bail;
                        }
                    }
                }
                else if (safe_strcmp(key_type, "dsa2") == 0) {
                    if (is_public) {
                        if (!ts_has_prefix_str(hostkey_value, "ssh-dss",
                                               false)) {
                            err = md_commit_set_return_status_msg
                                (commit, 1, _("Invalid dsa2 public key."));
                            bail_error(err);
                            goto bail;
                        }
                    }
                    else {
                        if (!ts_has_prefix_str(hostkey_value,
                                         "-----BEGIN DSA PRIVATE KEY-----",
                                               false)) {
                            err = md_commit_set_return_status_msg
                                (commit, 1, _("Invalid dsa2 private key."));
                            bail_error(err);
                            goto bail;
                        }
                    }
                }
            }
            continue;
        }

        if (change->mdc_change_type == mdct_delete) {
            /* Don't verify on things being deleted */
            continue;
        }

        /* ....................................................................
         * Check #6: verify usernames
         *
         * We only verify a valid user for keys that will be added
         * to the authorized key list.
         */
        username = tstr_array_get_str_quick(change->mdc_name_parts, 3);
        bail_null(username);

        ts_free(&home_dir);
        err = md_find_home_dir(commit, new_db, username, &home_dir,
                               NULL, NULL);
        bail_error(err);

        if (home_dir == NULL) {
            err = md_commit_set_return_status_msg_fmt
                (commit, 1, _("The account %s does not exist locally."),
                 username);
            bail_error(err);
            goto bail;
        }

	if(strncmp(username,"LogTransferUser",15) == 0){
	    if (!ts_has_prefix_str(home_dir, LOG_EXPORT_HOME, false)) {
		err = md_commit_set_return_status_msg_fmt
		    (commit, 1, _("The account %s cannot have identities."),
		     username);
		bail_error(err);
		goto bail;
	    }
	} else if (!ts_has_prefix_str(home_dir, "/var/home/", false)) {
            err = md_commit_set_return_status_msg_fmt
                (commit, 1, _("The account %s cannot have identities."),
                 username);
            bail_error(err);
            goto bail;
        }

        if (md_passwd_is_resv_acct(username)) {
            err = md_commit_set_return_status_msg_fmt
                (commit, 1,
                 _("The reserved account %s cannot have identites."),
                 username);
            bail_error(err);
            goto bail;
        }
    }

 bail:
    safe_free(type);
    safe_free(key);
    bn_attrib_free(&attrib);
    bn_binding_array_free(&port_bindings);
    ts_free(&home_dir);
    ts_free(&hostkey_value);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_commit_apply(md_commit *commit, const mdb_db *old_db,
                    const mdb_db *new_db, 
                    mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    uint32 num_changes = 0, i = 0;
    mdb_db_change *change = NULL;
    tstr_array *parts = NULL;
    const char *username = NULL;
    tbool write_sshd_config = false;
    tbool write_ssh_known_hosts = false;
    tbool write_ssh_config = false;
    tbool listen_active = false;
    tbool restart_ssh = false, host_key_update = false;
    tbool user_enabled = false;
    tstr_array *relevant_intf_names = NULL;
    tbool if_configured = false;
    const char *ifname = NULL;
    tstr_array *listen_intf_addrs = NULL;
    bn_binding *binding = NULL;
    char *user_enable_bn = NULL;
    bn_binding_array *host_bindings = NULL;
    tstring *hostkeycheck = NULL;

    /*
     * Remove any existing key files from deleted users.
     * XXX should account for other key file names instead
     * of hard coded.
     */
    err = md_net_utils_get_listen_config(commit, old_db, new_db,
                                         "/ssh/server/listen/enable",
                                         "/ssh/server/listen/interface", true,
                                         &listen_active, &relevant_intf_names,
                                         NULL, &listen_intf_addrs);
    bail_error(err);

    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; ++i) {
        change = mdb_db_change_array_get_quick(change_list, i);
        bail_null(change);

        /*
         * We used to watch for /net/interface here, but this is no
         * longer necessary because we listen for the interface state
         * change event instead.
         */

        if (ts_equal_str(change->mdc_name,
                         "/aaa/tally/config/enable", false)) {
            write_sshd_config = true;
            continue;
        }

        if (!ts_has_prefix_str(change->mdc_name, "/ssh/", false) &&
            !ts_has_prefix_str(change->mdc_name, "/auth/passwd/user/",
                               false)) {
            continue;
        }

        if (!ts_has_prefix_str(change->mdc_name, "/ssh/client/username/",
                               false)
            && !ts_has_prefix_str(change->mdc_name, "/ssh/server/",
                                  false)) {
            
            if (ts_has_prefix_str(change->mdc_name,
                                  "/ssh/client/global/known-host",
                                  false)) {
                write_ssh_known_hosts = true;
                continue;
            }
            
            if (ts_has_prefix_str(change->mdc_name,
                                  "/ssh/client/global/hostkey-check",
                                  false)) {
                write_ssh_config = true;
                continue;
            }

            if (md_net_utils_relevant_for_listen_quick
                (change->mdc_name_parts, relevant_intf_names)) {
                write_sshd_config |= listen_active;
            }

            if (bn_binding_name_parts_pattern_match_va
                (change->mdc_name_parts, 0, 5,
                 "auth", "passwd", "user", "*", "enable")) {
                if (change->mdc_change_type == mdct_delete) {
                    /* md_passwd.c will have removed the
                     * assoicated ssh bindings */
                    continue;
                }

                err = mdb_get_node_value_tbool(commit, new_db,
                                               ts_str(change->mdc_name), 0,
                                               NULL, &user_enabled);
                bail_error(err);

                username = tstr_array_get_str_quick(change->mdc_name_parts, 3);
                bail_null(username);

                if (user_enabled) {
                    err = md_write_user_key_file
                        (commit, new_db, username, "/ssh/server/username",
                         "public", 
                         "/ssh/server/username/%s/auth-key/sshv2",
                         "authorized_keys2", NULL, true);
                    bail_error(err);

                    err = md_write_user_key_file
                        (commit, new_db, username, "/ssh/client/username",
                         "keytype/dsa2/private", NULL, "id_dsa", NULL, true);
                    bail_error(err);

                    err = md_write_user_key_file
                        (commit, new_db, username, "/ssh/client/username",
                         "keytype/rsa2/private", NULL, "id_rsa", NULL, true);
                    bail_error(err);
                }
                else {
                    /*
                     * If we get here, we know the user record still
                     * exists, so we can use the new_db.  Cannot
                     * always use the old_db, as it will not exist
                     * during the initial commit.
                     */
                    err = md_remove_user_key_files(commit, new_db, username);
                    bail_error(err);
                }
            }
            continue;
        }

        if (ts_has_prefix_str(change->mdc_name, "/ssh/server/hostkey/",
                              false)) {
            host_key_update = true;
            continue;
        }

        if (ts_has_prefix_str(change->mdc_name, "/ssh/server/port/",
                              false)) {
            write_sshd_config = true;
            continue;
        }

        if (ts_has_prefix_str(change->mdc_name, "/ssh/server/min-version",
                              false)) {
            write_sshd_config = true;
            continue;
        }

        if (ts_has_prefix_str(change->mdc_name, "/ssh/server/listen/",
                              false)) {
            write_sshd_config = true;
            continue;
        }

        if (ts_equal_str(change->mdc_name,
                         "/ssh/server/x11_forwarding/enable", false)) {
            write_sshd_config = true;
            continue;
        }

        /* The rest of this loop is all about 'username' bindings */
        if (!ts_has_prefix_str(change->mdc_name, "/ssh/server/username/",
                               false) &&
            !ts_has_prefix_str(change->mdc_name, "/ssh/client/username/",
                               false)) {
            continue;
        }

        tstr_array_free(&parts);
        err = bn_binding_name_to_parts(ts_str(change->mdc_name), &parts, NULL);
        bail_error(err);
        username = tstr_array_get_str_quick(parts, 3);
        bail_null(username);

        /*
         * Only write out key file modifications based on adds, deletes,
         * or changes if user is enabled.
         * Key files mainly written out based on user enabled binding.
         */
        user_enable_bn = smprintf("/auth/passwd/user/%s/enable", username);
        bail_null(user_enable_bn);

        err = mdb_get_node_value_tbool(commit, new_db, user_enable_bn,
                                       0, NULL, &user_enabled);
        bail_error(err);

        safe_free(user_enable_bn);

        if (ts_has_prefix_str(change->mdc_name, "/ssh/server/", false)
            && user_enabled) {
            err = md_write_user_key_file
                (commit, new_db, username, "/ssh/server/username",
                 "public", "/ssh/server/username/%s/auth-key/sshv2",
                 "authorized_keys2", NULL, true);
            bail_error(err);
        }

        if (ts_has_prefix_str(change->mdc_name, "/ssh/client/", false)
            && user_enabled) {
            err = md_write_user_key_file
                (commit, new_db, username, "/ssh/client/username",
                 "keytype/dsa2/private", NULL, "id_dsa", NULL, true);
            bail_error(err);

            err = md_write_user_key_file
                (commit, new_db, username, "/ssh/client/username",
                 "keytype/rsa2/private", NULL, "id_rsa", NULL, true);
            bail_error(err);
        }
    }

    if (host_key_update) {
        /* If any host keys changed, write them all out */
        err = md_write_hostkey_file(commit, new_db, rsa1_host_key_path,
                                    rsa1_host_pub_key_path, "rsa1");
        bail_error(err);

        err = md_write_hostkey_file(commit, new_db, rsa2_host_key_path,
                                    rsa2_host_pub_key_path, "rsa2");
        bail_error(err);

        err = md_write_hostkey_file(commit, new_db, dsa2_host_key_path,
                                    dsa2_host_pub_key_path, "dsa2");
        bail_error(err);

        restart_ssh = true;
    }

    if (write_ssh_known_hosts) {
        err = mdb_iterate_binding(commit, new_db,
                                  "/ssh/client/global/known-host",
                                  0, &host_bindings);
        bail_error(err);

        err = md_ssh_write_known_hosts_file(host_bindings, commit, new_db);
        bail_error(err);
    }

    if (write_ssh_config) {
        err = mdb_get_node_value_tstr(commit, new_db, 
                                      "/ssh/client/global/hostkey-check",
                                      0, NULL, &hostkeycheck);
        bail_error(err);

        err = md_ssh_write_ssh_conf_file(hostkeycheck);
        bail_error(err);
    }

    if (write_sshd_config) {
        lc_log_basic(LOG_DEBUG, "md_ssh: reconfiguring during commit apply");
        err = md_ssh_reconfig_sshd(commit, old_db, new_db, restart_ssh,
                                   listen_active, listen_intf_addrs);
        bail_error(err);
    }

 bail:
    bn_binding_free(&binding);
    safe_free(user_enable_bn);
    ts_free(&hostkeycheck);
    tstr_array_free(&parts);
    tstr_array_free(&relevant_intf_names);
    tstr_array_free(&listen_intf_addrs);
    bn_binding_array_free(&host_bindings);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_write_sshd_conf_file(tstr_array *ip_addrs,
                            bn_binding_array *port_bindings,
                            tbool listen_active,
                            tbool aaa_tally_enabled,
                            tbool x11_forwarding_enabled, uint32 min_version,
                            tbool *ret_changed_file)
{
    int err = 0;
    tstring *filename_real = NULL;
    char *filename_temp = NULL;
    int fd_temp = -1;
    uint32 i = 0, num_ip_addrs = 0, num_bindings = 0;
    bn_binding *binding = NULL;
    uint32 port = 0;
    tbool match = false;

    /* ------------------------------------------------------------------------
     * Get the temporary output file set up
     */
    err = lf_path_from_va_str(&filename_real, true, 2,
                              md_gen_output_path, "sshd_config");
    bail_error(err);

    err = lf_temp_file_get_fd(ts_str(filename_real), 
                              &filename_temp, &fd_temp);
    bail_error(err);
    
    /*
     * No timestamp so we can detect if the file has not changed, and
     * thus avoid sending SIGHUP to sshd.
     */
    err = md_conf_file_write_header_opts(fd_temp, "md_ssh", "##",
                                         mchf_no_timestamp);
    bail_error(err);

    err = lf_write_bytes_printf
        (fd_temp, NULL,
         "SyslogFacility AUTH\n"
         "LogLevel INFO\n"
         "X11Forwarding %s\n"
         "PermitEmptyPasswords yes\n"
         "Banner /etc/issue.net\n"
         "UseDNS no\n"
         "UsePAM yes\n"

         /*
          * XXXX/EMT: bug 14542: all we want to do here is prevent
          * sshd from sidestepping PAM and checking with the passwd
          * file directly.  The PasswordAuthentication config option
          * is the way to do this.  Unfortunately this also disallows
          * the "password" protocol method, which we don't want to do.
          * So we limit the scope of this damage, only do this if 
          * the tally-by-name feature is enabled.
          *
          * We couldn't allow it even when just tracking is enabled,
          * because in the same way that it lets you sidestep
          * enforcement, it lets you sidestep tracking.  You could 
          * have unlimited tries at authenticating as a local user,
          * and none of the failures would be recorded as long as
          * you used the "password" method.
          */
         "PasswordAuthentication %s\n"

         "Subsystem       sftp    %s\n",
         x11_forwarding_enabled ? "yes" : "no",
         aaa_tally_enabled ? "no" : "yes",
         prog_sftp_server_path);
    bail_error(err);

    err = lf_write_bytes_printf
        (fd_temp, NULL,
         "IgnoreRhosts yes\n"
         "StrictModes yes\n"
         "PermitUserEnvironment no\n"
         "IgnoreUserKnownHosts yes\n"
         "HostbasedAuthentication no\n"
         "RhostsRSAAuthentication no\n");
    bail_error(err);

    if (min_version > 1) {
        err = lf_write_bytes_printf
            (fd_temp, NULL, "Protocol 2\n");
        bail_error(err);
    }
    else {
        err = lf_write_bytes_printf
            (fd_temp, NULL, "Protocol 1,2\n");
        bail_error(err);
    }

    /* IPv4 or both IPv4 and IPv6 */
    if (!md_proto_ipv6_supported()) {
        err = lf_write_bytes_printf
            (fd_temp, NULL, "AddressFamily inet\n");
        bail_error(err);
    }
    else {
        err = lf_write_bytes_printf
            (fd_temp, NULL, "AddressFamily any\n");
        bail_error(err);
    }

    err = bn_binding_array_length(port_bindings, &num_bindings);
    bail_error(err);

    for (i = 0; i < num_bindings; ++i) {
        err = bn_binding_array_get(port_bindings, i, &binding);
        bail_error(err);

        err = bn_binding_get_uint32(binding, ba_value, NULL, &port);
        bail_error(err);

        err = lf_write_bytes_printf
            (fd_temp, NULL, "Port %d\n", port);
        bail_error(err);
    }

    /*
     * Note that if listen wanted to be enabled (admin enabled, and at
     * least one eligible interface configured), but we have no usable
     * listen addresses, the call to md_net_utils_get_listen_config()
     * which yielded our 'ip_addrs' param, would have inserted the
     * loopback address.  So it's OK that an empty 'ip_addrs' yields
     * no listen restrictions.
     */
    if (listen_active && ip_addrs) {
        num_ip_addrs = tstr_array_length_quick(ip_addrs);
        for (i = 0; i < num_ip_addrs; i++) {
            err = lf_write_bytes_printf(fd_temp, NULL, "ListenAddress %s\n",
                                    ts_str(tstr_array_get_quick(ip_addrs, i)));
            bail_error(err);
        }
    }

    /* ........................................................................
     * See if the file is different.  If not, we don't need to do
     * anything to sshd.
     */
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

    /* ------------------------------------------------------------------------
     * Move the file into place
     */
    err = lf_temp_file_activate_fd(ts_str(filename_real), filename_temp, 
                                   &fd_temp,
                                   md_gen_conf_file_owner,
                                   md_gen_conf_file_group,
                                   md_gen_conf_file_mode, lao_backup_orig);
    bail_error(err);

 bail:
    safe_close(&fd_temp);
    ts_free(&filename_real);
    safe_free(filename_temp);
    if (ret_changed_file) {
        *ret_changed_file = !match;
    }
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_write_known_hosts_file(bn_binding_array *host_bindings,
                              md_commit *commit, const mdb_db *db)
{
    int err = 0;
    tstring *filename_real = NULL;
    char *filename_temp = NULL;
    int fd_temp = -1;
    uint32 i = 0, num_bindings = 0;
    bn_binding *binding = NULL;
    tstring *known_host = NULL;

    /* ------------------------------------------------------------------------
     * Get the temporary output file set up
     */
    err = lf_path_from_va_str(&filename_real, true, 2,
                              md_gen_output_path, "ssh_known_hosts");
    bail_error(err);

    err = lf_temp_file_get_fd(ts_str(filename_real), 
                              &filename_temp, &fd_temp);
    bail_error(err);
    
    err = md_conf_file_write_header(fd_temp, "md_ssh", "##");
    bail_error(err);

    err = bn_binding_array_length(host_bindings, &num_bindings);
    bail_error(err);

    for (i = 0; i < num_bindings; ++i) {
        err = bn_binding_array_get(host_bindings, i, &binding);
        bail_error_null(err, binding);

        err = mdb_get_child_value_tstr(commit, db, "entry", binding,
                                       0, &known_host);
        bail_error(err);

        err = lf_write_bytes_printf
            (fd_temp, NULL, "%s\n", ts_str(known_host));
        bail_error(err);

        ts_free(&known_host);
    }

    /* ------------------------------------------------------------------------
     * Move the file into place
     */

    err = lf_temp_file_activate_fd(ts_str(filename_real), filename_temp, 
                                   &fd_temp,
                                   md_gen_conf_file_owner,
                                   md_gen_conf_file_group,
                                   md_gen_conf_file_mode, lao_backup_orig);
    bail_error(err);

 bail:
    safe_close(&fd_temp);
    ts_free(&known_host);
    ts_free(&filename_real);
    safe_free(filename_temp);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_write_ssh_conf_file(tstring *hostkeycheck)
{
    int err = 0;
    tstring *filename_real = NULL;
    char *filename_temp = NULL;
    int fd_temp = -1;

    /* ------------------------------------------------------------------------
     * Get the temporary output file set up
     */
    err = lf_path_from_va_str(&filename_real, true, 2,
                              md_gen_output_path, "ssh_config");
    bail_error(err);

    err = lf_temp_file_get_fd(ts_str(filename_real), 
                              &filename_temp, &fd_temp);
    bail_error(err);
    
    err = md_conf_file_write_header(fd_temp, "md_ssh", "##");
    bail_error(err);

#if 0 /* REMXXX ssh_config doesn't support */
    "PermitLocalCommand no\n"
#endif

    err = lf_write_bytes_printf
        (fd_temp, NULL,
         "StrictHostKeyChecking %s\n",
         ts_str(hostkeycheck));
    bail_error(err);

    /* ------------------------------------------------------------------------
     * Move the file into place
     */

    err = lf_temp_file_activate_fd(ts_str(filename_real), filename_temp, 
                                   &fd_temp,
                                   md_gen_conf_file_owner,
                                   md_gen_conf_file_group,
                                   md_gen_conf_file_mode, lao_backup_orig);
    bail_error(err);

 bail:
    safe_close(&fd_temp);
    ts_free(&filename_real);
    safe_free(filename_temp);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_iterate_users(md_commit *commit, const mdb_db *db,
                     const char *parent_node_name,
                     const uint32_array *node_attrib_ids,
                     int32 max_num_nodes, int32 start_child_num,
                     const char *get_next_child,
                     mdm_mon_iterate_names_cb_func iterate_cb,
                     void *iterate_cb_arg, void *arg)
{
    int err = 0;
    uint32 i = 0, num_users = 0;
    bn_binding_array *user_binding_array = NULL;
    bn_binding *binding = NULL;
    tstring *account_name = NULL;

    err = mdb_iterate_binding(commit, db, "/auth/passwd/user",
                              0, &user_binding_array);
    bail_error(err);

    num_users = bn_binding_array_length_quick(user_binding_array);

    for (i = 0; i < num_users; ++i) {
        err = bn_binding_array_get(user_binding_array, i, &binding);
        bail_error_null(err, binding);

        ts_free(&account_name);
        err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
                                  &account_name);
        bail_error_null(err, account_name);

        if (ts_num_chars(account_name) > 0) {
            err = (*iterate_cb)(commit, db, ts_str(account_name),
                                iterate_cb_arg);
            bail_error(err);
        }
    }

 bail:
    ts_free(&account_name);
    bn_binding_array_free(&user_binding_array);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_get_user(md_commit *commit, const mdb_db *db,
                const char *node_name, 
                const bn_attrib_array *node_attribs,
                bn_binding **ret_binding, uint32 *ret_node_flags,
                void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *account = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);
    /* "/ssh/client/state/username/<user>" */
    bail_require(num_parts == 5); 
    account = tstr_array_get_str_quick(parts, 4);
    bail_null(account);

    /* XXX validate */

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
                             0, account);
    bail_error_null(err, *ret_binding);

 bail:
    tstr_array_free(&parts);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_iterate_known_host(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *user = NULL;
    char *user_dir_bn_name = NULL, *user_ssh_kh_path = NULL;
    tstring *user_dir = NULL;
    tbool kh_exists = false;
    lc_launch_params *params = NULL;
    lc_launch_result result = LC_LAUNCH_RESULT_INIT;
    tbool normal_exit = false;
    uint32 num_hosts = 0, i = 0;
    tstr_array *known_hosts = NULL;
    const tstring *host = NULL;

    err = bn_binding_name_to_parts(parent_node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);
    /* "/ssh/client/state/username/<user>/known-host" */
    bail_require(num_parts == 6); 
    user = tstr_array_get_str_quick(parts, 4);
    bail_null(user);

    err = lc_launch_params_get_defaults(&params);
    bail_error(err);

    err = ts_new_str(&(params->lp_path), rkn_path);
    bail_error(err);

    user_dir_bn_name = smprintf("/auth/passwd/user/%s/home_dir", user);
    bail_null(user_dir_bn_name);

    err = mdb_get_node_value_tstr(commit, db, user_dir_bn_name,
                                  0, NULL, &user_dir);
    bail_error(err);

    if (!user_dir) {
        goto bail;
    }

    user_ssh_kh_path = smprintf("%s/.ssh/known_hosts", ts_str(user_dir));
    bail_null(user_ssh_kh_path);

    err = lf_test_exists(user_ssh_kh_path, &kh_exists);
    bail_error(err);

    if (kh_exists == false) {
        goto bail;
    }

    err = tstr_array_new_va_str
        (&(params->lp_argv), NULL, 4, rkn_path, "-l", "-d", ts_str(user_dir));
    bail_error(err);

    params->lp_wait = true;
    params->lp_env_opts = eo_preserve_all;
    params->lp_log_level = LOG_INFO;
    params->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
    params->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;

    /* No base OS dependency: our own rkn.sh */
    err = lc_launch(params, &result);
    bail_error(err);

    err = lc_check_exit_status(result.lr_exit_status, &normal_exit, 
                               "%s", ts_str(params->lp_path));
    bail_error(err);

    if (!normal_exit || WEXITSTATUS(result.lr_exit_status)) {
        lc_log_basic(LOG_NOTICE, I_("#PROBLEM getting known hosts for %s"),
                     user);
        goto bail;
    }

    if (result.lr_captured_output &&
        ts_num_chars(result.lr_captured_output) > 2) {
        err = ts_tokenize(result.lr_captured_output, '\n', '\0', '\0', 0,
                          &known_hosts);
        bail_error_null(err, known_hosts);

        num_hosts = tstr_array_length_quick(known_hosts);
        for (i = 0; i < num_hosts; ++i) {
            host = tstr_array_get_quick(known_hosts, i);
            bail_null(host);

            if (ts_num_chars(host) > 0) {
                err = (*iterate_cb)(commit, db, ts_str(host), iterate_cb_arg);
                bail_error(err);
            }
        }
    }

 bail:
    tstr_array_free(&parts);
    ts_free(&user_dir);
    safe_free(user_dir_bn_name);
    safe_free(user_ssh_kh_path);
    tstr_array_free(&known_hosts);
    lc_launch_params_free(&params);
    lc_launch_result_free_contents(&result);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_get_known_host(md_commit *commit, const mdb_db *db,
                      const char *node_name, 
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding, uint32 *ret_node_flags,
                      void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *host = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);
    /* "/ssh/client/state/username/<user>/known-host/<host>" */
    bail_require(num_parts == 7); 
    host = tstr_array_get_str_quick(parts, 6);
    bail_null(host);

    /* XXX validate */

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
                             0, host);
    bail_error_null(err, *ret_binding);

 bail:
    tstr_array_free(&parts);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_iterate_global_known_host(md_commit *commit, const mdb_db *db,
                                 const char *parent_node_name,
                                 const uint32_array *node_attrib_ids,
                                 int32 max_num_nodes, int32 start_child_num,
                                 const char *get_next_child,
                                 mdm_mon_iterate_names_cb_func iterate_cb,
                                 void *iterate_cb_arg, void *arg)
{
    int err = 0;
    uint32 num_hosts = 0, i = 0;
    tstr_array *fps = NULL, *hosts = NULL;
    tstring *host = NULL;

    err = lc_ssh_util_keygen_finger_print(ssh_global_known_hosts_path,
                                          &fps, &hosts);
    bail_error(err);

    num_hosts = tstr_array_length_quick(hosts);
    for (i = 0; i < num_hosts; ++i) {
        host = tstr_array_get_quick(hosts, i);

        if (host && ts_num_chars(host) > 0) {
            err = (*iterate_cb)(commit, db, ts_str(host), iterate_cb_arg);
            bail_error(err);
        }
    }

 bail:
    tstr_array_free(&hosts);
    tstr_array_free(&fps);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_get_global_known_host(md_commit *commit, const mdb_db *db,
                             const char *node_name, 
                             const bn_attrib_array *node_attribs,
                             bn_binding **ret_binding, uint32 *ret_node_flags,
                             void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *host = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);
    /* "/ssh/client/state/global/known-host/<host>" */
    bail_require(num_parts == 6); 
    host = tstr_array_get_str_quick(parts, 5);
    bail_null(host);

    /* XXX validate */

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
                             0, host);
    bail_error_null(err, *ret_binding);

 bail:
    tstr_array_free(&parts);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_get_global_known_host_fp(md_commit *commit, const mdb_db *db,
                                const char *node_name, 
                                const bn_attrib_array *node_attribs,
                                bn_binding **ret_binding,
                                uint32 *ret_node_flags,
                                void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *entry = NULL, *fp = NULL;
    uint32 num_hosts = 0, i = 0;
    tstr_array *fps = NULL, *hosts = NULL;
    tstring *host = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);
    /* "/ssh/client/state/global/known-host/<host>/fingerprint" */
    bail_require(num_parts == 7); 
    entry = tstr_array_get_str_quick(parts, 5);
    bail_null(entry);

    err = lc_ssh_util_keygen_finger_print(ssh_global_known_hosts_path,
                                          &fps, &hosts);
    bail_error(err);

    num_hosts = tstr_array_length_quick(hosts);
    for (i = 0; i < num_hosts; ++i) {
        host = tstr_array_get_quick(hosts, i);

        if (host && strcmp(ts_str(host), entry) == 0) {
            fp = tstr_array_get_str_quick(fps, i);
            bail_null(fp);

            err = bn_binding_new_str(ret_binding, node_name, ba_value,
                                     bt_string, 0, fp);
            bail_error_null(err, *ret_binding);
        }
    }

 bail:
    tstr_array_free(&hosts);
    tstr_array_free(&fps);
    tstr_array_free(&parts);
    return(err);
}

/* ------------------------------------------------------------------------- */
/* This will regenerate one or more host keys (key_type may be "dsa2",
 * "rsa1", "rsa2", or "all").  set_direct specifies the mechanism it
 * uses to set these back into the db: if 'true', we use the mdb
 * interface; if 'false', we make up a set request and pass it to
 * md_commit_handle_commit_from_module().
 */
static int
md_ssh_gen_hostkey(md_commit *commit, mdb_db *db, const char *key_type,
                   tbool set_direct)
{
    int err = 0;
    bn_request *req = NULL;
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL; 
    char *pub_key_data = NULL, *priv_key_data = NULL;
    tbuf *priv_key_buf = NULL;
    uint32 pub_key_len = 0, priv_key_len = 0;

    if (!set_direct) {
        err = bn_set_request_msg_create(&req, 0);
        bail_error(err);
    }
    else {
        bail_null(db);
    }

    if ((strcmp(key_type, "all") == 0) ||
        (strcmp(key_type, "dsa2") == 0)) {
        lc_log_basic(LOG_NOTICE, "Generating new hostkey of type dsa2");

        err = lc_ssh_util_keygen("dsa", NULL, &pub_key_data, &pub_key_len,
                                 &priv_key_data, &priv_key_len);
        bail_error(err);

        err = bn_binding_new_str(&binding, "/ssh/server/hostkey/public/dsa2",
                                 ba_value, bt_string, 0, pub_key_data);
        bail_error(err);
    
        if (set_direct) {
            err = mdb_set_node_binding(commit, db, bsso_modify, 0,
                                       binding);
            bail_error(err);
        }
        else {
            err = bn_set_request_msg_append(req, bsso_modify, 0, binding,
                                            NULL);
            bail_error(err);
        }

        bn_binding_free(&binding);

        err = bn_binding_new_str(&binding, "/ssh/server/hostkey/private/dsa2",
                                 ba_value, bt_string, 0, priv_key_data);
        bail_error(err);
    
        if (set_direct) {
            err = mdb_set_node_binding(commit, db, bsso_modify, 0,
                                       binding);
            bail_error(err);
        }
        else {
            err = bn_set_request_msg_append(req, bsso_modify, 0, binding,
                                            NULL);
            bail_error(err);
        }

        bn_binding_free(&binding);
    }

    safe_free(pub_key_data);
    safe_free(priv_key_data);

    if ((strcmp(key_type, "all") == 0) ||
        (strcmp(key_type, "rsa1") == 0)) {
        lc_log_basic(LOG_NOTICE, "Generating new hostkey of type rsa1");

        err = lc_ssh_util_keygen("rsa1", NULL, &pub_key_data, &pub_key_len,
                                 &priv_key_data, &priv_key_len);
        bail_error(err);

        err = bn_binding_new_str(&binding, "/ssh/server/hostkey/public/rsa1",
                                 ba_value, bt_string, 0, pub_key_data);
        bail_error(err);

        if (set_direct) {
            err = mdb_set_node_binding(commit, db, bsso_modify, 0,
                                       binding);
            bail_error(err);
        }
        else {
            err = bn_set_request_msg_append(req, bsso_modify, 0, binding,
                                            NULL);
            bail_error(err);
        }

        bn_binding_free(&binding);

        err = tb_new_buf(&priv_key_buf, (const uint8 *) priv_key_data,
                         (int32) priv_key_len);
        bail_error(err);

        err = bn_binding_new_binary(&binding,
                                    "/ssh/server/hostkey/private/rsa1",
                                    ba_value, 0, priv_key_buf);
        bail_error_null(err, binding);

        if (set_direct) {
            err = mdb_set_node_binding(commit, db, bsso_modify, 0,
                                       binding);
            bail_error(err);
        }
        else {
            err = bn_set_request_msg_append(req, bsso_modify, 0, binding,
                                            NULL);
            bail_error(err);
        }

        bn_binding_free(&binding);
    }

    safe_free(pub_key_data);
    safe_free(priv_key_data);

    if ((strcmp(key_type, "all") == 0) ||
        (strcmp(key_type, "rsa2") == 0)) {
        lc_log_basic(LOG_NOTICE, "Generating new hostkey of type rsa2");

        err = lc_ssh_util_keygen("rsa", NULL, &pub_key_data, &pub_key_len,
                                 &priv_key_data, &priv_key_len);
        bail_error(err);

        err = bn_binding_new_str(&binding, "/ssh/server/hostkey/public/rsa2",
                                 ba_value, bt_string, 0, pub_key_data);
        bail_error(err);
    
        if (set_direct) {
            err = mdb_set_node_binding(commit, db, bsso_modify, 0,
                                       binding);
            bail_error(err);
        }
        else {
            err = bn_set_request_msg_append(req, bsso_modify, 0, binding,
                                            NULL);
            bail_error(err);
        }

        bn_binding_free(&binding);

        err = bn_binding_new_str(&binding, "/ssh/server/hostkey/private/rsa2",
                                 ba_value, bt_string, 0, priv_key_data);
        bail_error(err);
    
        if (set_direct) {
            err = mdb_set_node_binding(commit, db, bsso_modify, 0,
                                       binding);
            bail_error(err);
        }
        else {
            err = bn_set_request_msg_append(req, bsso_modify, 0, binding,
                                            NULL);
            bail_error(err);
        }

    }

    safe_free(pub_key_data);
    safe_free(priv_key_data);

    if (!set_direct) {
        err = md_commit_handle_commit_from_module(commit, req, 
                                                  NULL, &ret_code, &ret_msg,
                                                  NULL, NULL);
        complain_error(err);

        if (ret_code || (ret_msg && ts_length(ret_msg))) {
            err = md_commit_set_return_status_msg(commit, 1, ts_str(ret_msg));
            bail_error(err);
        }
    }

 bail:
    safe_free(pub_key_data);
    safe_free(priv_key_data);
    tb_free(&priv_key_buf);
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_action_gen_hostkey(md_commit *commit, mdb_db **db,
                          const char *action_name, bn_binding_array *params,
                          void *cb_arg)
{
    int err = 0, err2 = 0;
    tstring *key_type = NULL;

    bail_require(!strcmp(action_name, "/ssh/hostkey/generate"));

    err = bn_binding_array_get_value_tstr_by_name(params, "key_type", NULL, 
                                                  &key_type);
    bail_error_null(err, key_type);

    if ((strcmp(ts_str(key_type), "all") == 0) ||
        (strcmp(ts_str(key_type), "rsa1") == 0) ||
        (strcmp(ts_str(key_type), "rsa2") == 0) ||
        (strcmp(ts_str(key_type), "dsa2") == 0)) {
        err = md_ssh_gen_hostkey(commit, NULL, ts_str(key_type), false);
        bail_error(err);
        err = md_send_signal_to_process(commit, SIGHUP, "sshd", false);
        bail_error(err);
    }
    else {
        lc_log_basic(LOG_INFO, "Action %s: invalid key type '%s'",
                     action_name, ts_str(key_type));
        err2 = md_commit_set_return_status_msg_fmt
            (commit, 1,
             _("Invalid key type '%s'.  Must be rsa1, rsa2, dsa2 or all.\n"),
             ts_str(key_type));
        complain_error(err2);
        goto bail;
    }

 bail:
    ts_free(&key_type);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_action_gen_keypair(md_commit *commit, mdb_db **db,
                          const char *action_name, bn_binding_array *params,
                          void *cb_arg)
{
    int err = 0, err2 = 0;
    tstring *key_type = NULL, *bn_prefix = NULL;
    char *pub_key_str = NULL, *priv_key_str = NULL;
    uint32 pub_key_len = 0, priv_key_len = 0;
    bn_request *req = NULL;
    bn_binding *binding = NULL;
    uint32 ret_code = 0;
    tstring *ret_msg = NULL; 
    char *bn_name = NULL;

    bail_require(!strcmp(action_name, "/ssh/keypair/generate"));

    err = bn_binding_array_get_value_tstr_by_name(params, "key_type", NULL, 
                                                  &key_type);
    bail_error(err);

    /* Doesn't support rsa v1 key pair generation */
    if (strcmp(ts_str(key_type), "dsa") && strcmp(ts_str(key_type), "rsa")) {
        lc_log_basic(LOG_INFO, "Action %s: invalid key type '%s'",
                     action_name, ts_str(key_type));
        err2 = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Invalid key type '%s'.  Must be dsa or rsa.\n"),
             ts_str(key_type));
        complain_error(err2);
        goto bail;
    }

    /*
     * The action expects a binding prefix in order to create two
     * bindings (one for the public key and one for the private key)
     * bn_prefix = '/ssh/client/username/foo/keytype/rsa2'
     * will generate
     * /ssh/client/username/foo/keytype/rsa2/private
     * /ssh/client/username/foo/keytype/rsa2/public
     */
    err = bn_binding_array_get_value_tstr_by_name(params, "bn_prefix", NULL, 
                                                  &bn_prefix);
    bail_error(err);

    err = lc_ssh_util_keygen(ts_str(key_type), NULL, &pub_key_str,
                             &pub_key_len, &priv_key_str, &priv_key_len);
    if (err) {
        err2 = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Failed to generate key pair of type %s\n"),
             ts_str(key_type));
        complain_error(err2);
        goto bail;
    }

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    bn_name = smprintf("%s/public", ts_str(bn_prefix));
    bail_null(bn_name);

    err = bn_binding_new_str(&binding, bn_name,
                             ba_value, bt_string, 0, pub_key_str);
    bail_error(err);
    
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);

    safe_free(bn_name);
    bn_binding_free(&binding);

    bn_name = smprintf("%s/private", ts_str(bn_prefix));
    bail_null(bn_name);

    err = bn_binding_new_str(&binding, bn_name,
                             ba_value, bt_string, 0, priv_key_str);
    bail_error(err);
    
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);

    bn_binding_free(&binding);

    safe_free(pub_key_str);
    safe_free(priv_key_str);

    err = md_commit_handle_commit_from_module_flags(commit, req, 
                                                    mchmf_use_credentials,
                                                    NULL, &ret_code, &ret_msg,
                                                    NULL, NULL);
    complain_error(err);

    if (ret_code || (ret_msg && ts_length(ret_msg))) {
        err = md_commit_set_return_status_msg(commit, 1, ts_str(ret_msg));
        bail_error(err);
    }


 bail:
    safe_free(pub_key_str);
    safe_free(priv_key_str);
    safe_free(bn_name);
    ts_free(&key_type);
    ts_free(&bn_prefix);
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_remove_known_host_internal(md_commit *commit, const mdb_db *db,
                                  const char *host, const char *kh_path,
                                  const char *username)
{
    int err = 0;
    lc_launch_params *lp = NULL;
    lc_launch_result result = LC_LAUNCH_RESULT_INIT;
    tbool normal_exit = false;
    char *user_dir_bn_name = NULL;
    tstring *user_dir = NULL;

    bail_null(host);

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);

    err = ts_new_str(&(lp->lp_path), rkn_path);
    bail_error(err);

    if (kh_path) {
        bail_require(username == NULL);
        err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 3, rkn_path, "-f",
                                    kh_path);
        bail_error(err);
    }
    else if (username) {
        bail_require(kh_path == NULL);

        /*
         * XXX/EMT: username escaping?  Currently we permit usernames
         * that would need to be escaped, even though maybe we shouldn't.
         */
        user_dir_bn_name = smprintf("/auth/passwd/user/%s/home_dir",
                                    username);
        bail_null(user_dir_bn_name);

        err = mdb_get_node_value_tstr(commit, db, user_dir_bn_name,
                                      0, NULL, &user_dir);
        bail_error(err);

        if (user_dir == NULL) {
            err = md_commit_set_return_status_msg_fmt
                (commit, 1, _("Did not recognize username '%s'"),
                 username);
            bail_error(err);
            goto bail;
        }

        err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 3, rkn_path, "-d", 
                                    ts_str(user_dir));
        bail_error(err);
    }
    else {
        bail_force(lc_err_unexpected_null);
    }
    
    err = tstr_array_append_str(lp->lp_argv, host);
    bail_error(err);

    lp->lp_wait = true;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;
    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;

    /* No base OS dependency: our own rkn.sh */
    err = lc_launch(lp, &result);
    bail_error(err);

    err = lc_check_exit_status(result.lr_exit_status, &normal_exit, 
                               "%s", ts_str(lp->lp_path));
    bail_error(err);

    if (!normal_exit || WEXITSTATUS(result.lr_exit_status)) {
        lc_log_basic(LOG_NOTICE, I_("#PROBLEM removing known host %s from "
                                    "%s '%s'"), host,
                     username ? "user " : "file",
                     username ? username : kh_path);
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, I_("#PROBLEM removing known host %s from "
                           "%s '%s'"), host,
             username ? "user " : "file",
             username ? username : kh_path);
        bail_error(err);
    }

 bail:
    ts_free(&user_dir);
    safe_free(user_dir_bn_name);
    lc_launch_params_free(&lp);
    lc_launch_result_free_contents(&result);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_action_remove_known_host(md_commit *commit, const mdb_db *db,
                                const char *action_name,
                                bn_binding_array *params,
                                void *cb_arg)
{
    int err = 0;
    tstring *user = NULL, *host = NULL, *known_hosts_path = NULL;

    bail_require(!strcmp(action_name, "/ssh/known_host/remove"));

    err = bn_binding_array_get_value_tstr_by_name(params, "user", NULL, 
                                                  &user);
    bail_error(err); /* May be NULL */

    err = bn_binding_array_get_value_tstr_by_name(params, "host", NULL, 
                                                  &host);
    bail_error_null(err, host);

    err = bn_binding_array_get_value_tstr_by_name
        (params, "known_hosts_path", NULL, &known_hosts_path);
    bail_error(err);

    /*
     * They must specify either "known_hosts_path", or "user" (from
     * which rkn.sh calculates the path to the known hosts file).
     */
    if (!user && !known_hosts_path) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Must specify either 'user' or 'known_hosts_path' "
                          "parameter"));
        bail_error(err);
        goto bail;
    }
    else if (user && known_hosts_path) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Cannot specify both 'user' and 'known_hosts_path' "
                          "parameters"));
        bail_error(err);
        goto bail;
    }

    if (known_hosts_path) {
        err = md_ssh_remove_known_host_internal(commit, db, ts_str(host), 
                                                ts_str(known_hosts_path),
                                                NULL);
        bail_error(err);
    }
    else {
        err = md_ssh_remove_known_host_internal(commit, db, ts_str(host), 
                                                NULL, ts_str(user));
        bail_error(err);
    }

 bail:
    ts_free(&user);
    ts_free(&host);
    ts_free(&known_hosts_path);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_ssh_action_remove_known_host_safe(md_commit *commit, const mdb_db *db,
                                     const char *action_name,
                                     bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tstring *user = NULL, *host = NULL;
    tbool found = false, cmc_known_hosts = false;

    bail_require(!strcmp(action_name, "/ssh/known_host/remove_safe"));

    err = bn_binding_array_get_value_tstr_by_name(params, "user", NULL, 
                                                  &user);
    bail_error(err); /* May be NULL */

    err = bn_binding_array_get_value_tstr_by_name(params, "host", NULL, 
                                                  &host);
    bail_error_null(err, host);

    err = bn_binding_array_get_value_tbool_by_name
        (params, "cmc_known_hosts", &found, &cmc_known_hosts);
    bail_error(err);

    /*
     * They must specify either "cmc_known_hosts", or "user" (from
     * which rkn.sh calculates the path to the known hosts file).
     */
    if (!user && !cmc_known_hosts) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Must specify either 'user' or 'cmc_known_hosts' "
                          "parameter"));
        bail_error(err);
        goto bail;
    }
    else if (user && cmc_known_hosts) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Cannot specify both 'user' and 'cmc_known_hosts' "
                          "parameters"));
        bail_error(err);
        goto bail;
    }

#if defined(PROD_FEATURE_CMC_CLIENT) || defined(PROD_FEATURE_CMC_SERVER) || \
    defined(PROD_FEATURE_REMOTE_GCL)
    if (cmc_known_hosts) {
        err = md_ssh_remove_known_host_internal(commit, db, ts_str(host), 
                                                cmc_ssh_known_hosts_path,
                                                NULL);
        bail_error(err);
        goto bail;
    }
#endif

    err = md_ssh_remove_known_host_internal(commit, db, ts_str(host), 
                                            NULL, ts_str(user));
    bail_error(err);

 bail:
    ts_free(&user);
    ts_free(&host);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_write_hostkey_file(md_commit *commit, const mdb_db *db,
                      const char *priv_key_path, const char *pub_key_path,
                      const char *key_type)
{
    int err = 0;
    char *filename_temp = NULL;
    int fd_temp = -1;
    char *bn_name = NULL, *key_data = NULL;
    tbuf *key_buf = NULL;
    bn_binding *binding = NULL;
    tbool priv_key_binary = false;

    bail_null(priv_key_path);
    bail_null(pub_key_path);
    bail_null(key_type);

    priv_key_binary = strcmp(key_type, "rsa1") == 0 ? true : false;

    bn_name = smprintf("/ssh/server/hostkey/private/%s", key_type);
    bail_null(bn_name);

    err = lf_temp_file_get_fd(priv_key_path, &filename_temp, &fd_temp);
    bail_error(err);

    err = mdb_get_node_binding(commit, db, bn_name, 0, &binding);
    bail_error(err);

    if (binding) {
        if (priv_key_binary) {
            err = bn_binding_get_binary(binding, ba_value, 0,
                                        &key_buf);
            bail_error(err);
        }
        else {
            err = bn_binding_get_str(binding, ba_value, bt_string, 0,
                                     &key_data);
            bail_error(err);
        }
    }

    if (key_data) {
        err = lf_write_bytes_printf(fd_temp, NULL, "%s", key_data);
        bail_error(err);
    }
    else if (key_buf) {
        err = lf_write_bytes_tbuf(fd_temp, key_buf, NULL);
        bail_error(err);
    }

    err = lf_temp_file_activate_fd(priv_key_path, filename_temp, 
                                   &fd_temp,
                                   ssh_hostkey_file_owner,
                                   ssh_hostkey_file_group,
                                   ssh_file_priv_mode, lao_none);
    bail_error(err);

    err = bn_binding_free(&binding);
    bail_error(err);

    safe_free(key_data);
    safe_free(filename_temp);
    safe_free(bn_name);

    /* Now the public key */

    bn_name = smprintf("/ssh/server/hostkey/public/%s", key_type);
    bail_null(bn_name);

    err = lf_temp_file_get_fd(pub_key_path, &filename_temp, &fd_temp);
    bail_error(err);

    err = mdb_get_node_binding(commit, db, bn_name, 0, &binding);
    bail_error(err);

    if (binding) {
        err = bn_binding_get_str(binding, ba_value, bt_string, 0, &key_data);
        bail_error(err);
    }

    if (key_data) {
        err = lf_write_bytes_printf(fd_temp, NULL, "%s", key_data);
        bail_error(err);
    }

    err = lf_temp_file_activate_fd(pub_key_path, filename_temp,
                                   &fd_temp,
                                   ssh_hostkey_file_owner,
                                   ssh_hostkey_file_group,
                                   ssh_file_mode, lao_none);
    bail_error(err);

 bail:
    safe_close(&fd_temp);
    bn_binding_free(&binding);
    tb_free(&key_buf);
    safe_free(key_data);
    safe_free(filename_temp);
    safe_free(bn_name);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_ssh_hostkey_fp_get(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags,
                      void *arg)
{
    int_ptr iarg = (int_ptr) arg;
    int err = 0;
    tstr_array *fps = NULL;
    uint32 num_fps = 0;

    bail_null(db);
    bail_null(node_name);
    bail_null(ret_binding);

    if (iarg == 1) {
        err = lc_ssh_util_keygen_finger_print(rsa1_host_pub_key_path, &fps,
                                              NULL);
        bail_error(err);

        num_fps = fps ? tstr_array_length_quick(fps) : 0;

        if (num_fps) {
            complain_require(num_fps == 1);
            err = bn_binding_new_str(ret_binding, node_name, ba_value,
                                     bt_string, 0,
                                     tstr_array_get_str_quick(fps, 0));
            bail_error(err);
        }
    }
    else if (iarg == 2) {
        err = lc_ssh_util_keygen_finger_print(rsa2_host_pub_key_path, &fps,
                                              NULL);
        bail_error(err);

        num_fps = fps ? tstr_array_length_quick(fps) : 0;

        if (num_fps) {
            complain_require(num_fps == 1);
            err = bn_binding_new_str(ret_binding, node_name, ba_value,
                                     bt_string, 0,
                                     tstr_array_get_str_quick(fps, 0));
            bail_error(err);
        }
    }
    else if (iarg == 3) {
        err = lc_ssh_util_keygen_finger_print(dsa2_host_pub_key_path, &fps,
                                              NULL);
        bail_error(err);

        num_fps = fps ? tstr_array_length_quick(fps) : 0;

        if (num_fps) {
            complain_require(num_fps == 1);
            err = bn_binding_new_str(ret_binding, node_name, ba_value,
                                     bt_string, 0,
                                     tstr_array_get_str_quick(fps, 0));
            bail_error(err);
        }
    }
    else {
        bail_force(lc_err_unexpected_case);
    }

 bail:
    tstr_array_free(&fps);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* We're here to make sure that there are always valid host keys,
 * since without them, sshd won't run.  The main way you can end up
 * without valid host keys is by changing the encryption key
 * md_config_crypt_key_str, which prevents us from reading the private
 * portions of the host keys.  In this case, we just regenerate new
 * ones so life can go on.
 */
static int
md_ssh_commit_side_effects(md_commit *commit, const mdb_db *old_db, 
                           mdb_db *inout_new_db, 
                           mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    bn_attrib *attr = NULL;

    /* ........................................................................
     * Copy /ssh/server/enable to /pm/process/sshd/auto_launch.
     *
     * Note that we don't watch the /pm subtree, so someone could change 
     * that out from under us and we would not catch it.  But none of our
     * UI will do this.
     */
    err = mdb_copy_node_binding(commit, inout_new_db, "/ssh/server/enable",
                                inout_new_db, "/pm/process/sshd/auto_launch");
    bail_error(err);

    /* ........................................................................
     * Other stuff...
     */

    /* RSA v1 */
    err = mdb_get_node_attrib(commit, inout_new_db,
                              "/ssh/server/hostkey/private/rsa1", 0, ba_value,
                              &attr);
    bail_error(err);

    if (!attr) {
        lc_log_basic(LOG_WARNING, "RSA1 private host key missing; "
                     "regenerating");
        err = md_ssh_gen_hostkey(commit, inout_new_db, "rsa1", true);
        bail_error(err);
    }

    /* RSA v2 */
    bn_attrib_free(&attr);
    err = mdb_get_node_attrib(commit, inout_new_db,
                              "/ssh/server/hostkey/private/rsa2", 0, ba_value,
                              &attr);
    bail_error(err);

    if (!attr) {
        lc_log_basic(LOG_WARNING, "RSA2 private host key missing; "
                     "regenerating");
        err = md_ssh_gen_hostkey(commit, inout_new_db, "rsa2", true);
        bail_error(err);
    }

    /* DSA v2 */
    bn_attrib_free(&attr);
    err = mdb_get_node_attrib(commit, inout_new_db,
                              "/ssh/server/hostkey/private/dsa2", 0, ba_value,
                              &attr);
    bail_error(err);

    if (!attr) {
        lc_log_basic(LOG_WARNING, "DSA2 private host key missing; "
                     "regenerating");
        err = md_ssh_gen_hostkey(commit, inout_new_db, "dsa2", true);
        bail_error(err);
    }

    /*
     * If we have any new SSH keys or RSA/DSA public keys, let's replace
     * any CRs and LFs in them
     */

    /* SSH Client */
    err = md_utils_iterate_remove_crlf(commit, inout_new_db, change_list,
                          "/ssh/client/global/known-host/*/entry");
    bail_error(err);

    err = md_utils_iterate_remove_crlf(commit, inout_new_db, change_list,
                          "/ssh/client/username/*/keytype/*/public");
    bail_error(err);

    /* SSH Server */
    err = md_utils_iterate_remove_crlf(commit, inout_new_db, change_list,
                          "/ssh/server/username/*/auth-key/sshv2/*/public");
    bail_error(err);

    err = md_utils_iterate_remove_crlf(commit, inout_new_db, change_list,
                          "/ssh/server/hostkey/public/rsa1");
    bail_error(err);

    err = md_utils_iterate_remove_crlf(commit, inout_new_db, change_list,
                          "/ssh/server/hostkey/public/rsa2");
    bail_error(err);

    err = md_utils_iterate_remove_crlf(commit, inout_new_db, change_list,
                          "/ssh/server/hostkey/public/dsa2");
    bail_error(err);

 bail:
    bn_attrib_free(&attr);
    return(err);
}

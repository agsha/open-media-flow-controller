#include "common.h"
#include "dso.h"
#include "md_mod_reg.h"
#include "file_utils.h"
#include "md_mod_commit.h"
#include "md_utils.h"
#include "mdb_db.h"
#include "mdb_dbbe.h"
#include "array.h"
#include "tpaths.h"
#include "proc_utils.h"
#include "url_utils.h"
#include "md_mod_reg.h"

int md_publishlog_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_publishlog_ug_rules = NULL;

/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int
md_publishlog_commit_check(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg);


static int md_publishlog_file_upload_config(md_commit *commit, mdb_db **db,
                    const char *action_name, bn_binding_array *params,
                    void *cb_arg);

static int
md_publishlog_files_iterate(md_commit *commit, const mdb_db *db,
                         const char *parent_node_name,
                         const uint32_array *node_attrib_ids,
                         int32 max_num_nodes, int32 start_child_num,
                         const char *get_next_child,
                         mdm_mon_iterate_names_cb_func iterate_cb,
                         void *iterate_cb_arg, void *arg);
static int
md_publishlog_files_get(md_commit *commit, const mdb_db *db,
                     const char *node_name,
                     const bn_attrib_array *node_attribs,
                     bn_binding **ret_binding, uint32 *ret_node_flags,
                     void *arg);



/*----------------------------------------------------------------------------
 *MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
md_publishlog_init(const lc_dso_info *info, void *data)
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
                    "publishlog",                           // module_name
                    1,                                  // module_version
                    "/nkn/publishlog",                      // root_node_name
                    NULL,                               // root_config_name
                    modrf_namespace_unrestricted,       // md_mod_flags
                    NULL,				// side_effects_func
                    NULL,                               // side_effects_arg
                    md_publishlog_commit_check,             // check_func
                    NULL,                               // check_arg
                    NULL,				// apply_func
                    NULL,                               // apply_arg
                    200,                                // commit_order
                    0,                                  // apply_order
                    md_generic_upgrade_downgrade,        // updown_func
                    &md_publishlog_ug_rules,              // updown_arg
                    NULL,                               // initial_func
                    NULL,                               // initial_arg
                    NULL,                               // mon_get_func
                    NULL,                               // mon_get_arg
                    NULL,                               // mon_iterate_func
                    NULL,                               // mon_iterate_arg
                    &module);                           // ret_module
    bail_error(err);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = mdm_module_set_gettext_domain(module, GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */



    /*
     * Setup config node - /nkn/mfplog/config/path
     *  - Configure log path
     *  - Default is "/var/log/nkn"
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mfplog/config/path";
    node->mrn_value_type        = bt_string;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value     = "/var/log/nkn";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Path where publish log is saved";
    err = mdm_add_node(module, &node);
    bail_error(err);



    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mfplog/config/syslog";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "send output to syslogd";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mfplog/config/enable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Enable publishlog";
    err = mdm_add_node(module, &node);
    bail_error(err);




    /*
     * Setup config node - /nkn/mfplog/config/filename
     *  - Configure FileName
     *  - Default is "publish.log"
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mfplog/config/filename";
    node->mrn_value_type        = bt_string;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value     = "mfp.log";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_limit_str_no_charlist = "/\\*:|`\"?&^!@#%$(){}[]<>,";
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Filename where publish log is saved";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/mfplog/config/max-file-size
     *  - Configure Max-Filesize
     *  - Default is 100 (MBytes)
     *
     * Note that we dont expect this number to be greater than 65535
     * (since that would translate to holding a file of 64GByte),
     * and that is why the data type is set to uint16
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mfplog/config/max-filesize";
    node->mrn_value_type        = bt_uint16;
    node->mrn_initial_value     = "100";
    node->mrn_limit_num_min_int = 10;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Maximum single filesize allocated "
                                  "for one publishlog file, in MiB";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mfplog/config/time-interval";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "time-interval for log rotation in hours";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mfplog/config/upload/url";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Remote URL  (no password)";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mfplog/config/upload/pass";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/mfplog/action/upload";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_privileged;
    node->mrn_action_config_func = md_publishlog_file_upload_config;
    node->mrn_action_arg        = (void *) false;
    node->mrn_description       = "Upload a file";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mfplog/config/on-the-hour";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Set the on-the-hour log rotation";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mfplog/config/level";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 1;
    node->mrn_limit_num_min_int = 1;
    node->mrn_limit_num_max_int = 7;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "debug message level";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Monitoring node for the archieved logs
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/mfplog/logfiles/*";
    node->mrn_value_type	= bt_string;
    node->mrn_node_reg_flags	= mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_mon_get_func	= md_publishlog_files_get;
    node->mrn_mon_iterate_func	= md_publishlog_files_iterate;
    node->mrn_description	= "List of archieved publishlog files available";
    err = mdm_add_node(module, &node);
    bail_error(err);


    bail:
    return err;
}

/*---------------------------------------------------------------------------*/
static int
md_publishlog_commit_check(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
        int err = 0;
        const mdb_db_change *change = NULL;
        int i = 0, num_changes = 0;
        const bn_attrib *new_val = NULL;
	uint32 new_time_interval = 0;

        /*
         * DN : the template under demo/ doesnt provide much info on
         * what to do here.
         *
         * TODO: Possibly the ony thing to check here is to make sure
         * that max-filesize <= total-diskspace
         *
         * At present, just enable binding, so nothing to check.
         */

        /*
         * Enforce that disk-size is always 10x of filsize
         * OR
         * file size is 1/10 of disk size
         * AND
         * filesize > 10MB
         */
        num_changes = mdb_db_change_array_length_quick(change_list);
        for(i = 0; i < num_changes; i++) {
            change = mdb_db_change_array_get_quick(change_list, i);
            bail_null(change);

	    if(change->mdc_change_type == mdct_modify) {
                if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 4, "nkn", "mfplog", "config", "time-interval"))
                {
                    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
                    if (!new_val) {
                        goto bail;
                    }

                    err = bn_attrib_get_uint32(new_val, NULL, NULL, &new_time_interval);
                    bail_error(err);
		    if( new_time_interval <5){
			    err = md_commit_set_return_status_msg(commit, 0,
					    "NOTE:-Time-interval based log rotation is disabled.\n"
					    "Set value between 5 and 480 minutes to enable the time-interval"
					    "based log rotation\n");
		    } else if (new_time_interval >480) {
                        // Error
                        err = md_commit_set_return_status_msg(commit, 1,
                                "error: Time interval must set between 5 to 480 minutes");
                        bail_error(err);
                        goto bail;
                    }
                }
	    }

        }
        err = md_commit_set_return_status(commit, 0);
        bail_error(err);
bail:
        return err;
}


static int md_publishlog_file_upload_config(md_commit *commit, mdb_db **db,
                    const char *action_name, bn_binding_array *params,
                    void *cb_arg)
{
    int err = 0;
    char *pass = NULL, *rurl = NULL;
    char *user = NULL, *hostname = NULL, *path = NULL;
    tstring *remote_url = NULL, *password = NULL;
    bn_binding *binding = NULL;
    bn_request *req = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;

    bail_require(!strcmp(action_name, "/nkn/mfplog/action/upload"));

    err = bn_binding_array_get_value_tstr_by_name(params, "remote_url", NULL,
            &remote_url);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(params, "password", NULL,
            &password);
    bail_error(err);

    if (password == NULL) {
            if ((lc_is_prefix("scp://", ts_str(remote_url), false))){
        err = lu_parse_scp_url(ts_str(remote_url),
                &user, &pass,
                &hostname, NULL, &path);
        bail_error(err);

        rurl = smprintf("scp://%s@%s%s", user, hostname, path);
        bail_null(rurl);
        }
           else if ((lc_is_prefix("sftp://", ts_str(remote_url), false))){
        err = lu_parse_sftp_url(ts_str(remote_url),
                &user, &pass,
                &hostname, NULL, &path);
        bail_error(err);

        rurl = smprintf("sftp://%s@%s%s", user, hostname, path);
        bail_null(rurl);
        }
            else {
	rurl = smprintf("%s", ts_str(remote_url));
        bail_null(rurl);

        pass = smprintf("%s", ts_str(password));
        bail_null(pass);

       }

    }
    else {
        rurl = smprintf("%s", ts_str(remote_url));
        bail_null(rurl);

        pass = smprintf("%s", ts_str(password));
        bail_null(pass);
    }


    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_binding_new_str(&binding, "/nkn/mfplog/config/upload/url",
            ba_value, bt_string, 0, rurl);
    bail_error(err);

    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);

    bn_binding_free(&binding);

    err = bn_binding_new_str(&binding, "/nkn/mfplog/config/upload/pass",
            ba_value, bt_string, 0, pass);
    bail_error(err);

    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
            &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);

    if (ret_code || (ret_msg && ts_length(ret_msg))) {
        err = md_commit_set_return_status_msg(commit, 1, ts_str(ret_msg));
        bail_error(err);
    }

bail:
    ts_free(&remote_url);
    ts_free(&password);
    safe_free(pass);
    safe_free(user);
    safe_free(hostname);
    safe_free(path);
    safe_free(rurl);
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}


static int
md_publishlog_files_get(md_commit *commit, const mdb_db *db,
                     const char *node_name,
                     const bn_attrib_array *node_attribs,
                     bn_binding **ret_binding, uint32 *ret_node_flags,
                     void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *filename = NULL;

    /* XXX/EMT: should validate filename */

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error(err);

    /* Verify this is "/nkn/mfplog/logfiles/ *" */
    num_parts = tstr_array_length_quick(parts);
    bail_require(num_parts == 4);

    filename = tstr_array_get_str_quick(parts, 3);
    bail_null(filename);

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string, 0,
                             filename);
    bail_error(err);

 bail:
    tstr_array_free(&parts);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_publishlog_files_iterate(md_commit *commit, const mdb_db *db,
                         const char *parent_node_name,
                         const uint32_array *node_attrib_ids,
                         int32 max_num_nodes, int32 start_child_num,
                         const char *get_next_child,
                         mdm_mon_iterate_names_cb_func iterate_cb,
                         void *iterate_cb_arg, void *arg)
{
    int err = 0;
    tstr_array *names = NULL;
    const char *name = NULL;
    const char *pattern = NULL;
    uint32 num_names = 0, i = 0;
    tstring *t_log_format = NULL;

    /*
     * XXX/EMT to do:
     *   - have wildcard be a numeric ID; make absolute path a child
     *   - also have datetime children that say when the first and last
     *     entries in the log file are
     *   - return the current log file too with ID 0.
     */

    err = lf_dir_list_names("/var/log/nkn", &names);
    bail_error_null(err, names);

    err = mdb_get_node_value_tstr(commit, db,
	    "/nkn/mfplog/config/filename",
		    0, NULL, &t_log_format);
    bail_error(err);

    pattern = smprintf("%s", ts_str(t_log_format));
    num_names = tstr_array_length_quick(names);
    for (i = 0; i < num_names; ++i) {
        name = tstr_array_get_str_quick(names, i);
        bail_null(name);
        if (lc_is_prefix( ts_str(t_log_format), name, false)&& (lc_is_suffix(".gz", name, false)) ){
            err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
            bail_error(err);
        } else if ( (strcmp(pattern, name)) == 0) {
            err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
            bail_error(err);
	}
    }

 bail:
    tstr_array_free(&names);
    ts_free(&t_log_format);
    return(err);
}

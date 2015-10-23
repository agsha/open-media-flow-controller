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

int md_fmsaccesslog_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_fmsaccesslog_ug_rules = NULL;
/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int md_fmsaccesslog_file_upload_config(md_commit *commit, mdb_db **db,
                    const char *action_name, bn_binding_array *params,
                    void *cb_arg);

/*----------------------------------------------------------------------------
 *MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
md_fmsaccesslog_init(const lc_dso_info *info, void *data)
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
                    "fmsaccesslog",                           // module_name
                    1,                                  // module_version
                    "/nkn/fmsaccesslog",                      // root_node_name
                    NULL,                               // root_config_name
                    0,				           // md_mod_flags
                    NULL,			          // side_effects_func
                    NULL,                               // side_effects_arg
                    NULL,		                 // check_func
                    NULL,                               // check_arg
                    NULL,		                 // apply_func
                    NULL,                               // apply_arg
                    200,                                // commit_order
                    0,                                  // apply_order
                    md_generic_upgrade_downgrade,       // updown_func
                    &md_fmsaccesslog_ug_rules,              // updown_arg
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
     * Setup config node - /nkn/fmsaccesslog/config/path
     *  - Configure log path
     *  - Default is "/var/log/nkn"
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/fmsaccesslog/config/path";
    node->mrn_value_type        = bt_string;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value     = "/nkn/adobe/fms/logs";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Path where fmsaccess log is saved";
    err = mdm_add_node(module, &node);
    bail_error(err);



    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/fmsaccesslog/config/syslog";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "send output to syslogd";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/fmsaccesslog/config/enable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Enable fmsaccesslog";
    err = mdm_add_node(module, &node);
    bail_error(err);




    /*
     * Setup config node - /nkn/fmsaccesslog/config/filename
     *  - Configure FileName
     *  - Default is "fmsaccess.log"
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/fmsaccesslog/config/filename";
    node->mrn_value_type        = bt_string;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value     = "access.00.log";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_limit_str_no_charlist = "/\\*:|`\"?&^!@#%$(){}[]<>,";
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Filename where FMS Access log is saved";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/fmsaccesslog/config/max-file-size
     *  - Configure Max-Filesize
     *  - Default is 100 (MBytes)
     *
     * Note that we dont expect this number to be greater than 65535
     * (since that would translate to holding a file of 64GByte),
     * and that is why the data type is set to uint16
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/fmsaccesslog/config/max-filesize";
    node->mrn_value_type        = bt_uint16;
    node->mrn_initial_value     = "100";
    node->mrn_limit_num_min_int = 10;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Maximum single filesize allocated "
                                  "for one fmsaccesslog file, in MiB";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/fmsaccesslog/config/time-interval";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 65535;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "time-interval for log rotation in Minutes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/fmsaccesslog/config/upload/url";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Remote URL  (no password)";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/fmsaccesslog/config/upload/pass";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/fmsaccesslog/action/upload";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_privileged;
    node->mrn_action_config_func = md_fmsaccesslog_file_upload_config;
    node->mrn_action_arg        = (void *) false;
    node->mrn_description       = "Upload a file";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/fmsaccesslog/config/on-the-hour";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Set the on-the-hour log rotation";
    err = mdm_add_node(module, &node);
    bail_error(err);

bail:
    return err;
}

static int md_fmsaccesslog_file_upload_config(md_commit *commit, mdb_db **db,
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

    bail_require(!strcmp(action_name, "/nkn/fmsaccesslog/action/upload"));

    err = bn_binding_array_get_value_tstr_by_name(params, "remote_url", NULL,
            &remote_url);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(params, "password", NULL,
            &password);
    bail_error(err);

    if (password == NULL) {
        err = lu_parse_scp_url(ts_str(remote_url),
                &user, &pass,
                &hostname, NULL, &path);
        bail_error(err);

        rurl = smprintf("scp://%s@%s%s", user, hostname, path);
        bail_null(rurl);
    }
    else {
        rurl = smprintf("%s", ts_str(remote_url));
        bail_null(rurl);

        pass = smprintf("%s", ts_str(password));
        bail_null(pass);
    }


    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_binding_new_str(&binding, "/nkn/fmsaccesslog/config/upload/url",
            ba_value, bt_string, 0, rurl);
    bail_error(err);

    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);

    bn_binding_free(&binding);

    err = bn_binding_new_str(&binding, "/nkn/fmsaccesslog/config/upload/pass",
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

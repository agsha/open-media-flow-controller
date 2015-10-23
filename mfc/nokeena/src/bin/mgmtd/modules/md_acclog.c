/*
 *
 * Filename:    md_acclog.c
 * Date:        2008-11-12
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_acclog.c: shows how the nknaccesslog module is added to
 * the initial PM database.
 *
 *---------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "dso.h"
#include "file_utils.h"
#include "md_utils.h"
#include "md_mod_reg.h"
#include "md_mod_commit.h"
#include "mdb_db.h"
#include "mdb_dbbe.h"
#include "array.h"
#include "tpaths.h"
#include "proc_utils.h"
#include "url_utils.h"
#include "errors.h"

/*----------------------------------------------------------------------------
 * PREPROCESSOR MACROS/CONSTANTS
 *---------------------------------------------------------------------------*/
#define NKN_ACCESSLOG_PATH      "/nkn/bin/nknaccesslog"


#define NKN_ACCESSLOG_CONF_NAME         "accesslog.cfg"
#define NKN_ACCESSLOG_CONF_PATH         "/config/nkn"

#define NKN_LOG_PATH            "/var/log/nkn"

#define NODE_ROOT		"/nkn/accesslog/config/profile"

/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static const char log_access_path[] = NKN_ACCESSLOG_PATH;
static const char log_access_conf_path[] = NKN_ACCESSLOG_CONF_PATH;
static const char log_access_conf_name[] = NKN_ACCESSLOG_CONF_NAME;
static const char log_path[] = NKN_LOG_PATH;

static md_upgrade_rule_array *md_acclog_ug_rules = NULL;
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
md_acclog_init(
        const lc_dso_info *info,
        void *data);

int
md_acclog_check_format(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        const mdb_db_change_array *change_list,
        const tstring *node_name,
        const tstr_array *node_name_parts,
        mdb_db_change_type change_type,
        const bn_attrib_array *old_attribs,
        const bn_attrib_array *new_attribs,
        void *arg);

static int
md_acclog_rotate_size_commit_check(
        md_commit *commit,
        const mdb_db *old_db, const mdb_db *new_db,
        const mdb_db_change_array *change_list,
        const tstring *node_name,
        const tstr_array *node_name_parts,
        mdb_db_change_type change_type,
        const bn_attrib_array *old_attribs,
        const bn_attrib_array *new_attribs, void *arg);

static int
md_acclog_rotate_time_commit_check(
	md_commit *commit,
	const mdb_db *old_db, const mdb_db *new_db,
	const mdb_db_change_array *change_list,
	const tstring *node_name,
	const tstr_array *node_name_parts,
	mdb_db_change_type change_type,
	const bn_attrib_array *old_attribs,
	const bn_attrib_array *new_attribs, void *arg);
/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int
md_acclog_commit_check(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg);

static int
md_acclog_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_db,
        mdb_db_change_array *change_list,
        void *arg);

static int
md_acclog_commit_apply(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg);

static int md_acclog_file_upload_config(md_commit *commit, mdb_db **db,
                                 const char *action_name, bn_binding_array *params,
                                 void *cb_arg);

static int
md_acclog_files_iterate(md_commit *commit, const mdb_db *db,
                         const char *parent_node_name,
                         const uint32_array *node_attrib_ids,
                         int32 max_num_nodes, int32 start_child_num,
                         const char *get_next_child,
                         mdm_mon_iterate_names_cb_func iterate_cb,
                         void *iterate_cb_arg, void *arg);
static int
md_acclog_files_get(md_commit *commit, const mdb_db *db,
                     const char *node_name,
                     const bn_attrib_array *node_attribs,
                     bn_binding **ret_binding, uint32 *ret_node_flags,
                     void *arg);

static int
md_acclog_upgrade_downgrade(const mdb_db *old_db,
			    mdb_db *inout_new_db,
			    uint32 from_version,
			    uint32 to_version,
			    void *arg);
static void
md_acclog_ug_default_profile_free(bn_str_value  **profile_default, uint32 num_vals);

static int
md_acclog_create_profile_default(const mdb_db *old_db,
				 bn_str_value  **profile,
				 uint32 *num_vals);

static int
md_acclog_ug_delete_node_ver8(const mdb_db *old_db,
			      mdb_db *inout_new_db,
			      void *arg);

static int md_acclog_add_initial(md_commit *commit,
				 mdb_db *db,
				 void *arg);

const char *fmap = "bcdefghimopqrstuvwyzABDEFHILMNORSUVXYZ%";

const char *ncsa = "%h - %u %t \"%r\" %s %b  \"%{Referer}i\" \"{User-Agent}i\" \"%{Cookie}i\"";
const char *clf = "%h - %u %t \"%r\" %s %b";
#define w3c_ext_default  "%h - - %t %r %s %b" // "%h %V %u %t \"%r\" %s %b %R %D %c %N %v"
const char *w3c_custom_suffix = " %N %v";
#define NKN_MAX_FORMAT_LENGTH 256

static int
md_acclog_mon_get(md_commit *commit, const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding, uint32 *ret_node_flags,
	void *arg);

static int
md_acclog_get_format_class(md_commit *commit, const mdb_db *db,
	    const char *node_name,
	    const bn_attrib_array *node_attribs,
	    bn_binding **ret_binding,
	    uint32 *ret_node_flags, void *arg);

static int
md_acclog_mon_iterate(md_commit *commit, const mdb_db *db,
	const char *parent_node_name,
	const uint32_array *node_attrib_ids,
	int32 max_num_nodes, int32 start_child_num,
	const char *get_next_child,
	mdm_mon_iterate_names_cb_func iterate_cb,
	void *iterate_cb_arg, void *arg);


const bn_str_value md_acclog_default_profile[] = {
    {NODE_ROOT "/default", bt_string, "default"},
    {NODE_ROOT "/default/format-class", bt_string, "w3c-ext-default"},
    {NODE_ROOT "/default/format", bt_string,  w3c_ext_default},
    {NODE_ROOT "/default/filename", bt_string, "access.log"},
    {NODE_ROOT "/default/max-filesize", bt_uint16, "100"},
    {NODE_ROOT "/default/time-interval", bt_uint32, "0"},
    {NODE_ROOT "/default/object/filter/size", bt_uint32, "0"},
};

static int md_acclog_add_initial(md_commit *commit,
				 mdb_db *db,
				 void *arg)
{
    int err = 0;
    int size = sizeof(md_acclog_default_profile)/sizeof(bn_str_value);

    err = mdb_create_node_bindings(commit, db, md_acclog_default_profile, size);
    bail_error(err);

bail:
    return err;
}

/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
md_acclog_init(const lc_dso_info *info, void *data)
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
                    "acclog",                           // module_name
                    11,                                  // module_version
                    "/nkn/accesslog",                   // root_node_name
                    NULL,                               // root_config_name
                    0,				        // md_mod_flags
                    md_acclog_commit_side_effects,      // side_effects_func
                    NULL,                               // side_effects_arg
                    md_acclog_commit_check,             // check_func
                    NULL,                               // check_arg
                    md_acclog_commit_apply,             // apply_func
                    NULL,                               // apply_arg
                    200,                                // commit_order
                    0,                                  // apply_order
                    md_acclog_upgrade_downgrade,        // updown_func
                    &md_acclog_ug_rules,		// updown_arg
                    md_acclog_add_initial,              // initial_func
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


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/enable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Enable accesslog";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*
     * Setup config node - /nkn/accesslog/config/path
     *  - Configure log path
     *  - Default is "/var/log/nkn"
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/path";
    node->mrn_value_type        = bt_string;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value     = "/var/log/nkn";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Path where access log is saved";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/accesslog/config/syslog
     *  - Configure FileName
     *  - Default is "no"
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/syslog";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "send output to syslogd";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/accesslog/config/filename
     *  - Configure FileName
     *  - Default is "access.log"
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/filename";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "access.log";
    node->mrn_limit_str_min_chars = 1;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_limit_str_no_charlist = "/\\*:|`\"?&^!@#%$(){}[]<>,";
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Filename where access log is saved";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/accesslog/config/max-file-size
     *  - Configure Max-Filesize
     *  - Default is 100 (MBytes)
     *
     * Note that we dont expect this number to be greater than 65535
     * (since that would translate to holding a file of 64GByte),
     * and that is why the data type is set to uint16
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/max-filesize";
    node->mrn_value_type        = bt_uint16;
    node->mrn_initial_value     = "100";
    node->mrn_limit_num_min_int = 0;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Maximum single filesize allocated "
                                  "for one accesslog file, in MiB";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/time-interval";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "time-interval for log rotation in minutes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/accesslog/config/format
     *  - Configure Format
     *  - Default is "%t %Es %U %s %X %I %O"
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/format";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value	= "%h -- %t %r %s %b";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_check_node_func   = md_acclog_check_format;
    node->mrn_description       = "Format of the log written to "
                                  "access.log";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/upload/url";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Remote URL  (no password)";

    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/upload/pass";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";

    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/action/upload";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_privileged;
    node->mrn_action_config_func = md_acclog_file_upload_config;
    node->mrn_action_arg        = (void *) false;
    node->mrn_description       = "Upload a file";

    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/on-the-hour";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "set the on-the-hour log rotation";

    err = mdm_add_node(module, &node);
    bail_error(err);

    /*Bug 4954:
     * Config node for format-display in the log	
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/format-display";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "display format in logfile";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/max-fileid";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "10";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "max log files to retain";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/accesslog/config/restriction/enable";
    node->mrn_value_type	= bt_bool;
    node->mrn_initial_value	= "false";
    node->mrn_node_reg_flags 	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_description	= "Enable/Disable the restriction";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/accesslog/config/skip_object_size";
    node->mrn_value_type	= bt_uint32;
    node->mrn_initial_value	= "0";
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_description	= "Set the Object size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/accesslog/config/skip_response_code";
    node->mrn_value_type	= bt_string;
    node->mrn_initial_value	= "";
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_description	= "Set the response code list";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/rotate";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Enable the accesslog rotation";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*
     * Monitoring node for the archieved logs
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/accesslog/logfiles/*";
    node->mrn_value_type	= bt_string;
    node->mrn_node_reg_flags	= mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_mon_get_func	= md_acclog_files_get;
    node->mrn_mon_iterate_func	= md_acclog_files_iterate;
    node->mrn_description	= "List of archieved accesslog files available";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/accesslog/config/remote/ip";
    node->mrn_value_type	= bt_ipv4addr;
    node->mrn_initial_value 	= "127.0.0.1";
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_description	= "IP address where remote nknlogd is running";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/accesslog/config/remote/port";
    node->mrn_value_type	= bt_uint16;
    node->mrn_initial_value 	= "7958";
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_description	= "Port address where remote nknlogd is running";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/accesslog/config/remote/interface";
    node->mrn_value_type	= bt_string;
    node->mrn_initial_value 	= "lo";
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_description	= "interface name to bind to";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* enable/disable log analyzer */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name             = "/nkn/accesslog/config/analyzer/enable";
    node->mrn_value_type       = bt_bool;
    node->mrn_initial_value    = "false";
    node->mrn_node_reg_flags   = mrf_flags_reg_config_literal;
    node->mrn_cap_mask         = mcf_cap_node_restricted;
    node->mrn_description      = "enable/disable the real-time log analyzer module in accesslog";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*-----------------------------------------------------------------------
     * NEW PROFILE/TEMPLATE BASED LOG DEFINITIONS
     *
     * Added: DhruvaN, 09/01/2011
     *---------------------------------------------------------------------*/

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name             = "/nkn/accesslog/config/profile/*";
    node->mrn_value_type       = bt_string;
    node->mrn_limit_str_max_chars = 32;
    node->mrn_limit_wc_count_max = 32;
    node->mrn_node_reg_flags   = mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask         = mcf_cap_node_restricted;
    node->mrn_limit_str_no_charlist = "/\\*:|`\"?";
    node->mrn_description      = "A log profile/template name";
    node->mrn_bad_value_msg	= N_("Maximum profiles configurable is 32.");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name             = "/nkn/accesslog/config/profile/*/filename";
    node->mrn_value_type       = bt_string;
    node->mrn_initial_value	= "access.log";
    node->mrn_limit_str_min_chars = 1;
    node->mrn_limit_str_max_chars = 48;
    node->mrn_limit_str_no_charlist = "/\\*:|`\"?&^!@#%$(){}[]<>,";
    node->mrn_node_reg_flags   = mrf_flags_reg_config_literal;
    node->mrn_cap_mask         = mcf_cap_node_restricted;
    node->mrn_description      = "Filename where accesslog for this profile is saved";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/accesslog/config/profile/<name>/max-file-size
     *  - Configure Max-Filesize
     *  - Default is 100 (MBytes)
     *
     * Note that we dont expect this number to be greater than 65535
     * (since that would translate to holding a file of 64GByte),
     * and that is why the data type is set to uint16
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/profile/*/max-filesize";
    node->mrn_value_type        = bt_uint16;
    node->mrn_initial_value_int = 100;
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 100;
    node->mrn_bad_value_msg	= N_("max-filesize should be between"
			"10-100MiBytes or 0 to disable size based rotation");
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_check_node_func   = md_acclog_rotate_size_commit_check;
    node->mrn_check_node_arg    = NULL;
    node->mrn_description       = "Maximum single filesize allocated "
                                  "for one accesslog file, in MiB";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/profile/*/time-interval";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_limit_num_min_int	= 0;
    node->mrn_limit_num_max_int	= 60;
    node->mrn_bad_value_msg	= N_("max-duration should be between 5 to 60 minutes;"
				     " or 0 to disable time based rotation");
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_check_node_func	= md_acclog_rotate_time_commit_check;
    node->mrn_check_node_arg	= NULL;
    node->mrn_description       = "time-interval for log rotation in minutes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/accesslog/config/profile/<name>/format
     *  - Configure Format
     *
     *  NOTE: Both the format-class and the format should be set together
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/profile/*/format-class";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value	= "w3c-ext-default";
    node->mrn_limit_str_choices_str = ",w3c-ext-default,w3c-ext-custom,clf,ncsa-combined";
    node->mrn_bad_value_msg	= N_("Bad format class. should be either of "
	    "'w3c-ext-default, w3c-ext-custom, clf, ncsa-combined'");
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Format of the log written to "
                                  "access.log";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/accesslog/state/profile/*";
    node->mrn_value_type	= bt_string;
    node->mrn_node_reg_flags	= mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask		= mcf_cap_node_basic;
    node->mrn_mon_get_func	= md_acclog_mon_get;
    node->mrn_mon_iterate_func	= md_acclog_mon_iterate;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/accesslog/state/profile/*/format-class";
    node->mrn_value_type	= bt_string;
    node->mrn_node_reg_flags	= mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask		= mcf_cap_node_basic;
    node->mrn_mon_get_func	= md_acclog_get_format_class;
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/profile/*/format";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value	= w3c_ext_default; // "%h %X - %t %r %s %b"
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_check_node_func   = md_acclog_check_format;
    node->mrn_description       = "Actual Format of the log record written to "
                                  "access.log";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/accesslog/config/profile/*/object/filter/size";
    node->mrn_value_type	= bt_uint32;
    node->mrn_initial_value	= "0";
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_description	= "Set the Object size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/accesslog/config/profile/*/object/filter/resp-code/*";
    node->mrn_value_type	= bt_uint16;
    node->mrn_node_reg_flags	= mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_limit_wc_count_max = 32;
    node->mrn_bad_value_msg	= N_("cannot configure more than 32 response codes for filtering");
    node->mrn_description	= "Set the response code list";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/profile/*/upload/url";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Remote URL  (no password)";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/accesslog/config/profile/*/upload/pass";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_acclog_ug_rules);
    bail_error(err);
    ra = md_acclog_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_DELETE;
    rule->mur_name =           "/nkn/accesslog/config/serverip";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_DELETE;
    rule->mur_name =           "/nkn/accesslog/config/serverport";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/accesslog/config/enable";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/accesslog/config/time-interval";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);



    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_DELETE;
    rule->mur_name =           "/nkn/accesslog/config/total-diskspace";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/accesslog/config/on-the-hour";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/accesslog/config/format-display";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/accesslog/config/max-fileid";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "10";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name =	     	"/nkn/accesslog/config/restriction/enable";
    rule->mur_new_value_type = 	bt_bool;
    rule->mur_new_value_str = 	"false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/accesslog/config/skip_object_size";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/accesslog/config/skip_response_code";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/accesslog/config/remote/ip";
    rule->mur_new_value_type = bt_ipv4addr;
    rule->mur_new_value_str =  "127.0.0.1";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/accesslog/config/remote/port";
    rule->mur_new_value_type = bt_uint16;
    rule->mur_new_value_str =  "7958";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/accesslog/config/remote/interface";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =  "lo";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name =	     	"/nkn/accesslog/config/rotate";
    rule->mur_new_value_type = 	bt_bool;
    rule->mur_new_value_str = 	"true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/accesslog/config/analyzer/enable";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type =   MUTT_CLAMP;
    rule->mur_name =           "/nkn/accesslog/config/profile/*/max-filesize";
    rule->mur_lowerbound    =  10;
    rule->mur_upperbound    = 100;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =   MUTT_CLAMP;
    rule->mur_name =           "/nkn/accesslog/config/profile/*/max-filesize";
    rule->mur_lowerbound    =  0;
    rule->mur_upperbound    = 100;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =   MUTT_DELETE;
    rule->mur_name =           "/nkn/accesslog/config/profile/*/max-fileid";
    MD_ADD_RULE(ra);


bail:
    return err;
}

/*---------------------------------------------------------------------------*/
static int
md_acclog_commit_check(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
        int err = 0;
        const mdb_db_change *change = NULL;
        int i = 0, num_changes = 0;
        uint16 filesize = 0;
        uint16 diskspace = 0;
        const bn_attrib *new_val = NULL;
        uint16 new_filesize = 0;
        uint16 new_diskspace = 0;
	uint32 new_time_interval = 0;

	const bn_attrib *attrib = NULL;
	char *acclog_name = NULL, *name = NULL;
	tstr_array *acclogs = NULL;
	const char *nd_name = NULL;
	tstring *t_name = NULL;
	bn_binding *binding = NULL;
	tstr_array *name_parts = NULL;
	uint32_t j;


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

	    if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "accesslog", "config","profile", "default")) {
		if (change->mdc_change_type == mdct_delete) {
		    err = md_commit_set_return_status_msg(commit, 1, "Cannot delete default accesslog profile.");
		    bail_force(err);
		}
	    }

		if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "accesslog", "config", "profile", "*"))
		{

		    if (change->mdc_change_type == mdct_delete) {
			bail_null(change->mdc_old_attribs);
			attrib = bn_attrib_array_get_quick(change->mdc_old_attribs, ba_value);
			bail_null(attrib);

			err = bn_attrib_get_str(attrib, NULL, bt_string, NULL, &acclog_name);
			bail_error_null(err, acclog_name);

			err = mdb_get_matching_tstr_array(commit,
				new_db,
				"/nkn/nvsd/namespace/*/accesslog",
				0,
				&acclogs);
			bail_error_null(err, acclogs);

			for(j = 0; j < tstr_array_length_quick(acclogs); j++) {
			    nd_name = tstr_array_get_str_quick(acclogs, j);
			    bail_null(nd_name);

			    err = mdb_get_node_binding(commit, new_db, nd_name, 0, &binding);
			    bail_error_null(err, binding);

			    err = bn_binding_get_tstr(binding, ba_value, 0, 0, &t_name);
			    bail_error_null(err, t_name);

			    if (ts_equal_str(t_name, acclog_name, false)) {
				const char *ns_name = NULL;

				err = ts_tokenize_str(nd_name, '/', '\\', ' ', 0, &name_parts);
				bail_error(err);

				ns_name = tstr_array_get_str_quick(name_parts, 4);
				bail_null(ns_name);

				err = md_commit_set_return_status_msg_fmt(commit , 1,
					"Access log is bound to namespace %s", ns_name);
				bail_error(err);
				goto bail;
			    }

			    ts_free(&t_name);
			    bn_binding_free(&binding);
			    tstr_array_free(&name_parts);
			}
			tstr_array_free(&acclogs);
			safe_free(acclog_name);
		    }
		}

	}

	err = md_commit_set_return_status(commit, 0);
	bail_error(err);

bail:
	ts_free(&t_name);
	bn_binding_free(&binding);
	tstr_array_free(&name_parts);
	tstr_array_free(&acclogs);
	safe_free(acclog_name);

	return err;
}

/*---------------------------------------------------------------------------*/
static int
md_acclog_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_db,
        mdb_db_change_array *change_list,
        void *arg)
{
        int err = 0,offsetN = 0,offsetv =0;
	tbool found = false;
	uint32 num_changes = 0;
	uint32 i = 0;
        const mdb_db_change *change = NULL;
        tstring *format = NULL;

        num_changes = mdb_db_change_array_length_quick(change_list);
	for(i = 0; i < num_changes; i++) {
	    change = mdb_db_change_array_get_quick(change_list, i);
	    bail_null(change);

	    if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "accesslog", "config", "profile", "*", "format"))
		&& (change->mdc_change_type != mdct_delete)) {
	        err = mdb_get_node_value_tstr(commit, inout_db,
           			     ts_str(change->mdc_name),0, &found, &format);
 		bail_error_null(err, format);
		    err = ts_find_str(format,0,"%N",&offsetN);
                    bail_error(err);
		    err = ts_find_str(format,0,"%v",&offsetv);
                    bail_error(err);
		    if((!ts_equal_str(format, w3c_ext_default, false))&&((offsetN == -1)||(offsetv == -1))){
		        if(offsetN == -1){
			    ts_append_str(format," %N");
		        }
		        if(offsetv == -1){
                            ts_append_str(format," %v");
		        }
		            err = mdb_set_node_str(NULL,inout_db, bsso_modify, 0,
         		                   bt_string, ts_str(format), "%s",ts_str(change->mdc_name));
		            bail_error(err);
                        }
	             }
	    }
bail:

        return err;
}

/*---------------------------------------------------------------------------*/
static int
md_acclog_commit_apply(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
    int err = 0;
    tbool is_enabled = false;
    bn_binding *binding = NULL;

    err = mdb_get_node_binding(commit, new_db,
            "/pm/process/nknaccesslog/auto_launch",
            0, &binding);
    bail_error(err);

    if (binding) {
        err = bn_binding_get_tbool(binding, ba_value, 0, &is_enabled);
        bail_error(err);
    }

    if (is_enabled) {
        err = md_send_signal_to_process(commit, SIGHUP, "nknaccesslog", false);
        if (err) {
            err = md_commit_set_return_status_msg(commit, 1,
                    _("Unable to communicate changes to accesslog.  "
                      "Disable and then enable accesslog to flush changes."));
            bail_error(err);
        }
    }
bail:
        return err;
}

static int md_acclog_file_upload_config(md_commit *commit, mdb_db **db,
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
    tstring *profile = NULL;
    char *url_node = NULL;
    char *pass_node = NULL;

    bail_require(!strcmp(action_name, "/nkn/accesslog/action/upload"));

    err = bn_binding_array_get_value_tstr_by_name(params, "profile", NULL,
	    &profile);
    bail_error(err);

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
	} else if ((lc_is_prefix("sftp://", ts_str(remote_url), false))){
	    err = lu_parse_sftp_url(ts_str(remote_url),
		    &user, &pass,
		    &hostname, NULL, &path);
	    bail_error(err);

	    rurl = smprintf("sftp://%s@%s%s", user, hostname, path);
	    bail_null(rurl);
	} else {
	    rurl = smprintf("%s", ts_str(remote_url));
	    bail_null(rurl);

	    pass = smprintf("%s", ts_str(password));
	    bail_null(pass);
	}
	if(pass == NULL) {
	    pass = smprintf("");
	    bail_null(pass);
	}
    } else {
        rurl = smprintf("%s", ts_str(remote_url));
        bail_null(rurl);

        pass = smprintf("%s", ts_str(password));
        bail_null(pass);
    }

    url_node = smprintf("/nkn/accesslog/config/profile/%s/upload/url", ts_str(profile));
    bail_null(url_node);

    pass_node = smprintf("/nkn/accesslog/config/profile/%s/upload/pass", ts_str(profile));
    bail_null(pass_node);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_binding_new_str(&binding, url_node, ba_value, bt_string, 0, rurl);
    bail_error(err);

    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);

    bn_binding_free(&binding);

    err = bn_binding_new_str(&binding, url_node, ba_value, bt_string, 0, rurl);
    bail_error(err);

    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);

    err = bn_binding_new_str(&binding, pass_node, ba_value, bt_string, 0, pass);
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
    safe_free(url_node);
    safe_free(pass_node);
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}

int
md_acclog_check_format(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        const mdb_db_change_array *change_list,
        const tstring *node_name,
        const tstr_array *node_name_parts,
        mdb_db_change_type change_type,
        const bn_attrib_array *old_attribs,
        const bn_attrib_array *new_attribs,
        void *arg)
{

    int err = 0;
    const bn_attrib *new_value = NULL;
    tstring *new_value_ts = NULL;
    char ch;
    const char *p = NULL;

    if (new_attribs == NULL ) {
        goto bail;
    }
    new_value = bn_attrib_array_get_quick(new_attribs, ba_value);
    if (new_value == NULL ) {
        goto bail;
    }

    err = bn_attrib_get_tstr(new_value, NULL, bt_string, NULL, &new_value_ts);
    bail_error(err);

    p = ts_str(new_value_ts); //pformat;

    if ( (NULL !=  strstr(p, "%i")) ) {
        err = md_commit_set_return_status_msg(commit, 1, "Unexpected format at '%i'.Should be '{xxx}i'");
        bail_error(err);
    }
    if ( (NULL !=  strstr(p, "%o")) ) {
        err = md_commit_set_return_status_msg(commit, 1, "Unexpected format at '%o'.Should be '{xxx}o'");
        bail_error(err);
    }
    if ( (NULL !=  strstr(p, "%C"))) {
        err = md_commit_set_return_status_msg(commit, 1, "Unexpected format at '%C'.Should be '{xxx}C'");
        bail_error(err);
    }

	while(*p) {
        ch = *p++;

		if (ch == 0) break;

		// It is string.
		if(ch != '%') {
			while(1) {
				ch = *p++;
				if ((ch=='%') || (ch==0))
                    break;
			}
			if (ch == 0)
                break; // break the whole while(1) loop
		}
	
		/* search for the terminating command */
        ch = *p++;
		if (ch == '{') {
			while(1) {
                ch = *p++;
				if (ch == 0) {
                    // This is where we return commit error with message
                    err = md_commit_set_return_status_msg_fmt(commit, lc_err_parse_error,
                            _("Unexpected end of string."));
                    bail_error(err);
				}
				else if (ch == '}')
                    break;
			}

			// Get next char after '}'
            ch = *p++;
			if((ch != 'i') && (ch != 'o') && (ch != 'C')) {
            //err = get_field_value(ch);
            //if ( err == lc_err_parse_error ) {
                err = md_commit_set_return_status_msg_fmt(commit, lc_err_parse_error,
                        _("Unexpected format at '%c'."), ch);
                bail_error(err);
            }
		}
		else if(strchr(fmap, ch)==NULL){
			//err = get_field_value(ch);
            //if (err == lc_err_parse_error) {
            err = md_commit_set_return_status_msg_fmt(commit, lc_err_parse_error,
                        _("Unexpected format at '%c'."), ch);
            bail_error(err);
           	}
	}

bail:
    ts_free(&new_value_ts);
    return err;
}

static int
md_acclog_files_get(md_commit *commit, const mdb_db *db,
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

    /* Verify this is "/nkn/accesslog/logfiles/ *" */
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
md_acclog_files_iterate(md_commit *commit, const mdb_db *db,
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
	    "/nkn/accesslog/config/filename",
		    0, NULL, &t_log_format);
    bail_error(err);

    num_names = tstr_array_length_quick(names);
    for (i = 0; i < num_names; ++i) {
        name = tstr_array_get_str_quick(names, i);
        bail_null(name);
        if (t_log_format && lc_is_prefix( ts_str(t_log_format), name, false)&& (lc_is_suffix(".gz", name, false)) ){
            err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
            bail_error(err);
        } else if ( t_log_format && (strcmp(ts_str(t_log_format), name)) == 0) {
            err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
            bail_error(err);
	}
    }

 bail:
    tstr_array_free(&names);
    ts_free(&t_log_format);
    return(err);
}

static int
md_acclog_upgrade_downgrade(const mdb_db *old_db,
			    mdb_db *inout_new_db,
			    uint32 from_version,
			    uint32 to_version,
			    void *arg)
{
    int err = 0;
    bn_str_value *profile_default = NULL;
    uint32 num_vals = 0;

    /* Handle generic upgrade rules first */
    err = md_generic_upgrade_downgrade(old_db, inout_new_db,
				       from_version, to_version, arg);
    bail_error(err);

    if (to_version == 9) {
	/* NOTE: DhruvaN, 09/01/2011
	 * Major change in bringing in profile based log configs
	 */

	/* Create a profile called "default" */
	err = md_acclog_create_profile_default(inout_new_db, &profile_default, &num_vals);
	bail_error(err);

	/* We have a bn_str_value list which we can go ahead and create
	 * in the new db */
	err = mdb_create_node_bindings(NULL, inout_new_db, profile_default, num_vals);
	bail_error(err);

	/* Finally delete nodes that we dont want anymore */
	err = md_acclog_ug_delete_node_ver8(old_db, inout_new_db, arg);
	bail_error(err);
    }

bail:
    if (profile_default != NULL) {
	md_acclog_ug_default_profile_free(&profile_default, num_vals);
    }
    return err;
}


static void
md_acclog_ug_default_profile_free(bn_str_value  **profile_default, uint32 num_vals)
{
    uint32 i = 0;
    bn_str_value  *bn_str_list = *profile_default;

    if (bn_str_list == NULL || num_vals == 0)
	return;

    /* free all allocated fields */
    /* HACK: compiler idiocy.. complains because bv_value is a const */
#define  SAFE_FREE(var) \
    do {\
	void *p = (void*) var; \
	safe_free(p); \
	var = NULL; \
    } while(0)
    SAFE_FREE(bn_str_list[1].bv_value);
    SAFE_FREE(bn_str_list[2].bv_value);
    SAFE_FREE(bn_str_list[3].bv_value);
    SAFE_FREE(bn_str_list[4].bv_value);
    SAFE_FREE(bn_str_list[5].bv_value);
    SAFE_FREE(bn_str_list[6].bv_value);
    SAFE_FREE(bn_str_list[7].bv_value);
    SAFE_FREE(bn_str_list[8].bv_value);
    SAFE_FREE(bn_str_list[9].bv_value);

    /* index 9+ are resp-codes where the node and value was allocated. */
    if (num_vals > 10) {
	/* We have some response code nodes at the end of this list,
	 * that were strdup'ed, free them */
	for(i = 10; i < num_vals; i++) {
	    SAFE_FREE(bn_str_list[i].bv_name);
	    SAFE_FREE(bn_str_list[i].bv_value);
	}
    }
#undef SAFE_FREE

    /* free the list itself */
    safe_free(bn_str_list);
    *profile_default = NULL;
    return;
}

static int
md_acclog_create_profile_default(const mdb_db *old_db,
				 bn_str_value  **profile,
				 uint32 *num_vals)
{
    int err = 0;
    bn_str_value  *bn_str_list = NULL;
    tstr_array *ts_respcode_array = NULL;
    tstring *ts_respcode = NULL;
    uint32 num_resp_codes = 0;
    uint32 i = 0;
    tbool found = false;
    tstring *ts_filename = NULL,
	    *ts_filesize = NULL,
	    *ts_timeinterval = NULL,
	    *ts_format = NULL,
	    *ts_url = NULL,
	    *ts_pass = NULL,
	    *ts_maxfileid = NULL,
	    *ts_skip_size = NULL;
    const char *fmt = NULL;

    bail_null(profile);
    bail_null(num_vals);


    /* Get the number of response codes to be filled. */
    err = mdb_get_node_value_tstr(NULL, old_db,
				  "/nkn/accesslog/config/skip_response_code",
				  0,
				  &found,
				  &ts_respcode);
    bail_error(err);

    /* skip_response_code doesn't exist in 2.0.9 and old releases */
    if ( found && ts_respcode) {
	err = ts_tokenize(ts_respcode, ' ', '\\', '"',
		ttf_ignore_trailing_separator | ttf_single_empty_token_null,
		&ts_respcode_array);
	bail_error(err);
    }
    /* if input is null, then num_resp_codes will be 0 */
    num_resp_codes = tstr_array_length_quick(ts_respcode_array);

    bn_str_list = (bn_str_value  *) calloc(10 + num_resp_codes, sizeof(bn_str_value ));
    bail_null(bn_str_list);

    /* Get the file name */
    err = mdb_get_node_value_tstr(NULL, old_db,
				  "/nkn/accesslog/config/filename",
				  0, NULL,
				  &ts_filename);
    bail_error(err);

    /* Get the file-size */
    err = mdb_get_node_value_tstr(NULL, old_db,
				  "/nkn/accesslog/config/max-filesize",
				  0, NULL,
				  &ts_filesize);
    bail_error(err);

    /* Get the time-interval */
    err = mdb_get_node_value_tstr(NULL, old_db,
				  "/nkn/accesslog/config/time-interval",
				  0, NULL,
				  &ts_timeinterval);
    bail_error(err);


    /* Get the log-format */
    err = mdb_get_node_value_tstr(NULL, old_db,
				  "/nkn/accesslog/config/format",
				  0, NULL,
				  &ts_format);
    bail_error(err);

    /* Get the upload url */
    err = mdb_get_node_value_tstr(NULL, old_db,
				  "/nkn/accesslog/config/upload/url",
				  0, NULL,
				  &ts_url);
    bail_error(err);

    /* Get the upload passwd */
    err = mdb_get_node_value_tstr(NULL, old_db,
				  "/nkn/accesslog/config/upload/pass",
				  0, NULL,
				  &ts_pass);
    bail_error(err);

    /* get max-fileid */
    err = mdb_get_node_value_tstr(NULL, old_db,
				  "/nkn/accesslog/config/max-fileid",
				  0, NULL,
				  &ts_maxfileid);
    bail_error(err);

    /* Get the object size for skip */
    err = mdb_get_node_value_tstr(NULL, old_db,
				  "/nkn/accesslog/config/skip_object_size",
				  0, NULL,
				  &ts_skip_size);
    bail_error(err);

    /* Profile name */
    bn_str_list[0].bv_name	= "/nkn/accesslog/config/profile/default";
    bn_str_list[0].bv_type	= bt_string;
    bn_str_list[0].bv_value	= "default";

    bn_str_list[1].bv_name	= "/nkn/accesslog/config/profile/default/filename";
    bn_str_list[1].bv_type	= bt_string;
    bn_str_list[1].bv_value	= strdup(ts_str(ts_filename));

    bn_str_list[2].bv_name	= "/nkn/accesslog/config/profile/default/max-filesize";
    bn_str_list[2].bv_type	= bt_uint16;
    bn_str_list[2].bv_value	= strdup(ts_str(ts_filesize));

    const char *str_sz = "0";
    if (ts_equal_str(ts_filesize, "0", false)) {
rebase_size:
        bn_str_list[2].bv_value = strdup(str_sz);
    } else {
        uint16_t sz = atoi(ts_str(ts_filesize));
        if (sz <= 10) {
            str_sz = "10";
            goto rebase_size;
        } else if (sz >= 100) {
            str_sz = "100";
            goto rebase_size;
        } else {
            str_sz = ts_str(ts_filesize);
            goto rebase_size;
        }
    }


    bn_str_list[3].bv_name	= "/nkn/accesslog/config/profile/default/time-interval";
    bn_str_list[3].bv_type	= bt_uint32;
    //bn_str_list[3].bv_value	= strdup(ts_str(ts_timeinterval));

    const char *str_ti = "0";
    if (ts_equal_str(ts_timeinterval, "0", false)) {
rebase_time_interval:
	//safe_free(bn_str_list[3].bv_value);
	bn_str_list[3].bv_value = strdup(str_ti);
    } else {
	uint32_t ti = atoi(ts_str(ts_timeinterval));
	if (ti <= 5) {
	    str_ti = "5";
	    goto rebase_time_interval;
	} else if (ti >= 60) {
	    str_ti = "60";
	    goto rebase_time_interval;
	} else {
	    str_ti = ts_str(ts_timeinterval);
	    goto rebase_time_interval;
	}
    }


    if (ts_equal_str(ts_format, ncsa, false)) {
	fmt = "ncsa-combined";
    } else if (ts_equal_str(ts_format, clf, false)) {
	fmt = "clf";
    } else if (ts_equal_str(ts_format, w3c_ext_default, false)) {
	fmt = "w3c-ext-default";
    } else {
	fmt = "w3c-ext-custom";
	ts_append_str(ts_format, " %N %v");
    }

    bn_str_list[4].bv_name	= "/nkn/accesslog/config/profile/default/format-class";
    bn_str_list[4].bv_type	= bt_string;
    bn_str_list[4].bv_value	= strdup(fmt);

    bn_str_list[5].bv_name	= "/nkn/accesslog/config/profile/default/format";
    bn_str_list[5].bv_type	= bt_string;
    bn_str_list[5].bv_value	= strdup(ts_str(ts_format));

    bn_str_list[6].bv_name	= "/nkn/accesslog/config/profile/default/max-fileid";
    bn_str_list[6].bv_type	= bt_uint32;
    bn_str_list[6].bv_value	= strdup(ts_str(ts_maxfileid));

    bn_str_list[7].bv_name	= "/nkn/accesslog/config/profile/default/object/filter/size";
    bn_str_list[7].bv_type	= bt_uint32;

    /* BZ 11073: for 2.0.9, this node doesnt exist at all.
     * So an upgrade from 2.0.9 to 11.A.1 would yield a NULL or empty string.
     * In either case, we MUST rebase the value to a (uint32) 0 when we
     * create the node here
     */
    const char *s_size = ts_str_maybe_empty(ts_skip_size); // returns empty string "" if string is NULL
    s_size = (safe_strlen(s_size) == 0) ? "0" : s_size; // we dont know if we got a NULL or a ""
    bn_str_list[7].bv_value	= strdup(s_size);

    bn_str_list[8].bv_name	= "/nkn/accesslog/config/profile/default/upload/url";
    bn_str_list[8].bv_type	= bt_string;
    bn_str_list[8].bv_value	= strdup(ts_str(ts_url));

    bn_str_list[9].bv_name	= "/nkn/accesslog/config/profile/default/upload/pass";
    bn_str_list[9].bv_type	= bt_string;
    bn_str_list[9].bv_value	= strdup(ts_str(ts_pass));

    for(i = 0; i < num_resp_codes; i++) {
	char *node_name;
	node_name =
	    smprintf("/nkn/accesslog/config/profile/default/object/filter/resp-code/%s",
		    tstr_array_get_str_quick(ts_respcode_array, i));
	bn_str_list[10+i].bv_name = strdup(node_name);
	bn_str_list[10+i].bv_type = bt_uint16;
	bn_str_list[10+i].bv_value =
	    strdup(tstr_array_get_str_quick(ts_respcode_array, i));

	safe_free(node_name);
    }

    *profile = bn_str_list;
    *num_vals = (10 + num_resp_codes);

bail:
    ts_free(&ts_filename);
    ts_free(&ts_filesize);
    ts_free(&ts_timeinterval);
    ts_free(&ts_format);
    ts_free(&ts_url);
    ts_free(&ts_pass);
    ts_free(&ts_maxfileid);
    ts_free(&ts_skip_size);
    return err;
}

static int
md_acclog_ug_delete_node_ver8(const mdb_db *old_db,
			      mdb_db *inout_new_db,
			      void *arg)
{
    int err = 0;
    const char *old_node_list[] = {
	"/nkn/accesslog/config/filename",
	"/nkn/accesslog/config/max-filesize",
	"/nkn/accesslog/config/time-interval",
	"/nkn/accesslog/config/format",
	"/nkn/accesslog/config/upload/url",
	"/nkn/accesslog/config/upload/pass",
	"/nkn/accesslog/config/max-fileid",
	"/nkn/accesslog/config/skip_object_size",
	"/nkn/accesslog/config/skip_response_code"
    };
    uint32 i;

    for(i = 0; i < sizeof(old_node_list)/sizeof(char *); i++) {
	err = mdb_dbbe_delete_node(NULL, inout_new_db, old_node_list[i]);
	bail_error(err);
    }

bail:
    return err;
}

static int
md_acclog_get_format_class(md_commit *commit, const mdb_db *db,
	    const char *node_name,
	    const bn_attrib_array *node_attribs,
	    bn_binding **ret_binding,
	    uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    const char *profile = NULL;
    tstr_array *name_parts = NULL;
    tstring *format = NULL;
    char *fmt_node_name = NULL;
    const char *format_class = NULL;

    err = bn_binding_name_to_parts(node_name, &name_parts, NULL);
    bail_error(err);

    profile = tstr_array_get_str_quick(name_parts, 4);
    bail_null(profile);

    fmt_node_name = smprintf("/nkn/accesslog/config/profile/%s/format", profile);
    bail_null(fmt_node_name);

    err = mdb_get_node_value_tstr(NULL, db,
	    fmt_node_name, 0, NULL, &format);
    bail_error_null(err, format);

    if (ts_equal_str(format, w3c_ext_default, false)) {
	format_class = "w3c-ext-default";
    } else if (ts_equal_str(format, clf, false)) {
	format_class = "clf";
    } else if (ts_equal_str(format, ncsa, false)) {
	format_class = "ncsa-combined";
    } else {
	format_class = "w3c-ext-custom";
    }

    err = bn_binding_new_str(ret_binding, node_name, ba_value,
	    bt_string, 0, format_class);
    bail_error(err);

bail:
    safe_free(fmt_node_name);
    ts_free(&format);
    tstr_array_free(&name_parts);
    return err;
}

static int
md_acclog_mon_iterate(md_commit *commit, const mdb_db *db,
	const char *parent_node_name,
	const uint32_array *node_attrib_ids,
	int32 max_num_nodes, int32 start_child_num,
	const char *get_next_child,
	mdm_mon_iterate_names_cb_func iterate_cb,
	void *iterate_cb_arg, void *arg)
{
    int err = 0;
    uint32 i = 0, num_bindings = 0;
    char *name = NULL;
    bn_binding *binding = NULL;
    bn_binding_array *bindings = NULL;


    err = mdb_iterate_binding(commit, db, "/nkn/accesslog/config/profile",
	    0, &bindings);
    bail_error(err);

    err = bn_binding_array_length(bindings, &num_bindings);
    bail_error(err);

    for (i = 0; i < num_bindings; i++) {
	safe_free(name);

	err = bn_binding_array_get(bindings, i, &binding);
	bail_error(err);


	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &name);
	bail_error(err);

	err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
	bail_error(err);
    }

bail:
    bn_binding_array_free(&bindings);
    safe_free(name);
    return err;
}

static int
md_acclog_mon_get(md_commit *commit, const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding, uint32 *ret_node_flags,
	void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    const char *profile = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error(err);

    profile = tstr_array_get_str_quick(parts, 4);
    bail_null(profile);

    err = bn_binding_new_str(ret_binding, node_name, ba_value,
	    bt_string, 0, profile);
    bail_error(err);

bail:
    tstr_array_free(&parts);
    return err;
}
static int
md_acclog_rotate_size_commit_check(md_commit *commit,
                           const mdb_db *old_db, const mdb_db *new_db,
                           const mdb_db_change_array *change_list,
                           const tstring *node_name,
                           const tstr_array *node_name_parts,
                           mdb_db_change_type change_type,
                           const bn_attrib_array *old_attribs,
                           const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const bn_attrib *new_val = NULL;
    uint16 size;
    const char *profile = NULL;
    char acc_node[1024] = {0};
    uint32 value = 0;

    if ((mdct_add == change_type) ||
        (mdct_modify == change_type)) {
        new_val = bn_attrib_array_get_quick(new_attribs, ba_value);

        err = bn_attrib_get_uint16(new_val, NULL, NULL, &size);
        bail_error(err);

        if ((size == 0) ||
            (size >= 10 && size <= 100)) {
            if(size == 0){
                profile = tstr_array_get_str_quick(node_name_parts,4);
                bail_null(profile);
                snprintf(acc_node, sizeof(acc_node),
                    "/nkn/accesslog/config/profile/%s/time-interval", profile);
                err = mdb_get_node_value_uint32(commit, new_db, acc_node,
                         0, NULL, &value);
                bail_error(err);
                if(!value){
                    err = md_commit_set_return_status_msg_fmt(commit, 1,
                       "Both max-duration and max-filesize cannot be disabled");
                    goto bail;
                }
            }

            err = md_commit_set_return_status(commit, 0);
            bail_error(err);
        } else {
            err = md_commit_set_return_status_msg_fmt(commit, 1,
                    "max-size should be between 10 to 100 MiB;"
                    " or 0 to disable size based rotation");
            bail_error(err);
        }
    }

bail:
    return err;
}



static int
md_acclog_rotate_time_commit_check(md_commit *commit,
                           const mdb_db *old_db, const mdb_db *new_db,
                           const mdb_db_change_array *change_list,
                           const tstring *node_name,
                           const tstr_array *node_name_parts,
                           mdb_db_change_type change_type,
                           const bn_attrib_array *old_attribs,
                           const bn_attrib_array *new_attribs, void *arg)
{
    int err = 0;
    const bn_attrib *new_val = NULL;
    uint32 time_interval;
    const char *profile = NULL;
    char acc_node[1024] = {0};
    uint16 value = 0;

    if ((mdct_add == change_type) ||
	(mdct_modify == change_type)) {
	new_val = bn_attrib_array_get_quick(new_attribs, ba_value);

	err = bn_attrib_get_uint32(new_val, NULL, NULL, &time_interval);
	bail_error(err);

	if ((time_interval == 0) ||
	    (time_interval >= 5 && time_interval <= 60)) {
            if(time_interval == 0){
                profile = tstr_array_get_str_quick(node_name_parts,4);
                bail_null(profile);
                snprintf(acc_node, sizeof(acc_node),
                    "/nkn/accesslog/config/profile/%s/max-filesize", profile);
                err = mdb_get_node_value_uint16(commit, new_db, acc_node,
                         0, NULL, &value);
                bail_error(err);
                if(!value){
                    err = md_commit_set_return_status_msg_fmt(commit, 1,
                       "Both max-duration and max-filesize cannot be disabled");
                    goto bail;
                }
            }

	    err = md_commit_set_return_status(commit, 0);
	    bail_error(err);
	} else {
	    err = md_commit_set_return_status_msg_fmt(commit, 1,
		    "max-duration should be between 5 to 60 minutes;"
		    " or 0 to disable time based rotation");
	    bail_error(err);
	}
    }

bail:
    return err;
}


/*----------------------------------------------------------------------------
 * END OF FILE
 *---------------------------------------------------------------------------*/

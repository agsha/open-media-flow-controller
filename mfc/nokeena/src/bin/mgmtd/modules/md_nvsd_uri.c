/*
 *
 * Filename:    md_nvsd_uri.c
 * Date:        2009/01/11
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"


static md_commit_forward_action_args md_uri_fwd_args =
{
    GCL_CLIENT_NVSD, true, N_("Request failed; MFD subsystem did not respond"),
    mfat_blocking
#ifdef PROD_FEATURE_I18N_SUPPORT
	, GETTEXT_DOMAN
#endif
};


static md_upgrade_rule_array *md_nvsd_uri_ug_rules = NULL;

int md_nvsd_uri_init(
        const lc_dso_info *info,
        void *data);

static int md_nvsd_uri_commit_side_effects( md_commit *commit,
				const mdb_db *old_db, mdb_db *inout_new_db,
				mdb_db_change_array *change_list, void *arg);

static int md_nvsd_uri_commit_check(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int md_nvsd_uri_commit_apply(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

int md_nvsd_uri_nfs_host_validate(md_commit *commit,
                                const mdb_db *old_db,
                                const mdb_db *new_db,
                                const mdb_db_change_array *change_list,
                                const tstring *node_name,
                                const tstr_array *node_name_parts,
                                mdb_db_change_type change_type,
                                const bn_attrib_array *old_attribs,
                                const bn_attrib_array *new_attribs,
                                void *arg);

int md_nvsd_uri_http_host_validate(md_commit *commit,
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
md_nvsd_uri_upgrade_downgrade(const mdb_db *old_db,
                              mdb_db *inout_new_db,
                              uint32 from_module_version,
                              uint32 to_module_version,
                              void *arg);

static int
md_nvsd_uri_check_proxy_mode_allowed(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg,
		tbool *is_allowed);

int
md_nvsd_uri_init(
        const lc_dso_info *info,
        void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    err = mdm_add_module("nvsd-uri", 6,
	    "/nkn/nvsd/uri/", "/nkn/nvsd/uri/config",
	    modrf_all_changes,
	    md_nvsd_uri_commit_side_effects,   // side_effects_func
	    NULL,
	    md_nvsd_uri_commit_check, NULL,
	    md_nvsd_uri_commit_apply, NULL,
	    0, 0,
	    md_nvsd_uri_upgrade_downgrade, //  md_generic_upgrade_downgrade,
	    &md_nvsd_uri_ug_rules,
	    NULL, NULL,
	    NULL, NULL, NULL, NULL,
	    &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*";
    node->mrn_value_type =          bt_string;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Namespace configuration";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*";
    node->mrn_value_type =          bt_uri; // Should this be bt_uri??
    node->mrn_limit_str_max_chars = 256;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "URI Prefix";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/domain/host";
    node->mrn_value_type =          bt_string;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value =       "*"; /* default is any */
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "refers to hostname";
    err = mdm_add_node(module, &node);
    bail_error(err);

    // BZ 2376
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/domain/regex";
    node->mrn_value_type =          bt_regex;
    node->mrn_initial_value =       ""; /* default is any */
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "A regex mapping configuration";
    err = mdm_add_node(module, &node);
    bail_error(err);

    // BZ 2376
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/domain/regex/map";
    node->mrn_value_type =          bt_regex;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "A regex mapping configuration";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/delivery/proto/http";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "HTTP as delivery protocol";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/delivery/proto/rtmp";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "RTMP as delivery protocol";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/delivery/proto/rtsp";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "RTSP as delivery protocol";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/delivery/proto/rtp";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "RTP as delivery protocol";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/http/host/*";
    node->mrn_value_type =          bt_string;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_check_node_func =     md_nvsd_uri_http_host_validate;
    node->mrn_check_node_arg =      NULL;
    node->mrn_description =         "refers to hostname";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/http/host/*/port";
    node->mrn_value_type =          bt_uint16;
    node->mrn_initial_value =       "80";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "refers to port number";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/http/host/*/weight";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "50";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "refers to weight";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/http/host/*/keep-alive";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "refers to Keep Alive";
    err = mdm_add_node(module, &node);
    bail_error(err);


    // This appeared first in module_version 6
    // Will hold a reference to an exisiting server-map
    // instead of a http host origin
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/http/server-map";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Points to a server-map reference instead of a host defintion";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/nfs/host";
    node->mrn_value_type =          bt_string;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "NFS host";
    node->mrn_check_node_func =     md_nvsd_uri_nfs_host_validate;
    node->mrn_check_node_arg =      NULL;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/nfs/port";
    node->mrn_value_type =          bt_uint16;
    node->mrn_initial_value =       "2049";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "NFS port";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/nfs/local-cache";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "NFS - Cache locally";
    err = mdm_add_node(module, &node);
    bail_error(err);

    // This appeared first in module_version 6
    // Will hold a reference to an exisiting server-map
    // instead of a nfs host origin
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/nfs/server-map";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Points to a server-map reference instead of a host defintion";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*-----------------------------------------------------------------------
     * This was removed in module_version 6.
     * server-map is replaced by individual server-map definitions
     * under different origin-server types.
     *-----------------------------------------------------------------------
     */

    /* Holds server map reference
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/server-map";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Server map name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Action (handled by NVSD) to validate a domain regex entry
     */
    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name = 		"/nkn/nvsd/uri/actions/validate_domain_regex";
    node->mrn_node_reg_flags = 	mrf_flags_reg_action;
    node->mrn_cap_mask = 	mcf_cap_action_basic;
    node->mrn_action_func = 	md_commit_forward_action;
    node->mrn_action_arg = 	&md_uri_fwd_args;
    node->mrn_actions[0]->mra_param_name = 	"regex";
    node->mrn_actions[0]->mra_param_type = 	bt_regex;
    node->mrn_description = "Action to validate a domain regex";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/delivery/proto/rtstream";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "RTP as delivery protocol";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*-------------------------------------------------*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/rtstream/host/*";
    node->mrn_value_type =          bt_string;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_limit_wc_count_max =  1;
    node->mrn_bad_value_msg = 	    "error: cannot configure more than 1 rt-stream hosts";
    node->mrn_description =         "refers to hostname";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 		"/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/rtstream/host/*/port";
    node->mrn_value_type =	bt_uint16;
    //node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value =	"554";
    node->mrn_node_reg_flags = 	mrf_flags_reg_config_literal;
    node->mrn_cap_mask = 	mcf_cap_node_basic;
    node->mrn_description =	"RT-Stream origin port";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 		"/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/rtstream/host/*/alt_proto";
    node->mrn_value_type =	bt_string;
    node->mrn_initial_value =	"http";
    node->mrn_node_reg_flags = 	mrf_flags_reg_config_literal;
    node->mrn_cap_mask = 	mcf_cap_node_basic;
    node->mrn_description =	"RT-Stream origin - alternate protocol";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 		"/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/rtstream/host/*/alt_port";
    node->mrn_value_type =	bt_uint16;
    node->mrn_initial_value =	"80";
    node->mrn_node_reg_flags = 	mrf_flags_reg_config_literal;
    node->mrn_cap_mask = 	mcf_cap_node_basic;
    node->mrn_description =	"RT-Stream origin  - alternate protocol port";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*********************************************************
     *      UPGRADE PROCESSING RULES
     ********************************************************/
    err = md_upgrade_rule_array_new(&md_nvsd_uri_ug_rules);
    bail_error(err);

    ra = md_nvsd_uri_ug_rules;

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type = 		MUTT_ADD;
    rule->mur_name = 			"/nkn/nvsd/uri/config/*/uri-prefix/*/domain/regex";
    rule->mur_new_value_type = 		bt_regex;
    rule->mur_new_value_str = 		"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type = 		MUTT_ADD;
    rule->mur_name = 			"/nkn/nvsd/uri/config/*/uri-prefix/*/domain/regex/map";
    rule->mur_new_value_type = 		bt_regex;
    rule->mur_new_value_str = 		"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type = 		MUTT_DELETE;
    rule->mur_name = 			"/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/server-map";
    MD_ADD_RULE(ra);

bail:
    return err;
}



/* ------------------------------------------------------------------------- */
static int
md_nvsd_uri_commit_side_effects( md_commit *commit,
			const mdb_db *old_db,
			mdb_db *inout_new_db,
			mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    const char *t_namespace = NULL;
    const char *t_uri = NULL;
    bn_binding *binding = NULL;
    tstr_array *name_parts = NULL;
    int i = 0, num_changes = 0;
    const mdb_db_change *change = NULL;
    char *c_uri = NULL;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	/* Check if domain-name being set */
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 9, "nkn", "nvsd", "uri", "config", "*", "uri-prefix", 
			"*", "origin-server", "http"))
		&& (mdct_add == change->mdc_change_type))
	{
	    /* Get the binding and the nameparts */
	    err = mdb_get_node_binding (commit, inout_new_db, ts_str (change->mdc_name), 0, &binding);
	    bn_binding_get_name_parts (binding, &name_parts);
	    bail_error(err);
	    t_namespace = tstr_array_get_str_quick (name_parts, 4);
	    t_uri = tstr_array_get_str_quick(name_parts, 6);

	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 10, "nkn", "nvsd", "uri", "config", "*", "uri-prefix",
			"*", "origin-server", "nfs", "host"))
		&& (mdct_modify == change->mdc_change_type))
	{
	    /* Get the binding and the nameparts */
	    err = mdb_get_node_binding (commit, inout_new_db, ts_str (change->mdc_name), 0, &binding);
	    bn_binding_get_name_parts (binding, &name_parts);
	    bail_error(err);
	    t_namespace = tstr_array_get_str_quick (name_parts, 4);
	    t_uri = tstr_array_get_str_quick(name_parts, 6);
	}
    }

bail:
    safe_free(c_uri);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_nvsd_uri_commit_check(md_commit *commit,
                     const mdb_db *old_db,
                     const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0, i = 0;
    int num_uris = 2, num_changes = 0;
    tbool	found = false;
    char *node_name = NULL;
    char *binding_name = NULL;
    const char *t_namespace = NULL;
    const char *t_uri = NULL;
    char *c_uri = NULL;
    bn_binding *binding = NULL;
    tstr_array *name_parts = NULL;
    tstr_array *uris_config = NULL;
    const mdb_db_change *change = NULL;
    const char *c_uri_name = NULL;
    tstring *t_uri_prefix = NULL;
    const bn_attrib *new_val = NULL;
    tbool is_allowed = false;
    tstring *msg = NULL;
    uint16 code = 0;
    bn_request *req = NULL;
    tstring *dom_regex_value = NULL;
    tstring *t_server_map = NULL;
    tstring *t_format_type = NULL;
    char *smap_fmt_type_node = NULL;
    tstring *server_map = NULL;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	/* Check if uri-prefix being set */
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "uri", "config", "*", "uri-prefix", "*"))
		&& (mdct_add == change->mdc_change_type))
	{
	    /* Check whether the URI-Prefix has a leading slash '/' or not. If
	     * not, throw an error message
	     */
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL, bt_uri, NULL, &t_uri_prefix);
		bail_error(err);

		c_uri_name = ts_str(t_uri_prefix);
		if (c_uri_name[0] != '/') {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("error: uri-prefix names must start with a '/'"));
		    bail_error(err);
		    goto bail;
		}
	    }
	    /* Get the binding and the nameparts */
	    err = mdb_get_node_binding (commit, new_db, ts_str (change->mdc_name), 0, &binding);
	    bn_binding_get_name_parts (binding, &name_parts);
	    bail_error(err);
	    t_namespace = tstr_array_get_str_quick (name_parts, 4);

	    binding_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix", t_namespace);
	    bail_null(binding_name);

	    err = mdb_get_tstr_array(commit, new_db,
		    binding_name, NULL, false,
		    0, &uris_config);
	    bail_error_null(err, uris_config);
	    num_uris = tstr_array_length_quick(uris_config);
	    if (num_uris > 1)	// Then second entry
	    {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error: uri-prefix already configured for this namespace (%s)\n"),
			tstr_array_get_str_quick(uris_config, 0));
		bail_error(err);
		goto bail;
	    }
	    safe_free(binding_name); // Loop hence freeing it here
	}
	/* Check if DOMAIN-REGEX is being set */
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "uri", "config", "*", "uri-prefix",
			"*", "domain", "regex"))
		&& (mdct_modify == change->mdc_change_type))
	{
	    /* fire action to NVSD to validate the regex.
	     * If regex is unsupported, then fail on commit
	     */
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {

		err = bn_attrib_get_tstr(new_val, NULL, bt_regex, NULL, &dom_regex_value);
		bail_error(err);

		if ( dom_regex_value ) {
		    /* NOTE: The action that we fire MUST not change the database
		     *
		     * Send regex for validation only if it is being set. When the
		     * value is being reset, no need to validate.
		     */
		    if ( ts_num_chars(dom_regex_value) > 0 ) {
			err = bn_action_request_msg_create(&req, "/nkn/nvsd/uri/actions/validate_domain_regex");
			bail_error(err);

			err = bn_action_request_msg_append_new_binding(req, 0, "regex", bt_regex,
				ts_str(dom_regex_value), NULL);
			bail_error(err);

			err = md_commit_handle_action_from_module(commit, req, NULL, &code, &msg, NULL, NULL);
			bail_error(err);

			if (code && msg && (ts_num_chars(msg) > 0)) {
			    err = md_commit_set_return_status_msg_fmt(commit, 1,
				    _("error: Bad or unrecognized expression '%s'\n"),
				    ts_str(dom_regex_value));
			    bail_error(err);
			    goto bail;
			}
		    }
		}
	    }
	}

	/* Validate if server-map type being set is for a HTTP origin */
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 10, "nkn", "nvsd", "uri", "config", "*", "uri-prefix",
			"*", "origin-server", "http", "server-map"))
		&& (mdct_modify == change->mdc_change_type))
	{
	    /* Get the server-map name */
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL, bt_string, NULL, &t_server_map);
		bail_error(err);

		/* BZ 2858
		 * For this node, when the value is reset, the node value is set as
		 * "" - an empty string. Hence we need to check if this a set or
		 * a reset operation by doing this the crooked way. The variable
		 * mdc_new_attribs is always non-null in this case (when user
		 * does a reset).
		 */
		if (ts_num_chars(t_server_map) > 0) {
		    /* user is setting up a server-map */

		    smap_fmt_type_node = smprintf("/nkn/nvsd/server-map/config/%s/format-type",
			    ts_str(t_server_map));
		    bail_null(smap_fmt_type_node);

		    /* Get the format type of the server-map */
		    err = mdb_get_node_value_tstr(commit, new_db, smap_fmt_type_node, 0, &found,
			    &t_format_type);
		    bail_error(err);

		    if (!found) {
			/* Bad server-map name */
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				_("error: non-existant server-map : '%s'\n"),
				ts_str(t_server_map));
			bail_error(err);
			goto bail;
		    }
		    else {
			/* Server map name exists. Check format type */
			if (ts_equal_str(t_format_type, "0", true)) {
			    /* Bad! this is not a valid smap for this origin type */
			    err = md_commit_set_return_status_msg_fmt(commit, 1,
				    _("error: format type of server-map '%s' is invalid.\n"),
				    ts_str(t_server_map));
			    bail_error(err);
			    goto bail;
			}
		    }

		}

		/* BZ 2924: dont allow http server map when http host is already set */
		err = mdb_get_node_binding (commit, new_db, ts_str (change->mdc_name), 0, &binding);
		bn_binding_get_name_parts (binding, &name_parts);
		bail_error(err);
		t_namespace = tstr_array_get_str_quick (name_parts, 4);
		t_uri = tstr_array_get_str_quick(name_parts, 6);

		err = bn_binding_name_escape_str(t_uri, &c_uri);
		bail_error(err);
		binding_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/http/host", t_namespace, c_uri);
		bail_null(binding_name);
		err = mdb_get_tstr_array(commit, new_db,
			binding_name, NULL, false,
			0, &uris_config);
		bail_error_null(err, uris_config);
		num_uris = tstr_array_length_quick(uris_config);
		if (num_uris > 0)
		{
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("error: http host (%s) is already configured for this namespace\n"),
			    tstr_array_get_str_quick(uris_config,0));
		    bail_error(err);
		    goto bail;
		}	
		tstr_array_free(&uris_config); // Since in a loop need to free
		safe_free(binding_name); // Loop hence freeing it here
		/* BZ 2924 END */

	    }

	    /* new_val == NULL, user reset the server-map definition
	     * for this http origin. Do nothing */
	}

	/* Check if origin-server is being set */
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 11, "nkn", "nvsd", "uri", "config", "*", "uri-prefix",
			"*", "origin-server", "http", "host", "*"))
		&& (mdct_add == change->mdc_change_type))
	{
	    /* Get the binding and nameparts */
	    err = mdb_get_node_binding (commit, new_db, ts_str (change->mdc_name), 0, &binding);
	    bn_binding_get_name_parts (binding, &name_parts);
	    bail_error(err);
	    t_namespace = tstr_array_get_str_quick (name_parts, 4);
	    t_uri = tstr_array_get_str_quick(name_parts, 6);

	    /* BZ 2256: Dont allow if proxy-mode doesnt
	     * permit us to add an origin
	     */
	    err = md_nvsd_uri_check_proxy_mode_allowed(commit, old_db, new_db, change_list,
		    (void*) t_namespace, &is_allowed);
	    bail_error(err);

	    if (!is_allowed) {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error: cant set an origin-server when this namespace is "
			    " configured for proxy-mode as 'mid-tier' or 'virtual'"));
		bail_error(err);
		goto bail;
	    }
	    /* End BZ 2256 */

	    err = bn_binding_name_escape_str(t_uri, &c_uri);
	    bail_error(err);

	    binding_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/http/host",
		    t_namespace, c_uri);
	    bail_null(binding_name);

	    err = mdb_get_tstr_array(commit, new_db,
		    binding_name, NULL, false,
		    0, &uris_config);
	    bail_error_null(err, uris_config);
	    num_uris = tstr_array_length_quick(uris_config);
	    if (num_uris > 1)	// Then second entry
	    {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error: HTTP origin server already configured for this uri-prefix (%s)\n"),
			tstr_array_get_str_quick(name_parts, 6));
		bail_error(err);
		goto bail;
	    }
	    tstr_array_free(&uris_config); // Since in a loop need to free
	    safe_free(binding_name); // Loop hence freeing it here

	    /* BZ 2924: dont allow http host when http server map is already set */
	    binding_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/http/server-map",
		    t_namespace, c_uri);
	    bail_null(binding_name);

	    err = mdb_get_node_value_tstr (commit, new_db, binding_name,
		    0, NULL, &server_map);
	    bail_error(err);

	    if (ts_length(server_map) > 0)
	    {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error: http server map (%s) is already configured for this namespace\n"),
			ts_str(server_map));
		bail_error(err);
		goto bail;
	    }	
	    ts_free(&server_map); // Since in a loop need to free
	    safe_free(binding_name); // Loop hence freeing it here
	    /* END OF BZ 2924 */
	}
	/* Check if it is a NFS origin server */
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 10, "nkn", "nvsd", "uri", "config", "*", "uri-prefix",
			"*", "origin-server", "nfs", "host"))
		&& (mdct_modify == change->mdc_change_type))
	{
	    err = mdb_get_node_binding (commit, new_db, ts_str (change->mdc_name), 0, &binding);
	    bn_binding_get_name_parts (binding, &name_parts);
	    bail_error(err);
	    t_namespace = tstr_array_get_str_quick (name_parts, 4);
	    t_uri = tstr_array_get_str_quick(name_parts, 6);

	    err = bn_binding_name_escape_str(t_uri, &c_uri);
	    bail_error(err);

	    /* BZ 2256: Dont allow if proxy-mode doesnt
	     * permit us to add an origin
	     */
	    err = md_nvsd_uri_check_proxy_mode_allowed(commit, old_db, new_db, change_list,
		    (void*) t_namespace, &is_allowed);
	    bail_error(err);

	    if (!is_allowed) {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error: cant set an origin-server when this namespace is "
			    " configured for proxy-mode as 'mid-tier' or 'virtual'"));
		bail_error(err);
		goto bail;
	    }
	    /* End BZ 2256 */
	    /* BZ 2924: dont allow nfs host when nfs server map is already set */

	    binding_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/nfs/server-map",
		    t_namespace, c_uri);
	    bail_null(binding_name);

	    err = mdb_get_node_value_tstr (commit, new_db, binding_name,
		    0, NULL, &server_map);
	    bail_error(err);

	    if (ts_length(server_map) > 0)
	    {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error: nfs server map (%s) is already configured for this namespace\n"),
			ts_str(server_map));
		bail_error(err);
		goto bail;
	    }	
	    ts_free(&server_map); // Since in a loop need to free
	    safe_free(binding_name); // Loop hence freeing it here
	    /* End BZ 2924 */


	}

	/* Validate if server-map type being set is for a NFS origin */
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 10, "nkn", "nvsd", "uri", "config", "*", "uri-prefix",
			"*", "origin-server", "nfs", "server-map"))
		&& (mdct_modify == change->mdc_change_type))
	{
	    /* Get the server-map name */
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL, bt_string, NULL, &t_server_map);
		bail_error(err);

		smap_fmt_type_node = smprintf("/nkn/nvsd/server-map/config/%s/format-type",
			ts_str(t_server_map));
		bail_null(smap_fmt_type_node);

		/* Get the format type of the server-map */
		err = mdb_get_node_value_tstr(commit, new_db, smap_fmt_type_node, 0, &found,
			&t_format_type);
		bail_error(err);

		if (!found) {
		    /* Bad server-map name */
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("error: non-existant server-map : '%s'\n"),
			    ts_str(t_server_map));
		    bail_error(err);
		    goto bail;
		}
		else {
		    /* Server map name exists. Check format type */
		    if (ts_equal_str(t_format_type, "0", true)) {
			/* Bad! this is not a valid smap for this origin type */
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				_("error: format type of server-map '%s' is invalid.\n"),
				ts_str(t_server_map));
			bail_error(err);
			goto bail;
		    }
		}

		/* BZ 2924: dont allow server map when nfs host is already set */
		err = mdb_get_node_binding (commit, new_db, ts_str (change->mdc_name), 0, &binding);
		bn_binding_get_name_parts (binding, &name_parts);
		bail_error(err);

		t_namespace = tstr_array_get_str_quick (name_parts, 4);
		t_uri = tstr_array_get_str_quick(name_parts, 6);

		err = bn_binding_name_escape_str(t_uri, &c_uri);
		bail_error(err);
		binding_name = smprintf("/nkn/nvsd/uri/config/%s/uri-prefix/%s/origin-server/nfs/host",
			t_namespace, c_uri);
		bail_null(binding_name);
		err = mdb_get_node_value_tstr (commit, new_db, binding_name,
			0, NULL, &server_map);
		bail_error(err);

		if (ts_length(server_map) > 0)
		{
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("error: nfs host (%s) is already configured for this namespace\n"),
			    ts_str(server_map));
		    bail_error(err);
		    goto bail;
		}	
		ts_free(&server_map); // Since in a loop need to free
		safe_free(binding_name); // Loop hence freeing it here
		/* End BZ 2924 */

	    }

	    /* new_val == NULL, user reset the server-map definition
	     * for this http origin. Do nothing */
	}
	/* Free as this could get allocated again in the loop */
	tstr_array_free(&name_parts);
	tstr_array_free(&uris_config);
	bn_binding_free(&binding);

    } /* for (i = 0; i < num_changes; i++) */

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    tstr_array_free(&uris_config);
    tstr_array_free(&name_parts);
    ts_free(&t_uri_prefix);
    ts_free(&server_map);
    safe_free(c_uri);
    safe_free(binding_name);
    bn_binding_free(&binding);
    ts_free(&dom_regex_value);
    ts_free(&msg);
    safe_free(smap_fmt_type_node);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_nvsd_uri_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0;

    /*
     * No checking to be done.
     */
bail:
    return(err);
}

int md_nvsd_uri_http_host_validate(md_commit *commit,
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
    tstring *t_host = NULL;
    int32 at_offset = 0;

    new_value = bn_attrib_array_get_quick(new_attribs, ba_value);
    if (new_attribs == NULL) {
	goto bail;
    }

    err = bn_attrib_get_tstr(new_value, NULL, bt_string, NULL, &t_host);
    bail_error(err);

    if (!ts_length(t_host)) {
	goto bail;
    }
    lc_log_basic(LOG_NOTICE, _("Hostname:%s\n"),ts_str(t_host));
    err = ts_find_str(t_host, 0, ":", &at_offset);
    if (err != 0) {
	err = md_commit_set_return_status_msg_fmt(
		commit, 1, "Error: Invalid HTTP OriginServer Hostname: \"%s\"", ts_str(t_host));
	bail_error(err);
    }
    else if (at_offset != -1){
	err = md_commit_set_return_status_msg_fmt(
		commit, 1, "Error: Invalid HTTP OriginServer Hostname: \"%s\"", ts_str(t_host));
	bail_error(err);
    }
bail:
    ts_free(&t_host);
    return err;
}

int md_nvsd_uri_nfs_host_validate(md_commit *commit,
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
    tstring *t_host = NULL;
    int32 at_offset = 0;

    new_value = bn_attrib_array_get_quick(new_attribs, ba_value);
    if (new_attribs == NULL) {
	goto bail;
    }

    err = bn_attrib_get_tstr(new_value, NULL, bt_string, NULL, &t_host);
    bail_error(err);

    if (!ts_length(t_host)) {
	goto bail;
    }

    err = ts_find_str(t_host, 0, ":", &at_offset);
    if (err != 0) {
	err = md_commit_set_return_status_msg_fmt(
		commit, 1, "error validating NFS Origin name: \"%s\"", ts_str(t_host));
	bail_error(err);
    }
    else if ( at_offset < 0 ) {
	err = md_commit_set_return_status_msg_fmt(
		commit, 1,
		"bad NFS export specification: \"%s\" (Should be <hostname>:<export_path>)",
		ts_str(t_host));
	bail_error(err);
    } else if (at_offset >= (ts_length(t_host)-1)) {
	err = md_commit_set_return_status_msg_fmt(
		commit, 1,
		"bad NFS export specification: \"%s\" (Should be <hostname>:<export_path>)",
		ts_str(t_host));
	bail_error(err);
    }

bail:
    ts_free(&t_host);
    return err;
}

static int
md_nvsd_uri_upgrade_downgrade(const mdb_db *old_db,
                              mdb_db *inout_new_db,
                              uint32 from_module_version,
                              uint32 to_module_version,
                              void *arg)
{
    int err = 0;
    tbool found = false;
    uint32 i = 0, j = 0;
    tstr_array *ts_nfs_hosts = NULL;
    tstring *t_nfs_host = NULL;
    tstring *t_nfs_host_val = NULL;
    uint32 num_uri_bindings = 0;
    tstr_array *ts_domain_names = NULL;
    tstring *t_domain_name = NULL;
    tstring *t_domain_name_val= NULL;
    uint32 num_domain_bindings = 0;
    char *c_domain_host = NULL;


    err = md_generic_upgrade_downgrade(old_db, inout_new_db, from_module_version,
	    to_module_version, arg);
    bail_error(err);

    if (from_module_version == 1 && to_module_version == 2) {

	// Validate NFS host names when upgrades are done.
	err = mdb_get_matching_tstr_array(NULL, inout_new_db,
		"/nkn/nvsd/uri/config/*/uri-prefix/*/origin-server/nfs/host",
		0,
		&ts_nfs_hosts);
	bail_error(err);


	num_uri_bindings = tstr_array_length_quick(ts_nfs_hosts);
	for(i = 0; i < num_uri_bindings; ++i) {

	    t_nfs_host = tstr_array_get_quick(ts_nfs_hosts, i);

	    // Get the node value and check if it is a NFS host.
	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    ts_str(t_nfs_host), 0, &found, &t_nfs_host_val);
	    bail_error(err);

	    if (!found || (ts_length(t_nfs_host_val) == 0)) {
		// This URI-prefix doesnt have NFS configured for it
		// Skip it !!!
		continue;
	    }
	    else {
		int32 ret_offset = 0;
		//lc_log_debug(LOG_NOTICE, "NFS host : %s", ts_str(t_nfs_host_val));
		// Find first occurence of '\' - this
		// was the way the olde NFS exports were given.
		err = ts_find_char(t_nfs_host_val, 0, '/', &ret_offset);
		bail_error(err);

		//lc_log_debug(LOG_NOTICE, "Found slash at offset : %d", ret_offset);

		if (ret_offset > 0) {
		    err = ts_insert_char(t_nfs_host_val, ret_offset, ':');
		    bail_error(err);
		    //lc_log_debug(LOG_NOTICE, "Modified NFS host : %s", ts_str(t_nfs_host_val));

		    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0,
			    bt_string,
			    ts_str(t_nfs_host_val),
			    "%s",
			    ts_str(t_nfs_host));
		    bail_error(err);

		}
		else {
		    continue;
		}

	    }
	}
    } // End if (from_module_version==1 && to_module_version==2)

    if (from_module_version == 3 && to_module_version == 4) {
	// .../domain was changed to .../domain/host and .../domain/regex
	// in version 4.
	//
	// Change .../domain to .../domain/host
	//
	// This call gets a list of binding names in a tstring array.
	err = mdb_get_matching_tstr_array(NULL, inout_new_db,
		"/nkn/nvsd/uri/config/*/uri-prefix/*/domain",
		0,
		&ts_domain_names);
	bail_error(err);

	// Walk through the array (per NS per URI) and move the node values.
	num_domain_bindings = tstr_array_length_quick(ts_domain_names);
	for(i = 0; i < num_domain_bindings; ++i) {

	    // Get the domain name binding.
	    t_domain_name = tstr_array_get_quick(ts_domain_names, i);

	    // Get the node value.
	    err = mdb_get_node_value_tstr(NULL, inout_new_db,
		    ts_str(t_domain_name), 0, &found,
		    &t_domain_name_val);
	    bail_error(err);

	    // Now that we have the value delete the old
	    // node /nkn/nvsd/uri/config/*/uri-prefix/*/domain
	    c_domain_host = smprintf("%s", ts_str(t_domain_name));
	    bail_null(c_domain_host);

	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		    0, bt_string, "", "%s", c_domain_host);
	    bail_error(err);
	    safe_free(c_domain_host);

	    // Now create the new node to add
	    c_domain_host = smprintf("%s/host", ts_str(t_domain_name));
	    bail_null(c_domain_host);

	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		    0, bt_string,
		    ts_str(t_domain_name_val),
		    "%s",
		    c_domain_host);
	    bail_error(err);

	    safe_free(c_domain_host);
	}
    }
bail:
    safe_free(c_domain_host);
    tstr_array_free(&ts_nfs_hosts);
    tstr_array_free(&ts_domain_names);
    return err;
}

static int
md_nvsd_uri_check_proxy_mode_allowed(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg,
		tbool *is_allowed)
{
    int err = 0;
    const char *t_namespace = NULL;
    char *bn_name = NULL;
    tstring *proxy_mode = NULL;

    bail_require(arg);
    bail_require(is_allowed);

    t_namespace = (const char *) (arg);

    *is_allowed = false;

    bn_name = smprintf("/nkn/nvsd/namespace/%s/proxy-mode", t_namespace);
    bail_null(bn_name);

    /* Get the proxy mode string */
    err = mdb_get_node_value_tstr(commit, new_db,
	    bn_name, 0, NULL, &proxy_mode);
    bail_error(err);

    if (ts_equal_str(proxy_mode, "mid-tier", false) | ts_equal_str(proxy_mode, "virtual", false)) {
	/* Never allow for proxy-mode = mid-tier OR virtual */
	*is_allowed = false;
    }
    else {
	*is_allowed = true;
    }

bail:
    safe_free(bn_name);
    ts_free(&proxy_mode);
    return err;
}


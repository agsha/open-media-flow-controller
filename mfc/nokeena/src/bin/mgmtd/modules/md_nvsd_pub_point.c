/*
 *
 * Filename:    md_nvsd_pub_point.c
 * Date:        2009-08-16
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009 Nokeena Networks, Inc.
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
#include <ctype.h>
/*----------------------------------------------------------------------------
 * PREPROCESSOR MACROS/CONSTANTS
 *---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
md_nvsd_pub_point_init(
	const lc_dso_info *info,
	void *data);

static int
md_nvsd_pp_validate_name(const char *name,
			tstring **ret_msg);


static int
md_pub_point_commit_check(md_commit *commit,
			const mdb_db *old_db,
			const mdb_db *new_db,
			mdb_db_change_array *change_list,
			void *arg);

static int
md_nvsd_pp_validate_pub_server_uri(const tstring *name,
			tstring **ret_msg);

static int
md_nvsd_pp_validate_sdp_uri(const tstring *name,
			tstring **ret_msg);

static int
md_nvsd_pp_get_sdp(md_commit *commit, const mdb_db *db,
		const char *action_name, bn_binding_array *params,
		void *cb_arg);


static md_upgrade_rule_array *md_nvsd_pub_point_ug_rules = NULL;






int
md_nvsd_pub_point_init(
	const lc_dso_info *info,
	void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;


    err = mdm_add_module(
	    "nvsd-pub-point", 			// module name
	    1,					// module version
	    "/nkn/nvsd/pub-point",			// parent node
	    NULL,					// config root
	    0,					// module flags
	    NULL,					// side_effects_func
	    NULL, 					// side_effects_arg
	    md_pub_point_commit_check,		// check_func
	    NULL,					// check_arg
	    NULL,					// apply_func
	    NULL,					// apply_arg
	    0,					// commit_order
	    0,					// apply_order
	    md_generic_upgrade_downgrade,		// updown_func
	    &md_nvsd_pub_point_ug_rules,		// updown_arg
	    NULL,					// initial_func
	    NULL,					// initial_arg
	    NULL,					// mon_get_func
	    NULL,					// mon_get_arg
	    NULL,					// mon_iterate_func
	    NULL,					// mon_iterate_arg
	    &module);				// ret_module
    bail_error(err);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = mdm_module_set_gettext_domain(module, GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 			"/nkn/nvsd/pub-point/config/*";
    node->mrn_value_type =			bt_string;
    node->mrn_limit_str_max_chars = 	16;
    node->mrn_node_reg_flags = 		mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask = 			mcf_cap_node_restricted;
    node->mrn_limit_str_no_charlist = 	"/\\*:|`\"?";
    node->mrn_description =			"Publish point name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 			"/nkn/nvsd/pub-point/config/*/active";
    node->mrn_value_type = 			bt_bool;
    node->mrn_initial_value =		"false";
    node->mrn_node_reg_flags = 		mrf_flags_reg_config_literal;
    node->mrn_cap_mask = 			mcf_cap_node_restricted;
    node->mrn_description = 		"Pub-Point active or not?";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 			"/nkn/nvsd/pub-point/config/*/sdp/static";
    node->mrn_value_type = 			bt_uri;
    node->mrn_initial_value =		"";
    node->mrn_node_reg_flags = 		mrf_flags_reg_config_literal;
    node->mrn_cap_mask = 			mcf_cap_node_restricted;
    node->mrn_description = 		"Link (scp:// or http://) to a static SDP file";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 			"/nkn/nvsd/pub-point/config/*/event/duration";
    node->mrn_value_type = 			bt_string;
    node->mrn_initial_value =		"";
    node->mrn_node_reg_flags = 		mrf_flags_reg_config_literal;
    node->mrn_cap_mask = 			mcf_cap_node_restricted;
    node->mrn_description = 		"Duration of the event in HH:MM:SS";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 			"/nkn/nvsd/pub-point/config/*/pub-server/uri";
    node->mrn_value_type = 			bt_uri;
    node->mrn_initial_value =		"";
    node->mrn_node_reg_flags = 		mrf_flags_reg_config_literal;
    node->mrn_cap_mask = 			mcf_cap_node_restricted;
    node->mrn_description = 		"URI of the publishing server";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 			"/nkn/nvsd/pub-point/config/*/pub-server/mode";
    node->mrn_value_type = 			bt_string;
    node->mrn_initial_value =		"immediate";
    node->mrn_node_reg_flags = 		mrf_flags_reg_config_literal;
    node->mrn_cap_mask = 			mcf_cap_node_restricted;
    node->mrn_description = 		"Mode of publish - immediate, on-demand, start-time";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 			"/nkn/nvsd/pub-point/config/*/pub-server/start-time";
    node->mrn_value_type = 			bt_string;
    node->mrn_initial_value =		"";
    node->mrn_node_reg_flags = 		mrf_flags_reg_config_literal;
    node->mrn_cap_mask = 			mcf_cap_node_restricted;
    node->mrn_description = 		"Stream start time";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 			"/nkn/nvsd/pub-point/config/*/pub-server/end-time";
    node->mrn_value_type = 			bt_string;
    node->mrn_initial_value =		"";
    node->mrn_node_reg_flags = 		mrf_flags_reg_config_literal;
    node->mrn_cap_mask = 			mcf_cap_node_restricted;
    node->mrn_description = 		"Stream End time";
    err = mdm_add_node(module, &node);
    bail_error(err);
    /*------------------------------------------------------------------------
     * 		UPGRADE PROCESSING RULES
     *----------------------------------------------------------------------*/
    err = md_upgrade_rule_array_new(&md_nvsd_pub_point_ug_rules);
    bail_error(err);

    ra = md_nvsd_pub_point_ug_rules;

bail:
    return err;
}


static int
md_pub_point_commit_check(md_commit *commit,
			const mdb_db *old_db,
			const mdb_db *new_db,
			mdb_db_change_array *change_list,
			void *arg)
{
    int err = 0;
    tstring *ret_msg = NULL;
    int i = 0;
    int num_changes = 0;
    const mdb_db_change *change = NULL;
    const char *t_pp_name = NULL;
    const bn_attrib *new_val = NULL;
    tstring *t_duration = NULL;
    tstring *t_sdp = NULL;
    tstring *t_pub_server = NULL;
    tstring *t_mode = NULL;
    lt_time_sec ret_ts = 0;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; ++i)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null(change);

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "pub-point", "config", "*"))
		&& ((mdct_add == change->mdc_change_type)))
	{
	    t_pp_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);

	    err = md_nvsd_pp_validate_name(t_pp_name, &ret_msg);
	    if (err) {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			"error: %s", ts_str(ret_msg));
		bail_error(err);
		goto bail;
	    }
	}

	/* Check if EVENT-DURATION is correct format */
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "pub-point", "config", "*", "event", "duration"))
		&& ((mdct_modify == change->mdc_change_type)))
	{
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL, bt_string, NULL, &t_duration);
		bail_error(err);

		if (ts_length(t_duration) != 0) {
		    err = lt_str_to_daytime_sec(ts_str(t_duration), &ret_ts);

		    if ( err || (ret_ts < 0)) {
			err = md_commit_set_return_status_msg_fmt(commit, 1, "error: Bad duration");
			bail_error(err);
			goto bail;
		    }
		}
	    }
	}

	/* Check if START-TIME is correct format */
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "pub-point", "config", "*", "pub-server", "start-time"))
		&& ((mdct_modify == change->mdc_change_type)))
	{
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL, bt_string, NULL, &t_duration);
		bail_error(err);

		if (ts_length(t_duration) != 0) {
		    err = lt_str_to_daytime_sec(ts_str(t_duration), &ret_ts);
		    lc_log_basic(LOG_NOTICE, "ret = %d, %s", ret_ts, ts_str(t_duration));

		    if ( err || (ret_ts < 0)) {
			err = md_commit_set_return_status_msg_fmt(commit, 1, "error: Bad Start-time");
			bail_error(err);
			goto bail;
		    }
		}
	    }
	}

	/* Check if END-TIME is correct format */
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "pub-point", "config", "*", "pub-server", "end-time"))
		&& ((mdct_modify == change->mdc_change_type)))
	{
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL, bt_string, NULL, &t_duration);
		bail_error(err);

		if (ts_length(t_duration) != 0) {
		    err = lt_str_to_daytime_sec(ts_str(t_duration), &ret_ts);
		    lc_log_basic(LOG_NOTICE, "ret = %d, %s", ret_ts, ts_str(t_duration));

		    if ( err || (ret_ts < 0)) {
			err = md_commit_set_return_status_msg_fmt(commit, 1, "error: Bad Start-time");
			bail_error(err);
			goto bail;
		    }
		}
	    }
	}

	/* Check if SDP-STATIC is correct format */
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "pub-point", "config", "*", "sdp", "static"))
		&& ((mdct_modify == change->mdc_change_type)))
	{
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL, bt_uri, NULL, &t_sdp);
		bail_error(err);

	    }
	}
	/* Check if pub-server is correct format */
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, "nkn", "nvsd", "pub-point", "config", "*", "pub-server", "uri"))
		&& ((mdct_modify == change->mdc_change_type)))
	{
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL, bt_uri, NULL, &t_pub_server);
		bail_error(err);

		if (ts_length(t_pub_server) != 0) {
		    /* validate URI */
		    err = md_nvsd_pp_validate_pub_server_uri(t_pub_server, &ret_msg);
		    if (err) {
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				"error: %s", ts_str(ret_msg));
			bail_error(err);
			goto bail;
		    }
		}
	    }
	}
    }


    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    ts_free(&ret_msg);
    ts_free(&t_duration);
    ts_free(&t_sdp);
    ts_free(&t_pub_server);
    ts_free(&t_mode);
    return err;
}

static int
md_nvsd_pp_validate_name(const char *name,
			tstring **ret_msg)
{
    int err = 0;
    int i = 0;
    const char *p = "/\\:.- ";
    int j = 0;
    int k = 0;
    int l = strlen(p);

    if (name[0] == '_') {
	if (ret_msg) {
	    err = ts_new_sprintf(ret_msg,
		    "Bad pub-point name. "
		    "Name cannot start with a leading underscore ('_')");
	}
	err = lc_err_not_found;
	goto bail;
    }
    k = strlen(p);

    for (i = 0; i < k; i++) {
	for (j = 0; j < l; j++) {
	    if (p[j] == name[i]) {
		if (ret_msg)
		    err = ts_new_sprintf(ret_msg,
			    "Bad pub-point name. "
			    "Name cannot contain the characters '%s'", p);
		err = lc_err_not_found;
		goto bail;
	    }
	}
    }

bail:
    return err;
}

static int
md_nvsd_pp_validate_sdp_uri(const tstring *name,
			tstring **ret_msg)
{
    int err = 0;

    if ( !(ts_has_prefix_str(name, "rtsp://", true)))
    {

	err = ts_new_sprintf(ret_msg, "only rtsp:// protocol is supported");
	err = lc_err_not_found;
	goto bail;
    }

bail:
    return err;
}

static int
md_nvsd_pp_validate_pub_server_uri(const tstring *name,
			tstring **ret_msg)
{
    int err = 0, length = 0;
    unsigned int  i = 0;
    int32 colon_offset =0, slash_offset = 0, uri_offset = 0;
    tstring *absuri = NULL;
    tstring *port = NULL;
    const char *c_port = NULL;

    if (  !(ts_has_prefix_str(name, "rtsp://", true) ||
		ts_has_prefix_str(name, "http://", true))) {

	err = ts_new_sprintf(ret_msg, "only rtsp:// and http:// URLs are supported");
	err = lc_err_not_found;
	goto bail;
    }

    err = ts_find_str(name, 0, "://", &uri_offset);
    length = ts_length(name) - uri_offset + strlen("://");
    err = ts_substr (name, 7, length, &absuri);
    bail_null(absuri);

    err = ts_find_str(absuri, 0, ":", &colon_offset);
    err = ts_find_str(absuri, 0, "/", &slash_offset);
    if (colon_offset != -1) {
	if(slash_offset != -1) {
	    if(slash_offset < colon_offset){
		err = ts_new_sprintf(ret_msg,
			"Invalid Pub Server URL \n");
		err = lc_err_not_found;
		goto bail;

	    }
	    err = ts_substr (absuri,
		    colon_offset,
		    slash_offset - colon_offset, &port);
	    bail_null(port);
	}
	else {
	    err = ts_substr (absuri,
		    colon_offset,
		    length - colon_offset, &port);
	    bail_null( port);
	}

	err = ts_trim (port, ':', '/');
	bail_null(port);

	c_port = ts_str(port);

	for ( i = 0 ; i < strlen ( c_port) ; i++) {
	    if ( !(isdigit(c_port [i]) ) ) {
		err = ts_new_sprintf(ret_msg,
			"Invalid Pub Server Port Number \n");
		err = lc_err_not_found;
		goto bail;
	    }
	}	
    }
bail:
    ts_free(&absuri);
    ts_free(&port);
    return err;
}


static int
md_nvsd_pp_get_sdp(md_commit *commit, const mdb_db *db,
		const char *action_name, bn_binding_array *params,
		void *cb_arg)
{
    int err = 0;

bail:
    return err;
}

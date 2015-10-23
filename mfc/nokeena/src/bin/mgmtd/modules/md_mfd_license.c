/*
 * md_mfd_license.c: Enable licensing.
 *
 * 1: SSL - Enable SSL
 * 2: MFD (depreciated) - Enable "full" functionality.
 * 3: MFD_VP_TYPE_3 (depreciated) - Enbales 'Virtual Player Type 3', which is our
 *   internal name for the Yahoo virtual player.  As of 2013-June, no customers
 *   have ever used this and there are no plans for customers to use it in the future.
 *
 * Related code in:
 *     lib/nvsd/mgmt/nvsd_mgmt_main.c
 *     lib/nvsd/mgmt/nvsd_mgmt_system.c
 *     bin/nvsd/server.c
 *     bin/mgmtd/md_modules.inc.c
 */

#include "tpaths.h"
#include "common.h"
#include "dso.h"
#include "mdb_dbbe.h"
#include "md_mod_reg.h"
#include "md_utils.h"
#include "md_upgrade.h"
#include "md_graph_utils.h"
#include "nkn_mgmt_defs.h"
#include "file_utils.h"
#include "mdm_events.h"

int md_mfd_license_init(const lc_dso_info *info, void *data);

#define mfd_root           "/nkn/mf"
#define mfd_licensing_root "/nkn/mfd/licensing"
/*
 * This binding is set automatically by the licensing system when
 * licenses pertinent to mfd are installed.
 * The binding will have a boolean saying whether or not that particular
 * license is installed.
 */
static const char *nkn_mfd_lic_node ="/nkn/mfd/licensing/config/mfd_licensed";
static const char *nkn_mfd_vp_type03_lic_node ="/nkn/mfd/licensing/config/vp_type03_licensed";
static const char *nkn_mfd_fms_lic_node ="/nkn/mfd/licensing/config/fms_licensed";
static const char *nkn_mfd_ssl_lic_node ="/nkn/mfd/licensing/config/ssl_licensed";

static md_upgrade_rule_array *md_mfd_lic_ug_rules = NULL;

static int
md_mfd_license_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg);

static int
md_mfd_license_commit_apply(md_commit *commit, const mdb_db *old_db,
	const mdb_db *new_db, mdb_db_change_array *change_list, void *arg);

static int
md_ssl_handle_feature_state_change(md_commit *commit, const mdb_db *db,
	const char *event_name, const bn_binding_array *bindings,
	void *cb_reg_arg, void *cb_arg);

static int
md_ssl_update_cmds(tbool licensed);

// md_mfd_license_init() is called from GRAFT_POINT_2 in bin/mgmtd/md_modles.inc.c
int
md_mfd_license_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    err = mdm_add_module("mfd_license", 5,
                         mfd_licensing_root,
                         NULL, 0,
            		 md_mfd_license_commit_side_effects,   // side_effects_func
                         NULL, NULL, NULL,
			 md_mfd_license_commit_apply, NULL,	// commit apply func
                         0, 0,
                         md_generic_upgrade_downgrade,
                         &md_mfd_lic_ug_rules,                // updown_arg
			 NULL, NULL,
                         NULL, NULL, NULL, NULL,
                         &module);
    bail_error(err);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = mdm_module_set_gettext_domain(module, GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

#ifdef USE_MFD_LICENSE
    /*
     * Do we have an active MFD license?
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =           nkn_mfd_lic_node;
    node->mrn_value_type =     bt_bool;
    node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
    node->mrn_initial_value =  "false";
    node->mrn_cap_mask =       mcf_cap_node_basic;
    err = mdm_add_node(module, &node);
    bail_error(err);
#endif /* USE_MFD_LICENSE */

    /*
     * Do we have an active MFD_VP_TYPE_3 (Virtual-Player type 3) license?
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =           nkn_mfd_vp_type03_lic_node;
    node->mrn_value_type =     bt_bool;
    node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
    node->mrn_initial_value =  "false";
    node->mrn_cap_mask =       mcf_cap_node_basic;
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Do we have an active SSL license?
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =           nkn_mfd_ssl_lic_node;
    node->mrn_value_type =     bt_bool;
    node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
    node->mrn_initial_value =  "false";
    node->mrn_cap_mask =       mcf_cap_node_basic;
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Upgrade processing nodes
     */
    err = md_upgrade_rule_array_new(&md_mfd_lic_ug_rules);
    bail_error(err);
    ra = md_mfd_lic_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			= 	nkn_mfd_vp_type03_lic_node;
    rule->mur_new_value_type 		=  	bt_bool;
    rule->mur_new_value_str 		=   	"false";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			= 	nkn_mfd_fms_lic_node;
    rule->mur_new_value_type 		=  	bt_bool;
    rule->mur_new_value_str 		=   	"false";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type 		=    	MUTT_ADD;
    rule->mur_name 			= 	nkn_mfd_ssl_lic_node;
    rule->mur_new_value_type 		=  	bt_bool;
    rule->mur_new_value_str 		=   	"false";
    rule->mur_cond_name_does_not_exist 	= 	true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type 		=    	MUTT_DELETE;
    rule->mur_name 			= 	nkn_mfd_fms_lic_node;
    MD_ADD_RULE(ra);
    /* ........................................................................
     * Register the mapping between feature names and the nodes that
     * will reflect whether those licenses are installed.  The mgmtd
     * infrastructure will not allow these nodes to be artificially
     * changed, other than by supplying a valid license.
     */
#ifdef USE_MFD_LICENSE
    err = mdm_license_add_licensed_feature
        (module, "MFD", "Unrestricted Media Flow functionality", mfd_root, nkn_mfd_lic_node,
         true, NULL, false, NULL, false);
    bail_error(err);
#endif /* USE_MFD_LICENSE */

    err = mdm_license_add_licensed_feature
        (module, "MFD_VP_TYPE_3","Virtual Player Type 3", mfd_root, nkn_mfd_vp_type03_lic_node,
         true, NULL, false, NULL, false);
    bail_error(err);

    err = mdm_license_add_licensed_feature
        (module, "SSL", "SSL Delivery of MFC", mfd_root, nkn_mfd_ssl_lic_node,
         true, NULL, false, NULL, false);
    bail_error(err);

    /* We want to know when the SSL license changes active status,
     * so we can update the symlink
     */
    err = md_events_handle_int_interest_add("/license/events/feature_state_change",
	    md_ssl_handle_feature_state_change, NULL);
    bail_error(err);

#ifdef EXTRAS
    err = mdm_license_add_activation_tag(loti_echo_extras_num_packets,
                                         md_mfd_license_activation, NULL);
    bail_error(err);
#endif

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
#ifdef EXTRAS
// We do not need this now because there is no extra info in the License
#define md_mfd_active_node  "/license/feature/MFD/status/active"
static int
md_mfd_license_activation(md_commit *commit, const mdb_db *db,
                           const mdm_license_activation_data *lic_data,
                           void *data, tbool *ret_active)
{
    int err = 0;
    tbool found_it = false, mfd_active = false, found_state = false;
    uint32 min_service_level = 0, actual_service_level = 0;

    bail_null(lic_data);
    bail_require(lic_data->mld_option_tag_id == loti_mfd_extras_xyz);


    /*
     * Requirement #1: there must be an active MFD license.
     */
    err = mdb_get_node_value_tbool(commit, db, md_mfd_active_node, 0,
                                   &found_it, &mfd_active);
    bail_error(err);
    if (!found || !mfd_active) {
        active = false; /* No active MFD license */
        goto bail;
    }
    //TBD --- Check the extra info in the license...
    err = mdb_get_node_value_uint32(commit, db,
                                    md_mfd_xyz_summary_node, 0,
                                    &found, &value);
    bail_error(err);
    if (!found) {
        active = true; /* Active ECHO license with no restriction */
        goto bail;
    }
    //TBD...
    //TBD...
    //TBD...

 bail:
    return(err);
}
#endif /*EXTRAS*/

static int
md_mfd_license_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
	int err = 0;
	int idx, num_vps;
	int i = 0, num_changes = 0;
	tbool found = false;
	const mdb_db_change *change = NULL;
	char *ns_maxconnections = NULL;
	char *ns_maxbandwidth = NULL;
	tstr_array *t_vp_max_rate_list = NULL;

	num_changes = mdb_db_change_array_length_quick (change_list);
        lc_log_basic(LOG_ALERT, "In license commit %d", num_changes);
	for (i = 0; i < num_changes; i++)
	{
		change = mdb_db_change_array_get_quick (change_list, i);
		bail_null (change);

#ifdef USE_MFD_LICENSE
		/* Check if it is mfd license node*/
		if ((bn_binding_name_pattern_match (ts_str(change->mdc_name), nkn_mfd_lic_node))
			&& (mdct_modify == change->mdc_change_type))
		{
			tbool t_mfd_license = false;
			const char *t_max_connections = 0;
			const char *t_max_bandwidth = 0;

			/* First get the license node value */
			err = mdb_get_node_value_tbool(commit, inout_new_db,
							ts_str(change->mdc_name), 0, &found,
							&t_mfd_license);
			bail_error (err);

			/* Based on the license set the values for
			 * max connections and max bandwidth */
			if (t_mfd_license)
			{
				/* Change the defaults when license is good */
				t_max_connections = VALID_LICENSE_MAXCONN;
				t_max_bandwidth = VALID_LICENSE_MAXBW;
			}
			else
			{
				/* Change the defaults when license is bad or not present */
				t_max_connections = INVALID_LICENSE_MAXCONN;
				t_max_bandwidth = INVALID_LICENSE_MAXBW;
			}

			ns_maxconnections = smprintf("/nkn/nvsd/network/config/max_connections");
            		bail_null(ns_maxconnections);

			ns_maxbandwidth = smprintf("/nkn/nvsd/network/config/max_bandwidth");
			bail_null(ns_maxbandwidth);

			err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0,
					bt_uint32, t_max_connections, "%s", ns_maxconnections);
            		bail_error(err);

			err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0,
					bt_uint32, t_max_bandwidth, "%s", ns_maxbandwidth);
            		bail_error(err);

			/* Do the reset of max_session_rate of virtual player only if license expires */
			if (t_mfd_license)
				goto bail;

			/* Now iterate through the virtual-players and get the max_session_rate nodes */
			err = mdb_get_matching_tstr_array(commit, inout_new_db,
						"/nkn/nvsd/virtual_player/config/*",
						mdqf_sel_class_no_state, &t_vp_max_rate_list);

			num_vps = tstr_array_length_quick(t_vp_max_rate_list);
			for (idx = 0; idx < num_vps; idx++)
			{
				err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0,
						bt_uint32, "0", "%s/max_session_rate",
						tstr_array_get_str_quick(t_vp_max_rate_list, idx));
				bail_error(err);
			}
        	}
		else
#endif /* USE_MFD_LICENSE */
		/* check if it is MFD's SSL license node */
		if ((bn_binding_name_pattern_match(ts_str(change->mdc_name), nkn_mfd_ssl_lic_node))
		    && (mdct_modify == change->mdc_change_type))
		{
		    tbool t_mfd_ssl_license = false;

		    /* First get the license node value */
		    err = mdb_get_node_value_tbool(commit, inout_new_db,
			    ts_str(change->mdc_name), 0, &found,
			    &t_mfd_ssl_license);
		    bail_error(err);

		    if (t_mfd_ssl_license) {
			/* License is valid. Raise an event ?? */
		    } else {
			/* License if invalid */
		    }
		}
	}
#ifndef USE_MFD_LICENSE
	tbool t_mfd_license = false;
	const char *t_max_connections = 0;
	const char *t_max_bandwidth = 0;

	/* Change the defaults when license is good */
	t_max_connections = VALID_LICENSE_MAXCONN;
	t_max_bandwidth = VALID_LICENSE_MAXBW;

	ns_maxconnections = smprintf("/nkn/nvsd/network/config/max_connections");
        bail_null(ns_maxconnections);

	ns_maxbandwidth = smprintf("/nkn/nvsd/network/config/max_bandwidth");
	bail_null(ns_maxbandwidth);

	err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0,
			bt_uint32, t_max_connections, "%s", ns_maxconnections);
        bail_error(err);

	err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0,
			bt_uint32, t_max_bandwidth, "%s", ns_maxbandwidth);
        bail_error(err);

	/* Do the reset of max_session_rate of virtual player only if license expires */
	/* Now iterate through the virtual-players and get the max_session_rate nodes */
	err = mdb_get_matching_tstr_array(commit, inout_new_db,
				"/nkn/nvsd/virtual_player/config/*",
				mdqf_sel_class_no_state, &t_vp_max_rate_list);

	num_vps = tstr_array_length_quick(t_vp_max_rate_list);
	for (idx = 0; idx < num_vps; idx++)
	{
		err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0,
				bt_uint32, "0", "%s/max_session_rate",
				tstr_array_get_str_quick(t_vp_max_rate_list, idx));
		bail_error(err);
	}

#endif /* ! USE_MFD_LICENSE */

bail:
    tstr_array_free(&t_vp_max_rate_list);
    safe_free(ns_maxconnections);
    safe_free(ns_maxbandwidth);
    return err;
}



static int
md_mfd_license_commit_apply(md_commit *commit, const mdb_db *old_db,
	const mdb_db *new_db, mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    const mdb_db_change *change = NULL;
    int num_changes = 0, i = 0;
    tbool found = false, licensed = false;

    /* Refer to md_cli.c for this function */
    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (ts_equal_str(change->mdc_name, nkn_mfd_ssl_lic_node, false)) {
	    err = mdb_get_node_value_tbool(commit, new_db, nkn_mfd_ssl_lic_node, 0, &found, &licensed);
	    bail_error(err);

	    if (found) {
		err = md_ssl_update_cmds(licensed);
		bail_error(err);
	    }
	}
    }

bail:
    return (err);
}


static int
md_ssl_update_cmds(tbool licensed)
{
    int err = 0;
    const char *val = NULL;

    val = licensed ? "1" : "0";
    err = lf_symlink(val, file_ssl_cmds_license_path, true);
    bail_error_errno(err, "Symlinking %s", file_ssl_cmds_license_path);

bail:
    return (err);
}



static int
md_ssl_handle_feature_state_change(md_commit *commit, const mdb_db *db,
	const char *event_name, const bn_binding_array *bindings,
	void *cb_reg_arg, void *cb_arg)
{
    int err = 0;
    tstring *feature = NULL;
    tbool found = false, active = false;

    err = bn_binding_array_get_value_tstr_by_name(bindings, "feature_name",
	    NULL, &feature);
    bail_error(err);

    if (!ts_equal_str(feature, "SSL", false)) {
	goto bail;
    }

    err = bn_binding_array_get_value_tbool_by_name(bindings, "/new/status/active",
	    &found, &active);
    bail_error(err);

    if (!found) {
	/* TMaple: Not sure why this would happen...
	 * Refer : md_cli.c
	 */
	goto bail;
    }

    err = md_ssl_update_cmds(active);
    bail_error(err);

bail:
    ts_free(&feature);
    return (err);
}



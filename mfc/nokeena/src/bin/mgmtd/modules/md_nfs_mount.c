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
#include "nkn_mgmt_defs.h"

#define CLIST_MOUNT_NAME CLIST_ALPHANUM "_" "-"

extern unsigned int jnpr_log_level;
int init_done = 0;
int md_nfs_mount_init(const lc_dso_info *info, void *data);
int md_check_disk_owner(md_commit * commit,const mdb_db *db,
	tbool *owner_ok, const char  *owner, const char *diskname);

static md_upgrade_rule_array *md_nfs_mount_ug_rules = NULL;

/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int
md_nfs_mount_commit_check(
	md_commit *commit,
	const mdb_db *old_db,
	const mdb_db *new_db,
	mdb_db_change_array *change_list,
	void *arg);

static int
md_nfs_mount_commit_apply(
	md_commit *commit,
	const mdb_db *old_db,
	const mdb_db *new_db,
	mdb_db_change_array *change_list,
	void *arg);
static int
md_nfsmount_status_get(md_commit *commit, const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding, uint32 *ret_node_flags,
	void *arg);
static int
md_nfsmount_status_iterate(md_commit *commit, const mdb_db *db,
	const char *parent_node_name,
	const uint32_array *node_attrib_ids,
	int32 max_num_nodes, int32 start_child_num,
	const char *get_next_child,
	mdm_mon_iterate_names_cb_func iterate_cb,
	void *iterate_cb_arg, void *arg);
/*--------------------------------------------------------------------------*/
md_commit_forward_action_args md_gmgmthd_fwd_args =
{
    GCL_CLIENT_GMGMTHD, true, N_("Request failed; MFD subsystem 'gmgmthd' did not respond"),
    mfat_nonbarrier_async
#ifdef PROD_FEATURE_I18N_SUPPORT
	, GETTEXT_DOMAIN
#endif
};
/*----------------------------------------------------------------------------
 *MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
md_nfs_mount_init(const lc_dso_info *info, void *data)
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
	    "nfs_mount",                           // module_name
	    2,                                  // module_version
	    "/nkn/nfs_mount",                      // root_node_name
	    NULL,                               // root_config_name
	    0,				        // md_mod_flags
	    NULL,			        // side_effects_func
	    NULL,                               // side_effects_arg
	    md_nfs_mount_commit_check,             // check_func
	    NULL,                               // check_arg
	    md_nfs_mount_commit_apply,             // apply_func
	    NULL,                               // apply_arg
	    200,                                // commit_order
	    0,                                  // apply_order
	    md_generic_upgrade_downgrade,        // updown_func
	    &md_nfs_mount_ug_rules,              // updown_arg
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



    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nfs_mount/config/*";
    node->mrn_value_type        = bt_string;
    node->mrn_limit_str_max_chars 	= 	24;
    node->mrn_limit_str_charlist = CLIST_MOUNT_NAME;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_bad_value_msg =      N_("error:Either the Local Mount name has more than 24 characters or  Invalid characters in it");
    node->mrn_description       = "NFS mount name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nfs_mount/config/*/mountpath";
    node->mrn_value_type        = bt_string;
    node->mrn_limit_str_max_chars 	=  128;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "NFS Mount path";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nfs_mount/config/*/mountip";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "NFS Remote host ip";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* setting type as nfs, other type is disk */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nfs_mount/config/*/type";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "nfs";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "type of mount point";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * monitoring node for the mount status
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/nfs_mount/mount_status/*";
    node->mrn_value_type	= bt_string;
    node->mrn_node_reg_flags	= mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_mon_get_func	= md_nfsmount_status_get;
    node->mrn_mon_iterate_func	= md_nfsmount_status_iterate;
    node->mrn_description	= "list of mountpoints and their status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_nfs_mount_ug_rules);
    bail_error(err);
    ra = md_nfs_mount_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type  =   MUTT_ADD;
    rule->mur_name	    =   "/nkn/nfs_mount/config/*/type";
    rule->mur_new_value_type =	bt_string;
    rule->mur_new_value_str  =  "nfs";
    MD_ADD_RULE(ra);

bail:
    return err;
}

/*---------------------------------------------------------------------------*/
static int
md_nfs_mount_commit_check(
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
    const char *mount_name = NULL;
    const char *mount_opts = NULL;
    const char *mfp_nfs_mount_opts = "-osoft,timeo=20,retrans=1";
    tstring *mountpath = NULL, *type = NULL;
    int32 code = 0, nargs = 3;
    char nodename[256];

    tbool one_shot = false;

    err = md_commit_get_commit_mode(commit, &one_shot);
    bail_error(err);

    if (one_shot) {
	lc_log_basic(LOG_INFO, "Skipping NFS Mount"
		"one-shot configuration mode");
	goto bail;
    }


    /*Do the umount here to see if it is successful.
     * Then on success remove the node.
     */

    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);
	if ( change->mdc_change_type == mdct_delete ) {
	    if (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 4, 
			"nkn", "nfs_mount", "config", "*")){

		ts_free(&mountpath);
		err = mdb_get_node_value_tstr(commit, old_db,
			ts_str(change->mdc_name), 0, 0,
			&mountpath);
		bail_error_null(err, mountpath);

		snprintf(nodename, sizeof(nodename), "/nkn/nfs_mount/config/%s/type",
			ts_str(mountpath));

		ts_free(&type);
		err = mdb_get_node_value_tstr(commit, old_db,
			nodename, 0, 0,
			&type);

		/* skip following checks in case type is "disk" */
		if(type && (0 == safe_strcmp("disk", ts_str_maybe_empty(type)))) {
		    /* this is persistent store, no need to check for mountpath */
		    continue;
		} else {
		    /* either type is NULL or != "disk" */
		}
		err = lc_launch_quick_status(&code, NULL, false, 2,
			"/opt/nkn/bin/mgmt_nfs_umount.sh",
			ts_str(mountpath));
		snprintf(nodename, sizeof(nodename), "/nkn/nfs_mount/mount_status/%s", ts_str(mountpath));
		ts_free(&mountpath);

		err = mdb_get_node_value_tstr(commit, old_db,
			nodename, 0, 0,
			&mountpath);
		/* If the umount was unsuccessful and the actual mount point
		 * was valid then dont remove the config nodes.
		 * Else if the umount was successful or the actual mount point
		 * has a mount error then remove the config nodes.
		 */
		if(code && ts_equal_str(mountpath, "Success", false)) {
		    err = md_commit_set_return_status_msg_fmt (commit, 1, _(" %s umount failed.\n"), ts_str(change->mdc_name));
		    bail_error(err);
		}
	    }
	}
	if ( change->mdc_change_type == mdct_add ) {
	    if (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 4, 
			"nkn", "nfs_mount", "config", "*")){
		tstring *remotehost = NULL;
		tstring *remotepath = NULL;
		err = mdb_get_node_value_tstr(commit, new_db,
			ts_str(change->mdc_name), 0, 0,
			&mountpath);
		bail_null(mountpath);
		snprintf(nodename, sizeof(nodename), "/nkn/nfs_mount/config/%s/mountpath", ts_str(mountpath));
		err = mdb_get_node_value_tstr(commit, new_db,
			nodename, 0, 0,
			&remotepath);
		if(!remotepath) {
		    ts_free(&remotepath);
		    goto bail;
		}
		snprintf(nodename, sizeof(nodename), "/nkn/nfs_mount/config/%s/type",
			ts_str(mountpath));

		ts_free(&type);
		err = mdb_get_node_value_tstr(commit, new_db,
			nodename, 0, 0,
			&type);

		/* skip following checks in case type is "disk" */
		if(type && (0 == safe_strcmp("disk", ts_str_maybe_empty(type)))) {
		    /* this is persistent store, no need to check for mountpath */
		    if (init_done) {
			/* not checking as at start-up dc_1,dc_2 will not be
			 * available (external monitoring nodes)
			 */
			tbool owner_ok = false;
			err = md_check_disk_owner(commit, new_db, &owner_ok,
				"PS", ts_str(mountpath));
			bail_error(err);
			if (owner_ok == false) {
			    err = md_commit_set_return_status_msg_fmt (commit, EINVAL,
				    " %s not a persistent-store disk\n", ts_str(mountpath));
			    bail_error(err);
			    goto bail;
			}
		    }
		    continue;
		} else if (type && (0 == safe_strcmp("mfp_nfs", ts_str_maybe_empty(type)))) {
		    nargs = 4;
		    mount_opts = mfp_nfs_mount_opts;
		} else {
		    /* either type is NULL or != "disk" */
		}

		snprintf(nodename, sizeof(nodename), "/nkn/nfs_mount/config/%s/mountip", ts_str(mountpath));
		err = mdb_get_node_value_tstr(commit, new_db,
			nodename, 0, 0,
			&remotehost);
		if(!remotehost) {
		    ts_free(&remotehost);
		    goto bail;
		}

		snprintf(nodename, sizeof(nodename), "%s:%s", ts_str(remotehost), ts_str(remotepath));

		err = lc_launch_quick_status(&code, NULL, false, nargs,
			"/opt/nkn/bin/mgmt_nfs_mount.sh",
			nodename,
  		        ts_str(mountpath), mount_opts);
		/* BUGFIX:7927:Suppressing the error .If the error code is sent, mgmtd exists check for
		 * other modules.
		 */
		if(code) {
		    err = md_commit_set_return_status_msg_fmt (commit, 0, _(" %s mount failed.\n"), ts_str(mountpath));
		    bail_error(err);
		}
		ts_free(&remotehost);
		ts_free(&remotepath);
	    }
	}
    }

bail:
    ts_free(&mountpath);
    return err;

}


/*---------------------------------------------------------------------------*/
static int
md_nfs_mount_commit_apply(
	md_commit *commit,
	const mdb_db *old_db,
	const mdb_db *new_db,
	mdb_db_change_array *change_list,
	void *arg)
{
    int err = 0, i = 0, num_changes = 0, code = 0;
    const mdb_db_change *change = NULL;
    const char *mount_name = NULL;
    node_name_t nodename = {0};
    tstring *type = NULL, *mount_path = NULL;
    tbool one_shot = false;
    const mdb_db *db = NULL;

    err = md_commit_get_commit_mode(commit, &one_shot);
    bail_error(err);

    if (one_shot) {
	lc_log_basic(LOG_INFO, "Skipping Mount one-shot configuration mode");
	goto bail;
    }

    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);
	if (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 5, 
		    "nkn", "nfs_mount", "config", "*", "type")) {
	    mount_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);

	    if (change->mdc_change_type == mdct_add) {
		db = new_db;
	    } else if (change->mdc_change_type == mdct_delete) {
		db = old_db;
	    } else {
		/* type can't be modified */
		err = 1;
		goto bail;
	    }

	    ts_free(&type);
	    err = mdb_get_node_value_tstr(commit, db,
		    ts_str(change->mdc_name), 0, 0, &type);
	    bail_error(err);

	    if(type && (0 == strcmp("disk", ts_str_maybe_empty(type)))) {
		snprintf(nodename, sizeof(nodename), "/nkn/nfs_mount/config/%s/mountpath", mount_name);
		err = mdb_get_node_value_tstr(commit, db,
			nodename, 0, 0, &mount_path);
		bail_error(err);

		if ((mount_path == NULL) || (ts_length(mount_path) <= 0)) {
		    /* this is error, bail-out */
		    err = 1;
		    goto bail;
		}
		lc_log_debug(jnpr_log_level, "path- %s, %d",ts_str(mount_path), change->mdc_change_type );
		if (change->mdc_change_type == mdct_add) {
		    /* add the directory /nkn/pers/dc_X/MFP/ingest */
		    err = lc_launch_quick_status(&code, NULL, false, 3,
			    "/bin/mkdir", "-p", ts_str(mount_path));
		    bail_error(err);
		} else if (change->mdc_change_type == mdct_delete) {
		    /* delete directory /nkn/pers/dc_X/MFP/ingest  */
		    err = lc_launch_quick_status(&code, NULL, false, 3,
			    "/bin/rm", "-fr", ts_str(mount_path));
		    bail_error(err);
		} else {
		    /* we must not hit this issue */
		}
		if (code) {
		    lc_log_basic(LOG_NOTICE, "command failed (%d)", code);
		}
	    }
	}
    }
    init_done = 1;

bail:
    ts_free(&type);
    return err;
}
/* ------------------------------------------------------------------------- */
    static int
md_nfsmount_status_iterate(md_commit *commit, const mdb_db *db,
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

    err = lf_dir_list_names_matching("/nkn/nfs",
	    lf_dir_matching_nodots_names_func, NULL, &names);
    bail_error_null(err, names);

    num_names = tstr_array_length_quick(names);
    for (i = 0; i < num_names; ++i) {
	name = tstr_array_get_str_quick(names, i);
	bail_null(name);
	err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
	bail_error(err);
    }

bail:
    tstr_array_free(&names);
    ts_free(&t_log_format);
    return(err);
}

    static int
md_nfsmount_status_get(md_commit *commit, const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding, uint32 *ret_node_flags,
	void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *filename = NULL;
    int32 code = 0;
    node_name_t node = {0};
    tstring *type = NULL;


    /* XXX/EMT: should validate filename */

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error(err);

    /* Verify this is "/nkn/mfplog/logfiles/ *" */
    num_parts = tstr_array_length_quick(parts);
    bail_require(num_parts == 4);

    filename = tstr_array_get_str_quick(parts, 3);
    bail_null(filename);

    snprintf(node, sizeof(node), "/nkn/nfs_mount/config/%s/type",filename);
    err = mdb_get_node_value_tstr(commit, db, node, 0, 0, &type);
    bail_error_null(err,type);

    if (type && (0==strcmp("disk", ts_str(type)))) {
	code = 0;
    } else {
	err = lc_launch_quick_status(&code, NULL, false, 2,
		"/opt/nkn/bin/mgmt_nfs_mountstat.sh",
		filename);
	bail_error(err);
    }
    if(code) {
	err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string, 0,
		"Error in mount");
    }else {
	err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string, 0,
		"Success");
    }
    bail_error(err);

bail:
    tstr_array_free(&parts);
    return(err);
}

int
md_check_disk_owner(md_commit * commit,const mdb_db *db, tbool *owner_ok,
	const char  *owner, const char *diskname)
{
    int err = 0;
    node_name_t node_name = {0};
    tstring *disk_id = NULL, *owner_value = NULL;

    bail_require(diskname);
    bail_require(owner);
    bail_require(owner_ok);

    *owner_ok = false;

    /* get disk_id */
    snprintf(node_name, sizeof(node_name),
	    "/nkn/nvsd/diskcache/monitor/diskname/%s/disk_id", diskname);

    err = mdb_get_node_value_tstr(commit, db, node_name, 0, 0, &disk_id);
    bail_error_null(err, disk_id);

    /* get owner */
    snprintf(node_name, sizeof(node_name),
	    "/nkn/nvsd/diskcache/config/disk_id/%s/owner", ts_str(disk_id));

    err = mdb_get_node_value_tstr(commit, db, node_name, 0, 0, &owner_value);
    bail_error_null(err,owner_value);

    /* compare owner */
    if (0 == strcmp(owner, ts_str(owner_value))) {
	*owner_ok = true;
    } else {
	*owner_ok = false;
    }
bail:
    return err;
}

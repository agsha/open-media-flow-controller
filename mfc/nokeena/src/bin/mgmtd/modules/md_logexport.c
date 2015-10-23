/* File name:    md_logexport.c
 * Date:        2012-03-15
 * Author:      Madhumitha Baskaran
 *
 *
 */

/*----------------------------------------------------------------------------
 * md_logexport.c: adds the config and action nodes to aid logexport and set
 * the cron job to do the purging at regular interval
 *---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/

#include "dso.h"
#include "md_utils.h"
#include "tpaths.h"
#include "file_utils.h"

#define PROG_LOGEXPORT_PATH "/usr/bin/python /opt/nkn/bin/logCleanup.py"
#define NODE_PREFIX "/nkn/logexport/config/purge/criteria"

#define NODE_LEN 100
#define SIZE_OF_COMMAND 512

static const char *line_prefix = "";
static const char file_logexport_path[] = LOG_EXPORT_HOME;

static int md_logexport_write_conf(md_commit *commit, const mdb_db *db);

static int md_logexport_add_initial(md_commit *commit, mdb_db *db, void *arg);

int md_logexport_init(const lc_dso_info *info, void *data);
static int
md_logexport_commit_apply(md_commit *commit,
	       const mdb_db *old_db,
	       const mdb_db *new_db,
	       mdb_db_change_array *change_list,
	       void *arg);


static int
md_logexport_purge_force(md_commit *commit, const mdb_db *db,
	const char *action_name,
	bn_binding_array *params, void *cb_arg);

//intialize with respective default values
static const bn_str_value md_logexport_initial_values[] = {
    {NODE_PREFIX"/interval", bt_uint32, "0"},
    {NODE_PREFIX"/threshold_size_thou_pct",bt_uint32, "85000"}
};


static int
md_logexport_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings
	(commit, db, md_logexport_initial_values,
	 sizeof(md_logexport_initial_values) / sizeof(bn_str_value));
    bail_error(err);
    // Significance unknown-Is this really reqd
    err = md_mfg_copy_nodes
	(commit, db, 2,
	 NULL, "/nkn/logexport/config/purge/criteria/interval",
	 NULL, "/nkn/logexport/config/purge/criteria/threshold_size_thou_pct");
    bail_error(err);

bail:
    return(err);
}

int
md_logexport_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;

    err = mdm_add_module(
	    "logexport",                        // module_name
	    1,                                  // module_version
	    "/nkn/logexport",			// root_node_name
	    NULL,		                // root_config_name
	    modrf_namespace_unrestricted,       // md_mod_flags
	    NULL,				// side_effects_func
	    NULL,                               // side_effects_arg
	    NULL,				// check_func
	    NULL,                               // check_arg
	    md_logexport_commit_apply,		// apply_func
	    NULL,                               // apply_arg
	    0,                                  // commit_order
	    0,                                  // apply_order
	    NULL,			        // updown_func
	    NULL,		                // updown_arg
	    md_logexport_add_initial,           // initial_func
	    NULL,                               // initial_arg
	    NULL,                               // mon_get_func
	    NULL,                               // mon_get_arg
	    NULL,                               // mon_iterate_func
	    NULL,                               // mon_iterate_arg
	    &module);                           // ret_module
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =
	"/nkn/logexport/config/purge/criteria/threshold_size_thou_pct";
    node->mrn_value_type	 = bt_uint32;
    node->mrn_initial_value_int  = 85000;    /* 85% of /var disk space */
    node->mrn_limit_num_min_int  = 1;        /* 0.001% of /var space  */
    node->mrn_limit_num_max_int  = 100000;   /* 100% of /var space    */
    node->mrn_bad_value_msg	 = N_("Threshold must be between 0.001% and "
	    "100% of disk space");
    node->mrn_node_reg_flags	 = mrf_flags_reg_config_literal;
    node->mrn_cap_mask		 = mcf_cap_node_basic;
    node->mrn_audit_value_suffix = N_("/1000 percent");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name	= "/nkn/logexport/config/purge/criteria/interval";
    node->mrn_value_type	= bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 24;
    node->mrn_bad_value_msg   = N_("Interval must be between 0 and 24 hrs");
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_description       = "Frequency to purge the logs <in hours>";
    node->mrn_audit_descr       = N_("Maximum time interval before "
	    "purging logs 0-disable frequency based purging");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name                    = "/nkn/logexport/actions/force_purge";
    node->mrn_node_reg_flags          = mrf_flags_reg_action;
    node->mrn_cap_mask		      = mcf_cap_action_privileged;
    node->mrn_action_async_start_func = md_logexport_purge_force;
    node->mrn_action_audit_descr      = N_("force purging of files");
    err = mdm_add_node(module, &node);
    bail_error(err);

bail:
    return(err);
}

static int md_logexport_install_cron_job(void)
{
    int err = 0;
    const char *minute = NULL, *hour = NULL, *day_of_month = NULL;
    const char *month = NULL, *day_of_week = NULL, *user = NULL;
    tstring *filename_real = NULL;
    char *filename_temp = NULL;
    int fd_temp = -1;
    char command[512] = { 0 };
    char crontab[512] = { 0 };

    minute = "*/1";
    hour = "*";
    day_of_month = "*";
    month = "*";
    day_of_week = "*";
    user = "root";		/* Run as root */

    snprintf(command, SIZE_OF_COMMAND, "%s", PROG_LOGEXPORT_PATH );
    bail_null(command);

    /*
     * Tell it not to try to send mail with the output of the script.
     */
    snprintf(crontab, SIZE_OF_COMMAND, "MAILTO=\"\"\n"
	    "%s %s %s %s %s %s %s\n",
	    minute, hour, day_of_month, month, day_of_week,
	    user, command);
    bail_null(crontab);

    /* ------------------------------------------------------------------------
     * Get the temporary output file set up
     */
    err = lf_path_from_va_str(&filename_real, true, 2,
	    md_gen_output_path, "logCleanup");
    bail_error(err);

    err = lf_temp_file_get_fd(ts_str(filename_real),
	    &filename_temp, &fd_temp);
    bail_error(err);

    err = md_conf_file_write_header(fd_temp, "md_logexport", "##");
    bail_error(err);
    /* ------------------------------------------------------------------------
     * Write the file
     */

    err = lf_write_bytes(fd_temp, crontab, strlen(crontab), NULL);
    bail_error(err);

    /* ------------------------------------------------------------------------
     * And move the file into place
     */

    err =
	lf_temp_file_activate_fd(ts_str(filename_real), filename_temp,
		&fd_temp, md_gen_conf_file_owner,
		md_gen_conf_file_group,
		md_gen_conf_file_mode,
		lao_backup_orig | lao_sync_disable);
    bail_error(err);

bail:
    safe_close(&fd_temp);
    ts_free(&filename_real);
    safe_free(filename_temp);
    return (err);
}

static int
md_logexport_purge_force(md_commit *commit, const mdb_db *db,
	const char *action_name,
	bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tstring *ret_output = NULL;
    int32 status = 0;

    unsigned int fd_count = 0;
    err = lf_dir_count_names_matching(LOG_EXPORT_HOME,
                                      lf_dir_matching_nodots_names_func,
                                      NULL, &fd_count);

    bail_error(err);
    if(fd_count == 0){
	lc_log_basic(LOG_NOTICE, "No Files to purge in the logexport folder");
	goto bail;
    }
    err = lc_launch_quick_status(&status, &ret_output, false, 1,
	    "/opt/nkn/bin/logPurge.py");
    if (status){
        lc_log_basic(LOG_NOTICE, "failed to force purge the files status:%d",
	   	     WEXITSTATUS(status));
                }

bail:

    return(err);
}

static int
md_logexport_commit_apply(md_commit *commit,
	       const mdb_db *old_db,
	       const mdb_db *new_db,
	       mdb_db_change_array *change_list,
	       void *arg)
{
    int err = 0;

    err = md_logexport_install_cron_job();
    bail_error(err);

bail:
    return err;
}

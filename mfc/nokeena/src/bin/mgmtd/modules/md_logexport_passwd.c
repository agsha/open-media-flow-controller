/*
 *
 * Filename:    md_logexport_passwd.c
 * Date:        2012-03-15
 * Author:      Madhumitha Baskaran
 *
 *
 */

/*----------------------------------------------------------------------------
 * md_logexport_passwd.c: adds the default user LogTransferUser for exporting
 * the logs and also when the  user is configured to LogTransfer Group the
 * home dir and shell of the user is set to predefined values
 *---------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/

#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_mod_reg.h"
#include "md_utils.h"
#include "tpaths.h"

/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
#define SIZE_OF_NODE_NAME		1024
#define SIZE_OF_HOME_DIR		100
#define SIZE_OF_SHELL			100
#define LOGTRANSFER_GROUP_ID		2001
#define DEFAULT_SHELL			"/usr/bin/rssh"


const bn_str_value md_logexport_passwd_initial_values[] = {
    {"/auth/passwd/user/LogTransferUser", bt_string, "LogTransferUser"},
    {"/auth/passwd/user/LogTransferUser/enable", bt_bool, "true"},
    {"/auth/passwd/user/LogTransferUser/password", bt_string, "*"},
    {"/auth/passwd/user/LogTransferUser/uid", bt_uint32, "2020"},
    {"/auth/passwd/user/LogTransferUser/gid", bt_uint32, "2001"},
    {"/auth/passwd/user/LogTransferUser/gecos", bt_string, "LogTransferUser"},
    {"/auth/passwd/user/LogTransferUser/home_dir", bt_string, LOG_EXPORT_HOME},
    {"/auth/passwd/user/LogTransferUser/shell", bt_string, DEFAULT_SHELL},
    {"/auth/passwd/user/LogTransferUser/newly_created", bt_bool, "false"},
};



static int
md_logexport_passwd_add_initial(md_commit *commit, mdb_db *db, void *arg);

static int
md_logexport_passwd_commit_side_effects(md_commit *commit,
					const mdb_db *old_db, mdb_db *inout_new_db,
					mdb_db_change_array *change_list, void *arg);
static int
md_logexport_passwd_commit_check(md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list, void *arg);

static tbool
md_passwd_is_system_acct(const char *username);

int md_logexport_passwd_init(const lc_dso_info *info, void *data);
/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
static tbool
md_passwd_is_system_acct(const char *username)
{
    /*
     * These are accounts that may be modified, but not deleted.
     * XXX/EMT: these should be in whatever new system we have
     * (config nodes?) that control visibility and modifiability.
     *
     * NOTE: the 'operator' account is not considered a system
     * account.  Partly this is to avoid potential issues with
     * switching to other configurations which do not have that
     * account.  These issues might be possible to avoid, but we have
     * no strong reason to mandate that the account be present anyway.
     */
    return(!strcmp(username, "root") ||
	    !strcmp(username, "admin") ||
	    !strcmp(username, "monitor") ||
	    !strcmp(username, "cmcrendv"));
}

static int
md_logexport_passwd_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings
	(commit, db, md_logexport_passwd_initial_values,
	 sizeof(md_logexport_passwd_initial_values) / sizeof(bn_str_value));
    bail_error(err);

bail:
    return(err);
}


int md_logexport_passwd_init(const lc_dso_info *info, void *data)
{
    int err = 0;

    md_module *module = NULL;
    md_reg_node *node = NULL;

    err = mdm_add_module("logexport_passwd",
			 1,
		         "/auth/customer/logexport", NULL,
		         modrf_namespace_unrestricted|modrf_all_changes,
		         md_logexport_passwd_commit_side_effects, NULL,
			 md_logexport_passwd_commit_check, NULL,
			 NULL, NULL, 1, 1,
		  	 /* commit order higher than 0,0 may be 1,1 as
			 for 0,0 TM will be called first and lay the platform */
		         NULL, NULL, md_logexport_passwd_add_initial, NULL,
	                 NULL, NULL, NULL, NULL, &module);


    bail_error(err);

bail:
    return err;
}

    static int
md_logexport_passwd_commit_side_effects(md_commit *commit,const mdb_db *old_db,
	mdb_db *inout_new_db,
	mdb_db_change_array *change_list,
	void *arg)
{
    int err = 0;
    int ret_offset = 0;
    uint32 num_changes = 0, i = 0, num_name_parts = 0;
    uint32 gid = 0;
    tbool found = false;
    const char *username = NULL;
    const mdb_db_change *change = NULL;
    char node_name[SIZE_OF_NODE_NAME] = { 0 }  ;
    char home_dir[SIZE_OF_HOME_DIR] = { 0 };
    char shell[SIZE_OF_SHELL] = { 0 };
    tbool newly_created = false;
    bn_binding *binding = NULL;


    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; ++i) {
	change  = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	/* Since we are concerned about only the users get added under
	 * the GID 2001 we concentrate only on that node
	 *
	 * Consider and handle below given two cases
	 * 1)The user would have been already created
	 * with default group -Modify,
	 * 2)While creating the user itself
	 * the group specifed as LogTransfer group -Add
	 */
	if (((change->mdc_change_type == mdct_modify)||
	    (change->mdc_change_type == mdct_add )) &&
	    (ts_has_suffix_str(change->mdc_name,"/gid", false))){


	    /* Get the group id and check ,as we need to set the home
	     * directory and shell
	     * only for the users joining LogTransfer Group
	     */
	    err = mdb_get_node_value_uint32(commit, inout_new_db,
		    ts_str(change->mdc_name), 0,
		    &found, &gid);
	    bail_error(err);
	    bail_require(found);

	    username = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(username);

	    snprintf(node_name, SIZE_OF_NODE_NAME,
		    "/auth/passwd/user/%s/newly_created", username);
	    bail_null(node_name);
	    err = mdb_get_node_value_tbool(commit, inout_new_db,
		    node_name, 0,
		    &found, &newly_created);
	    bail_error(err);
	    bail_require_msg(found, "Couldn't find expected node: %s",
		    node_name);

	    if (md_passwd_is_resv_acct(username) ||
		    md_passwd_is_system_acct(username)) {
		goto bail;
	    }
            if (gid == LOGTRANSFER_GROUP_ID){

		//Set the Home Dir and Shell
		err = mdb_set_node_str(commit, inout_new_db,
			bsso_modify, 0, bt_string,
			LOG_EXPORT_HOME,
			"/auth/passwd/user/%s/home_dir",
			username);
		bail_error(err);

		err = mdb_set_node_str(commit, inout_new_db,
			bsso_modify, 0, bt_string,
			DEFAULT_SHELL,
			"/auth/passwd/user/%s/shell",
			username);
		bail_error(err);

	    }
	    else {
		/*
		 * Set the home directory.
		 */
		snprintf(home_dir, SIZE_OF_HOME_DIR, "%s/%s", HOME_ROOT_DIR,
			 username);
		bail_null(home_dir);
		err = mdb_set_node_str
		    (NULL, inout_new_db, bsso_modify, 0, bt_string, home_dir,
		     "/auth/passwd/user/%s/home_dir", username);
		bail_error(err);

		/* TODO:
		 * Currently set the shell to cli mode for the user who move
		 * out of logtransfer group,but this should be modified to the
		 * respective shell
		 */
		snprintf(shell, SIZE_OF_SHELL, "%s", "/opt/tms/bin/cli");
                bail_null(shell);
                err = mdb_set_node_str
                    (NULL, inout_new_db, bsso_modify, 0, bt_string, shell,
                     "/auth/passwd/user/%s/shell", username);
                bail_error(err);

	    }
	}

    }

bail:
    bn_binding_free(&binding);
    return(err);

}

/* ------------------------------------------------------------------------- */
static int
md_logexport_passwd_commit_check(md_commit *commit,
	const mdb_db *old_db,
	const mdb_db *new_db,
	mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    uint32 num_changes = 0, i = 0;
    mdb_db_change *change = NULL;
    const char *username = NULL;
    bn_binding_array *bindings = NULL;
    char node_name[SIZE_OF_NODE_NAME] = { 0 };


    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; ++i) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	// This default user of LogTransfer group should not be allowed to delete
	if (ts_has_prefix_str(change->mdc_name,
		    "/auth/passwd/user/",
		    false)) {
	    username = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(username);
	    if(strncmp(username,"LogTransferUser",15) == 0){
		if( change->mdc_change_type == mdct_delete ) {
		    err = md_commit_set_return_status_msg_fmt
			(commit, 1, _("The %s account may not be deleted."),
			 username);
		    bail_error(err);
		    goto bail;
		}

		/* The default user of LogTransfer group should not be allowed to
		 * change the group other than LogTransfer group
		 */
		if ((change->mdc_change_type == mdct_modify) &&
		     (ts_has_suffix_str(change->mdc_name,"/gid", false))){
		    err = md_commit_set_return_status_msg_fmt
			(commit, 1, _("The capability of account %s may not "
				      "be modified."), username);
		    bail_error(err);
		    goto bail;
		}

	    }
	}
    }
bail:
	bn_binding_array_free(&bindings);
	return(err);
}

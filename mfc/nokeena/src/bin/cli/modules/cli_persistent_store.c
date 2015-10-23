#include "common.h"
#include "climod_common.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

typedef int (*cli_ps_get_disks_t)( tstr_array **, tbool);

int cli_persistent_store_init(const lc_dso_info *info, const cli_module_context *context);
int cli_ps_disk_owner( void *data, const cli_command *cmd, const tstr_array *cmd_line,
        const tstr_array *params);
int show_cli_ps_disk_list( void *data, const cli_command *cmd, const tstr_array *cmd_line,
        const tstr_array *params);

cli_help_callback cli_ps_disk_help = NULL;
cli_completion_callback cli_ps_disk_completion = NULL;
cli_ps_get_disks_t cli_ps_disk_list = NULL;
int cli_ps_is_appl_attached(const char *disk_name, int *attached);


int 
cli_persistent_store_init(const lc_dso_info *info, const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;
    void *cli_nvsd_ps_ptr = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = cli_get_symbol("cli_nvsd_diskcache_cmds", "cli_diskcache_help",
	    &cli_nvsd_ps_ptr);
    cli_ps_disk_help = cli_nvsd_ps_ptr;
    bail_error_null(err, cli_ps_disk_help);

    cli_nvsd_ps_ptr= NULL;
    err = cli_get_symbol("cli_nvsd_diskcache_cmds", "cli_diskcache_completion",
	    &cli_nvsd_ps_ptr);
    cli_ps_disk_completion = cli_nvsd_ps_ptr;
    bail_error_null(err, cli_ps_disk_completion);

    cli_nvsd_ps_ptr= NULL;
    err = cli_get_symbol("cli_nvsd_diskcache_cmds", "cli_diskcache_get_disks",
	    &cli_nvsd_ps_ptr);
    cli_ps_disk_list = cli_nvsd_ps_ptr;
    bail_error_null(err, cli_ps_disk_list);

    CLI_CMD_NEW;
    cmd->cc_words_str =	    "persistent-store";
    cmd->cc_help_descr =    "Configure persistent store disks";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =	    "persistent-store disk";
    cmd->cc_help_descr =    "Configure persistent store disks";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =	    "persistent-store disk add";
    cmd->cc_help_descr =    "Add a disk to persistent store disks";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"persistent-store disk add *";
    cmd->cc_help_descr =	"Add a disk to persistent store disks";
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_priv_curr;
    cmd->cc_help_callback =	cli_ps_disk_help;
    cmd->cc_comp_callback =	cli_ps_disk_completion;
    cmd->cc_exec_callback = cli_ps_disk_owner;
    cmd->cc_exec_data	    =	(void *)"PS";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =	    "persistent-store disk delete";
    cmd->cc_help_descr =    "Remove a disk from persistent store";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =	    "persistent-store disk delete *";
    cmd->cc_help_descr =    "Remove a disk from persistent store";
    cmd->cc_flags =	    ccf_terminal;
    cmd->cc_capab_required = ccp_set_priv_curr;
    cmd->cc_help_callback = cli_ps_disk_help;
    cmd->cc_comp_callback = cli_ps_disk_completion;
    cmd->cc_exec_callback = cli_ps_disk_owner;
    cmd->cc_exec_data	  = (void *)"DM2";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =	    "persistent-store disk list";
    cmd->cc_help_descr =    "show all persistent store disks";
    cmd->cc_flags =	    ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_callback = show_cli_ps_disk_list;
    CLI_CMD_REGISTER;

    const char *ps_ignore_bindings[] = {
	"/nkn/nvsd/diskcache/config/disk_id/*/owner",
    };

    err = cli_revmap_ignore_bindings_arr(ps_ignore_bindings);
    bail_error(err);
bail:
    return err;
}
int 
cli_ps_disk_owner(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    node_name_t node_name;
    tstring *t_diskid = NULL, * disk_name= NULL, *curr_disk_owner = NULL;
    bn_request *req = NULL, *action_req = NULL;
    const char *cp_diskname = NULL, *disk_owner = data;
    uint32_t ret_val = 0;

    bail_null(disk_owner);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(cmd);
    /* Get the disk name from the params */
    cp_diskname = tstr_array_get_str_quick(params, 0);
    bail_null(cp_diskname);

    /* Now get the disk_id */
    snprintf (node_name, sizeof(node_name), "/nkn/nvsd/diskcache/monitor/diskname/%s/disk_id", cp_diskname);

    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
	    &t_diskid, node_name, NULL);
    if (err || (t_diskid == NULL))
    {
	cli_printf_error("Failed to find  disk '%s'\n", cp_diskname);
	err = 0;
	goto bail;
    }

    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/diskcache/config/disk_id/%s", ts_str(t_diskid));
    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &disk_name, node_name, NULL);
    bail_error_null(err, disk_name);

    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/diskcache/config/disk_id/%s/owner", ts_str(t_diskid));
    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &curr_disk_owner, node_name, NULL);
    bail_error(err);

    if(curr_disk_owner && (0 == strcmp(ts_str(curr_disk_owner), disk_owner))) {
	/* current and new owner is same don't do any thing. */
	if (0 == strcmp(disk_owner, "PS")) {
	    cli_printf_error("disk '%s' is already persistent disk", cp_diskname);
	} else {
	    cli_printf_error("disk '%s' is not a persistent disk", cp_diskname);
	}
	goto bail;
    } else if (curr_disk_owner) {
	/* user is changing the owner, disallow if disk is attached 
	 * to a application */
	if (0 == strcmp("DM2", disk_owner)) {
	    int attached = 0;
	    /* user is deleting a PS disk */
	    err = cli_ps_is_appl_attached(cp_diskname, &attached);
	    if (attached) {
		cli_printf_error("disk '%s' is attached to application,"
			"cannot remove", cp_diskname);
		goto bail;
	    }
	}
    } else {
	bail_null(curr_disk_owner);

    }
    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_string, 0, disk_owner, NULL, 
	    "/nkn/nvsd/diskcache/config/disk_id/%s/owner", ts_str(t_diskid));
    bail_error(err);

    /* sending code pointter as NULL, as it will print message internally */
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &ret_val, NULL);
    bail_error(err);

    if (ret_val) {
	goto bail;
    }
    /* needed only if adding to PS */
    if (0 == strcmp(disk_owner, "PS")) {
	/* send formatting action to mgmtd */
	err = bn_action_request_msg_create(&action_req,
		"/nkn/nvsd/diskcache/actions/ps-format");
	bail_error_null(err, action_req);

	err = bn_action_request_msg_append_new_binding(action_req,
		0, "diskname", bt_string, cp_diskname, NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding(action_req, 
		0, "disk_id", bt_string, ts_str(t_diskid), NULL);
	bail_error(err);

	err = mdc_send_mgmt_msg(cli_mcc, action_req, false, NULL, &ret_val, NULL);
	bail_error(err);
	if(ret_val) {
	    goto bail;
	}
	bn_request_msg_free(&action_req);
    }

    /* send mount action to mgmtd */
    err = bn_action_request_msg_create(&action_req,
	    "/nkn/nvsd/diskcache/actions/mount-xfs");
    bail_error_null(err, action_req);

    err = bn_action_request_msg_append_new_binding(action_req,
	    0, "diskname", bt_string, cp_diskname, NULL);
    bail_error(err);

    err = bn_action_request_msg_append_new_binding(action_req, 
	    0, "disk_id", bt_string, ts_str(t_diskid), NULL);
    bail_error(err);

    if (0 == strcmp(disk_owner, "DM2")) {
	err = bn_action_request_msg_append_new_binding(action_req, 
		0, "mount", bt_bool, "false", NULL);
	bail_error(err);
    } else {
	err = bn_action_request_msg_append_new_binding(action_req, 
		0, "mount", bt_bool, "true", NULL);
	bail_error(err);
    }

    err = mdc_send_mgmt_msg(cli_mcc, action_req, false, NULL, &ret_val, NULL);
    bail_error(err);

    if (ret_val) {
	goto bail;
    }

bail:
    ts_free(&t_diskid);
    ts_free(&disk_name);
    bn_request_msg_free(&req);
    bn_request_msg_free(&action_req);
    return err;
}
int 
show_cli_ps_disk_list(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0, once = 0;;
    uint32 num_disks = 0, idx = 0;
    tstr_array *disk_name_list = NULL;
    tstring *t_diskid = NULL, *t_diskowner = NULL;
    node_name_t node_name = {0};

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    err = cli_ps_disk_list(&disk_name_list, false);
    bail_error(err);

    err = tstr_array_length(disk_name_list, &num_disks);
    bail_error(err);

    for (idx = 0; idx < num_disks; idx++) {
	char disk_name[8];
	snprintf(disk_name, sizeof(disk_name), "dc_%d", idx + 1);
	/* Now get the disk_id */
	snprintf (node_name, sizeof(node_name),
		"/nkn/nvsd/diskcache/monitor/diskname/%s/disk_id", disk_name);

	err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_diskid, node_name, NULL);
	bail_error(err);
	if (t_diskid == NULL || (0 == strcmp(ts_str(t_diskid), ""))) {
	    ts_free(&t_diskid);
	    continue;
	}
	snprintf (node_name, sizeof(node_name),"/nkn/nvsd/diskcache/config/disk_id/%s/owner", ts_str(t_diskid));

	err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_diskowner, node_name, NULL);
	bail_error(err);
	if (!ts_equal_str(t_diskowner,"PS", false)) {
	    ts_free(&t_diskowner);
	    ts_free(&t_diskid);
	    continue;
	}
	/* we are here, so we have persistent store disk */
	if (once == 0) {
	    cli_printf("Persistent Store Disks :\n");
	    once++;
	}
	cli_printf("    %s\n", disk_name);
	ts_free(&t_diskowner);
	ts_free(&t_diskid);
    }
    if (once == 0) {
	cli_printf("No Persistent-Store Disks found\n");
    }

bail:
    ts_free(&t_diskowner);
    ts_free(&t_diskid);
    return err;
}
int
cli_ps_is_appl_attached(const char *disk_name, int *attached)
{
    int err = 0;
    bn_binding *binding = NULL;
    node_name_t nfs_node = {0};

    bail_require(disk_name);
    bail_require(attached);

    *attached = 0;

    snprintf(nfs_node, sizeof(nfs_node), "/nkn/nfs_mount/config/%s", disk_name);
    /* currently only MFP is attached, so directly querying /nkn/nfs_mount */
    err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding, nfs_node, NULL);
    bail_error(err);
    if (binding) {
	*attached = 1;
    }

bail:
    bn_binding_free(&binding);
    return err;
}

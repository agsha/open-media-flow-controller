/*
 * Filename :   cli_nvsd_resource_mgr_cmds.c
 * Date     :   2008/12/15
 * Author   :   Thilak Raj S
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "libnkncli.h"
#include "proc_utils.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

/*----------------------------------------------------------------------------
 * LOCAL TYPES 
 *---------------------------------------------------------------------------*/
#define NKN_MAX_NS_RP_MAPPING	    64

enum {
    cc_rm_bind,
    cc_rm_unbind,
};

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_nvsd_rm_help(
		void *data, 
		cli_help_type help_type,
		const cli_command *cmd, 
		const tstr_array *cmd_line,
		const tstr_array *params, 
		const tstring *curr_word,
		void *context);

int
cli_nvsd_resource_mgr_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_nvsd_rp_delete_cmd_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

#if 0
int
cli_nvsd_rm_new_cmd_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
#endif

int
cli_nvsd_rm_show_list(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

int
cli_nvsd_rm_show_one(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

int
cli_nvsd_rm_client_bw(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int 
cli_nvsd_rm_remove_alarms(const char* rp_name);


cli_expansion_option cli_disk_type_options[] = {
	{"SATA",
	 N_("Cache type SATA"),
	 NULL},

	{"SAS",
	 N_("Cache type SAS"),
	 NULL},

	{"SSD",
	 N_("Cache type SSD"),
	 NULL},

	{NULL, NULL, NULL}
};

int 
cli_rm_disk_size(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

int
cli_nvsd_rm_bind_cmd_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int
cli_nvsd_rm_completion(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params,
		const tstring *curr_word,
		tstr_array *ret_completions);

static int
cli_nvsd_rm_revmap(
		void *data, const cli_command *cmd,
		const bn_binding_array *bindings, const char *name,
		const tstr_array *name_parts, const char *value,
		const bn_binding *binding,
		cli_revmap_reply *ret_reply);

static int
cli_nvsd_rm_bind_revmap(
		void *data, const cli_command *cmd,
		const bn_binding_array *bindings, const char *name,
		const tstr_array *name_parts, const char *value,
		const bn_binding *binding,
		cli_revmap_reply *ret_reply);

static int
cli_nvsd_rm_get_rp(
		tstr_array **ret_name);

static int
cli_nvsd_rm_get_ns(
		const char *ns_name,
		tstr_array **ret_name);

static int
cli_nvsd_rm_is_rp_free(const char *rp_name, tbool *can_delete);


/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
int
cli_nvsd_rm_show_one(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params) {

	int err = 0;
	const char *rp = NULL;
	bn_binding *binding = NULL;
	char *bn_name = NULL;
	tstr_array *ns_names = NULL;
	node_name_t node_name = {0};
	tstring *t_bandwidth = NULL;
	uint64_t conf_bw = 0;
	uint64_t used_bw = 0;
	float calc_bw = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	rp = tstr_array_get_str_quick(params, 0);
	bail_null(rp);

	bn_name = smprintf("/nkn/nvsd/resource_mgr/config/%s",rp);
	bail_null(bn_name);

	//Check if RP exists..not necessay i think
	err = mdc_get_binding(cli_mcc, NULL, NULL,
			false, &binding, bn_name, NULL);
	bail_error(err);

	if ((strcmp(GLOBAL_RSRC_POOL, rp) != 0) && NULL == binding ) {
		cli_printf_error(_("Invalid Resource-Pool : %s\n"), rp);
		goto bail;
	}

	/* get the configured bandwidth for the resource pool */
	node_name[0] = '\0';
	snprintf(node_name, sizeof(node_name), 
		"/nkn/nvsd/resource_mgr/monitor/counter/%s/max/max_bandwidth",
		rp);

	err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
		&t_bandwidth, node_name, NULL);
	bail_error(err);
	bail_null_quiet(t_bandwidth);

	/* convert bandwidth in Mbps */
	conf_bw = strtoul(ts_str(t_bandwidth), NULL ,10);
	/*
	 * divide first, multuply later, avoid overflow. Use 1000.0 to make
	 * sure nothing is lost in cakcukation
	 */
	calc_bw = (conf_bw  / (1000.0 * 1000.0)) * 8;

	err = cli_printf(_("Resource-Pool: %s\n"), rp);
	bail_error(err);

	err = cli_printf(_("Actual Max Resources\n"));

	err = cli_printf_query(_("   client sessions         : "
				"#/nkn/nvsd/resource_mgr/monitor/counter/%s/max/client_sessions#\n"), rp);
	bail_error(err);
	err = cli_printf(_("   client bandwidth (Mbps) : %0.3f \n"), calc_bw);
	bail_error(err);

	cli_printf("Configured Max Resources\n");

	if (strcmp(rp, GLOBAL_RSRC_POOL)) {
	    float config_bw = 0;
	    node_name[0] = '\0';

	    err = cli_printf_query(_("   client sessions         : "
			"#/nkn/nvsd/resource_mgr/config/%s/client_sessions#\n"), rp);
	    bail_error(err);

	    snprintf(node_name, sizeof(node_name), 
		    "/nkn/nvsd/resource_mgr/config/%s/max_bandwidth", rp);

	    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
		    &t_bandwidth, node_name, NULL);
	    bail_error(err);
	    bail_null_quiet(t_bandwidth);

	    /* convert bandwidth in Mbps */
	    conf_bw = strtoul(ts_str(t_bandwidth), NULL ,10);

	    config_bw = (conf_bw  / (1000.0 * 1000.0)) * 8;

	    err = cli_printf(_("   client bandwidth (Mbps) : %0.3f \n"), config_bw);
	    bail_error(err);
	} else {
	    /* this is global pool */
	    err = cli_printf_query(_("   client sessions         : "
			"#/nkn/nvsd/resource_mgr/monitor/counter/%s/max/client_sessions#\n"), rp);
	    bail_error(err);
	    err = cli_printf(_("   client bandwidth (Mbps) : %0.3f \n"), calc_bw);
	    bail_error(err);
	}

	/* reset string to nothing */
	node_name[0] = '\0';
	snprintf(node_name, sizeof(node_name), 
		"/nkn/nvsd/resource_mgr/monitor/counter/%s/used/max_bandwidth",
		rp);

	err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
		&t_bandwidth, node_name, NULL);
	bail_error(err);
	bail_null(t_bandwidth);

	/* convert bandwidth in Mbps */
	used_bw = strtoul(ts_str(t_bandwidth), NULL ,10);
	/*
	 * divide first, multuply later, avoid overflow. Use 1000.0 to make
	 * sure nothing is lost in cakcukation
	 */
	calc_bw = (used_bw / (1000.0 * 1000.0)) * 8;

	err = cli_printf(_("Used Resources\n"));

	err = cli_printf_query(_("   client sessions         : "
				"#/nkn/nvsd/resource_mgr/monitor/counter/%s/used/client_sessions#\n"), rp);
	bail_error(err);

	err = cli_printf(_("   client bandwidth (Mbps) : %0.3f "), calc_bw);
	bail_error(err);

#if 0
//Thilak - 5/11/2010
//Current desing doesn't allow to show the global pools bound namespace,
//as we dont have a config for global resource pool.

	cli_printf("Bound Namespaces:\n");

	err = cli_nvsd_rm_get_ns(rp, &ns_names);
	bail_error_null(err, ns_names);

	num_ns = tstr_array_length_quick(ns_names);


	for(i = 0; i < num_ns; i++) {
		namespace_name = tstr_array_get_str_quick(ns_names, i);
		err = cli_printf(_("%s\t"),namespace_name);
		bail_error(err);
	}

	if(!num_ns)
		err = cli_printf("No namespace bound");
		bail_error(err);

	err = cli_printf_query(_("   disk-reservation-tier1: "
				"#/nkn/nvsd/resource_mgr/monitor/counter/%s/used/reserved_diskspace_tier1#\n"), rp);
	bail_error(err);

	err = cli_printf_query(_("   disk-reservation-tier2: "
				"#/nkn/nvsd/resource_mgr/monitor/counter/%s/used/reserved_diskspace_tier2#\n"), rp);
	bail_error(err);

	err = cli_printf_query(_("   disk-reservation-tier3: "
				"#/nkn/nvsd/resource_mgr/monitor/counter/%s/used/reserved_diskspace_tier3#\n"), rp);
	bail_error(err);
#endif
bail:
	safe_free(bn_name);
	tstr_array_free(&ns_names);
	bn_binding_free(&binding);
	return err;
}


int
cli_nvsd_rm_show_list(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params) {
	int err = 0;
	tstr_array *names = NULL;
	tstr_array *ns_names = NULL;
	uint32 num_names = 0;
	uint32 idx = 0;
	uint32 i = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	err = cli_nvsd_rm_get_rp(&names);
	num_names = tstr_array_length_quick(names);

	err = cli_printf(_("Currently defined resource-pool  :\n"));
	bail_error(err);

	if(num_names > 0) {

		err = cli_printf_query(_("        %16s               Associated Namespace\n"),"Name");
		bail_error(err);

		err = cli_printf_query(_("===========================================================\n"));
		bail_error(err);

		for(idx = 0; idx < num_names; idx++) {

			const char *rp_name = NULL;
			const char *namespace_name = NULL;
			uint32 num_ns = 0;

			rp_name = tstr_array_get_str_quick(names, idx);
			bail_null(rp_name);

			err = cli_printf_query(_("        %16s"),rp_name);
			bail_error(err);

			err = cli_nvsd_rm_get_ns(tstr_array_get_str_quick(names, idx),&ns_names);
			bail_error_null(err, ns_names);

			num_ns = tstr_array_length_quick(ns_names);

			if(num_ns)
			    namespace_name = tstr_array_get_str_quick(ns_names, 0);

			err = cli_printf_query(_("        %16s\n"),namespace_name ? namespace_name : "N/A");
			bail_error(err);

			for(i = 1; i < num_ns; i++) {
				namespace_name = tstr_array_get_str_quick(ns_names, i);
				err = cli_printf_query(_("                %32s\n"),namespace_name ? namespace_name: "N/A");
				bail_error(err);
			}
			cli_printf("------------------------------------------------------------\n");
			tstr_array_free(&ns_names);
		}
	} else {
		err = cli_printf(_("No Resource-Pools defined\n"));
		bail_error(err);

	}
bail:
	tstr_array_free(&names);
	tstr_array_free(&ns_names);
	return err;
}


int
cli_nvsd_resource_mgr_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

#ifdef PROD_FEATURE_I18N_SUPPORT
	err = cli_set_gettext_domain(GETTEXT_DOMAIN);
	bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */


	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool";
	cmd->cc_help_descr =        N_("Configure a resource pool");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no resource-pool";
	cmd->cc_help_descr =        N_("Remove a resource-pool");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool *";
	cmd->cc_help_exp =          N_("<pool-name>");
	cmd->cc_help_exp_hint =     N_("Create a resource pool");
//	cmd->cc_flags =             ccf_terminal;
//	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_comp_type =         cct_matching_names;
	cmd->cc_comp_pattern =      "/nkn/nvsd/resource_mgr/config/*";
        cmd->cc_help_callback =     cli_nvsd_rm_help;
//	cmd->cc_exec_operation =    cxo_set;
//	cmd->cc_exec_name =         "/nkn/nvsd/resource_mgr/config/$1$";
//	cmd->cc_exec_value =        "$1$";
//	cmd->cc_exec_type =         bt_string;
//	cmd->cc_exec_callback =	    cli_nvsd_rm_new_cmd_hdlr;
	cmd->cc_revmap_names =      "/nkn/nvsd/resource_mgr/config/*";
	cmd->cc_revmap_callback =   cli_nvsd_rm_revmap;
	cmd->cc_revmap_type =       crt_manual;
	cmd->cc_revmap_order =	    cro_mgmt;
	cmd->cc_revmap_suborder =   -9;
	CLI_CMD_REGISTER;

	//Not needed for this release.
#if 0
	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * parent";
	cmd->cc_help_descr =        N_("Configure the number of sessions");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * parent *";
	cmd->cc_help_exp =          N_("<pool-name>");
	cmd->cc_help_exp_hint =     N_("Parent resource pool");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_comp_type =         cct_matching_names;
	cmd->cc_comp_pattern =      "/nkn/nvsd/resource_mgr/config/*";
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/nvsd/resource_mgr/config/$1$/parent";
	cmd->cc_exec_type =	    bt_string;
	cmd->cc_exec_value =        "$2$";
	//	cmd->cc_exec_callback =	    cli_nvsd_pe_cmd_hdlr;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;
#endif

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * client";
	cmd->cc_help_descr =        N_("Configure resources for clients");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * client connection";
	cmd->cc_help_descr =        N_("Configure the maximum number of concurrent connections");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * client connection *";
	cmd->cc_help_exp =          N_("<Number of connection>");
	cmd->cc_help_exp_hint =     N_("Specify the number of concurrent connections allowed");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/nvsd/resource_mgr/config/$1$/client_sessions";
	cmd->cc_exec_type =	    bt_uint64;
	cmd->cc_exec_value =        "$2$";
	cmd->cc_revmap_type =	    crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * origin-sessions";
	cmd->cc_help_descr =        N_("Configure the number of sessions");
	cmd->cc_flags = 	    ccf_unavailable;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * origin-sessions *";
	cmd->cc_help_exp =          N_("<Number of sessions>");
	cmd->cc_help_exp_hint =     N_("Specify the number of client sessions allowed");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/nvsd/resource_mgr/config/$1$/origin_sessions";
	cmd->cc_exec_type =	    bt_uint64;
	cmd->cc_exec_value =        "$2$";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * client bandwidth";
	cmd->cc_help_descr =        N_("Configure maximum bandwidth");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * client bandwidth *";
	cmd->cc_help_exp =          N_("<Mbps>");
	cmd->cc_help_exp_hint =     N_("Specify maximum bandwidth allowed");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =     cli_nvsd_rm_client_bw;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * media-cache";
	cmd->cc_flags =             ccf_hidden;
	cmd->cc_help_descr =        N_("Configure the media-cache");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * media-cache disk ";
	cmd->cc_help_descr =        N_("Configure the media-cache disk");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * media-cache disk tier";
	cmd->cc_help_descr =        N_("Configure the media-cache disk tier");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * media-cache disk tier *";
	cmd->cc_help_exp =          N_("<disk type>");
	cmd->cc_help_exp_hint =     N_("Set the type of the disk");
	cmd->cc_help_options = 	    cli_disk_type_options;
	cmd->cc_help_num_options =
		sizeof(cli_disk_type_options) / sizeof(cli_expansion_option);
	cmd->cc_help_comma_delim = 	true;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * media-cache disk tier * size";
	cmd->cc_help_descr =        N_("Configure the media-cache disk tier size");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * media-cache disk tier * size *";
	cmd->cc_help_exp =          N_("<Size in MB>");
	cmd->cc_help_exp_hint =     N_("Min 1000 and Max is tier size");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =     cli_rm_disk_size;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * bind";
	cmd->cc_help_descr =        N_("Configure association between namespace and resource pool");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * bind namespace";
	cmd->cc_help_descr =        N_("Configure association between namespace and resource pool");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "resource-pool * bind namespace *";
	cmd->cc_help_exp =          N_("<name>");
	cmd->cc_help_exp_hint =     "Specify namespace to be bound with the resource pool";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_magic =		    cc_rm_bind;
	cmd->cc_exec_callback =	    cli_nvsd_rm_bind_cmd_hdlr;
	cmd->cc_comp_type =         cct_matching_names;
	cmd->cc_comp_pattern =      "/nkn/nvsd/namespace/*";
	cmd->cc_revmap_type =       crt_manual;
	cmd->cc_revmap_suborder =   1;
	cmd->cc_revmap_names =	    "/nkn/nvsd/namespace/*/resource_pool";
	cmd->cc_revmap_callback =   cli_nvsd_rm_bind_revmap;
	cmd->cc_revmap_order =	    cro_resource_pool;
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "no resource-pool *";
	cmd->cc_help_exp =          N_("<pool-name>");
	cmd->cc_help_exp_hint =     N_("Remove a resource pool");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_help_callback =     cli_nvsd_rm_help;
	cmd->cc_comp_pattern =      "/nkn/nvsd/resource_mgr/config/*";
	cmd->cc_exec_callback =	    cli_nvsd_rp_delete_cmd_hdlr;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no resource-pool * bind";
	cmd->cc_help_descr =        N_("Remove association between namespace and resource pool");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no resource-pool * bind namespace";
	cmd->cc_help_descr =        N_("Remove association between namespace and resource pool");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no resource-pool * bind namespace *";
	cmd->cc_help_exp =          N_("<name>");
	cmd->cc_help_exp_hint =     "Specify namespace to be un-bound from the resource pool";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_magic =		    cc_rm_unbind;
	cmd->cc_exec_callback =	    cli_nvsd_rm_bind_cmd_hdlr;
	cmd->cc_comp_type =         cct_matching_names;
	cmd->cc_comp_pattern =      "/nkn/nvsd/namespace/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show resource-pool";
	cmd->cc_help_descr =        N_("Display resource pool settings");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show resource-pool list";
	cmd->cc_help_descr =	    "List the available resource pools";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_revmap_type =       crt_none;
	cmd->cc_exec_callback =	    cli_nvsd_rm_show_list;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show resource-pool *";
	cmd->cc_help_exp =          N_("<pool-name>");
	cmd->cc_help_exp_hint =     N_("List a resource pool setting");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_revmap_type =       crt_none;
        cmd->cc_help_callback =     cli_nvsd_rm_help;
	cmd->cc_exec_callback =	    cli_nvsd_rm_show_one;
	cmd->cc_comp_callback =	    cli_nvsd_rm_completion;
	CLI_CMD_REGISTER;

	err = cli_revmap_ignore_bindings("/nkn/nvsd/resource_mgr/config/*/namespace/*");
	bail_error(err);

	err = cli_revmap_ignore_bindings_va(6, "/nkn/nvsd/resource_mgr/config/*/origin_sessions",
			"/nkn/nvsd/resource_mgr/config/*/reserved_diskspace_tier1",
			"/nkn/nvsd/resource_mgr/config/*/reserved_diskspace_tier2",
			"/nkn/nvsd/resource_mgr/config/*/reserved_diskspace_tier3",
			"/nkn/nvsd/resource_mgr/global_pool/conf_max_bw",
			"/nkn/nvsd/resource_mgr/total/conf_max_bw"
			);

bail:
	return err;
}

static int
cli_nvsd_rm_bind_revmap(
		void *data, const cli_command *cmd,
		const bn_binding_array *bindings, const char *name,
		const tstr_array *name_parts, const char *value,
		const bn_binding *binding,
		cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	const char *ns_name = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	ns_name = tstr_array_get_str_quick(name_parts, 3);
	bail_null(ns_name);

	if(!strcmp(value, GLOBAL_RSRC_POOL))
	    goto bail;

	err = ts_new_sprintf(&rev_cmd, "resource-pool %s bind namespace %s",
				value, ns_name);
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);


bail:

	ts_free(&rev_cmd);
	return err;
}

static int
cli_nvsd_rm_revmap(
		void *data, const cli_command *cmd,
		const bn_binding_array *bindings, const char *name,
		const tstr_array *name_parts, const char *value,
		const bn_binding *binding,
		cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	const char *rsrc_pool = NULL;
	tstring *t_client_sess = NULL;
	tstr_array *rsrc_nodes = NULL;
	const char *node = NULL;
	const char *cli_node = NULL;
	uint32 i = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

#define CONSUME_REVMAP_NODES(c) \
	{\
		err = tstr_array_append_sprintf(ret_reply->crr_nodes, c, name);\
		bail_error(err);\
	}


	rsrc_pool = tstr_array_get_str_quick(name_parts, 4);

	if(rsrc_pool && strcmp(rsrc_pool, GLOBAL_RSRC_POOL)) {

		err = mdc_get_binding_children_tstr_array(cli_mcc, NULL, NULL, &rsrc_nodes, name, NULL);
		bail_error(err);

		for( i = 0; i< tstr_array_length_quick(rsrc_nodes) ; i++) {
			node = 	tstr_array_get_str_quick(rsrc_nodes, i);
			bail_null(node);

			if(strcmp(tstr_array_get_str_quick(rsrc_nodes, i),"parent")== 0)
				continue;

			err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
					NULL, &t_client_sess,"%s/%s", name, tstr_array_get_str_quick(rsrc_nodes, i));
			bail_error(err);


			if(!strcmp(node, "client_sessions")) {
				cli_node = "client connection";
			}else if(!strcmp(node, "origin_sessions")) {
				cli_node = "origin-sessions";
			}else if(!strcmp(node, "max_bandwidth")) {
				cli_node = "client bandwidth";
			}
			else if(!strcmp(node, "reserved_diskspace_tier1")) {
				cli_node = "media-cache disk tier SSD size";
			}else if(!strcmp(node, "reserved_diskspace_tier2")) {
				cli_node = "media-cache disk tier SAS size";
			}else if(!strcmp(node, "reserved_diskspace_tier3")) {
				cli_node = "media-cache disk tier SATA size";
			}

			if(!strcmp(node, "reserved_diskspace_tier1") ||
					!strcmp(node, "reserved_diskspace_tier2") ||
					!strcmp(node, "reserved_diskspace_tier3") ||
					!strcmp(node, "origin_sessions")) {
#if 0
				/* Consume the nodes here as they are not exposed for 2.1.0 */
				CONSUME_REVMAP_NODES("%s/reserved_diskspace_tier3");
				CONSUME_REVMAP_NODES("%s/reserved_diskspace_tier2");
				CONSUME_REVMAP_NODES("%s/reserved_diskspace_tier1");
				CONSUME_REVMAP_NODES("%s/origin_sessions");
#endif
				continue;

			}

			/* For bw Convert it into Mbps again and show it */
			if(strcmp(node, "max_bandwidth") == 0) {
			    uint64 bw = 0;
			    bw = strtoul(ts_str(t_client_sess), NULL ,10);
			    bw = (bw * 8 )/(1000 * 1000);
			    ts_free(&t_client_sess);
			    err = ts_new_sprintf(&t_client_sess, "%lu", bw);

			}
			err = ts_new_sprintf(&rev_cmd, "resource-pool %s %s %s",
					rsrc_pool, cli_node, ts_str(t_client_sess));

			err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
			ts_free(&rev_cmd);
			bail_error(err);

			err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/%s", name,tstr_array_get_str_quick(rsrc_nodes, i));
			bail_error(err);


		}

	} else {
		CONSUME_REVMAP_NODES("%s/reserved_diskspace_tier3");
		CONSUME_REVMAP_NODES("%s/reserved_diskspace_tier2");
		CONSUME_REVMAP_NODES("%s/reserved_diskspace_tier1");
		CONSUME_REVMAP_NODES("%s/client_sessions");
		CONSUME_REVMAP_NODES("%s/origin_sessions");
		CONSUME_REVMAP_NODES("%s/max_bandwidth");
	}

	CONSUME_REVMAP_NODES("%s/parent");
	CONSUME_REVMAP_NODES("%s");

bail:
	ts_free(&t_client_sess);
	tstr_array_free(&rsrc_nodes);
	return err;

}

int
cli_nvsd_rm_bind_cmd_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *ns_name = NULL;
	const char *rp_name = NULL;
	char *ns_node_name = NULL;
	char *ns_rp_node = NULL;
	char *ns_node = NULL;
	tbool ns_active = false;
	tbool has_no_activ_res;
	tstr_array *ns_array = NULL;
	bn_binding *binding_rp = NULL;
	bn_binding *binding_ns = NULL;
	tstring *t_rp_bind = NULL;
	tstring *t_old_rp = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);

	ns_name = tstr_array_get_str_quick(params, 1);
	rp_name = tstr_array_get_str_quick(params, 0);

	bail_null(ns_name);
	bail_null(rp_name);

	ns_node_name = smprintf("/nkn/nvsd/namespace/%s/status/active", ns_name);
	bail_null(ns_node_name);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &ns_active, ns_node_name, NULL);
	bail_error(err);

	//Check if resource pool is valid
	err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding_rp,
			NULL,"/nkn/nvsd/resource_mgr/config/%s",
			rp_name);
	bail_error(err);

	if(!binding_rp) {
		cli_printf_error("Invalid Resource Pool: %s", rp_name);
		goto bail;
	}

	//Check if namespace is valid
	err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding_ns,
			NULL,"/nkn/nvsd/namespace/%s",
			ns_name);
	bail_error(err);

	if(!binding_ns) {
		cli_printf_error("Invalid Namespace: %s", ns_name);
		goto bail;
	}

	switch(cmd->cc_magic) {
		node_name_t rp_nd = {0};
		case cc_rm_bind:

			//A resource pool can have only 64 namespaces associated with it.
			ns_node = smprintf("/nkn/nvsd/resource_mgr/config/%s/namespace", rp_name);
			bail_null(ns_node);

			err = mdc_get_binding_children_tstr_array(cli_mcc,
					NULL, NULL, &ns_array,
					ns_node, NULL);
			bail_error_null(err, ns_array);

			if(tstr_array_length_quick(ns_array) > NKN_MAX_NS_RP_MAPPING - 1) {
				cli_printf_error("Resource Pool %s has reached the max-limit of namespaces that can be associated", rp_name);
				goto bail;
			}

			ns_rp_node = smprintf("/nkn/nvsd/namespace/%s/resource_pool",ns_name);
			bail_null(ns_rp_node);

			/*Get the previous resource pool attached to namespace */
			err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
					&t_old_rp, ns_rp_node, NULL);
			bail_error(err);

			if(t_old_rp && !ts_equal_str(t_old_rp, GLOBAL_RSRC_POOL, false)) {

			    snprintf(rp_nd, sizeof(rp_nd), "/nkn/nvsd/resource_mgr/config/%s/namespace/%s",
				    ts_str(t_old_rp), ns_name);

			    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_delete, bt_string, "", rp_nd);
			    bail_error(err);
			}


			err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
					rp_name, ns_rp_node);
			bail_error(err);

			/*Get the namespace names to the list under the resource-pool as well
			* Would be useful for listing */
			safe_free(ns_rp_node);

			ns_rp_node = smprintf("/nkn/nvsd/resource_mgr/config/%s/namespace/%s",
					rp_name, ns_name);
			bail_null(ns_rp_node);

			err = mdc_set_binding(cli_mcc, NULL, NULL,bsso_create, bt_string,
					ns_name, ns_rp_node);
			bail_error(err);
			safe_free(ns_rp_node);

			break;
		case cc_rm_unbind:

			ns_rp_node = smprintf("/nkn/nvsd/namespace/%s/resource_pool",ns_name);
			bail_null(ns_rp_node);

			err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
					&t_rp_bind, ns_rp_node, NULL);
			bail_error(err);

			if(t_rp_bind && !ts_equal_str(t_rp_bind, rp_name, false)) {
				cli_printf_error("Namespace %s is not associated with resource-pool %s.", ns_name, rp_name);
				goto bail;
			}

			err = mdc_set_binding(cli_mcc, NULL, NULL,bsso_modify, bt_string, GLOBAL_RSRC_POOL, ns_rp_node);
			bail_error(err);

			/*Remove namespace names to the list under the resource-pool as well
			* Would be useful for listing */
			safe_free(ns_rp_node);

			ns_rp_node = smprintf("/nkn/nvsd/resource_mgr/config/%s/namespace/%s",
					rp_name, ns_name);
			bail_null(ns_rp_node);

			err = mdc_set_binding(cli_mcc, NULL, NULL,bsso_delete, bt_string, "", ns_rp_node);
			bail_error(err);

			break;
	}
bail:
	safe_free(ns_rp_node);
	safe_free(ns_node_name);
	ts_free(&t_rp_bind);
	ts_free(&t_old_rp);
	bn_binding_free(&binding_rp);
	bn_binding_free(&binding_ns);
	return err;
}

int
cli_nvsd_rp_delete_cmd_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	char *rp_node = NULL;
	const char *rp_name = NULL;
	tbool is_rp_free = false;
	node_name_t rp_ns_bind_nd = {0};
	tstr_array *ns_array = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	rp_name = tstr_array_get_str_quick(params, 0);
	bail_null(rp_name);

	snprintf(rp_ns_bind_nd, sizeof(rp_ns_bind_nd), "/nkn/nvsd/resource_mgr/config/%s/namespace",
			rp_name);
	err = mdc_get_binding_children_tstr_array(cli_mcc,
			NULL, NULL, &ns_array,
			rp_ns_bind_nd, NULL);
	bail_error_null(err, ns_array);

	if(tstr_array_length_quick(ns_array)) {
		cli_printf_error("Resource Pool %s has namespace's bound to it.Please unbind them.",
					    rp_name);
		goto bail;
	}

	rp_node = smprintf("/nkn/nvsd/resource_mgr/config/%s", rp_name);
	bail_null(rp_node);
	//Check if there are no used resources.

	err = cli_nvsd_rm_is_rp_free(rp_name, &is_rp_free);
	bail_error(err);

	if(is_rp_free) {
		cli_nvsd_rm_remove_alarms(rp_name);
		bail_error(err);

		err = mdc_delete_binding(cli_mcc, NULL, NULL, rp_node);
		bail_error(err);

	}
bail:
	safe_free(rp_node);
	tstr_array_free(&ns_array);
	return err;
}

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
static int
cli_nvsd_rm_get_rp(
		tstr_array **ret_name)
{
	int err = 0;
	tstr_array_options opts;
	tstr_array *names = NULL;
	tstr_array *names_config = NULL;

	bail_null(ret_name);

	err = tstr_array_options_get_defaults(&opts);
	bail_error(err);

	opts.tao_dup_policy = adp_delete_old;

	err = tstr_array_new(&names, &opts);
	bail_error(err);

	err = mdc_get_binding_children_tstr_array(cli_mcc,
			NULL, NULL, &names_config,
			"/nkn/nvsd/resource_mgr/config", NULL);
	bail_error_null(err, names_config);

	err = tstr_array_concat(names, &names_config);
	bail_error(err);

	*ret_name = names;

bail:
	tstr_array_free(&names_config);
	//Caller to free names
	return err;
}


static int
cli_nvsd_rm_completion(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params,
		const tstring *curr_word,
		tstr_array *ret_completions)
{
	int err = 0;
	tstr_array *names = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);
	UNUSED_ARGUMENT(curr_word);

	err = cli_nvsd_rm_get_rp(&names);
	bail_error(err);

	err = tstr_array_concat(ret_completions, &names);
	bail_error(err);

	err = tstr_array_append_str (ret_completions, GLOBAL_RSRC_POOL);
	bail_error(err);

bail:
	tstr_array_free(&names);
	return err;
}

static int
cli_nvsd_rm_get_ns(
		const char *ns_name,
		tstr_array **ret_name)
{
	int err = 0;
	tstr_array_options opts;
	tstr_array *names = NULL;
	tstr_array *names_config = NULL;
	char *node_name = NULL;

	bail_null(ret_name);

	err = tstr_array_options_get_defaults(&opts);
	bail_error(err);

	opts.tao_dup_policy = adp_delete_old;

	err = tstr_array_new(&names, &opts);
	bail_error(err);

	node_name = smprintf("/nkn/nvsd/resource_mgr/config/%s/namespace", ns_name);
	bail_null(node_name);

	err = mdc_get_binding_children_tstr_array(cli_mcc,
			NULL, NULL, &names_config,
			node_name, NULL);
	bail_error_null(err, names_config);

	err = tstr_array_concat(names, &names_config);
	bail_error(err);

	*ret_name = names;

bail:
	tstr_array_free(&names_config);
	//Not freeing names here
	return err;
}

static int 
cli_nvsd_rm_is_rp_free(const char *rp_name, tbool *can_delete) 
{
	int err = 0;
	char *bn_name = NULL;
	uint64 pool_count = 0;
	bn_binding *binding = NULL;

	err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding,
			NULL,"/nkn/nvsd/resource_mgr/monitor/counter/%s/used/%s",
			rp_name, "client_sessions");
	bail_error(err);
	if(binding) {
		err = bn_binding_get_uint64(binding,ba_value,NULL,&pool_count);
		bail_error(err);
		bn_binding_free(&binding);
	}
	if(pool_count > 0)  {

		cli_printf_error("Cannot delete Resource pool %s as it has used %s",
		    rp_name, "client_sessions");
		*can_delete = false;
		return err;
	}

	err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding,
			NULL,"/nkn/nvsd/resource_mgr/monitor/counter/%s/used/%s",
			rp_name, "max_bandwidth");
	bail_error(err);
	if(binding) {
		err = bn_binding_get_uint64(binding,ba_value,NULL,&pool_count);
		bail_error(err);
		bn_binding_free(&binding);
	}
	if(pool_count > 0)  {
		cli_printf_error("Cannot delete Resource pool %s as it has used %s",
		    rp_name, "max_bandwidth");
		*can_delete = false;
		return err;
	}
#if 0
	/* Thilak = 15/11/2010
	* Disk check is not needed for 2.1.0
	*/
	err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding,
			NULL,"/nkn/nvsd/resource_mgr/monitor/counter/%s/used/%s",
			    rp_name, "reserved_diskspace_tier1");
	bail_error(err);
	if(binding) {
		err = bn_binding_get_uint64(binding,ba_value,NULL,&pool_count);
		bail_error(err);
		bn_binding_free(&binding);
	}
	if(pool_count > 0)  {
		cli_printf("Cannot delete Resource pool %s as it has used %s",
		    rp_name, "reserved_diskspace_tier1");
		*can_delete = false;
		return err;
	}

	err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding,
			NULL,"/nkn/nvsd/resource_mgr/monitor/counter/%s/used/%s",
			    rp_name, "reserved_diskspace_tier2");
	bail_error(err);
	if(binding) {
		err = bn_binding_get_uint64(binding,ba_value,NULL,&pool_count);
		bail_error(err);
		bn_binding_free(&binding);
	}
	if(pool_count > 0)  {
		cli_printf("Cannot delete Resource pool %s as it has used %s",
		    rp_name,"reserved_diskspace_tier2" );
		*can_delete = false;
		return err;
	}

	err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding,
			NULL,"/nkn/nvsd/resource_mgr/monitor/counter/%s/used/%s",
			    rp_name, "reserved_diskspace_tier3");
	bail_error(err);
	if(binding) {
		err = bn_binding_get_uint64(binding,ba_value,NULL,&pool_count);
		bail_error(err);
		bn_binding_free(&binding);
	}
	if(pool_count > 0)  {
		cli_printf("Cannot delete Resource pool %s as it has used %s",
		    rp_name, "reserved_diskspace_tier3");
		*can_delete = false;
		return err;
	}
#endif

bail:
	safe_free(bn_name);
	bn_binding_free(&binding);
	*can_delete = true;
	return err;

}

int
cli_nvsd_rm_client_bw(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char* strbytes = NULL;
	char *bw_node = NULL;
	char *str_mbps = NULL;

	uint64_t n_bytes = 0;
	uint64_t n_mbps = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	strbytes = tstr_array_get_str_quick(params, 1);
	n_bytes = strtol(strbytes,NULL,10);

	n_mbps = (n_bytes * 1000 *1000) / 8;

	bw_node = smprintf("/nkn/nvsd/resource_mgr/config/%s/max_bandwidth", tstr_array_get_str_quick(params,0));
	bail_null(bw_node);

	str_mbps = smprintf("%lu",n_mbps);
	bail_null(str_mbps);


	err = mdc_set_binding(cli_mcc, NULL, NULL,bsso_modify, bt_uint64,
			str_mbps, bw_node);
	bail_error(err);

bail:
	safe_free(bw_node);
	safe_free(str_mbps);
	return err;
}

int
cli_rm_disk_size(
    	void *data,
    	const cli_command *cmd,
    	const tstr_array *cmd_line,
    	const tstr_array *params)
{
	int err = 0;
	char *ssd_binding = NULL;
	char *sas_binding = NULL;
	char *sata_binding = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	if (!strcmp(tstr_array_get_str_quick(params, 1), "SSD")) {
	    ssd_binding = smprintf ("/nkn/nvsd/resource_mgr/config/%s/reserved_diskspace_tier1",
					 tstr_array_get_str_quick(params, 0));
	    bail_null(ssd_binding);

	    err = mdc_create_binding(
            	cli_mcc, 
            	NULL,
            	NULL,
            	bt_uint64,
            	tstr_array_get_str_quick(params, 2),
            	ssd_binding);
    	    bail_error(err);
	}else if(!strcmp(tstr_array_get_str_quick(params, 1), "SAS")) {
	    sas_binding = smprintf ("/nkn/nvsd/resource_mgr/config/%s/reserved_diskspace_tier2", 
					tstr_array_get_str_quick(params, 0));
	    bail_null(sas_binding);

	    err = mdc_create_binding(
                cli_mcc,
                NULL,
                NULL,
                bt_uint64,
                tstr_array_get_str_quick(params, 2),
                sas_binding);
            bail_error(err);

	}else if(!strcmp(tstr_array_get_str_quick(params, 1), "SATA")){
	    sata_binding = smprintf ("/nkn/nvsd/resource_mgr/config/%s/reserved_diskspace_tier3", 
					tstr_array_get_str_quick(params, 0));
	    bail_null(sata_binding);

	    err = mdc_create_binding(
                cli_mcc,
                NULL,
                NULL,
		bt_uint64,
		tstr_array_get_str_quick(params, 2),
		sata_binding);
	    bail_error(err);
	}
bail:
	return err;
    safe_free(ssd_binding);
    safe_free(sas_binding);
    safe_free(sata_binding);
}

/*Thilak - 05/11/2010
Could be used for prefix mode later
*/
#if 0
int
cli_nvsd_rm_new_cmd_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	uint64_t sessions = 0;
	uint64_t max_bw = 0;
	bn_binding *binding = NULL;
	const char *rp_name = NULL;

	rp_name = tstr_array_get_str_quick(params, 0);
	bail_null(rp_name);

	err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding,
			NULL,"/nkn/nvsd/resource_mgr/config/%s/client_sessions",
			    rp_name);
	bail_error(err);
	if(binding) {
		err = bn_binding_get_uint64(binding,ba_value,NULL,&sessions);
		bail_error(err);
		bn_binding_free(&binding);
	}


	err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding,
			NULL,"/nkn/nvsd/resource_mgr/config/%s/max_bandwidth",
			    rp_name);
	bail_error(err);
	if(binding) {
		err = bn_binding_get_uint64(binding,ba_value,NULL,&max_bw);
		bail_error(err);
		bn_binding_free(&binding);
	}

	if(sessions == 10 && max_bw == 200) {
		cli_printf("Resource pool created with Client sessions = %lu, Client Bandwidth = %lu",
				sessions, max_bw);
	} else {
		cli_printf("Resource pool:\n  Client sessions = %lu, Client Bandwidth = %lu",
				sessions, max_bw);
	}

bail:
	bn_binding_free(&binding);
	return err;
}
#endif

int
cli_nvsd_rm_remove_alarms(const char* rp_name)
{
	int err = 0;
	char *rp_node =  NULL;
	char *rp_alarm_node = NULL;
	const char *node = NULL;
	tstr_array *rsrc_nodes = NULL;
	bn_binding *binding = NULL;
	uint32_t i = 0;

	rp_node = smprintf("/nkn/nvsd/resource_mgr/config/%s", rp_name);
	bail_null(rp_node);

	err = mdc_get_binding_children_tstr_array(cli_mcc, NULL, NULL,
		&rsrc_nodes, rp_node, NULL);
	bail_error_null(err, rsrc_nodes);

	for( i = 0; i< tstr_array_length_quick(rsrc_nodes) ; i++){
		node = 	tstr_array_get_str_quick(rsrc_nodes, i);
		bail_null(node);

		if(strcmp(tstr_array_get_str_quick(rsrc_nodes, i),"parent")== 0)
			continue;

		rp_alarm_node = smprintf("/stats/config/alarm/rp_%s_%s",
				    rp_name, node);
		bail_null(rp_alarm_node);

		err = mdc_get_binding(cli_mcc, NULL, NULL,
				false, &binding, rp_alarm_node, NULL);
		bail_error(err);

		if(binding){
			err = mdc_delete_binding(cli_mcc, NULL,
				NULL, rp_alarm_node);
			bn_binding_free(&binding);
			bail_error(err);
		}
		safe_free(rp_alarm_node);

	}
bail:
	safe_free(rp_node);
	safe_free(rp_alarm_node);
	tstr_array_free(&rsrc_nodes);
	bn_binding_free(&binding);
	return err;
}

int
cli_nvsd_rm_help(
		void *data, 
		cli_help_type help_type,
		const cli_command *cmd, 
		const tstr_array *cmd_line,
		const tstr_array *params, 
		const tstring *curr_word,
		void *context)
{
	int err = 0;
	tstr_array *names = NULL;
	const char *rp = NULL;
	int i = 0, num_names = 0;
	const char* cmd_prefix = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(curr_word);
	UNUSED_ARGUMENT(params);


	switch(help_type)
	{
		case cht_termination:
			if(cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
				err = cli_add_termination_help(context, 
						GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
				bail_error(err);
			}
			else {
				err = cli_add_termination_help(context,
						GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
				bail_error(err);
			}
			break;

		case cht_expansion:
			if(cmd->cc_help_exp) {
				err = cli_add_expansion_help(context,
						GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
						GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
				bail_error(err);
			}

			err = cli_nvsd_rm_get_rp(&names);
			bail_error(err);

			num_names = tstr_array_length_quick(names);
			for(i = 0; i < num_names; ++i) {
				rp = tstr_array_get_str_quick(names, i);
				bail_null(rp);

				err = cli_add_expansion_help(context, rp, NULL);
				bail_error(err);
			}
			cmd_prefix = tstr_array_get_str_quick(cmd_line,0);
			bail_null(cmd_prefix);

			if(strcmp("show",cmd_prefix) == 0) {
				err = cli_add_expansion_help(context, GLOBAL_RSRC_POOL, NULL);
				bail_error(err);
			}

			break;
		default:
			bail_force(lc_err_unexpected_case);
			break;
	}
bail:
	tstr_array_free(&names);
	return err;

}
/* ................................End of File ...........................................*/


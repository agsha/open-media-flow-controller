/*
 *
 * Filename:  md_nkn_services.c
 * Date:      2011/04/19
 * Author:    Ramalingam Chandran
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "nkn_mgmt_defs.h"
#include "tpaths.h"
#include "file_utils.h"
#include "nkn_defs.h"

/* ------------------------------------------------------------------------- */

int md_nkn_services_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nkn_services_ug_rules = NULL;

static int md_nkn_services_commit_side_effects( md_commit *commit,
                const mdb_db *old_db,
                mdb_db *inout_new_db,
                mdb_db_change_array *change_list,
                void *arg);


static int md_nkn_services_commit_apply(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

int nkn_launch_process(tstring *lp_path, tstr_array *lp_argv, const char *msg);

static int
md_nkn_services_stop(md_commit *commit,
                         const mdb_db *db,
                         const char *action_name,
                         bn_binding_array *params,
                         void *cb_arg);


static int
md_nkn_services_get_hw_type(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

/* ------------------------------------------------------------------------- */
int
md_nkn_services_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nkn-services", 1,
	    "/nkn/nvsd/services", "/nkn/nvsd/services/config",
	    modrf_all_changes,
	    md_nkn_services_commit_side_effects, NULL,
	    NULL, NULL,
	    md_nkn_services_commit_apply, NULL,
	    0, 0,
	    md_generic_upgrade_downgrade,
	    &md_nkn_services_ug_rules,
	    NULL, NULL,
	    NULL, NULL, NULL, NULL,
	    &module);
    bail_error(err);


    err  = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name =            "/nkn/nvsd/services/actions/stop";
    node->mrn_node_reg_flags =  mrf_flags_reg_action;
    node->mrn_cap_mask =        mcf_cap_node_basic;
    node->mrn_action_async_nonbarrier_start_func =      md_nkn_services_stop;
    node->mrn_actions[0]->mra_param_name = "service_name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description =     "Stop the service";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/services/monitor/state/nvsd/global";
    node->mrn_value_type = bt_string;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/services/monitor/state/nvsd/mgmt";
    node->mrn_value_type = bt_string;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/services/monitor/state/nvsd/preread/global";
    node->mrn_value_type = bt_string;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/services/monitor/state/nvsd/preread/sas";
    node->mrn_value_type = bt_string;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/services/monitor/state/nvsd/preread/sata";
    node->mrn_value_type = bt_string;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/services/monitor/state/nvsd/preread/ssd";
    node->mrn_value_type = bt_string;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/services/monitor/state/nvsd/network";
    node->mrn_value_type = bt_string;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/services/hw_type";
    node->mrn_value_type = bt_uint8;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_services_get_hw_type;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);


bail:
    return(err);
}


static int
md_nkn_services_get_hw_type(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    int hw_type = 0;
    uint32_t i;
    tstr_array *ret_lines = NULL;

    err = lf_read_file_lines("/proc/cmdline", &ret_lines);
    bail_error_null(err, ret_lines);

    for(i = 0; i < tstr_array_length_quick(ret_lines); i++) {
	const char *line = tstr_array_get_str_quick(ret_lines, i);
	if(line && strstr(line, "model=pacifica")) {
	    hw_type = HW_TYPE_PACIFICA;
	    break;
	} else {
	    hw_type = HW_TYPE_OTHER;
	    break;
	}
    }

    err = bn_binding_new_uint8(ret_binding,
	    node_name, ba_value,
	    0, hw_type);
    bail_error(err);


bail:
    tstr_array_free(&ret_lines);
    return err;
}

int
nkn_launch_process(tstring *lp_path, tstr_array *lp_argv, const char *msg)
{
    int err = 0;
    lc_launch_params *lp = NULL;
    lc_launch_result lr;

    if (lp_path == NULL || lp_argv == NULL) {
        err = EINVAL;
        goto bail;
    }

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);

    lp->lp_path = lp_path;
    lp->lp_argv = lp_argv;

    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_DEBUG;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_devnull;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_devnull;

    err = lc_launch(lp, &lr);
    bail_error(err);
    bail_require(lr.lr_child_pid > 0);

bail:
    lc_launch_params_free(&lp);
    return err;
}



static int md_nkn_services_commit_side_effects( md_commit *commit,
                const mdb_db *old_db,
                mdb_db *inout_new_db,
                mdb_db_change_array *change_list,
                void *arg)
{
    int err = 0;
    uint32 num_changes = 0;
    uint32 i = 0, j = 0;
    const mdb_db_change *change = NULL;
    char *nodename = NULL;
    bn_binding *binding = NULL;

    nodename = (char *)malloc(256*sizeof(char));
    bail_null(nodename);
    memset(nodename,'\0',256);

    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5,
			"nkn", "nvsd", "system", "config", "mod_dmi"))) {
	    tbool dmi_flag = false;
	    err = mdb_get_node_value_tbool(commit, inout_new_db,
		    "/nkn/nvsd/system/config/mod_dmi",0,NULL,&dmi_flag);
	    bail_error(err);
	    if(dmi_flag == true) {
		snprintf(nodename,255,"/pm/process/ssh_tunnel/auto_launch");
		err = bn_binding_new_tbool(&binding, nodename, ba_value, 0, true);
		bail_error_null(err,binding);
		err = mdb_set_node_binding(commit, inout_new_db, bsso_modify, 0,
			binding);
		bail_error(err);
		bn_binding_free(&binding);

		snprintf(nodename,255,"/pm/process/agentd/auto_launch");
		err = bn_binding_new_tbool(&binding, nodename, ba_value, 0, true);
		bail_error_null(err,binding);
		err = mdb_set_node_binding(commit, inout_new_db, bsso_modify, 0,
			binding);
		bail_error(err);
		bn_binding_free(&binding);

	    } else {
		snprintf(nodename,255,"/pm/process/ssh_tunnel/auto_launch");
		err = bn_binding_new_tbool(&binding, nodename, ba_value, 0, false);
		bail_error_null(err,binding);
		err = mdb_set_node_binding(commit, inout_new_db, bsso_modify, 0,
			binding);
		bail_error(err);
		bn_binding_free(&binding);

		snprintf(nodename,255,"/pm/process/agentd/auto_launch");
		err = bn_binding_new_tbool(&binding, nodename, ba_value, 0, false);
		bail_error_null(err,binding);
		err = mdb_set_node_binding(commit, inout_new_db, bsso_modify, 0,
			binding);
		bail_error(err);
		bn_binding_free(&binding);

	    }
	}
    }
bail:
    safe_free(nodename);
    bn_binding_free(&binding);
    return err;
}


/* ------------------------------------------------------------------------- */
static int
md_nkn_services_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0, num_changes = 0, i = 0 ;
    const mdb_db_change *change = NULL;
    tstring *lp_path = NULL;
    tstr_array *lp_argv = NULL;
    tbool one_shot_mode = false;

    err = md_commit_get_commit_mode(commit, &one_shot_mode);
    bail_error(err);

    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; i++)
    {
        change = mdb_db_change_array_get_quick(change_list, i );
        bail_null(change);

        if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, 
               "nkn", "nvsd", "system", "config", "mod_dmi")) && (one_shot_mode == false)){
            tbool dmi_flag = false;
            err = mdb_get_node_value_tbool(commit, new_db,
                     "/nkn/nvsd/system/config/mod_dmi",0,NULL,&dmi_flag);
            bail_error(err);
            err = ts_new_str(&lp_path, "/opt/nkn/bin/startdmi.sh");
            bail_error(err);
            if(dmi_flag == true) {
                lc_log_basic(LOG_INFO,"Starting mod-dmi service...");
                err = tstr_array_new_va_str(&lp_argv, NULL, 2,
                        "/opt/nkn/bin/startdmi.sh", "start");
                bail_error(err);
            } else {
                lc_log_basic(LOG_INFO,"Stopping mod-dmi service...");
                err = tstr_array_new_va_str(&lp_argv, NULL, 2,
                        "/opt/nkn/bin/startdmi.sh", "stop");
                bail_error(err);
            }
            err = nkn_launch_process(lp_path, lp_argv, "dmi-service");
            bail_error(err);
        } else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
               "logging", "syslog", "action", "file", "*", "facility", "*", "min_priority"))){
            FILE *fp = NULL;
            fp = fopen("/var/opt/tms/output/syslog.conf","a+");
            bail_null(fp);
            fprintf(fp,"#Generated for ipmi events\n");
            fprintf(fp,"local4.*       |/var/log/local4.pipe");
            fclose(fp);
            err = lc_process_send_signal(SIGHUP, "syslogd",
                                         syslogd_pid_file_path);
            bail_error(err);
        }
    }
 bail:
    return(err);
}

/* ------------------------------------------------------------------------------ */
static int
md_nkn_services_stop(md_commit *commit,
                         const mdb_db *db,
                         const char *action_name,
                         bn_binding_array *params,
                         void *cb_arg)
{
	int err = 0;
        char *name = NULL;
        const bn_binding *binding = NULL;

        /* XXX: add action handling here */

        err = bn_binding_array_get_binding_by_name(params, "service_name", &binding);
        bail_error_null(err, binding);

        err = bn_binding_get_str(binding, ba_value, bt_string, 0, &name);
        bail_error_null(err, name);

	if (!strcmp(name, "mod-rtmp-fms"))
                system ("/nkn/adobe/fms/fmsmgr server fms stop 2> /dev/null");
        else if (!strcmp(name, "mod-rtmp-admin"))
                system ("/nkn/adobe/fms/fmsmgr adminserver stop 2> /dev/null");
bail:
        return err;
}


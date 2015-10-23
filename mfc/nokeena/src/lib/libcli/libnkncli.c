/*
 *
 * Filename:  libnkncli.c
 * Date:      2010/01/27 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-10 Ankeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include "common.h"
#include <climod_common.h>
#include <dso.h>
#include <env_utils.h>
#include "tstring.h"
#include "random.h"
#include "cli_module.h"
#include "clish_module.h"
#include "proc_utils.h"

#include "libnkncli.h"
#include "nkn_mgmt_defs.h"

/* ----------------------------------------------------------------------- */
/* Our version of clish_run_program_fg ()
 * Since the default function will not run when invoked through RGP
 * meaning commands coming from CMC, we have designed another route
 * to handle such requests. We have a action node that sends the 
 * command and params to mgmtd, that then runs the command and sends
 * back the stdout output. 
 * ----------------------------------------------------------------------- */
int 
nkn_clish_run_program_fg(const char *abs_path, const tstr_array *argv,
                         const char *working_dir,
                         clish_termination_handler_func termination_handler,
                         void *termination_handler_data)
{
	int err = 0;
	uint32_t i;
	uint32_t param_count = 0;
        uint32_t ret_err = 0;
	bn_request *req = NULL;
	tstring *ret_msg = NULL;
	tstring *stdout_output = NULL;
	lc_launch_params *lp = NULL;
	lc_launch_result lr;


	/* First check to see it is it running in CLI shell or otherwise */
    	if (clish_is_shell()) {
		/* This is through normal CLI hence call std fn */
		err = clish_run_program_fg(abs_path, argv, working_dir,
					termination_handler, termination_handler_data);
		return (err);
	}


	/* Initialize launch params */
	err = lc_launch_params_get_defaults(&lp);
	bail_error_null(err, lp);
	
	/* Get the number of parameters */
	param_count = tstr_array_length_quick(argv);
	
	/* Set the command */
	err = ts_new_str(&(lp->lp_path), abs_path);
	bail_error(err);
	
	err = tstr_array_new(&(lp->lp_argv), 0);
	bail_error(err);
	
	err = tstr_array_insert_str(lp->lp_argv, 0, abs_path);
	bail_error(err);
	
	/* Get the arguments */
	for (i = 1; i < param_count; i++)
	{
		const char *param_value = NULL;
	
		param_value = tstr_array_get_str_quick(argv, i);
		if (!param_value)
			break;	// If no parameter then break out of loop
	
		err = tstr_array_insert_str(lp->lp_argv, i, param_value);
		bail_error(err);
	}
	
	lp->lp_wait = true;
	lp->lp_env_opts = eo_preserve_all;
	lp->lp_log_level = LOG_INFO;
	
	lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
	lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;
	
	err = lc_launch(lp, &lr);
	bail_error(err);
	
	stdout_output = lr.lr_captured_output;
	
	/* Send the stdout output */
	cli_printf("%s", ts_str(stdout_output));
	
/* COMMENTED BY Ram on Feb 16th, 2010
 * The below was the older logic, that is now being made obselete
 * Invoking lc_launch () from within here for RGP is a better 
 * solution than the round about way of sending an action 
 * and driving it from mgmtd. Running lc_launch from mgmtd
 * had the drawback of blocking mdreq calls from the shell 
 * scripts as it is still handling the lc_launch ()
 */
#if 0
	/* This is the non CLI case like RGP */
	/* We now handle it through the action logic */

	/* Create an action request */
	err = bn_action_request_msg_create(&req, "/nkn/debug/generate/systemcall");
	bail_error(err);

	err = bn_action_request_msg_append_new_binding
			(req, 0, "command", bt_string, abs_path, NULL);
	bail_error(err);

	/* Adding parameters as bindings */
	param_count = tstr_array_length_quick(argv);

	for (i = 0; i < param_count; i++)
	{
		char param_index [5];
		const char *param_value = NULL;

		/* Parameter index starting at 1 */
		sprintf (param_index, "%d", (i + 1));

		param_value = tstr_array_get_str_quick(argv, (i + 1));
		if (!param_value)
			break;	// If no parameter then break out of loop

		err = bn_action_request_msg_append_new_binding
			(req, 0, param_index, bt_string, param_value, NULL);
		bail_error(err);
	}

	 /* Send the action message to get the return message */
	err = mdc_send_mgmt_msg (cli_mcc, req, false, NULL, &ret_err, &ret_msg);
	bail_error(err);
#endif // 0
bail:
    ts_free(&stdout_output);
    lc_launch_params_free(&lp);
    return err;

} /* end of nkn_clish_run_program_fg () */

/* nkn_cli_is_policy_bound() */

/*
*   Function to see if a policy is bound to some namespace
*/

tbool 
nkn_cli_is_policy_bound(const char *policy, tstring **associated_ns, const char *ns_active_check, const char *str_silent)
{
	int err = 0;
	uint32 ret_err = 0;
	tbool is_policy_bound = false;
	bn_binding *bn_policy = NULL;
	bn_binding *bn_ns_active = NULL;
	bn_binding *bn_silent = NULL;

	bn_binding_array *bn_bindings = NULL;
	bn_binding_array *barr = NULL;
	tstring *ns = NULL;
	
	err = bn_binding_array_new(&barr);
	bail_error(err);

	err = bn_binding_new_str_autoinval(&bn_policy, "policy", ba_value,
			bt_string, 0, policy);
	bail_error(err);

	err = bn_binding_new_str_autoinval(&bn_ns_active, "ns_active", ba_value,
			bt_bool, 0, ns_active_check);
	bail_error(err);

	err = bn_binding_new_str_autoinval(&bn_silent, "silent", ba_value,
			bt_bool, 0, str_silent);
	bail_error(err);


	err = bn_binding_array_append_takeover(barr, &bn_policy);
	bail_error(err);

	err = bn_binding_array_append_takeover(barr, &bn_ns_active);
	bail_error(err);

	err = bn_binding_array_append_takeover(barr, &bn_silent);
	bail_error(err);


	err = mdc_send_action_with_bindings_and_results(cli_mcc,
			&ret_err,
			NULL,
			"/nkn/nvsd/policy_engine/actions/check_policy_namespace_dependancy",
			barr,
			&bn_bindings);
	bail_error(err);

	if(bn_bindings &&  associated_ns != NULL ) {
		err = bn_binding_array_get_value_tstr_by_name(
				bn_bindings, "associated_ns", NULL, &ns);
		bail_error(err);
		*associated_ns = ns;
	}

	if(ret_err) {
		is_policy_bound = true;
		goto bail;
	}
bail:
	bn_binding_array_free(&barr);
	bn_binding_array_free(&bn_bindings);
	bn_binding_free(&bn_policy);
	bn_binding_free(&bn_ns_active);
	bn_binding_free(&bn_silent);
	return is_policy_bound;

}
/* end of nkn_cli_is_policy_bound () */

int
cli_nvsd_show_cluster(const char *cluster, int from_namespace)
{
    int err = 0, cl_type = 0;
    tstring *format_type = NULL, *cl_hash = NULL,
	*rr_method = NULL, *repli_mode = NULL, *namespace = NULL;
    node_name_t node = {0};
    tbool local_redir = false, configd = false, cl_configured = true;
    const char *cluster_type = NULL, *ns_cl_type = NULL;

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		    &format_type, NULL,
		    "/nkn/nvsd/cluster/config/%s/cluster-type",
		    cluster);
    bail_error(err);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		    &namespace, NULL,
		    "/nkn/nvsd/cluster/config/%s/namespace",
		    cluster);
    bail_error(err);

    if ((namespace == NULL) || (0 == strcmp(ts_str(namespace), ""))) {
	/* means "namespace * add cluster * " command has not been issued */
	cl_configured = false;
    }

    snprintf(node, sizeof(node), "/nkn/nvsd/cluster/config/%s/configured", cluster);
    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &configd, node, NULL);
    bail_error(err);

    if (configd == false) {
	cl_configured = false;
    }

    bail_error(err);
    if (ts_equal_str(format_type, "1", true)) {
	cl_type = 1;
	cluster_type = "redirect-cluster";
	ns_cl_type = "Redirect_Cluster";
    } else if (ts_equal_str(format_type, "2", true)) {
	cl_type = 2;
	cluster_type = "proxy-cluster";
	ns_cl_type = "Proxy_Cluster";
    } else if (ts_equal_str(format_type, "3", true)) {
	cl_type = 3;
	cluster_type = "cache-cluster";
	ns_cl_type = "Cache_Cluster";
    } else {
	cl_type = 0;
	cluster_type = "None";
	ns_cl_type = "Format-Type : None";
    }

    if (cl_configured == false) {
	cl_type = 0;
	cluster_type = "None";
	ns_cl_type = "Format-Type : None";
    }

    if (from_namespace == 1) {
        cli_printf( _("    %s   -    %s\n"), cluster, ns_cl_type);
    }
    cli_printf(_("  \nCluster Configurations \n"));

    cli_printf_query(_("  Cluster Name : #/nkn/nvsd/cluster/config/%s#\n"),
	    cluster);

    cli_printf(_("  Cluster Type : %s\n"), cluster_type);

    if (cl_type == 1) {
	err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &cl_hash, NULL,
		"/nkn/nvsd/cluster/config/%s/cluster-hash", cluster);
	bail_error_null(err, cl_hash);
    } else if ( cl_type == 2) {
	err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &cl_hash, NULL,
		"/nkn/nvsd/cluster/config/%s/proxy-cluster-hash", cluster);
	bail_error_null(err, cl_hash);
    }
    if (ts_equal_str(cl_hash, "1", true)) {
	cli_printf( _("  Cluster Hash : base-url\n"));
    } else if (ts_equal_str(cl_hash, "2", true)) {
	cli_printf( _("  Cluster Hash : complete-url\n"));
    } else {
	cli_printf( _("  Cluster Hash : None\n"));
    }
    cli_printf_query(_("  Associated Server-map : #/nkn/nvsd/cluster/config/%s/server-map#\n"),
	    cluster);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &rr_method, NULL, "/nkn/nvsd/cluster/config/%s/request-routing-method", cluster);
    bail_error_null(err, rr_method);

    if (cl_type == 1) {
	/* this is a redirect cluster */
	if (ts_equal_str(rr_method, "1", true)) {
	    cli_printf( _("  Request Routing Method : consistent-hash-redirect\n"));
	}else if (ts_equal_str(rr_method, "2", true)) {
	    cli_printf( _("  Request Routing Method : consistent-hash-redirect-load-balance\n"));
	}else {
	    cli_printf( _("  Request Routing Method : Unknown \n"));
	}

	cli_printf_query(_("  Redirect Address : #/nkn/nvsd/cluster/config/%s/redirect-addr-port#\n"),
		cluster);

	snprintf(node, sizeof(node), "/nkn/nvsd/cluster/config/%s/redirect-local",cluster);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &local_redir, node, NULL);
	bail_error(err);

	if (local_redir == true) {
	    cli_printf( _("  Redirect Local : Allowed\n"));
	} else {
	    cli_printf( _("  Redirect Local : Not-Allowed\n"));
	}

	cli_printf_query(_("  Redirect Base Path : #/nkn/nvsd/cluster/config/%s/redirect-base-path#\n"),
		cluster);
    } else if (cl_type == 2) {
	/* this is a proxy cluster */
	if (ts_equal_str(rr_method, "1", true)) {
	    cli_printf( _("  Request Routing Method : consistent-hash-proxy\n"));
	}else if (ts_equal_str(rr_method, "2", true)) {
	    cli_printf( _("  Request Routing Method : consistent-hash-proxy-load-balance\n"));
	}else {
	    cli_printf( _("  Request Routing Method : Unknown \n"));
	}

	cli_printf_query(_("  Proxy Address : #/nkn/nvsd/cluster/config/%s/proxy-addr-port#\n"),
		cluster);

	snprintf(node, sizeof(node), "/nkn/nvsd/cluster/config/%s/proxy-local",cluster);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &local_redir, node, NULL);
	bail_error(err);

	if (local_redir == true) {
	    cli_printf( _("  Proxy Local : Allowed\n"));
	} else {
	    cli_printf( _("  Proxy Local : Not-Allowed\n"));
	}

	cli_printf_query(_("  Proxy Base Path : #/nkn/nvsd/cluster/config/%s/proxy-base-path#\n"),
		cluster);
    }

    /* common config for all clusters */
    cli_printf_query(_("  Load Balance Threshold : #/nkn/nvsd/cluster/config/%s/load-threshold#\n"),
	    cluster);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &repli_mode, NULL, "/nkn/nvsd/cluster/config/%s/replicate-mode", cluster);
    bail_error_null(err, repli_mode);

    if (ts_equal_str(repli_mode, "1", true)) {
	cli_printf( _("  Replicate Mode: least-loaded\n"));
    } else if (ts_equal_str(repli_mode, "2", true)) {
	cli_printf( _("  Replicate Mode: Random\n"));
    } else {
	cli_printf( _("  Replicate Mode: None\n"));
    }

bail:
    ts_free(&repli_mode);
    ts_free(&cl_hash);
    ts_free(&rr_method);
    ts_free(&format_type);
    ts_free(&namespace);
    return err;
}
/* End of libnkncli.c */

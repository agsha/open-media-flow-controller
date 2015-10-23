/*
 * @file cli_jpsd_tdf_cmds.c
 * @brief
 * cli_jpsd_tdf_cmds.c - definations for jpsd-tdf cli functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "libnkncli.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

int cli_jpsd_tdf_cmds_init(const lc_dso_info *info,
			const cli_module_context *context);

static int cli_jpsd_tdf_get_domain_rules(
			tstr_array **ret_names,
			tbool hide_display)
{
        int err = 0;
        char *bn_name = NULL;
        tstr_array *names = NULL;
        tstr_array_options opts;
        tstr_array *names_config = NULL;
        bn_binding *display_binding = NULL;

        UNUSED_ARGUMENT(hide_display);

        bail_null(ret_names);
        *ret_names = NULL;

        err = tstr_array_options_get_defaults(&opts);
        bail_error(err);

        opts.tao_dup_policy = adp_delete_old;

        err = tstr_array_new(&names, &opts);
        bail_error(err);

        err = mdc_get_binding_children_tstr_array(cli_mcc,
                        NULL, NULL, &names_config,
                        "/nkn/jpsd/tdf/domain-rule", NULL);
        bail_error_null(err, names_config);

        err = tstr_array_concat(names, &names_config);
        bail_error(err);

        *ret_names = names;
        names = NULL;
bail:
        tstr_array_free(&names);
        tstr_array_free(&names_config);
        safe_free(bn_name);
        bn_binding_free(&display_binding);
        return err;
}

static int cli_jpsd_tdf_domain_rule_help(void *data,
			cli_help_type help_type,
			const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params,
			const tstring *curr_word,
			void *context)
{
	int err = 0;
        int i = 0, num_names = 0;
        const char *domain_rule = NULL;
        tstr_array *names = NULL;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd_line);
        UNUSED_ARGUMENT(params);
        UNUSED_ARGUMENT(curr_word);

        switch (help_type) {
	case cht_termination:
		if(cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
			err = cli_add_termination_help(context,
				GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
			bail_error(err);
		} else {
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
		err = cli_jpsd_tdf_get_domain_rules(&names, true);
		num_names = tstr_array_length_quick(names);
		for(i = 0; i < num_names; ++i) {
			domain_rule = tstr_array_get_str_quick(names, i);
			bail_null(domain_rule);
			err = cli_add_expansion_help(context, domain_rule, NULL);
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

static int cli_jpsd_tdf_show_domain_rule(void *data,
			const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
	int err = 0;
	int i = 0, num_names = 0;
        const char *domain_rule = NULL;
	tstr_array *names = NULL;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(cmd_line);
        UNUSED_ARGUMENT(params);

	err = cli_printf("Name\tPrecedence\tUplink-VRF\tDownlink-VRF\n");
	bail_error(err);
	err = cli_printf("----\t----------\t----------\t------------\n");
	bail_error(err);
	err = cli_jpsd_tdf_get_domain_rules(&names, true);
	num_names = tstr_array_length_quick(names);
	for (i = 0; i < num_names; ++i) {
		domain_rule = tstr_array_get_str_quick(names, i);
		bail_null(domain_rule);
		err = cli_printf("%s\t  ", domain_rule);
		bail_error(err);
		err = cli_printf_query(
			"#/nkn/jpsd/tdf/domain-rule/%s/precedence#\t\t", domain_rule);
		bail_error(err);
		err = cli_printf_query(
			"#/nkn/jpsd/tdf/domain-rule/%s/uplink-vrf#\t\t", domain_rule);
		bail_error(err);
		err = cli_printf_query(
			"#/nkn/jpsd/tdf/domain-rule/%s/downlink-vrf#\n", domain_rule);
		bail_error(err);
	}

bail:
	tstr_array_free(&names);
	return err;
}

static int cli_jpsd_tdf_get_domains(
			tstr_array **ret_names,
			tbool hide_display)
{
        int err = 0;
        char *bn_name = NULL;
        tstr_array *names = NULL;
        tstr_array_options opts;
        tstr_array *names_config = NULL;
        bn_binding *display_binding = NULL;

        UNUSED_ARGUMENT(hide_display);

        bail_null(ret_names);
        *ret_names = NULL;

        err = tstr_array_options_get_defaults(&opts);
        bail_error(err);

        opts.tao_dup_policy = adp_delete_old;

        err = tstr_array_new(&names, &opts);
        bail_error(err);

        err = mdc_get_binding_children_tstr_array(cli_mcc,
                        NULL, NULL, &names_config,
                        "/nkn/jpsd/tdf/domain", NULL);
        bail_error_null(err, names_config);

        err = tstr_array_concat(names, &names_config);
        bail_error(err);

        *ret_names = names;
        names = NULL;
bail:
        tstr_array_free(&names);
        tstr_array_free(&names_config);
        safe_free(bn_name);
        bn_binding_free(&display_binding);
        return err;
}

static int cli_jpsd_tdf_domain_help(void *data,
			cli_help_type help_type,
			const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params,
			const tstring *curr_word,
			void *context)
{
	int err = 0;
        int i = 0, num_names = 0;
        const char *domain = NULL;
        tstr_array *names = NULL;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd_line);
        UNUSED_ARGUMENT(params);
        UNUSED_ARGUMENT(curr_word);

        switch (help_type) {
	case cht_termination:
		if(cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
			err = cli_add_termination_help(context,
				GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
			bail_error(err);
		} else {
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
		err = cli_jpsd_tdf_get_domains(&names, true);
		num_names = tstr_array_length_quick(names);
		for (i = 0; i < num_names; ++i) {
			domain = tstr_array_get_str_quick(names, i);
			bail_null(domain);
			err = cli_add_expansion_help(context, domain, NULL);
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

static int cli_jpsd_tdf_show_domain(void *data,
			const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
	int err = 0;
	int i = 0, num_names = 0;
        const char *domain = NULL;
	tstr_array *names = NULL;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(cmd_line);
        UNUSED_ARGUMENT(params);

	err = cli_printf("Name\tRule\n");
	bail_error(err);
	err = cli_printf("----\t----\n");
	bail_error(err);
	err = cli_jpsd_tdf_get_domains(&names, true);
	num_names = tstr_array_length_quick(names);
	for (i = 0; i < num_names; ++i) {
		domain = tstr_array_get_str_quick(names, i);
		bail_null(domain);
		err = cli_printf("%s\t", domain);
		bail_error(err);
		err = cli_printf_query(
			"#/nkn/jpsd/tdf/domain/%s/rule#\n", domain);
		bail_error(err);
	}

bail:
	tstr_array_free(&names);
	return err;
}

static int cli_jpsd_domain_rule_delete(void *data,
			const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
	int err = 0;
	uint32_t ret_err = 0;
	uint32_t refcount = 0;
	char *node_name = NULL;
	const char *c_domain_rule = NULL;
	tstring *t_refcount = NULL;
	bn_request *req = NULL;

	c_domain_rule = tstr_array_get_str_quick(params, 0);
	bail_null(c_domain_rule);

	node_name = smprintf("/nkn/jpsd/tdf/domain-rule/%s/refcount", c_domain_rule);
	bail_null(node_name);
	err = mdc_get_binding_tstr(cli_mcc, &ret_err,
			NULL, NULL, &t_refcount, node_name, NULL);
	if (ret_err != 0) {
		goto bail;
	}
	sscanf(ts_str(t_refcount), "%u", &refcount);

	if (refcount != 0) {
		cli_printf_error("domain-rule %s assiociated to tdf-domains."
			"please remove all associtaions", c_domain_rule);
		goto bail;
	}

	err = bn_set_request_msg_create(&req, 0);
	bail_error(err);
	err = bn_set_request_msg_append_new_binding_fmt(
			req, bsso_delete, 0,
			bt_string, 0, "", NULL,
			"/nkn/jpsd/tdf/domain-rule/%s", c_domain_rule);
	bail_error(err);
	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
	bail_error(err);

bail:
	bn_request_msg_free(&req);
	ts_free(&t_refcount);
	safe_free(node_name);
	return err;
}

static int cli_jpsd_domain_rule_add(
			void *data,
			const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
	int err = 0;
	uint32_t ret_err = 0;
	char *node_name = NULL;
	const char *c_domain_rule = NULL;
	const char *c_precedence = NULL;
	const char *c_uplink_vrf = NULL;
	const char *c_downlink_vrf = NULL;
	const char *c_refcount = "0";
	tstring *ret_msg = NULL;

	c_domain_rule = tstr_array_get_str_quick(params, 0);
	bail_null(c_domain_rule);

	c_precedence = tstr_array_get_str_quick(params, 1);
	bail_null(c_precedence);

	c_uplink_vrf = tstr_array_get_str_quick(params, 2);
	bail_null(c_uplink_vrf);

	c_downlink_vrf = tstr_array_get_str_quick(params, 3);
	bail_null(c_downlink_vrf);

	node_name = smprintf("/nkn/jpsd/tdf/domain-rule/%s/precedence", c_domain_rule);
	bail_null(node_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_uint32, c_precedence, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

	node_name = smprintf(
			"/nkn/jpsd/tdf/domain-rule/%s/uplink-vrf", c_domain_rule);
	bail_null(node_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_string, c_uplink_vrf, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

	node_name = smprintf(
			"/nkn/jpsd/tdf/domain-rule/%s/downlink-vrf",
			 c_domain_rule);
	bail_null(node_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_string, c_downlink_vrf, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

	node_name = smprintf("/nkn/jpsd/tdf/domain-rule/%s/refcount", c_domain_rule);
	bail_null(node_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_uint32, c_refcount, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf(_("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

bail:
	ts_free(&ret_msg);
	safe_free (node_name);
	return err;
}

static int cli_jpsd_domain_delete(void *data,
			const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
	int err = 0;
	uint32_t ret_err = 0;
	uint32_t refcount = 0;
	char *node_name = NULL;
	const char *c_domain_name = NULL;
	char c_refcount[8];
	tstring *t_active = NULL;
	tstring *t_domain_rule = NULL;
	tstring *t_refcount = NULL;
	bn_request *req = NULL;
	tstring *ret_msg = NULL;

	c_domain_name = tstr_array_get_str_quick(params, 0);
	bail_null(c_domain_name);

	node_name = smprintf("/nkn/jpsd/tdf/domain/%s/active", c_domain_name);
	bail_null(node_name);
	err = mdc_get_binding_tstr(cli_mcc, &ret_err,
			NULL, NULL, &t_active, node_name, NULL);
	if (ret_err != 0) {
		goto bail;
	}

	if (safe_strcmp(ts_str(t_active), "Yes") == 0) {
		node_name = smprintf("/nkn/jpsd/tdf/domain/%s/rule", c_domain_name);
		bail_null(node_name);
		err = mdc_get_binding_tstr(cli_mcc, &ret_err,
				NULL, NULL, &t_domain_rule, node_name, NULL);
		if (ret_err != 0) {
			goto bail;
		}
		node_name = smprintf("/nkn/jpsd/tdf/domain-rule/%s/refcount",
					ts_str(t_domain_rule));
		bail_null(node_name);
		err = mdc_get_binding_tstr(cli_mcc, &ret_err,
				NULL, NULL, &t_refcount, node_name, NULL);
		if (ret_err != 0) {
			goto bail;
		}
		sscanf(ts_str(t_refcount), "%u", &refcount);
		refcount = refcount - 1;

		node_name = smprintf("/nkn/jpsd/tdf/domain-rule/%s/refcount",
					ts_str(t_domain_rule));
		sprintf(c_refcount, "%u", refcount);
		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_uint32, c_refcount, node_name);
		bail_error(err);
		if (ret_err != 0) {
			cli_printf( _("Error:%s\n"), ts_str(ret_msg));
			goto bail;
		}

	}

	err = bn_set_request_msg_create(&req, 0);
	bail_error(err);
	err = bn_set_request_msg_append_new_binding_fmt(
			req, bsso_delete, 0,
			bt_string, 0, "", NULL,
			"/nkn/jpsd/tdf/domain/%s", c_domain_name);
	bail_error(err);
	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
	bail_error(err);

bail:
	bn_request_msg_free(&req);
	ts_free(&t_active);
	ts_free(&t_domain_rule);
	ts_free(&t_refcount);
	ts_free(&ret_msg);
	safe_free(node_name);
	return err;
}

static int cli_jpsd_domain_add(void *data,
			const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
	int err = 0;
	unsigned int cmd_length = 0;
	int domain_rule = 0;
	uint32_t refcount = 0;
	uint32_t ret_err = 0;
	char c_refcount[8];
	char c_domain_active[4] = "No";
	char def_domain_rule[] = "-";
	char *node_name = NULL;
	const char *c_domain_name = NULL;
	const char *c_domain_rule = NULL;
	tstring *ret_msg = NULL;
	tstring *t_active = NULL;
	tstring *t_refcount = NULL;
	tstring *t_domain_rule = NULL;

	err = tstr_array_length(cmd_line, &cmd_length);
	if (err) {
		goto bail;
	}

	c_domain_name = tstr_array_get_str_quick(params, 0);
	bail_null(c_domain_name);

	node_name = smprintf("/nkn/jpsd/tdf/domain/%s/active", c_domain_name);
	bail_null(node_name);
	err = mdc_get_binding_tstr(cli_mcc, &ret_err,
			NULL, NULL, &t_active, node_name, NULL);
	if (ret_err != 0) {
		goto bail;
	}

	if (t_active && safe_strcmp(ts_str(t_active), "Yes") == 0) {
		node_name = smprintf("/nkn/jpsd/tdf/domain/%s/rule", c_domain_name);
		bail_null(node_name);
		err = mdc_get_binding_tstr(cli_mcc, &ret_err,
				NULL, NULL, &t_domain_rule, node_name, NULL);
		if (ret_err != 0) {
			goto bail;
		}

		node_name = smprintf("/nkn/jpsd/tdf/domain-rule/%s/refcount",
					ts_str(t_domain_rule));
		bail_null(node_name);
		err = mdc_get_binding_tstr(cli_mcc, &ret_err,
				NULL, NULL, &t_refcount, node_name, NULL);
		if (ret_err != 0) {
			goto bail;
		}
		sscanf(ts_str(t_refcount), "%u", &refcount);
		refcount = refcount - 1;

		node_name = smprintf("/nkn/jpsd/tdf/domain-rule/%s/refcount",
					ts_str(t_domain_rule));
		sprintf(c_refcount, "%u", refcount);
		err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_uint32, c_refcount, node_name);
		bail_error(err);
		if (ret_err != 0) {
			cli_printf( _("Error:%s\n"), ts_str(ret_msg));
			goto bail;
		}
	}

	c_domain_rule = tstr_array_get_str_quick(params, 1);
	bail_null(c_domain_rule);

	node_name = smprintf("/nkn/jpsd/tdf/domain/%s/rule", c_domain_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
		bsso_modify, bt_string, c_domain_rule, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

	node_name = smprintf("/nkn/jpsd/tdf/domain-rule/%s/refcount", c_domain_rule);
	err = mdc_get_binding_tstr(cli_mcc, &ret_err,
			NULL, NULL, &t_refcount, node_name, NULL);
	if (ret_err != 0) {
		cli_printf_error("committed rule can be bound");
		goto bail;
	}

	sscanf(ts_str(t_refcount), "%u", &refcount);
	refcount = refcount + 1;
	sprintf(c_refcount, "%u", refcount);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
			bsso_modify, bt_uint32, c_refcount, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

	strcpy(c_domain_active, "Yes");

	node_name = smprintf("/nkn/jpsd/tdf/domain/%s/active", c_domain_name);
	err = mdc_set_binding(cli_mcc, &ret_err, &ret_msg,
				bsso_modify, bt_string, c_domain_active, node_name);
	bail_error(err);
	if (ret_err != 0) {
		cli_printf( _("Error:%s\n"), ts_str(ret_msg));
		goto bail;
	}

bail:
	ts_free(&t_active);
	ts_free(&t_domain_rule);
	ts_free(&t_refcount);
	ts_free(&ret_msg);
	safe_free(node_name);
	return err;
}

int cli_jpsd_tdf_cmds_init(const lc_dso_info *info,
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
	cmd->cc_words_str = "cache-analyzer";
	cmd->cc_help_descr = N_("Keyword to direct commands to cache analyzer module");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "no cache-analyzer";
	cmd->cc_help_descr = N_("Keyword to direct commands to cache analyzer module");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer tdf-domain";
	cmd->cc_help_descr = N_("Keyword to specify domain names "
				"that TDF are allowed to register");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "no cache-analyzer tdf-domain";
	cmd->cc_help_descr = N_("Keyword to specify domain names "
				"that TDF are allowed to register");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer tdf-domain *";
	cmd->cc_help_exp = N_("<name>");
	cmd->cc_help_exp_hint = N_("New tdf-domain name");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "no cache-analyzer tdf-domain *";
        cmd->cc_help_exp = N_("<name>");
        cmd->cc_help_exp_hint = N_("tdf-domain name");
        cmd->cc_flags = ccf_terminal;
        cmd->cc_capab_required = ccp_set_rstr_curr;
        cmd->cc_exec_callback = cli_jpsd_domain_delete;
	cmd->cc_comp_type = cct_matching_names;
	cmd->cc_comp_pattern = "/nkn/jpsd/tdf/domain/*";
	cmd->cc_exec_value2 = "true";
	cmd->cc_help_callback = cli_jpsd_tdf_domain_help;
	cmd->cc_revmap_type = crt_none;
        CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer tdf-domain * domain-rule";
	cmd->cc_help_descr = N_("Keyword to specify rule that "
				"to be associated with subscriber-domain");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer tdf-domain * domain-rule *";
	cmd->cc_help_exp = N_("<name>");
	cmd->cc_help_exp_hint = N_("Name of domain rule defined earlier "
				"that you want to associate to the tdf-domain");
	cmd->cc_flags = ccf_terminal;
	cmd->cc_capab_required = ccp_set_rstr_curr;
	cmd->cc_exec_callback = cli_jpsd_domain_add;
	cmd->cc_comp_type = cct_matching_names;
	cmd->cc_comp_pattern = "/nkn/jpsd/tdf/domain-rule/*";
	cmd->cc_exec_value2 = "true";
	cmd->cc_help_callback = cli_jpsd_tdf_domain_rule_help;
	cmd->cc_revmap_type = crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer domain-rule";
	cmd->cc_help_descr =  N_("Keyword to specify rules that are later "
				"associated with tdf-domains");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "no cache-analyzer domain-rule";
	cmd->cc_help_descr = N_("Keyword to specify rules that are later "
				"associated with tdf-domains");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer domain-rule *";
	cmd->cc_help_exp = N_("<name>");
	cmd->cc_help_exp_hint = N_("New domain-rule name");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "no cache-analyzer domain-rule *";
	cmd->cc_help_exp = N_("<name>");
	cmd->cc_help_exp_hint = N_("domain-rule name");
	cmd->cc_flags = ccf_terminal;
	cmd->cc_capab_required = ccp_set_rstr_curr;
	cmd->cc_exec_callback = cli_jpsd_domain_rule_delete;
	cmd->cc_comp_type = cct_matching_names;
	cmd->cc_comp_pattern = "/nkn/jpsd/tdf/domain-rule/*";
	cmd->cc_exec_value2 = "true";
	cmd->cc_help_callback = cli_jpsd_tdf_domain_rule_help;
	cmd->cc_revmap_type = crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer domain-rule * precedence";
	cmd->cc_help_descr = N_("Keyword to specify precedence associated with the rule");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer domain-rule * precedence *";
	cmd->cc_help_exp = N_("<number>");
	cmd->cc_help_exp_hint = N_("Signed number between 1 and 2^31-1");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer domain-rule * precedence * uplink-vrf";
	cmd->cc_help_descr = N_("Keyword to specify the VRF name to be used for uplink");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer domain-rule * precedence * uplink-vrf *";
	cmd->cc_help_exp = N_("<vrf-name>");
	cmd->cc_help_exp_hint = N_("AlphaNumeric string");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer domain-rule * precedence * "
				"uplink-vrf * downlink-vrf";
	cmd->cc_help_descr = N_("Keyword to specify the VRF name to be used for downlink");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "cache-analyzer domain-rule * precedence * "
				"uplink-vrf * downlink-vrf *";
	cmd->cc_help_exp = N_("<vrf-name>");
	cmd->cc_help_exp_hint = N_("AlphaNumeric string");
	cmd->cc_flags = ccf_terminal | ccf_ignore_length;
	cmd->cc_capab_required = ccp_set_rstr_curr;
	cmd->cc_exec_operation = cxo_set;
	cmd->cc_exec_callback = cli_jpsd_domain_rule_add;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "show cache-analyzer";
	cmd->cc_help_descr = N_("Show cache-analyzer information");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "show cache-analyzer tdf-domain";
	cmd->cc_help_descr = N_("Show tdf-domain information");
	cmd->cc_flags = ccf_terminal;
	cmd->cc_capab_required = ccp_query_basic_curr;
	cmd->cc_exec_callback = cli_jpsd_tdf_show_domain;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = "show cache-analyzer domain-rule";
	cmd->cc_help_descr = N_("Show domain-rule information");
	cmd->cc_flags = ccf_terminal;
	cmd->cc_capab_required = ccp_query_basic_curr;
	cmd->cc_exec_callback = cli_jpsd_tdf_show_domain_rule;
	CLI_CMD_REGISTER;

	err = cli_revmap_ignore_bindings_va(4,
				"/nkn/jpsd/tdf/domain-rule/*",
				"/nkn/jpsd/tdf/domain-rule/*/precedence",
				"/nkn/jpsd/tdf/domain-rule/*/uplink-vrf",
				"/nkn/jpsd/tdf/domain-rule/*/downlink-vrf");
	bail_error(err);

bail:
	return err;
}

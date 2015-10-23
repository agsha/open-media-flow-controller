
#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

int
cli_url_filter_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_url_filter_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components,
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);
int
cli_url_filter_show_all(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_url_filter_show(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
static int
cli_url_filter_format_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply);
int
cli_url_filter_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    /*
     * [no] filter-map <name> file-url <url> format-type {iwf | calea | url-list}
     * | {refresh-interval <time>
     * | force-refresh}
     * | [crypto-key <key>]
     */
    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map";
    cmd->cc_help_descr =        N_("Import URL filter DB");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map *";
    cmd->cc_help_exp =          N_("<url-map-name>");
    cmd->cc_help_exp_hint =     N_("URL Filter Map");
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/url-filter/config/id/*";
    cmd->cc_help_use_comp =         true;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map * file-url";
    cmd->cc_help_descr =        N_("URL for downloading filter-map");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map * file-url *";
    cmd->cc_help_exp =          N_("<http[s] url>");
    cmd->cc_help_exp_hint =     N_("Specify URL map file ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map * file-url * format-type";
    cmd->cc_help_descr =        N_("Format of URL map file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map * file-url * format-type calea";
    cmd->cc_help_descr =        N_("Format conforms to CALEA standard (USA)");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/url-filter/config/id/$1$/file-url";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_name2 =            "/nkn/nvsd/url-filter/config/id/$1$/type";
    cmd->cc_exec_type2 =	    bt_string;
    cmd->cc_exec_value2 =           "calea";
    cmd->cc_revmap_names =	    "/nkn/nvsd/url-filter/config/id/*";
    cmd->cc_revmap_callback =	    cli_url_filter_format_revmap;
    cmd->cc_revmap_order =	    cro_url_filtermap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map * file-url * format-type url-list";
    cmd->cc_help_descr =        N_("Juniper format where each record is a domain only, domain + partial or full URI");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/url-filter/config/id/$1$/file-url";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_name2 =            "/nkn/nvsd/url-filter/config/id/$1$/type";
    cmd->cc_exec_type2 =	    bt_string;
    cmd->cc_exec_value2 =           "url";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map * file-url * format-type iwf";
    cmd->cc_help_descr =        N_("Format conforms to IWF standard (UK-based)");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/url-filter/config/id/$1$/file-url";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_name2 =            "/nkn/nvsd/url-filter/config/id/$1$/type";
    cmd->cc_exec_type2 =	    bt_string;
    cmd->cc_exec_value2 =           "iwf";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no filter-map";
    cmd->cc_help_descr =        N_("Delete Filter-Map");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no filter-map *";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =          N_("<url-map-name>");
    cmd->cc_help_exp_hint =     N_("Url-map to be deleted");
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/url-filter/config/id/*";
    cmd->cc_help_use_comp =         true;
    cmd->cc_exec_operation =        cxo_delete;
    cmd->cc_exec_name =             "/nkn/nvsd/url-filter/config/id/$1$";
    cmd->cc_exec_type =             bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map * refresh-force";
    cmd->cc_help_descr =        N_("Immediate retrieval of the DB list");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_action;
    cmd->cc_exec_action_name=	    "/nkn/nvsd/url-filter/action/refresh-now";
    cmd->cc_exec_name =             "name";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =	    "$1$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map * refresh-interval";
    cmd->cc_help_descr =        N_("How often the DB file should be refreshed");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map * refresh-interval *";
    cmd->cc_help_exp =          N_("<time>");
    cmd->cc_help_exp_hint =     N_("0-24 hrs");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/url-filter/config/id/$1$/refresh-interval";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_uint8;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "filter-map * crypto-key";
    cmd->cc_help_descr =        N_("Key used to decrypt the DB list file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"filter-map * crypto-key *";
    cmd->cc_help_exp =          N_("<Key>");
    cmd->cc_help_exp_hint =     N_("Specify map-file encryption key");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/url-filter/config/id/$1$/key";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_string;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =		"show filter-map";
    cmd->cc_help_descr =	N_("Show details of a filter-map");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"show filter-map list";
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_help_descr =	"List all the filter-maps";
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_url_filter_show_all;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"show filter-map *";
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_help_exp =		"<map-name>";
    cmd->cc_help_exp_hint =	"Filter-map name";
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/url-filter/config/id/*";
    cmd->cc_help_use_comp =         true;
    cmd->cc_exec_callback =	cli_url_filter_show;
    CLI_CMD_REGISTER;
bail:
    return err;
}

static int
cli_url_filter_format_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL, *t_file = NULL, *t_type = NULL,
	    *t_key = NULL, *t_refresh_interval = NULL;

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &t_file, "%s/file-url", name);
    bail_error_null(err, t_file);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &t_type, "%s/type", name);
    bail_error_null(err, t_type);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &t_key, "%s/key", name);
    bail_error_null(err, t_type);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &t_refresh_interval, "%s/refresh-interval", name);
    bail_error_null(err, t_refresh_interval);

    err = ts_new_sprintf(&rev_cmd, "filter-map %s file-url %s format-type %s",
	    value, ts_str(t_file), ts_str(t_type));

    err = tstr_array_append_takeover(ret_reply->crr_cmds,&rev_cmd);
    bail_error(err);

    err = ts_new_sprintf(&rev_cmd, "filter-map %s refresh-interval %s ",
	    value, ts_str(t_refresh_interval));

    err = tstr_array_append_takeover(ret_reply->crr_cmds,&rev_cmd);
    bail_error(err);

    if (ts_equal_str(t_key, "", true)) {
	err = ts_new_sprintf(&rev_cmd, "filter-map %s crypto-key \"\" ",
		value);
    } else {
	err = ts_new_sprintf(&rev_cmd, "filter-map %s crypto-key %s ",
		value, ts_str_maybe_empty(t_key));
    }

    err = tstr_array_append_takeover(ret_reply->crr_cmds,&rev_cmd);
    bail_error(err);

#define CONSUME_REVMAP_NODES(c) \
    {\
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, c, name);\
        bail_error(err);\
    }

    CONSUME_REVMAP_NODES("%s");
    CONSUME_REVMAP_NODES("%s/file-url");
    CONSUME_REVMAP_NODES("%s/type");
    CONSUME_REVMAP_NODES("%s/key");
    CONSUME_REVMAP_NODES("%s/refresh-interval");

bail:
    ts_free(&rev_cmd);
    ts_free(&t_type);
    ts_free(&t_file);
    return err;
}
int
cli_url_filter_show_all(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0 ;
    uint32 ret_matches = 0 ;

    err = mdc_foreach_binding(cli_mcc, "/nkn/nvsd/url-filter/config/id/*",
	    NULL, cli_url_filter_show_one, NULL, &ret_matches);
    bail_error(err);

    if (ret_matches == 0) {
	/* no filter-maps configured */
	cli_printf("No Filter-maps configured\n");
    }

bail:
    return err;
}
int
cli_url_filter_show(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0 ;
    uint32 ret_matches = 0, ret_code = 0;
    const char *uf_name = NULL;
    node_name_t node_name  ={0};
    bn_binding *binding = NULL;


    uf_name = tstr_array_get_str_quick(params, 0);
    bail_null(uf_name);

    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/url-filter/config/id/%s", uf_name);

    err = mdc_get_binding(cli_mcc, &ret_code, NULL, false, &binding, node_name, NULL);
    bail_error(err);

    if (binding == NULL) {
	cli_printf_error("Filter map %s not configured", uf_name);
	goto bail;
    }
    err = mdc_foreach_binding(cli_mcc, node_name, NULL, cli_url_filter_show_one,
	    NULL, &ret_matches);
    bail_error(err);

bail:
    return err;
}
int
cli_url_filter_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components,
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data)
{
    int err = 0;

    cli_printf_query("Filter-Map #/nkn/nvsd/url-filter/config/id/%s# status:\n", ts_str(value));
    cli_printf_query("  URL:               #/nkn/nvsd/url-filter/config/id/%s/file-url#\n", ts_str(value));
    cli_printf_query("  Format-Type:       #v:url~m:url-list~/nkn/nvsd/url-filter/config/id/%s/type#\n", ts_str(value));
    cli_printf_query("  Refresh-Interval:  #v:0~m:Download-Once~/nkn/nvsd/url-filter/"
	    "config/id/%s/refresh-interval# Hour\n", ts_str(value));
    cli_printf_query("  Crypto-Key:        #/nkn/nvsd/url-filter/config/id/%s/key#\n", ts_str(value));
    cli_printf_query("  Status:            #/nkn/nvsd/url-filter/mon/id/%s/state#\n", ts_str(value));
    cli_printf_query("  Last trigger at:   #n:r~/nkn/nvsd/url-filter/mon/id/%s/last-trigger#\n", ts_str(value));
    cli_printf_query("  Next refresh at:   #n:r~/nkn/nvsd/url-filter/mon/id/%s/next-time#\n", ts_str(value));
    cli_printf("\n");

bail:
    return err;
}


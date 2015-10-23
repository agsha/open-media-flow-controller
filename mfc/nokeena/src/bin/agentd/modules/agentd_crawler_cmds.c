#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_crawler_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_crawler_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 

int
agentd_crawler_schedule_cfg (agentd_context_t *context,
        agentd_binding *abinding, void *cb_arg, char **cli_buff);

/* ---- Operational command handler prototype and structure definition ---- */ 

/* Structure used as callback data for getting the file extensions */
typedef struct agentd_crawler_file_context_s {
    tstr_array * preloaded_files;
    tstr_array * nonpreloaded_files;
}agentd_crawler_file_context;

int agentd_op_show_crawler (agentd_context_t *context, void *data);

static int agentd_op_show_crawler_list (agentd_context_t * context, void *data);

static int agentd_crawler_foreach_file_extn (const bn_binding_array *bindings, uint32 idx,
                   const bn_binding *binding, const tstring *name,
                   const tstr_array *name_components,
                   const tstring *name_last_part,
                   const tstring *value, void *callback_data);

static int agentd_op_show_crawler_name (agentd_context_t * context, void *data, const char * crawl_name);

/*---------------------------------------------------------------------------*/
int 
agentd_crawler_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    /* Translation table array for configuration commands */
    static  mfc_agentd_nd_trans_tbl_t crawler_cmds_trans_tbl[] =  {
        #include "../translation/rules_crawler.inc"
        #include "../translation/rules_crawler_delete.inc"
        TRANSL_ENTRY_NULL
    };

    /* Translation table array for operational commands */
    oper_table_entry_t op_tbl[] = {
        {"/crawler/mfc-cluster/*/*", DUMMY_WC_ENTRY, agentd_op_show_crawler, NULL},
        OP_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (crawler_cmds_trans_tbl);
    bail_error (err);

    err = agentd_register_op_cmds_array (op_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_crawler_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_crawler_cmds_deinit called");

bail:
    return err;
}

int
agentd_crawler_schedule_cfg (agentd_context_t *context,
        agentd_binding *abinding, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    str_value_t tm_date_time = {0};
    char *nd_detail = (char *)cb_arg;

    const char *crawler = abinding->arg_vals.val_args[1];
    const char *junos_time = abinding->arg_vals.val_args[2];

    err = agentd_convert_junos_time_fmt (junos_time, &tm_date_time);
    bail_error (err);
    snprintf(node_name, sizeof(node_name), nd_detail, crawler);
    err = agentd_append_req_msg(context, node_name,
               bsso_modify, bt_datetime_sec, tm_date_time);
    bail_error(err);

bail:
    return err;
}


static int agentd_op_show_crawler_list (agentd_context_t * context, void *data){
    int err = 0;
    node_name_t node_base = {0};
    tstr_array *crawl_arr = NULL;
    uint32 crawl_count = 0;
    uint8 status_val = 0;
    char status[MAX_TMP_BUF_LEN] = {0};

    snprintf(node_base, sizeof(node_base), "/nkn/crawler/config/profile");
    err = mdc_get_binding_children_tstr_array(agentd_mcc , NULL, NULL, &crawl_arr, node_base, NULL);
    bail_error_null (err, crawl_arr);

    crawl_count = tstr_array_length_quick(crawl_arr);
    lc_log_debug (jnpr_log_level, "Crawler count: %d", crawl_count);
    if (crawl_count == 0) {
        lc_log_debug (jnpr_log_level, "No crawlers configured");
        err = 1;
        goto bail;
    }
    OP_EMIT_TAG_START(context, ODCI_MFC_CRAWLER_LIST);
    for (uint32 i = 0; i < crawl_count; i++) {
        const char *crawl_name = tstr_array_get_str_quick (crawl_arr, i);
        /* Get the status of each crawler instance */
        err = agentd_mdc_get_value_uint8_fmt(&status_val, "/nkn/crawler/config/profile/%s/status", crawl_name);
        bail_error (err);
        snprintf(status, sizeof(status), "%s", status_val? "Active":"Inactive");
        lc_log_debug(jnpr_log_level, "Crawler name: %s, Status : %s", crawl_name, status);
        OP_EMIT_TAG_START(context, ODCI_CRAWLER);
            OP_EMIT_TAG_VALUE(context, ODCI_CRAWLER_NAME, crawl_name);
            OP_EMIT_TAG_VALUE(context, ODCI_CRAWLER_CFG_STATUS, status);
        OP_EMIT_TAG_END(context, ODCI_CRAWLER);
    }
    OP_EMIT_TAG_END(context, ODCI_MFC_CRAWLER_LIST);

bail:
    tstr_array_free (&crawl_arr);
    return err;
}

static int
agentd_crawler_foreach_file_extn (const bn_binding_array *bindings, uint32 idx,
                   const bn_binding *binding, const tstring *name,
                   const tstr_array *name_components,
                   const tstring *name_last_part,
                   const tstring *value, void *callback_data){
    int err = 0;
    agentd_crawler_file_context * files_context = (agentd_crawler_file_context *) callback_data;
    const char *crawl_name = NULL;
    const tstring * file_extn = NULL;
    tstring * skip_preload = NULL;

    bail_null(files_context->preloaded_files);
    bail_null(files_context->nonpreloaded_files);

    crawl_name = tstr_array_get_str_quick(name_components, 4);
    file_extn = name_last_part;

    err = agentd_binding_array_get_value_tstr_fmt (bindings, &skip_preload, "/nkn/crawler/config/profile/%s/file_extension/%s/skip_preload", crawl_name, ts_str(file_extn));
    bail_error (err);

    lc_log_debug (jnpr_log_level, "File Extn: %s Skip preload: %s", ts_str(file_extn), ts_str(skip_preload));

    if (ts_equal_str(skip_preload, "false", true)) {
        tstr_array_append_str(files_context->preloaded_files, ts_str(file_extn));
    } else {
        tstr_array_append_str(files_context->nonpreloaded_files, ts_str(file_extn));
    }

bail:
    ts_free (&skip_preload);
    return err;
}

static int agentd_op_show_crawler_name (agentd_context_t * context, void *data, const char * crawl_name){
    int err = 0;
    node_name_t cfg_node_pat = {0};
    node_name_t mon_node_pat = {0};
    bn_binding_array * cfg_bindings = NULL;
    bn_binding_array * mon_bindings = NULL;
    uint8 status = 0, op_status = 0, auto_gen = 0;
    uint32 file_ext_count = 0, preload_files_count = 0, nonpreload_files_count = 0;
    char crawler_cfg_status [MAX_TMP_BUF_LEN] = {0};
    char crawler_op_status [MAX_TMP_BUF_LEN] = {0};
    agentd_crawler_file_context files_context = {0,0};
    tstring * url = NULL;
    tstring * depth = NULL;
    tstring * auto_gen_new = NULL;
    tstring * auto_gen_source = NULL;
    tstring * last_start = NULL;
    tstring * last_end = NULL;

    const char * agentd_crawler_op_status_str [] = {"CRAWL_NOT_SCHEDULED", 
	                                            "CRAWL_IN_PROGRESS",
						    "CRAWL_IN_PROGRESS",
						    "CRAWL_STOPPED",
						    "CRAWL_STOPPED"
                                                 };
    #define MAX_CRAWLER_OP_STATUS (sizeof(agentd_crawler_op_status_str)/sizeof (const char *))

    bail_null (crawl_name);

    /* Get the configuration nodes */
    snprintf(cfg_node_pat, sizeof(cfg_node_pat), "/nkn/crawler/config/profile/%s", crawl_name);
    err = mdc_get_binding_children(agentd_mcc, NULL, NULL, true, &cfg_bindings,
                                  true, true, cfg_node_pat);
    bail_error_null(err, cfg_bindings);
    bn_binding_array_dump("CRAWLER-CFG-BINDINGS", cfg_bindings, jnpr_log_level);

    err = agentd_binding_array_get_value_tstr_fmt (cfg_bindings, &url,  "/nkn/crawler/config/profile/%s/url", crawl_name);
    bail_error (err);

    if (!url) {
        /* URL is mandatory for crawler. If it is null, probably crawler is not configured. Throw error */
        lc_log_debug (jnpr_log_level, "Crawler %s is not configured", crawl_name);
        err = 1;
        goto bail;
    }
    err = agentd_binding_array_get_value_tstr_fmt (cfg_bindings, &depth, "/nkn/crawler/config/profile/%s/link_depth", crawl_name);
    bail_error (err);
    err = agentd_binding_array_get_value_uint8_fmt (cfg_bindings, &status, "/nkn/crawler/config/profile/%s/status", crawl_name);
    bail_error (err);
    snprintf(crawler_cfg_status, sizeof(crawler_cfg_status), "%s", (status ? "Active" : "Inactive"));
    err = agentd_binding_array_get_value_uint8_fmt (cfg_bindings, &auto_gen, "/nkn/crawler/config/profile/%s/auto_generate", crawl_name);
    bail_error (err);
    if (auto_gen){
        /* TODO : Get the file types from TM nodes */
        /* Hardcoding now as currently we are supporting only one action */
        ts_new_str(&auto_gen_new, "ASX");
        ts_new_str(&auto_gen_source, "WMV");
    }

    /* Get the list of preloaded and non-preloaded file extensions */
    tstr_array_new (&files_context.preloaded_files, NULL);
    tstr_array_new (&files_context.nonpreloaded_files, NULL);

    err = mdc_foreach_binding_prequeried (cfg_bindings, "/nkn/crawler/config/profile/*/file_extension/*",
                                          NULL, agentd_crawler_foreach_file_extn, &files_context, &file_ext_count);
    bail_error (err);

    lc_log_debug (jnpr_log_level, "Number of file extensions: %d", file_ext_count);

    /* Get the monitoring nodes */
    snprintf(mon_node_pat, sizeof(mon_node_pat), "/nkn/crawler/monitor/profile/external/%s", crawl_name);
    err = mdc_get_binding_children(agentd_mcc, NULL, NULL, true, &mon_bindings,
                                  true, true, mon_node_pat);
    bail_error (err);

    bn_binding_array_dump("CRAWLER-MON-BINDINGS", mon_bindings, jnpr_log_level);

    err = agentd_binding_array_get_value_tstr_fmt (mon_bindings, &last_start, "/nkn/crawler/monitor/profile/external/%s/start_ts", crawl_name);
    bail_error (err);
    err = agentd_binding_array_get_value_tstr_fmt (mon_bindings, &last_end, "/nkn/crawler/monitor/profile/external/%s/end_ts", crawl_name);
    bail_error (err);
    err = agentd_binding_array_get_value_uint8_fmt (mon_bindings, &op_status, "/nkn/crawler/monitor/profile/external/%s/status", crawl_name);
    bail_error (err);
    snprintf(crawler_op_status, sizeof(crawler_op_status), "%s", (op_status >= MAX_CRAWLER_OP_STATUS)? "UNKNOWN":agentd_crawler_op_status_str[op_status]);

    /* Construct XML response */
    OP_EMIT_TAG_START(context, ODCI_MFC_CRAWLER_DETAIL);
        OP_EMIT_TAG_VALUE(context, ODCI_CRAWLER_NAME, crawl_name);
        OP_EMIT_TAG_VALUE(context, ODCI_URL, ts_str(url));
        OP_EMIT_TAG_VALUE(context, ODCI_DEPTH, ts_str(depth));
        OP_EMIT_TAG_VALUE(context, ODCI_CRAWLER_CFG_STATUS, crawler_cfg_status);
    preload_files_count = tstr_array_length_quick(files_context.preloaded_files);
    nonpreload_files_count = tstr_array_length_quick(files_context.nonpreloaded_files);
    if (preload_files_count){
        OP_EMIT_TAG_START(context, ODCI_PRELOADED_FILES);
        for (uint32 i = 0; i < preload_files_count; i++) {
            OP_EMIT_TAG_VALUE(context, ODCI_CRAWL_FILE_TYPE,tstr_array_get_str_quick(files_context.preloaded_files, i));
        }
        OP_EMIT_TAG_END (context, ODCI_PRELOADED_FILES);
    }
    if (nonpreload_files_count){
        OP_EMIT_TAG_START(context, ODCI_NON_PRELOADED_FILES);
        for (uint32 i = 0; i < nonpreload_files_count; i++) {
            OP_EMIT_TAG_VALUE(context, ODCI_CRAWL_FILE_TYPE,tstr_array_get_str_quick(files_context.nonpreloaded_files, i));
        }
        OP_EMIT_TAG_END (context, ODCI_NON_PRELOADED_FILES);
    }
    if (auto_gen) {
        OP_EMIT_TAG_START (context, ODCI_AUTO_GENERATE);
            OP_EMIT_TAG_START (context, ODCI_AUTO_GENERATE_CFG);
                OP_EMIT_TAG_VALUE(context, ODCI_NEW_FILE_TYPE, ts_str(auto_gen_new));
                OP_EMIT_TAG_VALUE(context, ODCI_SOURCE_FILE_TYPE, ts_str(auto_gen_source));
            OP_EMIT_TAG_END (context, ODCI_AUTO_GENERATE_CFG);
        OP_EMIT_TAG_END (context, ODCI_AUTO_GENERATE);
    }
        OP_EMIT_TAG_VALUE(context, ODCI_LAST_START, ts_str(last_start));
        OP_EMIT_TAG_VALUE(context, ODCI_LAST_END, ts_str(last_end));
        OP_EMIT_TAG_VALUE(context, ODCI_CRAWL_STATUS, crawler_op_status);
    OP_EMIT_TAG_END(context, ODCI_MFC_CRAWLER_DETAIL);

bail:
    bn_binding_array_free(&cfg_bindings);
    bn_binding_array_free(&mon_bindings);
    ts_free(&url);
    ts_free(&depth);
    ts_free(&auto_gen_new);
    ts_free(&auto_gen_source);
    ts_free(&last_start);
    ts_free(&last_end);
    tstr_array_free(&files_context.preloaded_files);
    tstr_array_free(&files_context.nonpreloaded_files);
    return err;
}

int agentd_op_show_crawler (agentd_context_t *context, void *data) {
/* This function handles the following command patterns */
// "/crawler/mfc-cluster/*/*"
    #define CRAWLER_NAME_POS 3

    int err = 0;
    oper_table_entry_t *cmd = (oper_table_entry_t *) data;
    const char * crawl_name = NULL;

    lc_log_debug(jnpr_log_level, "got %s", cmd->pattern);
    agentd_dump_op_cmd_details (context);

    crawl_name = tstr_array_get_str_quick (context->op_cmd_parts, CRAWLER_NAME_POS);
    if (!crawl_name) {
        lc_log_debug (LOG_ERR, "Crawler name cannot be null");
        err = 1;
        goto bail;
    }
    lc_log_debug (jnpr_log_level, "Crawler name:%s", crawl_name);
    if (strcmp (crawl_name, "list") == 0) {
        err = agentd_op_show_crawler_list (context, data);
    } else {
        err = agentd_op_show_crawler_name (context, data, crawl_name);
    }

bail:
    return err;
}


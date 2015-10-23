#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_resource_pool_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_resource_pool_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 

int
agentd_rp_delete(agentd_context_t *context,
        agentd_binding *abinding, void *cb_arg, char **cli_buff);

int
agentd_rp_delete_instance(agentd_context_t *context,
        agentd_binding *abinding, void *cb_arg, char **cli_buff);

/* ---- Operational command handler prototype and structure definition ---- */ 

int agentd_op_show_resource_pool    (agentd_context_t *context, void *data);

static int agentd_show_rp_list_pools(agentd_context_t *context, void *data);

static int agentd_show_rp_with_name(agentd_context_t *context, void *data,const char* name);

static int agentd_show_rp_global_pool(agentd_context_t *context, void *data);

static int cli_nvsd_rm_show_one(agentd_context_t * context, const char* p_szNzme);
/*---------------------------------------------------------------------------*/
int 
agentd_resource_pool_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    /* Translation table array for configuration commands */
    static  mfc_agentd_nd_trans_tbl_t resource_pool_cmds_trans_tbl[] =  {
        #include "../translation/rules_resource_pool.inc"
        #include "../translation/rules_resource_pool_delete.inc"
        TRANSL_ENTRY_NULL
    };

    /* Translation table array for operational commands */
    oper_table_entry_t op_tbl[] = {
        {"/resource-pool/mfc-cluster/*/*", DUMMY_WC_ENTRY, agentd_op_show_resource_pool, NULL},
        OP_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (resource_pool_cmds_trans_tbl);
    bail_error (err);

    err = agentd_register_op_cmds_array (op_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_resource_pool_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_resource_pool_cmds_deinit called");

bail:
    return err;
}

/* Configuration custom handlers */
int
agentd_rp_delete(agentd_context_t *context,
        agentd_binding *abinding, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    const char *rp_name = abinding->arg_vals.val_args[1];

    /* check if resource-pool is attached to namespace */

    /* if attached, add deletion of resource-pool to post cmd */

    /* if not, delete resource pool */
    snprintf(node_name, sizeof(node_name),
             "/nkn/nvsd/resource_mgr/config/%s", rp_name);
    err = agentd_append_req_msg(context, node_name,
               bsso_delete, TYPE_STR, rp_name);
    bail_error(err);

bail:
    return err;
}

int
agentd_rp_delete_instance(agentd_context_t *context,
        agentd_binding *abinding, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    tbool ns_active = false;
    const char *ns_name = abinding->arg_vals.val_args[2];
    const char *rp_name = abinding->arg_vals.val_args[1];
#if 0
    snprintf(node_name, sizeof(node_name),
            "/nkn/nvsd/namespace/%s/status/active", ns_name);
    err = mdc_get_binding_tbool(agentd_mcc, NULL, NULL, NULL,
            &ns_active, node_name, NULL);
    bail_error(err);

    if (ns_active) {
        /* make it in-active first */
        snprintf(node_name, sizeof(node_name),
                "/nkn/nvsd/namespace/%s/status/active", ns_name);
        err = agentd_append_req_msg(context, node_name,
                bsso_reset, TYPE_BOOL, "false");
        bail_error(err);

        err = agentd_save_post_command(context, abinding);
        bail_error(err);
    } else {
#endif
        snprintf(node_name, sizeof(node_name),
                "/nkn/nvsd/resource_mgr/config/%s/namespace/%s",
                rp_name, ns_name);
        err = agentd_append_req_msg(context, node_name,
                bsso_delete, TYPE_STR, ns_name);
        bail_error(err);

        snprintf(node_name, sizeof(node_name),
                "/nkn/nvsd/namespace/%s/resource_pool", ns_name);
        err = agentd_append_req_msg(context, node_name,
                bsso_reset, TYPE_STR, ns_name);
        bail_error(err);

bail:
    return err;
}

/* Operational show command handler */

int agentd_op_show_resource_pool(agentd_context_t *context, void *data)
{
    int err = 0;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;
    unsigned int length = 0;
    const char * mfc_cluster_name = NULL;
    const char * list = NULL;
    const char * global_pool = NULL;
    const char * name = NULL;
    const char * tmp = NULL;
    const char * list_val = "list";
    const char * global_pool_val = "global-pool";

    lc_log_debug(jnpr_log_level, "got %s, cmd-parts-size=%d", cmd->pattern,
     tstr_array_length_quick(context->op_cmd_parts) );

    length = tstr_array_length_quick(context->op_cmd_parts);

    agentd_dump_op_cmd_details(context);

    mfc_cluster_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_CLUSTER_NAME) ;

    lc_log_debug(jnpr_log_level, "Cluster Name =  %s", mfc_cluster_name);

    tmp = tstr_array_get_str_quick(context->op_cmd_parts,POS_CLUSTER_NAME+1);

    lc_log_debug(jnpr_log_level, "The cmd arr checking for |%s|",tmp);

    if( strcmp(tmp,list_val) == 0 )
    {
       list = tmp;
    }
    else if( strcmp(tmp,global_pool_val) == 0 )
    {
       global_pool = tmp;
    }
    else
    {
       name = tmp;
    }


    lc_log_debug(jnpr_log_level, " show resource-pool for name = %s, global_pool = %s, list = %s ", name, global_pool,list);

    if (global_pool)
      err = agentd_show_rp_global_pool(context,data);
    else if (list)
      err = agentd_show_rp_list_pools(context,data);
    else if (name)
        err = agentd_show_rp_with_name(context,data,name);
    else
    {
      err=1;
      bail_error(err);
    }

bail:
    return err;
}

static int agentd_show_rp_list_pools(agentd_context_t *context, void *data)
{
  int err = 0;
  lc_log_debug(jnpr_log_level,"Showing list of pool");

bail:
  return err;
}

static int agentd_show_rp_with_name(agentd_context_t *context, void *data,const char* name)
{
  lc_log_debug(jnpr_log_level,"Showing pool : %s",name);

  return cli_nvsd_rm_show_one(context, name);
}

static int agentd_show_rp_global_pool(agentd_context_t *context, void *data)
{
  lc_log_debug(jnpr_log_level,"Showing global pool");

  return cli_nvsd_rm_show_one(context, GLOBAL_RSRC_POOL);
}

int cli_nvsd_rm_show_one(agentd_context_t * context, const char* p_szNzme)
{

    int err = 0;
    const char *rp = NULL;
    bn_binding *binding = NULL;
    char *bn_name = NULL;
    tstr_array *ns_names = NULL;
    node_name_t node_name =
    { 0 };
    tstring *t_bandwidth = NULL;
    uint64_t conf_bw = 0;
    uint64_t used_bw = 0;
    float calc_bw = 0;

    tstring *max_client_sessions = NULL;
    tstring *max_client_bandwidth = NULL;
    tstring *conf_client_sessions = NULL;
    tstring *conf_client_bandwidth = NULL;
    tstring *used_client_sessions = NULL;
    tstring *used_client_bandwidth = NULL;

    rp = p_szNzme;

    bn_name = smprintf("/nkn/nvsd/resource_mgr/config/%s", rp);
    bail_null(bn_name);

    //Check if RP exists..not necessay i think
    err = mdc_get_binding(agentd_mcc, NULL, NULL, false, &binding, bn_name, NULL);
    bail_error(err);

    if ((strcmp(GLOBAL_RSRC_POOL, rp) != 0) && NULL == binding)
    {
        lc_log_debug(jnpr_log_level,"Invalid Resource-Pool : %s\n", rp);
        goto bail;
    }

    err = agentd_mdc_get_value_tstr_fmt(&max_client_bandwidth,"/nkn/nvsd/resource_mgr/monitor/counter/%s/max/max_bandwidth",rp);
    bail_error(err);

    err = agentd_mdc_get_value_tstr_fmt(&max_client_sessions,"/nkn/nvsd/resource_mgr/monitor/counter/%s/max/client_sessions",rp);
    bail_error(err);

    err = agentd_mdc_get_value_tstr_fmt(&used_client_bandwidth,"/nkn/nvsd/resource_mgr/monitor/counter/%s/used/max_bandwidth",rp);
    bail_error(err);

    err = agentd_mdc_get_value_tstr_fmt(&used_client_sessions,"/nkn/nvsd/resource_mgr/monitor/counter/%s/used/client_sessions",rp);
    bail_error(err);

    if (strcmp(rp, GLOBAL_RSRC_POOL))
    {
        err = agentd_mdc_get_value_tstr_fmt(&conf_client_sessions,"/nkn/nvsd/resource_mgr/config/%s/client_sessions",rp);
        bail_error(err);
        err = agentd_mdc_get_value_tstr_fmt(&conf_client_bandwidth,"/nkn/nvsd/resource_mgr/config/%s/max_bandwidth",rp);
        bail_error(err);
    }
    else
    {
        err = agentd_mdc_get_value_tstr_fmt(&conf_client_sessions,"/nkn/nvsd/resource_mgr/monitor/counter/%s/max/client_sessions",rp);
        bail_error(err);

        err = ts_dup(max_client_bandwidth, &conf_client_bandwidth);
        bail_error(err);
    }
    lc_log_debug(jnpr_log_level, "Max     : %s : %s", ts_str(max_client_bandwidth),
            ts_str(max_client_sessions));
    lc_log_debug(jnpr_log_level, "Config : %s : %s", ts_str(conf_client_sessions),
            ts_str(conf_client_bandwidth));
    lc_log_debug(jnpr_log_level, "Used   : %s : %s", ts_str(used_client_bandwidth),
            ts_str(used_client_sessions));

        OP_EMIT_TAG_START(context, ODCI_RESOURCE_POOL_DETAIL);

        OP_EMIT_TAG_VALUE(context, ODCI_POOL_NAME, rp);

        OP_EMIT_TAG_START(context, ODCI_ACTUAL_MAX);
            OP_EMIT_TAG_VALUE(context, ODCI_CLIENT_SESSIONS, ts_str(max_client_sessions) );
            OP_EMIT_TAG_VALUE(context, ODCI_CLIENT_BANDWIDTH, ts_str(max_client_bandwidth) );
        OP_EMIT_TAG_END(context, ODCI_ACTUAL_MAX);

        OP_EMIT_TAG_START(context, ODCI_CONFIGURED_MAX);
            OP_EMIT_TAG_VALUE(context, ODCI_CLIENT_SESSIONS, ts_str(conf_client_sessions) );
            OP_EMIT_TAG_VALUE(context, ODCI_CLIENT_BANDWIDTH, ts_str(conf_client_bandwidth) );
        OP_EMIT_TAG_END(context, ODCI_CONFIGURED_MAX);

        OP_EMIT_TAG_START(context, ODCI_USED);
            OP_EMIT_TAG_VALUE(context, ODCI_CLIENT_SESSIONS, ts_str(used_client_sessions) );
            OP_EMIT_TAG_VALUE(context, ODCI_CLIENT_BANDWIDTH, ts_str(used_client_bandwidth) );
        OP_EMIT_TAG_END(context, ODCI_USED);

        OP_EMIT_TAG_END(context, ODCI_RESOURCE_POOL_DETAIL);

bail:
    ts_free(&max_client_sessions);
    ts_free(&max_client_bandwidth);
    ts_free(&conf_client_sessions);
    ts_free(&conf_client_bandwidth);
    ts_free(&used_client_sessions);
    ts_free(&used_client_bandwidth);

    safe_free(bn_name);
    tstr_array_free(&ns_names);
    bn_binding_free(&binding);
    return err;
}


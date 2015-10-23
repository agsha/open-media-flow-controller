#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"
#include <arpa/inet.h>

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_bgp_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_bgp_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Utility function prototype ----- */
static int
agentd_bgp_chk_prefix_exists (agentd_context_t * context, const char * local_as,
                                const char *input_norm_ip, tbool * exists);

static int normalize_network(const char *prefix_str, const char *mask_len_str,
                      char *norm_prefix, char *norm_mask);

/* ---- Configuration custom handler prototype ---- */ 
static int
agentd_bgp_delete_network (agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

static int
agentd_bgp_set_network (agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

/* ---- Operational command handler prototype and structure definition ---- */ 


/*---------------------------------------------------------------------------*/
int 
agentd_bgp_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    /* Translation table array for configuration commands */
    static  mfc_agentd_nd_trans_tbl_t bgp_cmds_trans_tbl[] =  {
        #include "../translation/rules_bgp.inc"
        #include "../translation/rules_bgp_delete.inc"
        TRANSL_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (bgp_cmds_trans_tbl);
    bail_error (err);


bail:
    return err;
}

int
agentd_bgp_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_bgp_cmds_deinit called");

bail:
    return err;
}

/*
 * Check Sanity of prefix and mask string, and apply mask to prefix.
 * With out these conversion, it can result in multiple network entries
 * in tall maple nodes for the same network.
 *
 * Arguments:
 *      prefix_str:     Original prefix string from CLI
 *      mask_len_str:   Original mask length string from CLI
 *      norm_prefix:    Normalized prefix string, pointer provided by the caller
 *      norm_mask:      Normalized mask string, pointer provided by the caller
 *
 * Return: 0 if success:
 */
static int normalize_network(const char *prefix_str, const char *mask_len_str,
                      char *norm_prefix, char *norm_mask)
{
    struct in_addr prefix_addr;
    long int mask_len;
    uint32_t prefix;
    uint32_t mask = 0xffffffff;
    int err = 0;

    bail_null (prefix_str);
    bail_null (mask_len_str);
    bail_null (norm_prefix);
    bail_null (norm_mask);

    if (inet_pton(AF_INET, prefix_str, &prefix_addr) <= 0) {
        lc_log_debug (jnpr_log_level, "invalid ip address\n");
        err = 1; goto bail;
    }

    prefix = ntohl(prefix_addr.s_addr);

    if (strspn(mask_len_str, "0123456789") != strlen(mask_len_str)) {
        /* mask_len_str contains non digits */
        lc_log_debug (jnpr_log_level, "wrong mask length\n");
        err = 1; goto bail;
    }

    mask_len = strtol(mask_len_str, NULL, 10);
    if (mask_len < 0 || mask_len > 32) {
        lc_log_debug (jnpr_log_level, "wrong mask length\n");
        err = 1; goto bail;
    }

    if (mask_len == 0) {
        mask = 0;
    } else {
        mask = mask << (32 - mask_len);
    }

    prefix = prefix & mask;
    prefix_addr.s_addr = htonl(prefix);
    inet_ntop(AF_INET, &prefix_addr, norm_prefix, sizeof(net_buf_t)-1);
    snprintf(norm_mask, sizeof(net_buf_t)-1, "%ld", mask_len);

bail:
    return err;
}

static int
agentd_bgp_chk_prefix_exists (agentd_context_t * context, const char * local_as,
                                const char *input_norm_ip, tbool * exists)
{
    int err = 0, i = 0;
    temp_buf_t xpath_str = {0};
    xmlXPathObjectPtr xpathObj = NULL;
    xmlNodeSetPtr nodeset = NULL;
    char * ip_mask = NULL, * p =NULL;
    temp_buf_t ip = {0}, mask = {0};
    net_buf_t norm_ip = {0}, norm_mask = {0};

    *exists = false;
    /* Get all bgp network configuration */
    snprintf(xpath_str, sizeof(xpath_str),"//service-elements/router/bgp[local-as=\"%s\"]/network/ipaddress",local_as);

    xpathObj = xmlXPathEvalExpression((const xmlChar *) xpath_str, context->xpathCtx);
    if(xpathObj && !xmlXPathNodeSetIsEmpty(xpathObj->nodesetval)) {
        lc_log_debug (jnpr_log_level, "BGP network configuration exists for local as:%s", local_as);
        /* Normalize each network prefix & check if it same as norm_ip input */
        nodeset = xpathObj->nodesetval;
        for (i=0; i<nodeset->nodeNr; i++){
            ip_mask = (char *) xmlNodeGetContent(nodeset->nodeTab[i]);

            p = strchr(ip_mask,'/');
            bail_null (p);
            strncpy(ip, ip_mask, strlen(ip_mask)-strlen(p));
            snprintf(mask, sizeof(mask), "%s", p+1);

            err = normalize_network (ip, mask, norm_ip, norm_mask);
            lc_log_debug (jnpr_log_level, "Prefix- Before normalizing: %s, After normalizing: %s", ip, norm_ip);
            if (strcmp(input_norm_ip, norm_ip) == 0) {
                lc_log_debug (jnpr_log_level, "Configuration exists for prefix- ip:%s", input_norm_ip);
                *exists = true;
                break;
            }
            xmlFree(ip_mask);
            ip_mask = NULL;
        }
    }

bail:
    xmlXPathFreeObject(xpathObj);
    if (ip_mask) xmlFree(ip_mask);
    return err;
}

static int
agentd_bgp_set_network (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0}, local_as = {0}, ip = {0}, mask = {0}, ip_mask = {0};
    net_buf_t norm_ip = {0}, norm_mask = {0};
    char *p = NULL;

    snprintf(local_as, sizeof(local_as), "%s",  br_cmd->arg_vals.val_args[1]);
    snprintf(ip_mask, sizeof(ip_mask), "%s",  br_cmd->arg_vals.val_args[2]);

    p = strchr(ip_mask,'/');
    bail_null (p);
    strncpy(ip, ip_mask, strlen(ip_mask)-strlen(p));
    snprintf(mask, sizeof(mask), "%s", p+1);

    err = normalize_network (ip, mask, norm_ip, norm_mask);
    if (err != 0) {
        lc_log_basic (LOG_ERR, "Applying mask to ip-prefix failed");
        bail_error (err);
    }
    snprintf(node_name, sizeof(node_name), (char *)cb_arg, local_as, norm_ip, norm_mask);
    err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, bt_string, norm_mask);
    bail_error(err);

bail:
    return err;
}

static int
agentd_bgp_delete_network (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0}, local_as = {0}, ip = {0}, mask = {0}, ip_mask = {0}, norm_ip_mask = {0};
    net_buf_t norm_ip = {0}, norm_mask = {0};
    char *p = NULL;
    tbool exists = false;

    snprintf(local_as, sizeof(local_as), "%s",  br_cmd->arg_vals.val_args[1]);
    snprintf(ip_mask, sizeof(ip_mask), "%s",  br_cmd->arg_vals.val_args[2]);


    p = strchr(ip_mask,'/');
    bail_null (p);
    strncpy(ip, ip_mask, strlen(ip_mask)-strlen(p));
    snprintf(mask, sizeof(mask), "%s", p+1);

    err = normalize_network (ip, mask, norm_ip, norm_mask);
    if (err != 0) {
        lc_log_basic (LOG_ERR, "Applying mask to ip-prefix failed");
        bail_error (err);
    }

    /* Delete mask node */
    snprintf(norm_ip_mask, sizeof(node_name), "%s/%s", norm_ip, norm_mask);
    snprintf(node_name, sizeof(node_name), (char *)cb_arg, local_as, norm_ip_mask);
    err = agentd_append_req_msg (context, node_name, SUBOP_DELETE, bt_string, "");
    bail_error(err);

    /* Delete prefix node if no other mask configuration present  */
    err = agentd_bgp_chk_prefix_exists  (context, local_as, norm_ip, &exists);
    bail_error (err);
    if (!exists) {
        snprintf(node_name, sizeof(node_name), (char *) cb_arg, local_as, norm_ip);
        err = agentd_append_req_msg (context, node_name, SUBOP_DELETE, bt_string, "");
        bail_error (err);
    }
bail:
    return err;
}





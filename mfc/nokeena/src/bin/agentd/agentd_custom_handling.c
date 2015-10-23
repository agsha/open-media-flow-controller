/*
 * Filename :   adnsd_mgmt.c
 * Date:        23 May 2011
 * Author:      Kiran Desai
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */


#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include "agentd_mgmt.h"
#include "nkn_cfg_params.h"
#include "nkn_mgmt_defs.h"
#include <glib.h>
#include "tcrypt.h"

extern md_client_context * agentd_mcc ;
extern int jnpr_log_level;
/*
    1050 Config request Failed
    1051 Unable to find Translational entry
    1052 lookup failed for operational command
*/

extern  agentd_array_move_context cache_age_content_type;  // Context for cache-age content-type headers
extern  agentd_array_move_context http_cl_resp_add_hdr;  //Context for client-reponse add headers
extern  agentd_array_move_context http_cl_resp_del_hdr;  //Context for client-reponse add headers
extern  agentd_array_move_context http_org_req_add_hdr;  //Context for origin-request add headers
extern  agentd_array_move_context bond_interface;  //Context for bonded interfaces
extern  agentd_array_move_context cache_index_hdr; // Context for cache-index include headers
extern  agentd_array_move_context ipv6_ctxt; //Context for interface ipv6 address

static int
agentd_ns_match_binding(const bn_binding_array *bindings,
    uint32 idx, const bn_binding *binding,const tstring *name,
    const tstr_array *name_components, const tstring *name_last_part,
    const tstring *value, void *cb_data);

static int agentd_save_post_command(agentd_context_t *context,
    agentd_binding *abinding);

static int normalize_network(const char *prefix_str, const char *mask_len_str,
                      char *norm_prefix, char *norm_mask);

/*----------------------------------------------------------------------------*/

int ns_del_proto_client_rsp_del_hdr(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
	char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    uint32_array *header_indices = NULL;
    int32 db_rev = 0;
    uint32 num_headers = 0;

    char *nd_detail = (char *)cb_arg;

    //args[1] has the namespace name
    snprintf(node_name, sizeof(node_name), nd_detail,  br_cmd->arg_vals.val_args[1]);
    /* Check if the type already exists.. If it does, then
     * we need to update the value,
     * If it doesnt exist, then create it and then set the
     * value*/
    /* Get all matching indices */
    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
	    node_name, NULL, "name",br_cmd->arg_vals.val_args[2],7,
	    bn_db_revision_id_none,
	    NULL, &header_indices, NULL, NULL);
    bail_error(err);

    num_headers = uint32_array_length_quick(header_indices);
    if ( num_headers == 0 ) {
	/*New header*/
	err = bn_binding_array_new(&barr);
	bail_error(err);
	err = bn_binding_new_str_autoinval(&binding, "name",
	    ba_value, bt_string, 0,
	    br_cmd->arg_vals.val_args[2]);
	bail_error_null(err, binding);
	err = bn_binding_array_append(barr, binding);
	bail_error(err);
	err = agentd_array_append(agentd_mcc, &http_cl_resp_del_hdr, context->mod_bindings,
	    node_name, barr, &code, NULL);
	bail_error(err);
    } else {
	// node is already available. ignore.
	err = 0;
	return err;
    }
bail:
    uint32_array_free (&header_indices);
    bn_binding_array_free (&barr);
    bn_binding_free (&binding);
    return err;
}

int
ns_prestage_ftp_password(agentd_context_t *context, agentd_binding *binding, void *cb_arg,
    char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0}, passwd = {0}, bn_name = {0}, ns_name = {0};
    char *crypt_passwd = NULL;
    tbool valid = false;

    bail_null(context);

    snprintf(ns_name, sizeof(ns_name), "%s", binding->arg_vals.val_args[1]);
    snprintf(bn_name, sizeof(bn_name), "/nkn/nvsd/namespace/%s",ns_name);

    snprintf(node_name, sizeof(node_name),
        "%s/prestage/user/%s/password",
        bn_name, binding->arg_vals.val_args[2]);

    err = agentd_append_req_msg(context, node_name,
        SUBOP_MODIFY, TYPE_STR, binding->arg_vals.val_args[2]);
    bail_error(err);

    snprintf(passwd, sizeof(passwd),"%s",binding->arg_vals.val_args[3]);

    err = ltc_encrypt_password(passwd, lpa_default, &crypt_passwd);
    bail_error_null(err, crypt_passwd);

    err = agentd_append_req_msg(context,node_name,
        SUBOP_MODIFY, TYPE_STR, crypt_passwd);
    bail_error(err);

bail:
    safe_free(crypt_passwd);
    return err;

}
int
agentd_append_req_msg(agentd_context_t *context,const char *node,
	subop_t subop, nd_dt_type_t type, const char *value)
{
    int err = 0;
    bn_binding *binding = NULL;
    bn_binding_array *barr= NULL;

    bail_null(context);
    switch (subop) {
    case SUBOP_RESET:
        barr = context->reset_bindings;
        break;
    case SUBOP_MODIFY:
        barr = context->mod_bindings;
        break;
    case SUBOP_DELETE:
        barr = context->del_bindings;
        break;
    default:
        lc_log_debug (LOG_ERR, "Invalid subop");
        goto bail;
    }

    err = bn_binding_new_str_autoinval(&binding, node, ba_value,
	    type, 0, value);
    bail_error_null(err, binding);

    err = bn_binding_array_append_takeover (barr, &binding);
    bail_error(err);

bail:
    return err;
}

int
agentd_custom_entry_ignore(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
	char **cli_buff)
{
	int err = 0;

	return err;
}


int ns_org_http_ser_map(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
	 char **cli_buff)
{
	return 0;
}


int ns_del_proto_cache_age_content_type(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
	char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    node_name_t node_pat = {0};
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    uint32_array *cache_age_indices = NULL;
    int32 db_rev = 0;
    uint32 num_cache_ages = 0;
    char *nd_detail = (char *)cb_arg;
    int32 content_type_any=0;
    agentd_value_t type_name,type_value;

    //args[1] has the namespace name
    snprintf(node_name, sizeof(node_name), nd_detail,  br_cmd->arg_vals.val_args[1]);

    if ( strstr(br_cmd->pattern,"content-type-any") ){
        content_type_any = 1;
        strcpy(type_name,"");
        strcpy(type_value,br_cmd->arg_vals.val_args[2]);
    }
    else {
        strcpy(type_name,br_cmd->arg_vals.val_args[2]);
        strcpy(type_value,br_cmd->arg_vals.val_args[3]);
    }


    /* Check if the type already exists.. If it does, then
     *  we need to update the value,
     *  If it doesnt exist, then create it and then set the
     *  value
     */
    /* Get all matching indices */

    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
            node_name, NULL, "type", type_name,7,
            bn_db_revision_id_none,
            NULL, &cache_age_indices, NULL, NULL);
    bail_error(err);

    num_cache_ages = uint32_array_length_quick(cache_age_indices);

    if(num_cache_ages == 0){
	err = bn_binding_array_new(&barr);
	bail_error(err);
	err = bn_binding_new_str_autoinval(&binding, "type",
	    ba_value, bt_string, 0,
	    type_name);
	bail_error_null(err, binding);
	err = bn_binding_array_append(barr, binding);
	bail_error(err);
        bn_binding_free (&binding);
	// Set timeout
        err = bn_binding_new_str_autoinval(&binding, "value",
	    ba_value, bt_uint32, 0,
	    type_value);
	bail_error_null(err, binding);
	err = bn_binding_array_append(barr, binding);
	bail_error(err);
        bn_binding_free (&binding);
	err = agentd_array_append(agentd_mcc, &cache_age_content_type, context->mod_bindings,
	    node_name, barr, &code, NULL);
	bail_error(err);
    }
    else {
        snprintf(node_pat, sizeof(node_pat), "%s/%d/value", node_name, uint32_array_get_quick(cache_age_indices,0));
        err = agentd_append_req_msg (context, node_pat, SUBOP_MODIFY, bt_uint32, type_value);
        bail_error(err);
    }
bail:
    uint32_array_free (&cache_age_indices);
    bn_binding_array_free (&barr);
    bn_binding_free (&binding);
    return err;
}


int ns_del_proto_client_rsp_add_hdr(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
	char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    node_name_t node_pat = {0};
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    uint32_array *header_indices = NULL;
    uint32 num_headers = 0;
    char *nd_detail = (char *)cb_arg;
    agentd_array_move_context * ctx = NULL;

    //args[1] has the namespace name
    snprintf(node_name, sizeof(node_name), nd_detail,  br_cmd->arg_vals.val_args[1]);

    if (strstr (node_name, "origin-request/header/add") ) {
        ctx = &http_org_req_add_hdr;
    } else { /* client-response/header/add */
        ctx =  &http_cl_resp_add_hdr; 
    }
    /* Check if the type already exists.. If it does, then
     *  we need to update the value,
     *  If it doesnt exist, then create it and then set the
     *  value
     */

    /* Get all matching indices */
    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
	    node_name, NULL, "name",br_cmd->arg_vals.val_args[2],7,
	    bn_db_revision_id_none,
	    NULL, &header_indices, NULL, NULL);
    bail_error(err);


    num_headers = uint32_array_length_quick(header_indices);
    if ( num_headers == 0 ) {
	err = bn_binding_array_new(&barr);
	bail_error(err);
	err = bn_binding_new_str_autoinval(&binding, "name",
	    ba_value, bt_string, 0,
	    br_cmd->arg_vals.val_args[2]);
	bail_error_null(err, binding);
	err = bn_binding_array_append(barr, binding);
	bail_error(err);
        bn_binding_free (&binding);
	err = bn_binding_new_str_autoinval(&binding, "value",
	    ba_value, bt_string, 0,
	    br_cmd->arg_vals.val_args[3]);
	bail_error_null(err, binding);
	err = bn_binding_array_append(barr, binding);
	bail_error(err);
        bn_binding_free (&binding);
	err = agentd_array_append(agentd_mcc, ctx, context->mod_bindings,
	    node_name, barr, &code, NULL);
	bail_error(err);
    }
    else {
	//val_args[3] gives the header value.
        snprintf(node_pat, sizeof(node_pat), "%s/%d/value", node_name, uint32_array_get_quick(header_indices,0));
        err = agentd_append_req_msg (context, node_pat, SUBOP_MODIFY, bt_string, br_cmd->arg_vals.val_args[3]);
	bail_error(err);
    }
bail:
    uint32_array_free (&header_indices);
    bn_binding_array_free (&barr);
    bn_binding_free (&binding);
	return err;

}
#if  0
int ns_del_proto_origin_req_add_hdr(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
	char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    node_name_t node_pat = {0};
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    uint32_array *header_indices = NULL;
    uint32 num_headers = 0;
    char *nd_detail = (char *)cb_arg;

    //args[1] has the namespace name
    snprintf(node_name, sizeof(node_name), nd_detail,  br_cmd->arg_vals.val_args[1]);

    /* Check if the type already exists.. If it does, then
     *  we need to update the value,
     *  If it doesnt exist, then create it and then set the
     *  value
     */
    err = bn_binding_array_new(&barr);
    bail_error(err);


    /* Get all matching indices */
    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
	    node_name, NULL, "name",br_cmd->arg_vals.val_args[2],7,
	    bn_db_revision_id_none,
	    NULL, &header_indices, NULL, NULL);
    bail_error(err);


    num_headers = uint32_array_length_quick(header_indices);
    if ( num_headers == 0 ) {
	err = bn_binding_array_new(&barr);
	bail_error(err);
	err = bn_binding_new_str_autoinval(&binding, "name",
	    ba_value, bt_string, 0,
	    br_cmd->arg_vals.val_args[2]);
	bail_error_null(err, binding);
	err = bn_binding_array_append(barr, binding);
	bail_error(err);
	err = bn_binding_new_str_autoinval(&binding, "value",
	    ba_value, bt_string, 0,
	    br_cmd->arg_vals.val_args[3]);
	bail_error_null(err, binding);
	err = bn_binding_array_append(barr, binding);
	bail_error(err);
	err = agentd_array_append(agentd_mcc, &http_org_req_add_hdr, context->mod_bindings,
	    node_name, barr, &code, NULL);
	bail_error(err);
    }
    else {
	//val_args[3] gives the header value.
        snprintf(node_pat, sizeof(node_pat), "%s/%d/value", node_name, uint32_array_get_quick(header_indices, 0));
        err = agentd_append_req_msg (context, node_pat, SUBOP_MODIFY, bt_string,  br_cmd->arg_vals.val_args[3]);
	bail_error(err);
    }
bail:
	return err;
}
#endif

/* This is similar to mdc_array_get_matching_indices(), except
 * that this function does a case sensitive match for the
 * name that we want.
 */

int
agentd_ns_mdc_array_get_matching_indices(
		md_client_context *mcc,
		const char *root_name,
		const char *db_name,
		const char *bn_name,
		const char *value,
                uint32 node_index,
		//const bn_binding_array *children,
		int32 rev_before,
		int32 *ret_rev_after,
		uint32_array **ret_indices,
		uint32 *ret_code,
		tstring **ret_msg)
{
	int err = 0;
	char *pattern = NULL;
	uint32 code = 0;
	bn_binding_array *bindings = NULL;
	ns_header_match_context cb_data;
	unsigned int num_matches = 0;

	bail_null(root_name);
	bail_null(value);
	bail_null(ret_indices);


	err = uint32_array_new(ret_indices);
	bail_error(err);

	cb_data.child_name = value;
   	cb_data.binding_name = bn_name;
	cb_data.indices  = *ret_indices;
    	cb_data.node_idx = node_index;

	pattern = smprintf("%s/*", root_name);
	bail_null(pattern);

	err = mdc_get_binding_children(
			agentd_mcc,
			&code,
			NULL,
			true,
			&bindings,
			true,
			true,
			root_name);
	if (code)
		goto bail;

	safe_free(pattern);

	pattern = smprintf("%s/*/%s", root_name, bn_name);
	err = mdc_foreach_binding_prequeried(bindings, pattern, NULL,
			agentd_ns_match_binding,
			(void *) &cb_data,
			&num_matches);
	bail_error(err);

bail:
	bn_binding_array_free(&bindings);
	safe_free(pattern);
	return err;
}

/*---------------------------------------------------------------------------*/
static int
agentd_ns_match_binding(
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
	uint32 int_index = 0;
	const char *node_idx = NULL;
	ns_header_match_context *ctxt = (ns_header_match_context *) cb_data;



	if (ts_equal_str(name_last_part, ctxt->binding_name, false)) {
		if (ts_equal_str(value, ctxt->child_name, true)) {
			node_idx = tstr_array_get_str_quick(name_components, ctxt->node_idx);
			int_index = strtoul(node_idx, NULL, 10);
			err = uint32_array_append(ctxt->indices, int_index);
			bail_error(err);
		}
	}

bail:
	return err;
}

int ns_del_proto_cache_age_rm_content_type(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
	char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    uint32 code = 0;
    uint32_array *cache_age_indices = NULL;
    uint32 num_cache_ages = 0;
    char *nd_detail = (char *)cb_arg;
    int32 content_type_any=0;
    agentd_value_t type_name,type_value;

    //args[1] has the namespace name
    snprintf(node_name, sizeof(node_name), nd_detail,  br_cmd->arg_vals.val_args[1]);

    if ( strstr(br_cmd->pattern,"content-type-any") ){
        content_type_any = 1;
        strcpy(type_name,"");
        strcpy(type_value,br_cmd->arg_vals.val_args[2]);
    }
    else {
        strcpy(type_name,br_cmd->arg_vals.val_args[2]);
        strcpy(type_value,br_cmd->arg_vals.val_args[3]);
    }
    /* Check if the type already exists.. If it does, then
     *  we need to update the value,
     *  If it doesnt exist, then create it and then set the
     *  value
     */
    /* Get all matching indices */

    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
            node_name, NULL, "type", type_name,7,
            bn_db_revision_id_none,
            NULL, &cache_age_indices, NULL, NULL);
    bail_error(err);

    num_cache_ages = uint32_array_length_quick(cache_age_indices);

    if ( num_cache_ages == 0 ) {
	/*Ignore the handling*/

    }
    else {
	node_name_t wc = {0};
	snprintf(wc, sizeof(wc), "%s/%d", node_name, uint32_array_get_quick(cache_age_indices, 0));
        err = agentd_append_req_msg (context, wc, SUBOP_DELETE, bt_string, "");
	bail_error(err);
    }

bail:
    uint32_array_free(&cache_age_indices);
    return err;
}

int ns_del_proto_rm_header(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
	char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    uint32 code = 0;
    uint32_array *cache_age_indices = NULL;
    uint32 num_cache_ages = 0;
    char *nd_detail = (char *)cb_arg;

    //args[1] has the namespace name
    //args[2] has the  header name
    snprintf(node_name, sizeof(node_name), nd_detail,  br_cmd->arg_vals.val_args[1]);

    /* Check if the type already exists.. If it does, then
     *  we need to update the value,
     *  If it doesnt exist, then create it and then set the
     *  value
     */
    /* Get all matching indices */

    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
            node_name, NULL, "name", br_cmd->arg_vals.val_args[2],7,
            bn_db_revision_id_none,
            NULL, &cache_age_indices, NULL, NULL);
    bail_error(err);

    num_cache_ages = uint32_array_length_quick(cache_age_indices);

    if ( num_cache_ages == 0 ) {
	/*Ignore the handling*/

    }
    else {
	node_name_t wc = {0};
	snprintf(wc, sizeof(wc), "%s/%d", node_name, uint32_array_get_quick(cache_age_indices, 0));
        err = agentd_append_req_msg (context, wc, SUBOP_DELETE, bt_string, "");
	bail_error(err);
    }

bail:
    uint32_array_free (&cache_age_indices);
    return err;
}

int
agentd_interface_ip_addr(agentd_context_t *context, 
	agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t intf = {0}, ip = {0}, mask = {0}, ipaddr = {0}, alias_ind = {0}, intf_name = {0};
    tbool is_alias = false;
    char *p = NULL;
    node_name_t node_name = {0}, alias_dev = {0};

    snprintf(intf, sizeof(intf), "%s",  br_cmd->arg_vals.val_args[2]);

    if ( strstr(br_cmd->pattern,"alias") ){
        snprintf(alias_ind, sizeof(alias_ind), "%s",  br_cmd->arg_vals.val_args[3]);
        snprintf(ipaddr, sizeof(ipaddr), "%s",  br_cmd->arg_vals.val_args[4]);
        snprintf(alias_dev, sizeof(alias_dev), "%s", intf);
        is_alias = true;
    }
    else {
        snprintf(ipaddr, sizeof(ipaddr), "%s",  br_cmd->arg_vals.val_args[3]);
    }

    p = strchr(ipaddr,'/');
    strncpy(ip,ipaddr,strlen(ipaddr)-strlen(p));
    snprintf(mask, sizeof(mask),"%s",p+1);

    if (is_alias)
	snprintf(intf_name, sizeof(intf_name), "%s:%s", intf, alias_ind);
    else
	snprintf(intf_name, sizeof(intf_name), "%s", intf);

    snprintf (node_name, sizeof(node_name), "/net/interface/config/%s/addr/ipv4/static/1/ip", intf_name);
    err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, bt_ipv4addr, ip);
    bail_error (err);

    snprintf (node_name, sizeof(node_name), "/net/interface/config/%s/addr/ipv4/static/1/mask_len", intf_name);
    err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, bt_uint8, mask);
    bail_error(err);

    snprintf(node_name, sizeof(node_name), "/net/interface/config/%s/alias/ifdevname", intf_name);
    err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, bt_string, alias_dev);
    bail_error(err);

    snprintf(node_name, sizeof(node_name), "/net/interface/config/%s/alias/index", intf_name);
    err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, bt_string, alias_ind);
    bail_error(err);


bail:
    return err;
}

int
agentd_configure_interface(agentd_context_t *context, 
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    int is_bond = 0;
    node_name_t node_name = {0};
    str_value_t intf_name = {0};
    str_value_t mem_node_type = {0};
    node_name_t pattern = {0};
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    uint32_array *bond_indices = NULL;
    uint32 num_bonds = 0;

    snprintf(intf_name, sizeof(intf_name), "%s",  br_cmd->arg_vals.val_args[2]);

    /*Constructing the query*/
    if (strstr(br_cmd->pattern, "vxa-standalone")) {
        snprintf(mem_node_type, sizeof(mem_node_type), "vxa-standalone");
    } else {
        snprintf(mem_node_type, sizeof(mem_node_type), "blade");
    }

    snprintf(pattern, sizeof(pattern), "//member-node[name=\"%s\"]/type/%s/interface[name=\"%s\"][type=\"bond\"]/type",
            br_cmd->arg_vals.val_args[1],mem_node_type, intf_name);

    err = agentd_xml_node_exists(context, pattern, &is_bond);
    if (is_bond) {
        /* Bonding interface. Add node under /net/bonding/config hierarchy */
        snprintf(node_name, sizeof(node_name), "%s", "/net/bonding/config");
        err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
            node_name, NULL, "name", intf_name,3,
            bn_db_revision_id_none,
            NULL, &bond_indices, NULL, NULL);
        bail_error(err);

        num_bonds = uint32_array_length_quick(bond_indices);

        if(num_bonds == 0) {
            err = bn_binding_array_new(&barr);
            bail_error(err);
            err = bn_binding_new_str_autoinval(&binding, "name",
                ba_value, bt_string, 0,
                intf_name);
            bail_error_null(err, binding);

            err = bn_binding_array_append(barr, binding);
            bail_error(err);

            err = agentd_array_append(agentd_mcc, &bond_interface, context->mod_bindings, 
            node_name, barr, &code, NULL);
            bail_error(err);
        }
        else {
            /* Bond interface already exists. Just ignore */
        }
    }
    /* Add node under /net/interface/config hierarchy for any type of interface */
    snprintf(node_name, sizeof(node_name), "/net/interface/config/%s", intf_name);
    err = agentd_append_req_msg(context, node_name, SUBOP_MODIFY, TYPE_STR,intf_name);
    bail_error(err);

bail:
    bn_binding_array_free(&barr);
    bn_binding_free(&binding);
    uint32_array_free(&bond_indices);
    return err;
}

int
agentd_delete_interface(agentd_context_t *context, 
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0}, intf_name = {0};
    uint32_array *bond_indices = NULL;
    uint32 num_bonds = 0;

    //args[2] has the interface name
    snprintf(intf_name, sizeof(intf_name), "%s",  br_cmd->arg_vals.val_args[2]);

    /* MFC does not allow deletion of loopback interface configuration.
       So, ignore handling, if loopback interface */
    if (!strcmp (intf_name, "lo")) goto bail;

    /* Check if it is a bonding interfae */
    snprintf(node_name, sizeof(node_name), "%s", "/net/bonding/config");
    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
            node_name, NULL, "name", intf_name,3,
            bn_db_revision_id_none,
            NULL, &bond_indices, NULL, NULL);
    bail_error(err);

    num_bonds = uint32_array_length_quick(bond_indices);

    if(num_bonds){
        node_name_t node_to_delete = {0};
        snprintf(node_to_delete, sizeof(node_to_delete),"%s/%d",
                             node_name, uint32_array_get_quick(bond_indices,0));
        err = agentd_append_req_msg(context, node_to_delete, SUBOP_DELETE, TYPE_STR, "");
        bail_error(err);
    }
    /* Delete the node from /net/interface/config hierarchy for any type of interface */
    snprintf(node_name, sizeof(node_name), "/net/interface/config/%s", intf_name);
    err = agentd_append_req_msg(context, node_name, SUBOP_DELETE, TYPE_STR, "");
    bail_error(err);

bail:
    uint32_array_free(&bond_indices);
    return err;
}

int
agentd_bond_interface_params(agentd_context_t *context, 
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0}, param_name = {0}, param_value = {0}, intf_name = {0},bond = {0};
    node_name_t node_pat = {0};
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    uint32_array *bond_indices = NULL;
    uint32 num_bonds = 0;
    char *param = (char *)cb_arg;
    bn_type param_type = bt_NONE;

    snprintf(node_name, sizeof(node_name), "%s", "/net/bonding/config");

    /* args[2] contains the bond name and args[3] contains the parameter value */
    snprintf(bond, sizeof(bond), "%s",  br_cmd->arg_vals.val_args[2]);
    snprintf(param_value, sizeof(param_value), "%s",  br_cmd->arg_vals.val_args[3]);

    if( strstr(br_cmd->pattern,"bonded-interface")) {
    	snprintf(param_name, sizeof(param_name),"%s/%s", param,  br_cmd->arg_vals.val_args[3]);
        param_type = bt_string;
    }
    else {
   	 //param contains the parameter name .i.e. mode/link-mon-time/down-delay-time/up-delay-time
    	snprintf(param_name, sizeof(param_name),"%s", param);
    	if ( ! strcmp( param_name, "mode" ))
	    param_type = bt_string;
    	else
	    param_type = bt_duration_ms;
    }
    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
            node_name, NULL, "name", bond,3,
            bn_db_revision_id_none,
            NULL, &bond_indices, NULL, NULL);
    bail_error(err);

    num_bonds = uint32_array_length_quick(bond_indices);

    if(num_bonds == 0){
        err = bn_binding_array_new(&barr);
        bail_error(err);

	err = bn_binding_new_str_autoinval(&binding, param_name,
        	 ba_value, param_type, 0,
           	 param_value);
        bail_error_null(err, binding);

        err = bn_binding_array_append(barr, binding);
        bail_error(err);

        err = agentd_array_set(agentd_mcc, &bond_interface, context->mod_bindings,
            node_name, barr, &code, NULL);
        bail_error(err);
    }
    else {
        snprintf(node_pat, sizeof(node_pat), "%s/%d/%s", node_name,  uint32_array_get_quick(bond_indices, 0), param_name);
        err = agentd_append_req_msg (context, node_pat, SUBOP_MODIFY, param_type, param_value);
        bail_error(err);
    }

bail:
    bn_binding_array_free(&barr);
    bn_binding_free(&binding);
    uint32_array_free(&bond_indices);
    return err;
}

int
agentd_delete_bonded_interface(agentd_context_t *context, 
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0}, bond = {0}, intf_name = {0};
    uint32_array *bond_indices = NULL;
    uint32 num_bonds = 0;
    node_name_t node_to_delete = {0};

    snprintf(node_name, sizeof(node_name), "%s", "/net/bonding/config");
    //args[2] has the bond name
    snprintf(bond, sizeof(bond), "%s",  br_cmd->arg_vals.val_args[2]);
    // args[3] contains the interface name
    snprintf(intf_name, sizeof(intf_name), "%s", br_cmd->arg_vals.val_args[3]);

    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
            node_name, NULL, "name", bond,3,
            bn_db_revision_id_none,
            NULL, &bond_indices, NULL, NULL);
    bail_error(err);
    num_bonds = uint32_array_length_quick(bond_indices);
    if(num_bonds == 0){
	/* Ignore the handling */
    }
    else {
        snprintf(node_to_delete, sizeof(node_to_delete),"%s/%d/interface/%s",
                             node_name, uint32_array_get_quick(bond_indices,0),intf_name);
        err = agentd_append_req_msg (context, node_to_delete, SUBOP_DELETE, bt_string, "");
        bail_error(err);
    }

bail:
    uint32_array_free(&bond_indices);
    return err;
}

int
agentd_delete_bond_interface_params(agentd_context_t *context, 
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_to_reset= {0};
    str_value_t bond = {0};
    uint32_array *bond_indices = NULL;
    uint32 num_bonds = 0;
    char *nd_detail = (char *)cb_arg;


    //args[2] has the bond name
    snprintf(bond, sizeof(bond), "%s",  br_cmd->arg_vals.val_args[2]);

    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
            "/net/bonding/config", NULL, "name", bond,3,
            bn_db_revision_id_none,
            NULL, &bond_indices, NULL, NULL);
    bail_error(err);

    num_bonds = uint32_array_length_quick(bond_indices);

    if(num_bonds == 0){
	/* This should not happen */
        /* Ignore the handling */
    } else {
        snprintf(node_to_reset, sizeof(node_to_reset), nd_detail, uint32_array_get_quick(bond_indices,0));
        lc_log_debug (jnpr_log_level, "Node to reset: %s", node_to_reset);
        err = agentd_append_req_msg(context, node_to_reset, bsso_reset, bt_duration_ms, "");
        bail_error(err);
    }

bail:
    uint32_array_free(&bond_indices);
    return err;
}

int
agentd_del_alias(agentd_context_t *context, 
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0}, intf = {0}, ali_ind = {0}, alias = {0};

    snprintf(intf, sizeof(intf), "%s", br_cmd->arg_vals.val_args[2]);
    snprintf(ali_ind, sizeof(ali_ind), "%s", br_cmd->arg_vals.val_args[3]);
    snprintf(alias, sizeof(alias), "%s:%s", intf,ali_ind);

    snprintf(node_name, sizeof(node_name), "%s/%s", "/net/interface/config",alias);

    err = agentd_append_req_msg(context, node_name,
                   bsso_delete, TYPE_STR, "");
    bail_error(err);

bail:
    return err;
}

int
agentd_namespace_delete(agentd_context_t *context, 
	agentd_binding *abinding, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    int bound = 0;
    tbool ns_active = false;
    /*  br_cmd->arg_vals.val_args[0] is mfc-cluster "mfc" */
    const char *ns_name = abinding->arg_vals.val_args[1];
    tstring *t_rp_name = NULL;

    bail_null(ns_name);
    lc_log_debug(jnpr_log_level, "deleting namespace %s", ns_name);
 
    err = mdc_get_binding_tstr_fmt(agentd_mcc,
		    NULL, NULL, NULL,
		    &t_rp_name,
		    NULL,
		    "/nkn/nvsd/namespace/%s/resource_pool",
		     ns_name);
    bail_error(err);

    if ((t_rp_name == NULL) || (0 == strcmp(ts_str(t_rp_name), ""))) {
	/* do noting for now */
        lc_log_basic(LOG_INFO, "namespace %s already "
 		"deleted OR RP is empty", ns_name);
	goto bail;
    }
    /* delete namepsace now */

    snprintf(node_name, sizeof(node_name),
             "/nkn/nvsd/resource_mgr/config/%s/namespace/%s",
             ts_str(t_rp_name), ns_name);
    err = agentd_append_req_msg(context, node_name,
             bsso_delete, TYPE_STR, ns_name);
    bail_error(err);

    snprintf(node_name, sizeof(node_name),
             "/nkn/nvsd/namespace/%s/resource_pool", ns_name);
    err = agentd_append_req_msg(context, node_name,
             bsso_reset, TYPE_STR, ns_name);
    bail_error(err);

    snprintf(node_name, sizeof(node_name),
	"/nkn/nvsd/namespace/%s", ns_name);
    err = agentd_append_req_msg(context, node_name,
		bsso_delete, TYPE_STR, ns_name);
    bail_error(err);
      

bail:
    ts_free(&t_rp_name);
    return err;
}

static int
agentd_save_post_command(agentd_context_t *context,
	agentd_binding *abinding)
{
    int err = 0;
    /* check if br_cmd is NULL */
    if (abinding == NULL) {
	return 0;
    }

    /* check if we are full */
    if (context->num_post_cmds >= AGENTD_MAX_POST_CMDS) {
	return ENOMEM;
    }

    context->post_cmds[context->num_post_cmds++] = abinding;

bail:
    return err;
}

#if 0
int
agentd_save_pre_command(agentd_context_t *context,
	agentd_binding *abinding)
{
    int err = 0;
    /* check if bidning is NULL */
    if (abinding == NULL) {
	return 0;
    }

    /* check if we are full */
    if (context->num_pre_cmds >= AGENTD_MAX_PRE_CMDS) {
	return ENOMEM;
    }

    context->pre_cmds[context->num_pre_cmds++] = abinding;

bail:
    return err;
}
#endif

int agentd_convert_junos_time_fmt (const char *junos_time, str_value_t * out_tm_date_time) {
/* This function converts junos time format (YYYY-MM-DD.HH:MM::SS) to
   TM cli date time format (YYYY/MM/DD HH:MM:SS)
*/
    int err = 0;
    int year = 0, month = 0, day = 0, hr = 0, min = 0, sec = 0;

    bail_null (out_tm_date_time);
    bail_null (junos_time);

    sscanf (junos_time, "%d-%d-%d.%d:%d:%d", &year, &month, &day, &hr, &min, &sec);
    snprintf(*out_tm_date_time, sizeof(str_value_t), "%04d/%02d/%02d %02d:%02d:%02d",
                                                        year, month, day, hr, min, sec);
    lc_log_debug (jnpr_log_level, "JunOS Date: %s, TM Date: %s",
                         junos_time, *out_tm_date_time);

bail:
    return err;
}

int
agentd_xml_node_exists(agentd_context_t *context,const char *pattern, int *ispresent)
{
    int err = 0;
    xmlXPathObjectPtr xpathObj;

    xpathObj = xmlXPathEvalExpression((const xmlChar *) pattern, context->xpathCtx);
    if(!xpathObj) {
	*ispresent = 0; // The data is not present.
    }else if (xpathObj->nodesetval == 0) {
	*ispresent = 0; // the object is present.
    }else {
	*ispresent = 1; // the object is present.
    }

bail:
    if(xpathObj) {
	xmlXPathFreeObject(xpathObj);
    }
    return err;
}

int
agentd_handle_querystr_cache_reset (agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    int present = 0;
    temp_buf_t  pattern = {0}; // passed as the xml path for  obtaining data
    node_name_t node = {0};

    /*Get the namespace name*/
    const char *ns_name = br_cmd->arg_vals.val_args[1];

    /*Constructing the query*/
    snprintf(pattern, sizeof(pattern), "//service/http/instance[name=\"%s\"]/client-request/action/query-string/cache",
            ns_name);

    err = agentd_xml_node_exists(context, pattern, &present);

    if(present) { // The delete is only for exclude-query-string option
        lc_log_debug (jnpr_log_level, "Delete is for exclude-query-string only");
        snprintf(node, sizeof(node), "/nkn/nvsd/origin_fetch/config/%s/query/strip", ns_name);
        err = agentd_append_req_msg(context, node,
                    SUBOP_RESET, TYPE_BOOL, "false");
            bail_error(err);
    }else { // The delete is for cache and exclude-query-string
        lc_log_debug (jnpr_log_level, "Delete is for both cache & exclude-query-string");
        snprintf(node, sizeof(node), "/nkn/nvsd/origin_fetch/config/%s/query/strip", ns_name);
        err = agentd_append_req_msg(context, node,
                SUBOP_RESET, TYPE_BOOL, "false");
        bail_error(err);
        snprintf(node, sizeof(node), "/nkn/nvsd/origin_fetch/config/%s/query/cache/enable", ns_name);
        err = agentd_append_req_msg(context, node,
                SUBOP_RESET, TYPE_BOOL, "false");
        bail_error(err);
    }

bail:
    return err;
}

int
agentd_ns_cache_index_header_add (agentd_context_t * context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    temp_buf_t ns_name = {0};
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;

    /* args[1] has the namespace name and args[2] has the header name*/
    snprintf(ns_name, sizeof(ns_name), "%s", br_cmd->arg_vals.val_args[1]);
    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/cache-index/include-headers", ns_name);

    err = bn_binding_array_new(&barr);
    bail_error_null(err, barr);

    err = bn_binding_new_str_autoinval(&binding, "header-name", ba_value, bt_string, 0,
                br_cmd->arg_vals.val_args[2]);
    bail_error_null(err, binding);

    err = bn_binding_array_append(barr, binding);
    bail_error(err);

    err = agentd_array_append(agentd_mcc, &cache_index_hdr, context->mod_bindings,
            node_name, barr, &code, NULL);
    bail_error(err);

    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/cache-index/include-header", ns_name);
    err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, bt_bool, "true");
    bail_error(err);

bail:
    bn_binding_array_free(&barr);
    bn_binding_free(&binding);
    return err;
}

int
agentd_ns_cache_index_header_delete (agentd_context_t * context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    temp_buf_t ns_name = {0}, header = {0}, pattern = {0};
    uint32_array *hdr_indices = NULL;
    uint32 num_hdrs = 0;
    node_name_t node_to_delete = {0};
    int is_incl_hdr = 0;

    //args[1] has the namespace name
    snprintf(ns_name, sizeof(ns_name), "%s",  br_cmd->arg_vals.val_args[1]);
    // args[2] contains the header
    snprintf(header, sizeof(header), "%s", br_cmd->arg_vals.val_args[2]);
    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/cache-index/include-headers", ns_name);

    /* Get the index of the particular header */
    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
                        node_name, NULL, "header-name", header, 7,
                        bn_db_revision_id_none,
                        NULL, &hdr_indices, NULL, NULL);
    bail_error_null(err, hdr_indices);
    num_hdrs = uint32_array_length_quick(hdr_indices);
    if(num_hdrs == 0){
        /* Ignore. Cannot happen */
    } else {
        snprintf(node_to_delete, sizeof(node_to_delete),"%s/%d",
                    node_name, uint32_array_get_quick(hdr_indices,0));
        err = agentd_append_req_msg (context, node_to_delete, SUBOP_DELETE, bt_string, "");
        bail_error(err);
    }

    /* Reset include-header node if the configuration does not exist */
    snprintf(pattern, sizeof(pattern), "//service/http/instance[name=\"%s\"]/cache-index/header-include",
                           ns_name);
    err = agentd_xml_node_exists(context, pattern, &is_incl_hdr);
    if (!is_incl_hdr) {
        snprintf(node_name, sizeof(node_name), "/nkn/nvsd/origin_fetch/config/%s/cache-index/include-header", ns_name);
        err = agentd_append_req_msg (context, node_name, SUBOP_RESET, bt_bool, "false");
        bail_error(err);
    }

bail:
    uint32_array_free(&hdr_indices);
    return err;
}

int agentd_set_ipv6_addr (agentd_context_t *context,
        agentd_binding *abinding, void *cb_arg, char **cli_buff){
    int err = 0;
    node_name_t node_name = {0};
    temp_buf_t intf_name = {0}, ipaddr = {0}, ip = {0}, mask = {0};
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    char * p = NULL;

    /* args[2] has the interface name */
    snprintf(intf_name, sizeof(intf_name), "%s", abinding->arg_vals.val_args[2]);
    /* args[3] has the ipv6 address */
    snprintf(ipaddr, sizeof(ipaddr), "%s", abinding->arg_vals.val_args[3]);

    p = strchr(ipaddr,'/');
    bail_null (p);
    strncpy(ip,ipaddr,strlen(ipaddr)-strlen(p));
    snprintf(mask, sizeof(mask),"%s",p+1);

    snprintf(node_name, sizeof(node_name), "/net/interface/config/%s/addr/ipv6/static", intf_name);

    err = bn_binding_array_new(&barr);
    bail_error_null(err, barr);

    err = bn_binding_new_str_autoinval(&binding, "ip", ba_value, bt_ipv6addr, 0,
              ip); 
    bail_error_null(err, binding);

    err = bn_binding_array_append_takeover(barr, &binding);
    bail_error (err);

    err = bn_binding_new_str_autoinval(&binding, "mask_len", ba_value, bt_uint8, 0,
              mask); 
    bail_error_null(err, binding);

    err = bn_binding_array_append_takeover(barr, &binding);
    bail_error(err);

    err = agentd_array_append(agentd_mcc, &ipv6_ctxt, context->mod_bindings,
            node_name, barr, &code, NULL);
    bail_error(err);

bail:
    bn_binding_array_free(&barr);
    return err;
}

int agentd_del_ipv6_addr (agentd_context_t *context,
        agentd_binding *abinding, void *cb_arg, char **cli_buff){
    int err = 0;
    node_name_t node_name = {0}, node_to_delete = {0};;
    temp_buf_t  intf_name = {0}, ipaddr = {0}, ip = {0}, mask = {0};
    char * p = NULL;
    uint32_array *ipv6_indices = NULL;
    uint32 num_ipv6 = 0;


    /* args[2] has the interface name */
    snprintf(intf_name, sizeof(intf_name), "%s", abinding->arg_vals.val_args[2]);
    /* args[3] has the ipv6 address */
    snprintf(ipaddr, sizeof(ipaddr), "%s", abinding->arg_vals.val_args[3]);

    p = strchr(ipaddr,'/');
    bail_null(p);
    strncpy(ip,ipaddr,strlen(ipaddr)-strlen(p));
    snprintf(mask, sizeof(mask),"%s",p+1);

    snprintf(node_name, sizeof(node_name), "/net/interface/config/%s/addr/ipv6/static", intf_name);

    /* Get the index of the particular ipv6 address */
    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
                        node_name, NULL, "ip", ip, 7,
                        bn_db_revision_id_none,
                        NULL, &ipv6_indices, NULL, NULL);
    bail_error_null(err, ipv6_indices);
    num_ipv6 = uint32_array_length_quick(ipv6_indices);
    if(num_ipv6 == 0){
        /* Ignore. Cannot happen */
    } else {
        snprintf(node_to_delete, sizeof(node_to_delete),"%s/%d",
                    node_name, uint32_array_get_quick(ipv6_indices,0));
        err = agentd_append_req_msg (context, node_to_delete, SUBOP_DELETE, bt_string, "");
        bail_error(err);
    }

bail:
    uint32_array_free (&ipv6_indices);
    return err;

}

int agentd_set_ns_domain_name (agentd_context_t *context,
        agentd_binding *abinding, void *cb_arg, char **cli_buff){
    int err = 0;
    node_name_t node_name = {0};
    temp_buf_t ns_name = {0}, domain_name = {0};
    const char * domain_to_set = NULL;

    /* args[1] has the namespace name */
    snprintf (ns_name, sizeof(ns_name), "%s", abinding->arg_vals.val_args[1]);

    /* args[2] has the domain name */
    snprintf (domain_name, sizeof(domain_name), "%s", abinding->arg_vals.val_args[2]);

#if 0
    /* "any" is a reserved domain name. Set domain host to "*" for "any" domain */
    if (!strcmp(domain_name,"any")) {
        domain_to_set = "*";
    } else {
        domain_to_set = domain_name;
    }
#endif

    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/fqdn-list/%s",
	    ns_name, domain_name);
    err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_STR, domain_name);
    bail_error (err);

    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/domain/regex", ns_name);
    err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_REGEX, "");
    bail_error(err);

bail:
    return err;
}

int
agentd_configure_login_method (agentd_context_t * context) {
    int err = 0;
    xmlXPathObjectPtr xpathObj;
    temp_buf_t xpath_pattern = {0};
    int num_methods = 0, i = 0;
    xmlChar * method = NULL;
    node_name_t node_name = {0};

    snprintf (xpath_pattern, sizeof(xpath_pattern), "%s",
        "//system/aaa/authentication/login/default/method");

    xpathObj = xmlXPathEvalExpression((const xmlChar *) xpath_pattern, context->xpathCtx);
    if (xpathObj && xpathObj->nodesetval->nodeNr) {
        num_methods = xpathObj->nodesetval->nodeNr;
        lc_log_debug (jnpr_log_level, "Methods: count = %d", num_methods);
        for (i = 0; i < num_methods; i++) {
            method = xmlNodeGetContent (xpathObj->nodesetval->nodeTab[i]);
            lc_log_debug (jnpr_log_level, "Method %d: %s\n", i+1, method);
            snprintf (node_name, sizeof(node_name), "/aaa/auth_method/%d/name", i+1);
            err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_STR, (char *)method);
            bail_error (err);
            xmlFree(method);
            method = NULL;
        }
    } else {
        /* this means deletion of all login methods */
        /* set "local" as the only authentication method in this case */
        lc_log_debug (jnpr_log_level, "Setting only local authentication method");
        err = agentd_append_req_msg (context, "/aaa/auth_method/1/name", SUBOP_MODIFY, TYPE_STR, "local");
        bail_error (err);
        i = 1; /* Set to 1, so that other authentication methods in mfc can be deleted */
    }
    /* Delete all the other authentication order nodes that are already set in MFC */
    for (; i < MAX_MFC_AUTH_METHODS; i++) {
        lc_log_debug (jnpr_log_level, "Deleting all other authentication methods");
        snprintf (node_name, sizeof(node_name), "/aaa/auth_method/%d", i+1);
        err = agentd_append_req_msg (context, node_name, SUBOP_DELETE, TYPE_STR, "");
        bail_error (err);
    }

bail:
    if (method) {
        xmlFree (method);
    }
    if(xpathObj) {
        xmlXPathFreeObject(xpathObj);
    }
    return err;
}


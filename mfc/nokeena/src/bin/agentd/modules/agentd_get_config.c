
#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"

typedef struct odl_xml_tags {
    int index[MAX_PARAMS];
} odl_xml_tags_t;

typedef uint32_t agentd_node_flags_bf;

typedef struct node_to_xml {
    uint32_t index;
    const char *root_node;
    const char *child_node;
    struct node_to_xml *children;
    agentd_node_flags_bf flags;
} node_to_xml_t;

typedef struct agentd_xml_callback_data {
    agentd_context_t *context;
    const char *node_name;
    node_to_xml_t *xml_array;
}agentd_xml_callback_data_t;

typedef enum {
    agentd_iterate_nodes = 0x0100,
} agentd_node_flags;


extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

int
mdc_lib_get_bindings_by_value(md_client_context *mcc, const char *opattern,
	const char *iparent_name, const char *ichild_name, const char *value,
	bn_binding_array **ret_bindings);
int
agentd_xml_node_iterator(const bn_binding_array *bindings,
	uint32_t idx, const bn_binding *binding, const tstring *name,
	const tstr_array *name_parts, const tstring *name_last_part,
	const tstring *value, void *callback_data);
int
agentd_send_tags_array(agentd_context_t *context,const bn_binding_array *bindings,
	node_to_xml_t *node_xml_array, uint32_t num_entries,
	const char *name, const char *base_node);
int
agentd_send_namespace_once(const bn_binding_array *bindings,
	uint32_t idx, const bn_binding *binding, const tstring *name,
	const tstr_array *name_parts, const tstring *name_last_part,
	const tstring *value, void *callback_data);
int
agentd_send_all_namespaces(agentd_context_t *context, bn_binding_array *bindings);
int
agentd_send_namespaces(agentd_context_t *context, bn_binding_array *bindings,
	const char *ns_name);
int
agentd_send_namespace(agentd_context_t *context, const char *ns_name,
	const bn_binding_array *bindings);
int
agentd_query_config_nodes(const char *nodes[], uint32_t num_nodes,
	bn_binding_array **bindings);
int
agentd_query_nodes(const char *nodes[], uint32_t num_nodes,
	bn_binding_array **bindings, bn_query_node_flags_bf query_flags);
int
agentd_get_config(agentd_context_t * context, void *callback_data);
int
agentd_get_config_init(
        const lc_dso_info *info,
        void *context);
int
agentd_get_config_deinit(
        const lc_dso_info *info,
        void *context);

int
agentd_get_config_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    oper_table_entry_t op_tbl[] = {
        {"/config/mfc-cluster/*", DUMMY_WC_ENTRY, agentd_get_config, (void *)"1" },
        {"/config/mfc-cluster/*/http-service/*", DUMMY_WC_ENTRY, agentd_get_config, (void *) "2" },
        {"/config/mfc-cluster/*/distribution-id/*", DUMMY_WC_ENTRY, agentd_get_config, (void *) "3" },
        OP_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_op_cmds_array(op_tbl);
    bail_error (err);

    lc_log_debug (LOG_DEBUG, "agentd_get_config_init done");
bail:
    return err;
}

int
agentd_get_config_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_get_config_deinit");

bail:
    return err;
}


int
agentd_get_config(agentd_context_t * context, void *callback_data)
{
    /* default to all */
    uint32_t config_details = 1;
    int err =0 , i = 0;
    uint32_t num_nodes = 0, num_bindings = 0;
    bn_binding_array *bindings = NULL, *origin_bindings = NULL;
    lt_time_sec timestamp;
    str_value_t str_value = {0};
    const char *arg_value = NULL;

    timestamp = lt_curr_time();

    const char *full_nodes[] = {
	"/system/load",
	"/system/memory/physical",
	"/nkn/nvsd/namespace",
	"/nkn/nvsd/origin_fetch/config",
    };

    const char *ns_nodes[] = {
	"/nkn/nvsd/namespace",
	"/nkn/nvsd/origin_fetch/config",
    };

    if (callback_data) {
	config_details = atoi((char *)callback_data);
    }
    lc_log_basic(LOG_NOTICE, "config_details: %d", config_details);

    OP_EMIT_TAG_START(context, ODCI_MFC_CONFIG);

    bzero(str_value, sizeof(str_value));
    snprintf(str_value, sizeof(str_value), "%d", (uint32_t)timestamp);
    OP_EMIT_TAG_VALUE(context, ODCI_TIMESTAMP, str_value);

    switch (config_details) {
	case 1: /* complete config */
	    num_nodes = sizeof(full_nodes)/sizeof (const char *);
	    err = agentd_query_config_nodes(full_nodes, num_nodes, &bindings);
	    complain_error(err);
	    OP_EMIT_TAG_START(context, ODCI_GLOBAL);
	    OP_EMIT_TAG_END(context, ODCI_GLOBAL);

	    OP_EMIT_TAG_START(context, ODCI_HTTP_SERVICES);
	    /* context contains requested namespace */
	    err = agentd_send_namespaces(context, bindings, NULL);
	    complain_error(err);
	    OP_EMIT_TAG_END(context, ODCI_HTTP_SERVICES);
	    break;

	case 2: /* all namespaces */
	    num_nodes = sizeof(ns_nodes)/sizeof (const char *);
	    err = agentd_query_config_nodes(ns_nodes, num_nodes, &bindings);
	    bail_error(err);

	    arg_value = tstr_array_get_str_quick(context->op_cmd_parts,
		    POS_CLUSTER_NAME + 2) ;
	    if (arg_value == NULL || (0 == strcmp(arg_value, ""))) {
		set_ret_code(context, 1);
		set_ret_msg(context, "Invalid HTTP-Instance");
		goto bail;
	    }
	    OP_EMIT_TAG_START(context, ODCI_HTTP_SERVICES);
	    /* context contains requested namespace */
	    err = agentd_send_namespaces(context, bindings, arg_value);
	    complain_error(err);
	    OP_EMIT_TAG_END(context, ODCI_HTTP_SERVICES);
	    break;

	case 3: /* all config based on distribution-id */
	    arg_value = tstr_array_get_str_quick(context->op_cmd_parts,
		    POS_CLUSTER_NAME + 2) ;
	    if (arg_value == NULL || (0 == strcmp(arg_value, ""))) {
		set_ret_code(context, 1);
		set_ret_msg(context, "Invalid Distribution-ID");
		goto bail;
	    }
	    err = mdc_lib_get_bindings_by_value(agentd_mcc,
		    "/nkn/nvsd/namespace/*", "/nkn/nvsd/namespace",
		    "distrib-id", arg_value, &bindings);
	    bail_error(err);

	    err = mdc_lib_get_bindings_by_value(agentd_mcc,
		    "/nkn/nvsd/origin_fetch/config/*", "/nkn/nvsd/namespace",
		    "distrib-id", arg_value, &origin_bindings);
	    bail_error(err);

	    err = bn_binding_array_concat(bindings, &origin_bindings);
	    bail_error(err);

	    bn_binding_array_dump("RES-NODES", bindings, LOG_NOTICE);

	    num_bindings = bn_binding_array_length_quick(bindings);
	    if (num_bindings == 0) {
		set_ret_code(context, 1);
		set_ret_msg(context, "Non-Existant Distribution-ID");
		goto bail;
	    }
	    OP_EMIT_TAG_START(context, ODCI_HTTP_SERVICES);
	    /* context contains requested namespace */
	    err = agentd_send_namespaces(context, bindings, "list");
	    complain_error(err);
	    OP_EMIT_TAG_END(context, ODCI_HTTP_SERVICES);

	    break;

	default:
	    set_ret_code(context, 1);
	    set_ret_msg(context, "Unknown option");
	    break;
    }
    OP_EMIT_TAG_END(context, ODCI_MFC_CONFIG);
    //bn_binding_array_dump("GET-NODES", bindings, LOG_NOTICE);
bail:
    if (err) {
	set_ret_code(context, 1);
	set_ret_msg(context, "Internal error occured, see MFC logs");
    }
    return err;
}

int
agentd_query_config_nodes(const char *nodes[], uint32_t num_nodes, bn_binding_array **bindings)
{
    return agentd_query_nodes(nodes, num_nodes, bindings,
	    bqnf_sel_iterate_include_self|bqnf_sel_iterate_subtree|bqnf_sel_class_no_state);
}

int
agentd_query_nodes(const char *nodes[], uint32_t num_nodes,
	bn_binding_array **bindings, bn_query_node_flags_bf query_flags)
{
    uint32_t ret_code = 0, i = 0;
    int err  =0;
    tstring *ret_msg = NULL;
    bn_request *req = NULL;

    if (nodes == NULL) {
	goto bail;
    }

    err = bn_request_msg_create(&req,bbmt_query_request);
    bail_error(err);

    /* Now add the node's characteristics to the message */
    for(i = 0;i < num_nodes; i++) {
	err = bn_query_request_msg_append_str(req,
		bqso_iterate, query_flags,
		nodes[i], NULL);
	bail_error(err);
    }

    //bn_request_msg_dump(NULL, req, LOG_NOTICE, "REQ-MSG");
    err = mdc_send_mgmt_msg(agentd_mcc, req, true, bindings, &ret_code, &ret_msg);
    bail_error(err);

    if (ret_code) {
	/* query failed */
	lc_log_basic(LOG_WARNING, "Unable to fetch values from mgmtd (%d)/%s",
		ret_code, ts_str(ret_msg));
    }
    //bn_binding_array_dump("GET-NODES", *bindings, LOG_NOTICE);

bail:
    ts_free(&ret_msg);
    bn_request_msg_free(&req);
    return err;
}

/*
 * handling following commands
 * config mfc-cluster mfc
 * config mfc-cluster mfc http-service list
 * config mfc-cluster mfc http-service *
 */
int
agentd_send_namespaces(agentd_context_t *context, bn_binding_array *bindings,
	const char *data)
{
    int err = 0;
    node_name_t ns_node = {0};
    error_msg_t error_msg = {0};
    //const char *ns_name = NULL;
    tstring *node_value = NULL;
    const char *ns_name = data;

    bail_require(bindings);
    bail_require(context);


    lc_log_basic(LOG_NOTICE, "ns-name: %s", ns_name?:"NO-NS");

    if ((ns_name == NULL) || (strcmp(ns_name, "list") == 0 )) {
	/* send details of all nodes */
	err = agentd_send_all_namespaces(context, bindings);
	bail_error(err);
    } else {
	ns_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_CLUSTER_NAME + 2) ;
	/* check if namespace exists */
	snprintf(ns_node, sizeof(ns_node), "/nkn/nvsd/namespace/%s", ns_name);
	err = bn_binding_array_get_value_tstr_by_name(bindings, ns_node,
		NULL, &node_value);
	bail_error(err);

	if ((node_value == NULL) || strcmp(ts_str_maybe_empty(node_value), ns_name)) {
	    snprintf(error_msg, sizeof(error_msg), "HTTP Instance %s: not configured", ns_name);
	    set_ret_code(context, 1);
	    set_ret_msg(context, error_msg);
	    goto bail;
	} else {

	    err = agentd_send_namespace(context, ts_str(node_value), bindings);
	    bail_error(err);
	}
    }

bail:
    ts_free(&node_value);
    return err;
}

int
agentd_send_all_namespaces(agentd_context_t *context, bn_binding_array *bindings)
{
    int err = 0;

    err = mdc_foreach_binding_prequeried(bindings, "/nkn/nvsd/namespace/*", NULL,
	    agentd_send_namespace_once, (void *)context, NULL);
    bail_error(err);

bail:
    return err;
}

int
agentd_send_namespace_once(const bn_binding_array *bindings,
	uint32_t idx, const bn_binding *binding, const tstring *name,
	const tstr_array *name_parts, const tstring *name_last_part,
	const tstring *ns_name, void *callback_data)
{
    int err = 0;
    agentd_context_t *context = (agentd_context_t *)callback_data;

    bn_binding_dump(LOG_NOTICE, "SEND-NS", binding);

    err = agentd_send_namespace(context, ts_str(ns_name), bindings);
    bail_error(err);

bail:
    return err;
}

node_to_xml_t ns_cfg_match_tag[] = {
    {
	ODCI_MATCH_TYPE,
	"/nkn/nvsd/namespace", "match/type",
	NULL, 0
    },
    {
	ODCI_REGEX,
	"/nkn/nvsd/namespace", "match/uri/regex",
	NULL, 0
    },
    {
	ODCI_URI_PREFIX,
	"/nkn/nvsd/namespace", "match/uri/uri_name",
	NULL, 0
    },
    {0, NULL, NULL, NULL, 0 }
};

node_to_xml_t ns_cfg_origin_http_tag[] = {
    {
	ODCI_ORIGIN_FQDN,
	"/nkn/nvsd/namespace", "origin-server/http/host/name",
	 NULL, 0
    },
    {
	ODCI_ORIGIN_PORT,
	"/nkn/nvsd/namespace", "origin-server/http/host/port",
	 NULL, 0
    },
    {0, NULL, NULL, NULL, 0 }
};

node_to_xml_t ns_cfg_origin_tag[] = {
    {
	ODCI_HTTP, NULL, NULL,
	ns_cfg_origin_http_tag, 0
    },
    {0, NULL, NULL, NULL, 0 }
};

node_to_xml_t ns_cfg_query_string_tag [] = {
    {
	ODCI_CACHE,
	"/nkn/nvsd/origin_fetch/config", "query/cache/enable",
	NULL, 0
    },
    {
	ODCI_EXCLUDE,
	"/nkn/nvsd/origin_fetch/config", "query/strip",
	NULL, 0
    },
    {0, NULL, NULL, NULL, 0 }
};
node_to_xml_t ns_cfg_client_req_tag [] = {
    {
	ODCI_QUERY_STRING, NULL, NULL,
	ns_cfg_query_string_tag, 0
    },
    {0, NULL, NULL, NULL, 0 }
};

#if 0
node_to_xml_t ns_cfg_origin_fetch_test2 [] = {
    {
	ODCI_CRAWLER_NAME,
	NULL, "name",
	NULL, 0
    },
    {
	ODCI_CRAWLER_CFG_STATUS,
	NULL, "value",
	NULL, 0
    },
    {0, NULL, NULL, NULL, 0 }
};
node_to_xml_t ns_cfg_origin_fetch_test1 [] = {
    {
	ODCI_FQDN,
	NULL, NULL,
	ns_cfg_origin_fetch_test2, 0
    },
    {0, NULL, NULL, NULL, 0 }
};

#endif
node_to_xml_t ns_cfg_fqdn_list [] = {
    {
	ODCI_FQDN,
	NULL, NULL, NULL, 0
    },
    {0, NULL, NULL, NULL, 0 }
};
node_to_xml_t ns_config_to_tags[] = {
    {
	ODCI_PRECEDENCE,
	"/nkn/nvsd/namespace", "ns-precedence",
	NULL,0
    },
    {
	ODCI_SERVICE_STATUS,
	"/nkn/nvsd/namespace", "status/active",
	NULL,0
    },
    {
	ODCI_ACCESSLOG,
	"/nkn/nvsd/namespace", "accesslog",
	NULL,0
    },
    {
	ODCI_CACHE_AGE_DEFAULT,
	"/nkn/nvsd/origin_fetch/config", "cache-age/default",
	NULL,0
    },
    {
	ODCI_DISTRIBUTION_ID,
	"/nkn/nvsd/namespace", "distrib-id",
	NULL,0
    },
    {
	ODCI_CFG_ORIGIN,
	NULL, NULL,
	ns_cfg_origin_tag, 0
    },
    {
	ODCI_CLIENT_REQUEST,
	NULL, NULL,
	ns_cfg_client_req_tag, 0
    },
    {
	ODCI_FQDN_LIST,
	"/nkn/nvsd/namespace", "fqdn-list/*",
	ns_cfg_fqdn_list, agentd_iterate_nodes,

    },
#if 0
    {
	ODCI_ORIGIN_FETCH,
	"/nkn/nvsd/namespace", "origin-request/header/add/*",
	ns_cfg_origin_fetch_test1, agentd_iterate_nodes,
    },
#endif
    {
	ODCI_MATCH,
	NULL, NULL,
	ns_cfg_match_tag,0
    },
    {0, NULL, NULL, NULL, 0}
};

/*
 * /nkn/nvsd/namespace, /<*>, child => uint16_t nodes[4]
 */
int
agentd_send_namespace(agentd_context_t *context, const char *ns_name,
	const bn_binding_array *bindings)
{
    int err = 0;
    const char *precedence = NULL;
    str_value_t str_value = {0};

    OP_EMIT_TAG_START(context, ODCI_HTTP_SERVICE);

	bzero(str_value, sizeof(str_value));
	snprintf(str_value, sizeof(str_value), "%s", ns_name);
	OP_EMIT_TAG_VALUE(context, ODCI_SNAME, str_value);

	err = agentd_send_tags_array(context, bindings, ns_config_to_tags,
		sizeof(ns_config_to_tags)/sizeof(node_to_xml_t), ns_name, NULL);
	complain_error(err);

    OP_EMIT_TAG_END(context, ODCI_HTTP_SERVICE);

bail:
    return err;
}

int
agentd_send_tags_array(agentd_context_t *context,const bn_binding_array *bindings,
	node_to_xml_t *node_xml_array, uint32_t num_entries,
	const char *name, const char *base_node)
{
    int err = 0, k =0;
    uint32_t i = 0, j = 0;
    node_to_xml_t *node_xml_entry = NULL;
    node_name_t node_name = {0};
    tstring *node_value = NULL;

    node_xml_entry = node_xml_array + i;
    while(node_xml_entry->index != 0) {

	if (node_xml_entry->children == NULL) {
	    /* no more child tags , send tags and value */
	    if (base_node) {
		snprintf(node_name, sizeof(node_name), "%s%s%s",
			base_node,
			node_xml_entry->child_node?"/":"",
			node_xml_entry->child_node?:"");
	    } else {
		if (name) {
		    /* use  child node if it there*/
		    snprintf(node_name, sizeof(node_name), "%s/%s%s%s",
			    node_xml_entry->root_node, name,
			    node_xml_entry->child_node?"/":"",
			    node_xml_entry->child_node?:"");
		} else {
		    snprintf(node_name, sizeof(node_name), "%s",node_xml_entry->root_node);
		}
	    }
	    lc_log_basic(LOG_NOTICE, "node: %s", node_name);
	    ts_free(&node_value);
	    err = bn_binding_array_get_value_tstr_by_name(bindings, node_name,
		    NULL, &node_value);
	    bail_error(err);
	    OP_EMIT_TAG_VALUE(context, node_xml_entry->index,
		    ts_str_maybe_empty(node_value));
	} else {
	    /* children are there, so send our tag and resurse */
	    OP_EMIT_TAG_START(context, node_xml_entry->index);
	    if (node_xml_entry->flags & agentd_iterate_nodes) {
		/* call iterator over root_node/name/child_node */
		if (name) {
		    /* use  child node if it there*/
		    snprintf(node_name, sizeof(node_name), "%s/%s%s%s",
			    node_xml_entry->root_node, name,
			    node_xml_entry->child_node?"/":"",
			    node_xml_entry->child_node?:"");
		} else {
		    snprintf(node_name, sizeof(node_name), "%s",node_xml_entry->root_node);
		}
		agentd_xml_callback_data_t cb_data = {
		    context,
		    node_name,
		    node_xml_entry->children,
		};
		err = mdc_foreach_binding_prequeried(bindings, node_name, NULL,
			agentd_xml_node_iterator, (void *) &cb_data, NULL);
	    } else {
		agentd_send_tags_array(context, bindings, node_xml_entry->children,
			0 , name, base_node);
	    }
	    complain_error(err);
	    err = 0;
	    OP_EMIT_TAG_END(context,node_xml_entry->index);
	}

	i++;
	node_xml_entry = node_xml_array + i;
    }

bail:
    ts_free(&node_value);
    return err;
}
int
agentd_xml_node_iterator(const bn_binding_array *bindings,
	uint32_t idx, const bn_binding *binding, const tstring *name,
	const tstr_array *name_parts, const tstring *name_last_part,
	const tstring *value, void *callback_data)
{
    agentd_xml_callback_data_t *cb_data = (agentd_xml_callback_data_t *)callback_data;
    int err =0 ;

    bn_binding_dump(LOG_NOTICE, "ITER: ", binding);
    err = agentd_send_tags_array(cb_data->context, bindings, cb_data->xml_array,
	    0, cb_data->node_name, ts_str(name));
    bail_error(err);

bail:
    return err;
}

/* first wildcard of ipattern will be picked to be used at opattern */
int
mdc_lib_get_bindings_by_value(md_client_context *mcc, const char *opattern,
	const char *iparent_name, const char *ichild_name, const char *value,
	bn_binding_array **ret_bindings)
{
    int err = 0 ;
    uint32_t i = 0, num_bindings = 0, ret_code = 0, num_ns = 0,
	     num_parts = 0, wildcard_index = 0, j = 0, wc_found = 0;
    tstring *ret_msg = NULL, *binding_value = NULL;
    node_name_t node_name = {0};
    bn_binding_array *bindings = NULL, *match_bindings = NULL;
    const bn_binding *binding = NULL;
    const bn_attrib *attrib = NULL;
    const tstring *binding_name = NULL;
    tstr_array *wc_names = NULL, *bn_parts = NULL, *opattern_parts = NULL;
    const char *ns_name = NULL, *opattern_str = NULL;
    bn_request *req = NULL;
    char *query_node = NULL;

    snprintf(node_name, sizeof(node_name), "%s/*/%s",
	    iparent_name, ichild_name);
    lc_log_basic(LOG_NOTICE, "ipattern: %s, opattern: %s, value: %s",
	    node_name, opattern, value);

    err = mdc_get_binding_children(mcc, &ret_code, &ret_msg, true,
	    &bindings, true, true, iparent_name);
    bail_error(err);

    //bn_binding_array_dump("LIB-NODES", bindings, LOG_NOTICE);

    num_bindings = bn_binding_array_length_quick(bindings);

    err = bn_binding_array_new(&match_bindings);
    bail_error(err);

    for (i = 0; i < num_bindings; i++) {
        binding = bn_binding_array_get_quick(bindings, i);
        if (binding == NULL) {
	    continue;
	}

	binding_name = bn_binding_get_name_quick(binding);
	if (binding_name == NULL) {
	    continue;
	}
	/* XXX : this is not optimized code */
	if (0 == bn_binding_name_pattern_match(ts_str_maybe_empty(binding_name),
		    node_name)) {
	    continue;
	}

        err = bn_binding_get_attrib(binding, ba_value, &attrib);
        bail_error(err);

	ts_free(&binding_value);
        err = bn_attrib_get_tstr(attrib, NULL, bt_NONE, NULL, &binding_value);
        bail_error(err);

	if (0 == strcmp(ts_str_maybe_empty(binding_value), value)) {
	    err = bn_binding_array_append(match_bindings, binding);
	    bail_error(err);
	}
    }
    bn_binding_array_dump("MATCHING-NODES", match_bindings, LOG_NOTICE);

    err = bn_binding_array_get_name_part_tstr_array(match_bindings,
	    node_name, 3, &wc_names);
    bail_error(err);

    tstr_array_dump(wc_names, "NS-NAMES");

    err = bn_binding_name_to_parts(opattern, &opattern_parts, NULL);
    bail_error(err);

    num_parts = tstr_array_length_quick(opattern_parts);

    tstr_array_dump(opattern_parts , "OUTPUT-NODES");

    wildcard_index = 0;
    for (i = 0; i < num_parts; i++) {
	opattern_str = tstr_array_get_str_quick(opattern_parts, i);
	if (0 == strcmp("*", opattern_str)) {
	    wildcard_index = i;
	    wc_found = 1;
	    lc_log_basic(LOG_NOTICE, "WC=> %d", wildcard_index);
	    break;
	}
    }

    if (wc_found == 0) {
	lc_log_basic(LOG_NOTICE, "No wildcard found");
    } else {
    }

    err = bn_request_msg_create(&req,bbmt_query_request);
    bail_error(err);

    num_ns = tstr_array_length_quick(wc_names);
    for (i = 0; i < num_ns; i++) {
	ns_name = tstr_array_get_str_quick(wc_names, i);
	if (ns_name == NULL) {
	    continue;
	}
	tstr_array_free(&bn_parts);
	err = tstr_array_new(&bn_parts, NULL);
	bail_error(err);
	bail_null(bn_parts);

	/* duplicate array */
	for (j = 0; j < num_parts; j++) {
	    if (wildcard_index == j) {
		err = tstr_array_append_str(bn_parts, ns_name);
	    } else {
		err = tstr_array_append_str(bn_parts,
			tstr_array_get_str_quick(opattern_parts, j));
	    }
	    bail_error(err);
	}
	tstr_array_dump(bn_parts, "nodes:");
	safe_free(query_node);
	err = bn_binding_name_parts_to_name(bn_parts, true, &query_node);
	bail_error(err);

	err = bn_query_request_msg_append_str(req,
		bqso_iterate,
		bqnf_sel_iterate_include_self|bqnf_sel_iterate_subtree,
		query_node, NULL);
	bail_error(err);
    }
    bn_binding_array_free(&bindings);
    err = mdc_send_mgmt_msg(agentd_mcc, req, true, &bindings, &ret_code, &ret_msg);
    bail_error(err);

    //bn_binding_array_dump("RES-NODES", bindings, LOG_NOTICE);

    if (bindings) {
	*ret_bindings = bindings;
	bindings = NULL;
    }
bail:
    bn_request_msg_free(&req);
    tstr_array_free(&wc_names);
    bn_binding_array_free(&bindings);
    bn_binding_array_free(&match_bindings);
    return err;
}

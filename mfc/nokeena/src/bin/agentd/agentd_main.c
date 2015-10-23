#include "agentd_mgmt.h"
#include "typed_arrays.h"
#include "array.h"
#include "agentd_conf_omap.h"
#include "unistd.h"
#include "xml_utils.h"
#include "agentd_modules.h"
#include "nkn_mem_counter_def.h"
#include "nkn_stat.h"
#include "agentd_op_cmds_base.h"

extern int jnpr_log_level;
int ign_fail = 0;
int env_prov_timestamp = 0;
int env_rpc_timestamp = 0;
static lew_event *agentd_dmi_conx = NULL;
extern lew_context * agentd_lew;

#define AGENTD_INC_PATH		"/config/nkn"
#define AGENTD_INC_CONF_XML_FILE "/config/nkn/agentd-conf-inc.xml"
#define AGENTD_INC_OPER_XML_FILE "/config/nkn/agentd-oper-inc.xml"
#define AGENTD_INC_XML_FILE "/config/nkn/agentd-inc.xml"

static void init_signal_handling(void);
static void exitclean(int signal_num);
extern void catcher(int sig);
TYPED_ARRAY_IMPL_NEW_NONE(agentd_binding, agentd_binding *, NULL);

static int  agentd_binding_array_new(agentd_binding_array **ret_arr);
static void agentd_binding_free_for_array(void *elem);
static int  agentd_binding_compare_for_array(const void *elem1, const void *elem2);
static int  agentd_binding_dup_for_array(const void *src, void **ret_dest);
static int agentd_binding_dup(const agentd_binding *src, agentd_binding **ret_dest);
static int agentd_binding_array_dump(const char *prefix, const agentd_binding_array *agentd_array, int log_level);
static int agentd_binding_free(agentd_binding **ret_binding);
static int agentd_binding_new_vals(agentd_binding **ret_binding, const char *node_name,
    const char *node_value,
    const char *pattern, struct arg_values *arg_vals, int flags);
static int agentd_arg_vals_dump(int log_level, const char *prefix, const struct arg_values* arg_vals);
static int agentd_binding_compare_value(const void *elem1, const void *elem2, int *diff);
static int agentd_parse_request(agentd_context_t *context, xmlNode * root);
static int parse_req_payload(agentd_context_t *context, agentd_req_type req_type, xmlNode *data);
static int agentd_parse_req_config(agentd_context_t *context, xmlNode *data);
static int agentd_set_context_new_config(agentd_context_t *context,
    agentd_binding_array *new_config);
static int has_identifier(xmlNode *node);
static int agentd_dmi_callback(int listen_fd, short event_type, void *data, lew_context *ctxt);

static int check_child_identifer(xmlNode *node, xmlNode **idend_node);

static int
agentd_get_config_diff(agentd_context_t *context, agentd_binding_array *current,
    agentd_binding_array *new);

static int agentd_handle_request(agentd_context_t *context, xmlDocPtr doc);
static int agentd_save_config_req(agentd_context_t *context, xmlDocPtr xml_doc);
static int agentd_init_comm(agentd_context_t *context);
static int connection_handler(agentd_context_t *context, int connfd);

static int agentd_read_input(agentd_context_t *context, int connfd,
    char **input, int *input_len);

static int agentd_send_response(agentd_context_t *context, int connfd);

static int agentd_binding_add_changes(agentd_binding_array *changes,
    agentd_binding_array *source, int index, int flag);

static int agentd_parse_req_oper(agentd_context_t *context, xmlNode *data);
static int agentd_read_config_req(agentd_context_t *context);
static void agentd_get_next_file(char* p_fileName,int p_size);
static void agentd_backup_old_incoming_req(int dummy);

void * distrib_id_copy_val_fn (void *value);
static int agentd_read_distrib_config(agentd_context_t *context);
tbool agentd_match_distrib_id(const char *name, void *mf_arg);
static int agentd_read_config_file(agentd_context_t *context, const char *fname);



const lc_enum_string_map agentd_req_type_map[] = {
    { agentd_req_config, AGENTD_STR_REQ_TYPE_CONFIG },
    { agentd_req_get, AGENTD_STR_REQ_TYPE_GET },
    { agentd_req_operational, AGENTD_STR_REQ_TYPE_OPERATIONAL },
    { agentd_req_rest_post, AGENTD_STR_REQ_TYPE_REST_POST},
    { agentd_req_rest_put, AGENTD_STR_REQ_TYPE_REST_PUT},
    { agentd_req_rest_delete, AGENTD_STR_REQ_TYPE_REST_DELETE},
    { agentd_req_rest_get, AGENTD_STR_REQ_TYPE_REST_GET},
    { agentd_req_none, AGENTD_STR_REQ_TYPE_NONE }
};

static int
agentd_get_config_diff(agentd_context_t *context, agentd_binding_array *current,
	agentd_binding_array *new)
{
    int err = 0, diff = 0, i = 0, j = 0,
	new_num = 0, curr_num = 0, value_diff = 0;
    agentd_binding_array *changes = NULL;
    agentd_binding *curr_binding = NULL, *new_binding = NULL,
		   *dup_binding = NULL;

    err = agentd_binding_array_new(&changes);
    bail_error(err);

    bail_null(new);

    /* variable j iterates new bindings */
    new_num = agentd_binding_array_length_quick(new);
    /* variable i iterates current bindings */
    curr_num = agentd_binding_array_length_quick(current);
    lc_log_debug(jnpr_log_level, "new: %d, current: %d", new_num, curr_num);

    while(1) {
	if (i == curr_num) {
	    /* add all new bindings into changes as additions */
	    err = agentd_binding_add_changes(changes, new, j,
		    AGENTD_CFG_CHANGE_ADDITION);
	    bail_error(err);
	    break;
	}
	if (j == new_num) {
	    /* add all old bindings into changes as deletion */
	    err = agentd_binding_add_changes(changes, current, i,
		    AGENTD_CFG_CHANGE_DELETION);
	    bail_error(err);
	    break;
	}
	curr_binding = agentd_binding_array_get_quick(current, i);
	new_binding = agentd_binding_array_get_quick(new, j);
	if ((curr_binding == NULL) || (new_binding == NULL)) {
	    /* MUST not happen */
	    break;
	}
	diff = agentd_binding_compare_for_array(&curr_binding, &new_binding);
	if (diff == lc_err_unexpected_null) {
	    lc_log_debug(jnpr_log_level, "recvd diff as lc_err_unexpected_null");
	    err = lc_err_unexpected_null;
	    /* break the loop */
	    goto bail;
	}
	if (diff == 0) {
	    /* no difference, no change with names, compare values */
	    err = agentd_binding_compare_value(&curr_binding, &new_binding, &value_diff);
	    bail_error(err);
	    if (value_diff) {
		dup_binding = NULL;
		err = agentd_binding_dup(new_binding, &dup_binding);
		bail_error_null(err, dup_binding);
		dup_binding->flags |= AGENTD_CFG_CHANGE_MODIFICATION;
		err = agentd_binding_array_append_takeover(changes, &dup_binding);
		bail_error(err);
	    }
	    /* goto next of both arrays */
	    i++; j++;
	} else if (diff > 0) {
	    /*
	     * ns2 is greater than ns1,
	     * this is sorted array- so this is addition
	     * increment new array
	     */
	    dup_binding = NULL;
	    err = agentd_binding_dup(new_binding, &dup_binding);
	    bail_error_null(err, dup_binding);
	    dup_binding->flags |= AGENTD_CFG_CHANGE_ADDITION;
	    err = agentd_binding_array_append_takeover(changes, &dup_binding);
	    bail_error(err);
	    j++;
	}  else {
	    /*
	     * ns3 is less than ns4
	     * this is sorted array- so this is deletion
	     * increment current array
	     */
	    dup_binding = NULL;
	    err = agentd_binding_dup(curr_binding, &dup_binding);
	    bail_error_null(err, dup_binding);
	    dup_binding->flags |= AGENTD_CFG_CHANGE_DELETION;
	    err = agentd_binding_array_append_takeover(changes, &dup_binding);
	    bail_error(err);
	    i++;
	}
    }

    agentd_binding_array_dump("CHANGES", changes, jnpr_log_level);
    context->config_diff = changes;
    changes = NULL;

bail:
    if (err) {
	agentd_binding_free(&dup_binding);
	agentd_binding_array_free(&changes);
    }
    return err;
}
#if 0
int dump_doc(void)
{
    const char *file = "/var/tmp/junos.xml";
    xmlNodePtr nodeLevel1;
    xmlNodePtr nodeLevel2;
    xmlDocPtr doc;

    doc = xmlParseFile(file);
    for(    nodeLevel1 = doc->children;
	    nodeLevel1 != NULL;
	    nodeLevel1 = nodeLevel1->next)
    {
	printf("%s\n",nodeLevel1->name);
	for(    nodeLevel2 = nodeLevel1->children;
		nodeLevel2 != NULL;
		nodeLevel2 = nodeLevel2->next)
	{
	    printf("\t%s\n",nodeLevel2->name);
	}
    }
    xmlFreeDoc(doc);
    return 0;
}
#endif
static int has_identifier(xmlNode *node)
{
    char *flags = NULL;
    int found = 0;

    flags = (char *)xmlGetProp(node, (const xmlChar *)"flag");
    if (flags) {
	/* we have flags */
	if (0 == strcmp(flags, "identifier")) {
	    //printf("WE GOT IDENT\n");
	    found = 1;
	}
	xmlFree(flags);
    } else {
	found = 0;
    }
    return found;
}

static int check_child_identifer(xmlNode *node, xmlNode **ident)
{
    xmlNode *cur_node = NULL;
    int num_child = 0, i =0;
    if (node == NULL) {
	return 0;
    }
    i = 0;

    //printf("%s\n", __FUNCTION__);
    //printf("NAME- %s\n", node->name);
    for (cur_node = node; cur_node; cur_node = cur_node->next) {
	//printf("NODE-> %d : %s (%d)\n", i, cur_node->name, cur_node->type);
	//printf("CHILD NAME - %s, %d\n", cur_node->name, i);
	i++;
	if (cur_node->type == XML_ELEMENT_NODE) {
	    if (has_identifier(cur_node)) {
		/* we have node with identifier */
		//printf("GOT ID FOR -> %s \n", cur_node->name);
		num_child++;
		*ident = cur_node;
	    }
	}
    }
    //printf("FIND IDENT: %s\n", *ident?(char *)(*ident)->name:" NO IDENT FOUND");
    return num_child;
}

int agentd_convert_cfg(xmlNode * a_node, const char *base_name,
	const char * pattern, struct arg_values *arg_vals,
	int nargs, int root_node,
	agentd_binding_array *agentd_array)
{
    xmlNode *cur_node = NULL, *ident_node = NULL;
    char node_name[1024] = {0}, base_node_name[1024] = {0}, temp_buf[1024] = {0},
	 pattern_name[1024] = {0}, base_pattern[1024] = {0};
    int i = 0, err =0 , ident = 0;
    agentd_binding *binding = NULL;
    char contents[1024] = {0}, *content_ptr = NULL;


    bail_null(arg_vals);

    bzero(&base_node_name, sizeof(base_node_name));
    snprintf(base_node_name, sizeof(base_node_name), "%s", base_name?:"");

    bzero(&base_pattern, sizeof(base_pattern));
    snprintf(base_pattern, sizeof(base_pattern), "%s", pattern?:"");

    if ((0 == strcmp(base_node_name, "")) && (root_node == 0)) {
	return 0;
    }
    //printf("base-node=> %s\n",base_node_name);
    //printf("base-patt=> %s\n",base_pattern);
    ident = check_child_identifer(a_node, &ident_node);
    if (a_node) {
	//printf("NAME-> %s, IDENT - %d, %d\n", a_node->name, ident, ident_added);
	if (ident_node) {
	    //printf("IDENT_NODE- %s, BASE=> %s\n", ident_node->name, base_node_name);
	    snprintf(temp_buf, sizeof(temp_buf), "%s", base_node_name);
	    content_ptr = (char *)xmlNodeGetContent(ident_node);
	    bzero(&contents, sizeof(contents));

	    bail_null(content_ptr);

	    snprintf(contents, sizeof(contents), "%s", content_ptr);
            xmlFree(content_ptr);
            content_ptr = NULL;

	    snprintf(base_node_name, sizeof(base_node_name), "%s/%s", temp_buf, contents );
	    snprintf(temp_buf, sizeof(temp_buf), "%s", base_pattern);
	    snprintf(base_pattern,sizeof(base_pattern), "%s*", temp_buf);
	    //printf("IDENT: %s\n", base_node_name);
	    //printf("WNODE=> %s (%s)\n",base_node_name, contents);
	    snprintf(arg_vals->val_args[nargs++], sizeof(agentd_value_t),
		    "%s", contents);
	    arg_vals->nargs = nargs ;
	    //agentd_arg_vals_dump(arg_vals);
	    err = agentd_binding_new_vals(&binding, base_node_name, contents,
		    base_pattern, arg_vals, 0 );
	    bail_error(err);
	    //agentd_binding_dump(jnpr_log_level, "PARSING", binding);
	    err = agentd_binding_array_append_takeover(agentd_array, &binding);
	    bail_error(err);
	}
    }
    //printf("base-node=> %s\n",base_node_name);
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
	i++;
	if (cur_node->type == XML_ELEMENT_NODE) {
	    bzero(&node_name, sizeof(node_name));
	    /* XXX - xmlGetProp allocates the memory */
	    if (has_identifier(cur_node)) {
		/* ignore it */
	    } else {
		snprintf(node_name, sizeof(node_name), "%s/%s",
			base_node_name, cur_node->name? (char *)cur_node->name:"");
		snprintf(pattern_name, sizeof(pattern_name), "%s%s",
			base_pattern, cur_node->name? (char *)cur_node->name:"");
	    }
	}
	//printf("node-> %d : %s (%d)\n", i, cur_node->name, cur_node->type);
	//printf("node_name: %s\n", node_name);
	if (cur_node->children == NULL ) {
	    /* we got an attribute */
	    char *node_ptr = base_node_name, *pattern_ptr = base_pattern;
	    //printf("node=> %s (%s)\n", base_node_name, (char *)xmlNodeGetContent(cur_node));
	    //printf("base-node1 => %s\n",base_node_name);
	    //printf("base-patt2 => %s\n",base_pattern);
	    content_ptr = (char *)xmlNodeGetContent(cur_node);
	    bzero(&contents, sizeof(contents));
	    if (NULL == content_ptr) {
		/* TODO: log error */
		//printf("content ptr is NULL\n");
	    } else {
		snprintf(contents, sizeof(contents), "%s", content_ptr);
                xmlFree(content_ptr);
                content_ptr = NULL;
		if (0 == strcmp(contents, "")) {
		    /* do special handling for cases where
		     * <virtual-player></virtual-player>
		     */
		    //printf("content is empty\n");
		    node_ptr = node_name;
		    pattern_ptr = temp_buf;
		    temp_buf[0] = '\0';
		    snprintf(temp_buf, sizeof(temp_buf), "%s*", pattern_name);
		    //snprintf(base_pattern,sizeof(base_pattern), "%s*", temp_buf);
		} else {
		    /* update base-pattern */
		    temp_buf[0] = '\0';
		    snprintf(temp_buf, sizeof(temp_buf), "%s", base_pattern);
		    snprintf(base_pattern,sizeof(base_pattern), "%s*", temp_buf);
		}
	    }
	    //printf("IDENT: %s\n", base_node_name);
	    snprintf(arg_vals->val_args[nargs], sizeof(agentd_value_t),
		    "%s", contents);
	    arg_vals->nargs = nargs + 1;
	    //agentd_arg_vals_dump(arg_vals);
	    err = agentd_binding_new_vals(&binding, node_ptr, contents,
		    pattern_ptr, arg_vals, 0 );
	    bail_error(err);
	    agentd_binding_dump(jnpr_log_level, "PARSING", binding);
	    err = agentd_binding_array_append_takeover(agentd_array, &binding);
	    bail_error(err);
	}
	err = agentd_convert_cfg(cur_node->children, node_name,
		pattern_name, arg_vals, nargs, 0, agentd_array);
	bail_error(err);
    }

bail:
    xmlFree(content_ptr);
    return err;
}



int
main(int argc, char *argv[])
{
    int err = 0, c = 0;
    char *ign_fail_env, *log_prov_time_env, *log_rpc_time_env;
    agentd_context_t context;
    int rest_api_support = 0, vjx_support = 0, log_level = 0;

    jnpr_log_level = 7;

    while ((c = getopt (argc, argv, "rxl:")) != -1) {
	switch (c) {
	    case 'r':
		rest_api_support = 1;
		break;
	    case 'x':
		vjx_support = 1;
		break;
	    case 'l':
		log_level = atoi(optarg);
		break;
	    default:
		break;
	}
    }
    if ((rest_api_support == 1) && (vjx_support == 1)) {
	fprintf(stderr, "both rest-api and vjx cannot be enabled \n");
	exit(1);
    } else if (rest_api_support == 0) {
	/* make sure vjx_support is enabled if rest-api support is disabled */
	vjx_support = 1;
    }

    if (log_level < 1 || log_level > 7) {
	//fprintf(stderr, "invalid log_level value : %d\n", log_level);
	/* ignore its value */
    } else {
	jnpr_log_level = log_level;
    }
    /* setting it to LOG_DEBUG, Use 5 for LOG_NOTICE */

    bzero(&context, sizeof(context));
    context.rapi_enabled = rest_api_support;

    /* init connection with mgmtd, register logger */
    err = agentd_mgmt_init(&context);
    bail_error(err);

    /* Get the value of "IGN_FAILED_LOOKUP" to determine whether to ignore failed lookups or not */
    ign_fail_env = getenv("IGN_FAILED_LOOKUP");
    if (ign_fail_env) ign_fail = atoi(ign_fail_env);
    lc_log_debug (jnpr_log_level, "Value of IGN_FAILED_LOOKUP: [ %s ],saved as [ %d ]", ign_fail_env, ign_fail);

    /* Get the value of "LOG_PROV_TIMESTAMP" to determine whether to log provisioning timestamp */
    log_prov_time_env = getenv("LOG_PROV_TIMESTAMP");
    if (log_prov_time_env) env_prov_timestamp = atoi(log_prov_time_env);
    lc_log_debug (jnpr_log_level, "Value of LOG_PROV_TIMESTAMP: [ %s ],saved as [ %d ]", log_prov_time_env, env_prov_timestamp);

    /* Get the value of "LOG_RPC_TIMESTAMP" to determine whether to log RPC timestamp */
    log_rpc_time_env = getenv("LOG_RPC_TIMESTAMP");
    if (log_rpc_time_env) env_rpc_timestamp = atoi(log_rpc_time_env);
    lc_log_debug (jnpr_log_level, "Value of LOG_RPC_TIMESTAMP: [ %s ],saved as [ %d ]", log_rpc_time_env, env_rpc_timestamp);

//    {
//        return agentd_dummy_testing(0);
//    }

    /* Load the agentd modules */
    err = agentd_load_modules();
    bail_error (err);

    /* Call the init function of each module */
    err = agentd_init_modules();
    bail_error (err);

    err = agentd_init_comm(&context);
    bail_error(err);

bail:
    err = agentd_deinit_modules();
    if (err) {
        lc_log_basic (LOG_ERR, "agentd_deinit_modules() returned error");
    }
    err = agentd_unload_modules();
    if (err) {
        lc_log_basic (LOG_ERR, "agentd_unload_modules() returned error");
    }
    return err;
}

static int
agentd_binding_array_new(agentd_binding_array **ret_arr)
{
    int err = 0;
    array_options opts;

    bail_null(ret_arr);
    *ret_arr = NULL;

    err = array_options_get_defaults(&opts);
    bail_error(err);

    opts.ao_initial_size = 1000;
    opts.ao_elem_compare_func = agentd_binding_compare_for_array;
    opts.ao_elem_dup_func = agentd_binding_dup_for_array;
    opts.ao_elem_free_func = agentd_binding_free_for_array;

    err = agentd_binding_array_new_full(ret_arr, &opts);
    bail_error(err);

 bail:
    return(err);
}

static int agentd_binding_free(agentd_binding **ret_binding)
{
    int err = 0;
    agentd_binding *binding = NULL;

    bail_null(ret_binding);
    if (!*ret_binding) {
	goto bail;
    }
    binding = *ret_binding;

    bn_binding_free(&(binding->binding));
    safe_free(*ret_binding);

bail:
    return err;
}

static void agentd_binding_free_for_array(void *elem)
{
    agentd_binding *src = elem;

    agentd_binding_free(&src);
    return;
}

static int
agentd_binding_dup(const agentd_binding *sbinding, agentd_binding **ret_binding)
{
    int err = 0;
    agentd_binding *nbinding = NULL;
    bn_binding *temp_binding = NULL;

    if (ret_binding) {
	*ret_binding = NULL;
    }

    bail_null(sbinding);
    bail_null(ret_binding);

    nbinding = malloc(sizeof(agentd_binding));
    bail_null(nbinding);
    memset(nbinding, 0, sizeof(agentd_binding));

    err = bn_binding_dup(sbinding->binding, &temp_binding);
    bail_error_null(err, temp_binding);

    nbinding->binding = temp_binding;
    temp_binding = NULL;

    /* copy structure */
    nbinding->arg_vals = sbinding->arg_vals;
    nbinding->flags = sbinding->flags;

    strlcpy(nbinding->pattern, sbinding->pattern, sizeof(nbinding->pattern));

    *ret_binding = nbinding;
    nbinding = NULL;

bail:
    agentd_binding_free(&nbinding);
    bn_binding_free(&temp_binding);
    return err;
}

static int
agentd_binding_dup_for_array(const void *src, void **ret_dest)
{
    return(agentd_binding_dup(src, (agentd_binding **) ret_dest));
    /* dup bn_binding using bn_binding_dup() */
}

static int
agentd_binding_compare_for_array(const void *elem1, const void *elem2)
{
    int err = 0;
    agentd_binding *agt_elem1, *agt_elem2 ;

    bail_null(elem1);
    agt_elem1 = *(agentd_binding **)elem1;
    bail_null(agt_elem1);

    bail_null(elem2);
    agt_elem2 = *(agentd_binding **)elem2;
    bail_null(agt_elem2);

    /* get bindings and pass it to bn_binding_compare_names_for_array() */
    return bn_binding_compare_names_for_array(&(agt_elem1->binding),
						&(agt_elem2->binding));

bail:
    return err;

}

static int
agentd_binding_array_dump(const char *prefix,
	const agentd_binding_array *agentd_array,
	int log_level)
{
    int i = 0, num_bindings = 0;
    const agentd_binding *binding = NULL;
    num_bindings = agentd_binding_array_length_quick(agentd_array);

    for (i = 0; i < num_bindings; i++) {
	binding = agentd_binding_array_get_quick(agentd_array, i);
	if (binding == NULL) {
	    continue;
	}
	agentd_binding_dump(log_level, prefix, binding);
    }

    return 0;
}

static int
agentd_binding_new_vals(agentd_binding **ret_binding, const char *node_name,
	const char *node_value, const char *pattern,
	struct arg_values *arg_vals, int flags)
{
    int err = 0;
    bn_binding *binding = NULL;
    agentd_binding *agt_binding = NULL;

    lc_log_debug(LOG_DEBUG,"node=> %s, val=> %s, patt=>%s",
	    node_name?:"NO NODE", node_value?:"NO VAL", pattern?:"NO PATTERN");
    agt_binding = (agentd_binding *)malloc(sizeof(agentd_binding));
    if (agt_binding == NULL) {
	err =1;
	lc_log_basic(jnpr_log_level, "malloc() failed");
	goto bail;
    }
    bzero(agt_binding, sizeof(agentd_binding));

    err = bn_binding_new_str(&binding, node_name, ba_value,
	    bt_string, 0, node_value);
    bail_error_null(err, binding);

    agt_binding->binding = binding;
    binding = NULL;

    if (pattern) {
	strlcpy(agt_binding->pattern, pattern, sizeof(agt_binding->pattern));
    } else {
	lc_log_debug(jnpr_log_level, "NULL pattern");
	err = 1;
	goto bail;
    }

    if (flags) {
	/* do nothing for now */
    }
    if (arg_vals) {
	/* copying the entire structure */
	agt_binding->arg_vals = *arg_vals;
    }
    *ret_binding = agt_binding;
    agt_binding = NULL;

bail:
    agentd_binding_free(&agt_binding);
    bn_binding_free(&binding);
    return err;
}


int
agentd_binding_dump(int log_level, const char *prefix, const agentd_binding *binding)
{
    int err = 0;
    bail_require(binding);
    if (prefix == NULL) {
	prefix = "NO PFX";
    }
    if (binding) {
	bn_binding_dump(log_level, prefix, binding->binding);
    } else {
	lc_log_basic(log_level, "%s: No bn_binding found", prefix);
    }
    lc_log_basic(log_level, "%s: flag: %d, pattern=> %s",
	    prefix, binding->flags, binding->pattern);
    agentd_arg_vals_dump(log_level, prefix, &binding->arg_vals);

bail:
    return err;
}

static int agentd_arg_vals_dump(int log_level, const char *prefix, const struct arg_values* arg_vals)
{
    int i = 0;

    if (arg_vals == NULL) {
	return 0;
    }
    if (prefix == NULL) {
	prefix = "NO PFX";
    }

    for(i = 0; i < arg_vals->nargs; i++) {
	lc_log_basic(log_level, "VAL %d: %s ", i, arg_vals->val_args[i]);
    }
    return 0;
}
static int
agentd_binding_compare_value(const void *elem1, const void *elem2, int *diff)
{
    int err = 0;
    agentd_binding *agt_elem1, *agt_elem2 ;
    const tstring *value1 = NULL, *value2 = NULL;
    const bn_binding *binding1 = NULL, *binding2 = NULL;

    bail_null(diff);
    *diff = 0;

    bail_null(elem1);
    agt_elem1 = *(agentd_binding **)elem1;
    bail_null(agt_elem1);

    bail_null(elem2);
    agt_elem2 = *(agentd_binding **)elem2;
    bail_null(agt_elem2);

    binding1 = agt_elem1->binding;
    binding2 = agt_elem2->binding;

    bail_null(binding1);
    bail_null(binding2);

    /*
     * XXX: comparing bindings this is not optimized, instead nameparts
     * should be  compared
     */
    err = bn_binding_get_tstring_const(binding1, ba_value,bt_string, 0, &value1);
    bail_error_null(err, value1);

    err = bn_binding_get_tstring_const(binding2, ba_value,bt_string, 0, &value2);
    bail_error_null(err, value2);

    if (!ts_equal_tstr(value1, value2, false)) {
	/* values are not equal for both bindings */
	*diff = 1;
    }

bail:
    return err;
}

static int
agentd_parse_request(agentd_context_t *context, xmlNode *root)
{
    int err = 0;
    xmlNode *mfc_req = NULL, *header = NULL, *data = NULL, *type = NULL,
	    *distrib_id = NULL;
    uint32_t req_type = 0;
    char *type_contents = NULL, *distrib_contents = NULL;

    bail_null(root);
    bail_null(context);

    /* iterate and find header, get request-type from header */
    mfc_req = find_element_by_name(root, AGENTD_STR_MFC_REQUEST);
    bail_null(mfc_req);

    header = find_element_by_name(mfc_req->children, AGENTD_STR_HEADER);
    bail_null(mfc_req);

    type = find_element_by_name(header->children, AGENTD_STR_TYPE);
    bail_null(type);

    distrib_id = find_element_by_name(header->children, AGENTD_STR_DISTRIB_ID);
    /* NOTE distrib-id in header is optional */
    if (distrib_id) {
	distrib_contents = (char *) xmlNodeGetContent(distrib_id);
	lc_log_basic(jnpr_log_level, "distrib-id: %s", distrib_contents);
	snprintf(context->distrib_id, sizeof(context->distrib_id),
		"%s",distrib_contents);
	xmlFree(distrib_contents);
	if (0== strcmp(context->distrib_id, "")) {
	    lc_log_basic(LOG_ERR, "distrib-id is empty");
	}
    }

    data = find_element_by_name(mfc_req->children, AGENTD_STR_DATA);

    lc_log_basic(LOG_DEBUG, "type : %s", xmlNodeGetContent(type));
    type_contents = (char *) xmlNodeGetContent(type);

    /* get data, and parse it based on request-type */
    err = lc_string_to_enum(agentd_req_type_map, type_contents, &req_type);
    if (err == lc_err_bad_type) {
	/* TODO: set the response as unknown req type */
    } else if (err) {
	/* TODO: set the response internal error, check logs */
	goto bail;
    }

    context->req_type = req_type;

    if (data == NULL) {
	lc_log_basic(LOG_NOTICE, "Wrong payload received, rejecting it");
	set_ret_code(context, 1101);
	set_ret_msg(context, "Wrong payload received, rejecting it");
	context->request_rejected = 1;
	goto bail;
    }
    /* sanity check for rapi and type of request received */
    if (context->rapi_enabled) {
	/* rest-api mode, but got vjx requests, reject them */
	if ((req_type != agentd_req_rest_put)
		&& (req_type != agentd_req_rest_post)
		&& (req_type != agentd_req_rest_delete)
		&& (req_type != agentd_req_get)
		&& (req_type != agentd_req_operational)) {
	    lc_log_basic(LOG_NOTICE, "Wrong payload received, rejecting it");
	    set_ret_code(context, 1100);
	    set_ret_msg(context, "Wrong payload received, rejecting it");
	    context->request_rejected = 1;
	    goto bail;
	}
	if (0== strcmp(context->distrib_id, "")) {
	    /* no distrib_id recevied */
	    lc_log_basic(LOG_NOTICE, "distrib-id is NULL");
	    lc_log_basic(LOG_NOTICE, "Wrong payload received, rejecting it");
	    set_ret_code(context, 1103);
	    set_ret_msg(context, "Wrong payload received, rejecting it");
	    context->request_rejected = 1;
	    goto bail;
	}
    } else {
	/* vjx mode, but got rest-api requests, reject them */
	if ((req_type != agentd_req_config)
		&& (req_type != agentd_req_get)
		&& (req_type != agentd_req_operational)) {
	    lc_log_basic(LOG_NOTICE, "Wrong payload received, rejecting it");
	    set_ret_code(context, 1102);
	    set_ret_msg(context, "Wrong payload received, rejecting it");
	    context->request_rejected = 1;
	    goto bail;
	}
    }

    /* we have received the request type */
    err = parse_req_payload(context, req_type, data);
    bail_error(err);

bail:
    xmlFree(type_contents);
    return err;
}

static int
parse_req_payload(agentd_context_t *context, agentd_req_type req_type,
	xmlNode *data)
{
    int err = 0;

    lc_log_debug(jnpr_log_level, "parsing req type -%d", req_type);
    switch (req_type) {
	case agentd_req_config :
	    /* REST API PUT/POST support has same format */
	case agentd_req_rest_post:
	case agentd_req_rest_put:
	case agentd_req_rest_delete:
	    err = agentd_parse_req_config(context, data);
	    bail_error(err);
	    break;
	case agentd_req_get :
	case agentd_req_stats:
	case agentd_req_operational:
	    /* NOTE: get/stats/oper has same format of data */
	    err = agentd_parse_req_oper(context, data);
	    break;
	default:
	    lc_log_basic(LOG_NOTICE, "Unknown request type received");
	    err = 1;
	    break;
    }

bail:
    return err;
}

static int
agentd_parse_req_oper(agentd_context_t *context, xmlNode *data)
{
    int err = 0;
    char *oper_data = NULL;

    oper_data = (char *)xmlNodeGetContent(data);
    bail_null(oper_data);

    lc_log_debug(jnpr_log_level, "data: %s", oper_data);
    snprintf(context->oper_cmd, sizeof(context->oper_cmd),
	    "%s", oper_data);

bail:
    xmlFree(oper_data);
    return err;
}

static int agentd_parse_req_config(agentd_context_t *context, xmlNode *data)
{
    int err = 0;
    agentd_binding_array *new_array = NULL;
    time_t now;
    struct arg_values arg_vals;

    err = agentd_binding_array_new(&new_array);
    bail_error_null(err, new_array);

    bzero(&arg_vals, sizeof(arg_vals));

    err = agentd_convert_cfg(data->children,NULL, NULL,
	    &arg_vals,0, 1, new_array);
    bail_error(err);

    agentd_binding_array_dump("NEW", new_array, LOG_DEBUG);

    err = agentd_binding_array_sort(new_array);
    bail_error(err);

    time(&now); lc_log_debug(jnpr_log_level, "TIME3 => %s", ctime(&now));

    err = agentd_set_context_new_config(context, new_array);
    bail_error(err);

    agentd_binding_array_dump("NEW", context->new_config, jnpr_log_level);

    new_array = NULL;


bail:
    agentd_binding_array_free(&new_array);
    return err;
}

static int
agentd_set_context_new_config(agentd_context_t *context,
	agentd_binding_array *new_config)
{
    int err = 0;

    bail_null(context);
    bail_null(new_config);

    context->new_config = new_config;

bail:
    return err;
}

static int
agentd_update_distid_config(agentd_context_t *context, const char *distid,
	agentd_binding_array *new_config)
{
    int err = 0, delete_entry = 0;
    agentd_config_detail *cfg_detail = NULL, config_details;
    long distid_count = 0;

    if (new_config == NULL) {
	/* remove entry from hash table */
    }
    cfg_detail = cp_hashtable_get(context->hash_table, (void *)distid);

    if (cfg_detail == NULL) {
	if (delete_entry) {
	    /* this should not have happened, report it */
	    lc_log_basic(LOG_ERR, "Distribution %s not present in table",
		    distid);
	    goto bail;
	}
	distid_count = cp_hashtable_count(context->hash_table);
	if (distid_count > NKN_MAX_DISTRIB_ID) {
	    lc_log_basic(LOG_ERR, "Maximum of %d distrutions supported",
		    NKN_MAX_DISTRIB_ID);
	    err = 1;
	    goto bail;
	}
	/* we don't have distid details available, so add it */
	bzero(&config_details, sizeof(config_details));
	config_details.running_config = new_config;
	snprintf(config_details.dist_id, sizeof(config_details.dist_id),
		"%s", distid);
	cp_hashtable_put(context->hash_table, (void *)(distid),
		&config_details);
    } else {
	agentd_binding_array_free(&(cfg_detail->running_config));
	if (delete_entry) {
	    cp_hashtable_remove(context->hash_table, (void *)distid);
	} else {
	    /* update running config for distid */
	    cfg_detail->running_config = new_config;
	}
    }

bail:
    return err;
}
static int
agentd_get_distid_config(agentd_context_t *context, const char *distid,
	agentd_binding_array **distid_config)
{
    int err = 0;
    agentd_config_detail *cfg_detail = NULL;

    *distid_config = NULL;

    cfg_detail = cp_hashtable_get(context->hash_table, (void *)distid);

    if (cfg_detail == NULL) {
	lc_log_basic(jnpr_log_level, "distid not found: %s", distid);
	goto bail;
    }

    *distid_config = cfg_detail->running_config;
    lc_log_basic(LOG_NOTICE, "found distid : %s", cfg_detail->dist_id);

bail:
    return err;
}
static int
agentd_handle_request(agentd_context_t *context, xmlDocPtr doc)
{
    int err = 0;
    xmlNode *root_element = NULL;
    int ret_code = 0;
    temp_buf_t log_str = {0};
    agentd_binding_array *distid_config = NULL;

    bail_null(context);
    bail_null(doc);

    root_element = xmlDocGetRootElement(doc);
    bail_null(root_element);

    err = agentd_log_timestamp(agentd_req_none, "Parsing data-START");
    complain_error (err);

    err = agentd_parse_request(context, root_element);
    bail_error(err);

    err = agentd_log_timestamp(agentd_req_none, "Parsing data-END");
    complain_error(err);

    if (context->request_rejected) {
	goto bail;
    }

    /*Creating the Xpath context*/
    context->xpathCtx = xmlXPathNewContext(doc);
    if(!context->xpathCtx) {
	lc_log_basic(jnpr_log_level, ("Xpath creation for the new xml doc failed\n"));
	err = 1;
	goto bail;
    }


    switch(context->req_type) {
      case agentd_req_rest_put:
      case agentd_req_rest_post:
      case agentd_req_rest_delete:
	  /* set the running config based on distrib_id */
	  if (context->running_config != NULL) {
	      /* running_config should be NULL for rest-api support */
	      lc_log_basic(LOG_ERR, "running_config not NULL");
	      context->running_config  = NULL;
	  }
	  /* find running_config based on distribtion-id */
	  lc_log_basic(LOG_NOTICE, "request %d, for %s",context->req_type,
		  context->distrib_id);
	  err = agentd_get_distid_config(context,  context->distrib_id, &distid_config);
	  bail_error(err);

	  context->running_config = distid_config;

	  /* FALL-THROUGH */

      case agentd_req_config:
	/* this is a config request, get the diff */
	/* NOTE: not sorting running config array, as it is already sorted */
        err = agentd_log_timestamp(agentd_req_config, "Config diff-START");
	complain_error(err);
	err = agentd_get_config_diff(context, context->running_config,
		context->new_config);
	bail_error(err);
        err = agentd_log_timestamp(agentd_req_config, "Config diff-END");
	complain_error(err);

	err = agentd_handle_cfg_request(context, context->config_diff);
	bail_error(err);

	break;

      case agentd_req_operational:
      case agentd_req_get:
      case agentd_req_stats:
      case agentd_req_rest_get:
	/* for now, all three work similarly, ignoring ret_code */
        snprintf(log_str, sizeof(log_str), "Processing \'%s\'-START", context->oper_cmd);
        err = agentd_log_timestamp(context->req_type, log_str);
	complain_error(err);
	err = agentd_process_oper_request(context, &ret_code);
	bail_error(err);
        snprintf(log_str, sizeof(log_str), "Processing \'%s\'-END", context->oper_cmd);
        agentd_log_timestamp(context->req_type, log_str);
        complain_error(err);
	break;

      default:
	err = 1;
	break;
    }

bail:
    /* TODO: free config_diff array */
    /*Free the context xpath value*/
    if(context->xpathCtx) {
	xmlXPathFreeContext(context->xpathCtx);
    }
    return err;
}

tbool
agentd_match_distrib_id(const char *name, void *mf_arg)
{
    lc_log_basic(jnpr_log_level, "filename: %s", name?:"NO-NAME");
    if (false == lf_dir_matching_nodots_names_func(name, mf_arg)) {
	/* filter ".filenames" */
	return false;
    }
    if (strncmp(name, "dist-id-",
		strlen("dist-id-"))) {
	/* filter non "dist-id-%s.xml" */
	return false;
    }
    lc_log_basic(LOG_NOTICE, "adding filename: %s", name?:"NO-NAME");
    return true;
}

void *
distrib_id_copy_val_fn (void *value)
{
    if (value == NULL) return NULL;

    agentd_config_detail *cfg_detail = malloc(sizeof(agentd_config_detail));
    if (cfg_detail == NULL) return NULL;

    memcpy(cfg_detail, value, sizeof(agentd_config_detail));
    return (void *)cfg_detail;
}
static int
agentd_read_distrib_config(agentd_context_t *context)
{
    int err = 0, num_files = 0, i =0 ;
    tstr_array *agentd_distrib_files = NULL;
    cp_hashtable *hash_table = NULL;
    agentd_config_detail config_details;
    file_path_t dist_file_name = {0};

    hash_table = cp_hashtable_create_by_option(COLLECTION_MODE_NOSYNC
	    | COLLECTION_MODE_COPY
	    | COLLECTION_MODE_DEEP,
	    NKN_MAX_DISTRIB_ID,
	    cp_hash_string,
	    (cp_compare_fn) strcmp,
	    (cp_copy_fn) strdup,
	    (cp_destructor_fn) free,
	    (cp_copy_fn)distrib_id_copy_val_fn,
	    (cp_destructor_fn) free);
    bail_null(hash_table);

    context->hash_table = hash_table;

    err = lf_dir_list_names_matching("/config/nkn/", agentd_match_distrib_id,
	    NULL, &agentd_distrib_files);
    bail_error(err);

    num_files = tstr_array_length_quick(agentd_distrib_files);
    tstr_array_dump(agentd_distrib_files, "DIST-IDs");
    for (i = 0; i < num_files; i++) {
	/*
	 * XXX: should we bailout here, just report and ignore this
	 * and move-on
	 */
	snprintf(dist_file_name, sizeof(dist_file_name), "/config/nkn/%s",
		tstr_array_get_str_quick(agentd_distrib_files, i));

	err = agentd_read_config_file(context, dist_file_name);
	bail_error(err);

	bzero(&config_details, sizeof(config_details));
	config_details.running_config = context->new_config;
	snprintf(config_details.dist_id, sizeof(config_details.dist_id),
		"%s", context->distrib_id);

	cp_hashtable_put(context->hash_table, (void *)config_details.dist_id,
		&config_details);
    }

bail:
    return err;
}

static int
agentd_read_config_req(agentd_context_t *context)
{
    int err = 0;

    if (context->rapi_enabled) {
	err = agentd_read_distrib_config(context);
	bail_error(err);
    } else {
	err = agentd_read_config_file(context, AGENTD_INC_CONF_XML_FILE);
	bail_error(err);
	context->running_config = context->new_config;
	context->new_config = NULL;
    }
bail:
    return err;
}

static int
agentd_read_config_file(agentd_context_t *context, const char *fname)
{
    int err = 0;
    xmlDocPtr xml_doc = NULL;
    xmlNode *root_element = NULL;
    tbool isfile = false;

    if (fname == NULL) {
	goto bail;
    }

    lc_log_basic(LOG_INFO, "start-up: reading file %s", fname);

    /*If the XML file doesnt exists  dont do the parsing.It is OK to bail out*/
    err = lf_test_exists(fname, &isfile);
    if (!isfile) {
	lc_log_basic(LOG_INFO, "%s file doesn't exist", fname);
	goto  bail;
    }

    lc_log_basic(LOG_INFO, "reading config : %s", fname);

    xml_doc = xmlReadFile(fname,
	    NULL, XML_PARSE_NOBLANKS);
    bail_null(xml_doc);

    root_element = xmlDocGetRootElement(xml_doc);
    err = agentd_parse_request(context, root_element);

    /* agentd_parse_request() will set into new_config,
     * we need to set this into current_config
     */

    agentd_binding_array_dump("START-UP",
		context->new_config, jnpr_log_level);

bail:
    if (xml_doc) {
        xmlFreeDoc(xml_doc);
    }
    return err;
}


static int
agentd_save_config_req(agentd_context_t *context, xmlDocPtr xml_doc)
{
    int err = 0, delete_file = 0;
    FILE *fp = NULL;
    file_path_t fname = {0};

    if (context->ret_code != 0) {
	/*
	 * there is some error in xml file, don't save this not setting
	 * err =1, as this would make us to reset ret_code and ret_msg
	 */
	goto bail;
    }

    if (context->request_rejected) {
	/* just ensuring, again */
	lc_log_basic(LOG_ERR, "Request is rejected, but ret_code is non-zero");
	goto bail;
    }
    switch(context->req_type) {
        case agentd_req_config:
	    snprintf(fname, sizeof(fname), "%s",AGENTD_INC_CONF_XML_FILE);

	    /* free existing running-config */
	    agentd_binding_array_free(&(context->running_config));
	    context->running_config = context->new_config;
	    context->new_config = NULL;
	    err = agentd_mgmt_save_config(context);
	    bail_error(err);
            break;

        case agentd_req_get:
        case agentd_req_stats:
        case agentd_req_operational:
	case agentd_req_rest_get:
	    snprintf(fname, sizeof(fname), "%s", AGENTD_INC_OPER_XML_FILE);
            break;

	case agentd_req_rest_delete:
	    /* TODO: delete the file, remove entry from hash table */
	    delete_file = 1;

	    /* FALL-THROUGH */

	case agentd_req_rest_post:
	case agentd_req_rest_put:
	    err = agentd_update_distid_config(context, context->distrib_id,
		    delete_file ? NULL : context->new_config);
	    bail_error(err);

	    /* new config has been copied to dist_config */
	    context->new_config = NULL;
	    context->running_config = NULL;

	    snprintf(fname, sizeof(fname), "%s/dist-id-%s.xml",
		    AGENTD_INC_PATH, context->distrib_id);

	    err = agentd_mgmt_save_config(context);
	    bail_error(err);
	    break;
        default:
            err = 1;
            goto bail;
            break;
    }

    if (delete_file) {
	/* unlink file */
	err = lf_remove_file(fname);
	complain_error_msg(err, "Unable to delete %s", fname);
	err = 0;
    } else {
	fp = fopen(fname, "w+");
	bail_null(fp);

	xmlIndentTreeOutput = 1;
	xmlDocFormatDump(fp, xml_doc, 1);
	fclose(fp);
    }

bail:
    return err;
}

static int
agentd_init_comm(agentd_context_t *context)
{
    int socket_fd, connection_fd, err = 0;
    struct stat stats;
    struct sockaddr_un address;
    socklen_t address_length;

    /* Create agentd UDS directory, if doesnot exist */
    err = lstat (AGENTD_UDS_PATH, &stats);
    if (err && errno == ENOENT) {
        lc_log_debug (jnpr_log_level, "Creating %s", AGENTD_UDS_PATH);
        if ((err = mkdir(AGENTD_UDS_ROOT, AGENTD_UDS_MASK)) != 0) {
            if (errno != EEXIST) {
                lc_log_basic (LOG_ERR, "Error while creating %s.", AGENTD_UDS_ROOT);
                bail_error(err);
            }
        }

        if ((err = mkdir(AGENTD_UDS_PATH, AGENTD_UDS_MASK)) != 0) {
            lc_log_basic (LOG_ERR, "Error while creating %s.", AGENTD_UDS_PATH);
            bail_error(err);
        }
    }

    socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    bail_require(socket_fd >= 0);

    unlink(AGENTD_SOCKET);

    /* start with a clean address structure */
    memset(&address, 0, sizeof(struct sockaddr_un));

    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, UNIX_PATH_MAX, AGENTD_SOCKET);

    err = bind(socket_fd,
	    (struct sockaddr *) &address,
	    sizeof(struct sockaddr_un));
    bail_error(err);

    context->uds_server_fd = socket_fd;

    err = agentd_read_config_req(context);

    err = listen(socket_fd, 5);
    bail_error(err);

    chmod(AGENTD_SOCKET, 00777);

    err = lew_event_reg_fd(agentd_lew, &agentd_dmi_conx,
	    socket_fd, EV_READ, true, agentd_dmi_callback, context);
    bail_error(err);

    // Registering for signal handling
    init_signal_handling();

    err = lew_dispatch(agentd_lew, NULL, NULL);
    bail_error(err);


bail:
    close(socket_fd);
    unlink(AGENTD_SOCKET);
    return err;
}

static int
agentd_dmi_callback(int listen_fd, short event_type, void *data, lew_context *ctxt)
{
    int connection_fd = 0, err = 0;
    struct sockaddr_un address;
    socklen_t address_length;
    agentd_context_t *context = (agentd_context_t *) data;

    connection_fd = accept(listen_fd, (struct sockaddr *)&address, &address_length);
    if(connection_fd == -1) {
	if(errno == EINTR) {
	    lc_log_basic(jnpr_log_level, "select handled SIGINT\n");
	}
	lc_log_basic(jnpr_log_level, "accept call failed\n");
	goto bail;
    }
    err = connection_handler(context, connection_fd);
    close(connection_fd);

bail:
    return err;
}

static int
connection_handler(agentd_context_t *context, int connfd)
{
    int err = 0;
    xmlDocPtr xml_doc = NULL;
    time_t now;
    char *input_data = NULL;
    int input_len = 0;

    lc_log_debug(jnpr_log_level, "got data on %d fd", connfd);

    /* cleanup the context for new request */
    /* Cleanup should be done before calling any other function so 
       that bail section receives proper context when any of the function call fails */
    bzero(context->oper_cmd, sizeof(context->oper_cmd));
    bzero(context->ret_msg, sizeof(context->ret_msg));
    bzero(&(context->resp_data), sizeof(agentd_resp_data));
    bzero(&(context->distrib_id), sizeof(context->distrib_id));

    context->req_type = agentd_req_none;
    context->ret_code = 0;
    context->request_rejected = 0;
    context->num_post_commit_hndlrs = 0;

    bzero(&(context->post_commit_hndlr), sizeof(context->post_commit_hndlr));

    err = agentd_log_timestamp (agentd_req_none, "Receiving data-START");
    complain_error(err);
    err = agentd_read_input(context, connfd, &input_data, &input_len);
    bail_error(err);
    err = agentd_log_timestamp (agentd_req_none, "Receiving data-END");
    complain_error(err);

    lc_log_debug(jnpr_log_level, "ip: %p, len= %d",input_data, input_len );
    xml_doc = xmlReadMemory(input_data, input_len, NULL, NULL, XML_PARSE_NOBLANKS);
    bail_null(xml_doc);

    context->runningConfigDoc = xml_doc;


    lc_log_debug(jnpr_log_level, "ip: %p, len= %d",input_data, input_len );
//Commenting out this function
/*    err = agentd_save_incoming_req(context, xml_doc);
    bail_error(err);*/

    lc_log_debug(jnpr_log_level, "ip: %p, len= %d",input_data, input_len );
    time(&now); lc_log_debug(jnpr_log_level, "TIME21 => %s", ctime(&now));

    err = agentd_handle_request(context, xml_doc);
    bail_error(err);

    /*Dont save the config if there is error */
    lc_log_debug(jnpr_log_level, "ip: %p, len= %d",input_data, input_len );
    /* NOTE: not bailing out here */

    time(&now); lc_log_debug(jnpr_log_level, "TIME3 => %s", ctime(&now));

    err = agentd_save_config_req(context, xml_doc);
    bail_error(err);
    lc_log_debug(jnpr_log_level, "ip: %p, len= %d",input_data, input_len );

bail:
    if (err) {
	set_ret_code(context, 1);
	set_ret_msg(context, "Internal error occured, check logs at MFC");
    }
    agentd_log_timestamp (agentd_req_none, "Sending response-START"); 
    err = agentd_send_response(context, connfd);
    lc_log_debug (jnpr_log_level, "agentd_send_response returned %d", err);
    agentd_log_timestamp (agentd_req_none, "Sending response-END"); 

    lc_log_debug(jnpr_log_level, "ip-ptr: %p", input_data);
    safe_free(input_data);
    if (xml_doc) {
	xmlFreeDoc(xml_doc);
    }
    xmlCleanupParser();
    /* free changes_list array also */

    /*TODO: Do we need to cleanup context here??*/
    context->conf_flags=0;
    tstr_array_free (&context->op_cmd_parts);
    agentd_binding_array_free(&(context->config_diff));
    agentd_binding_array_free(&(context->new_config));
    return err;

}
static int
agentd_read_input(agentd_context_t *context, int connfd,
	        char **input, int *input_len)
{
    int err = 0;
    int n =0;
    char *data = NULL;
    unsigned long total_out_size = 0;
    agentd_cgi_hdr hdr = {0};

    context = context;

    n = read (connfd, &hdr, sizeof(hdr));

    if (n < 0) {
	err = 1;
        lc_log_basic (LOG_ERR, "Reading input failed - %s", strerror(errno));
	goto bail;
    }
    lc_log_debug (jnpr_log_level, "Message length : %ld", hdr.data_len);
    data = (char *)malloc (hdr.data_len+1);
    if (data == NULL) {
        err = 1 ;
        lc_log_basic (LOG_ERR, "Malloc for %ld bytes failed",hdr.data_len);
        goto bail;
    }
    memset (data, 0, hdr.data_len+1);
    n = 0;
    while(total_out_size != hdr.data_len) {
	n = read(connfd,&(data[total_out_size]),hdr.data_len-total_out_size);
	if (n < 0) {
            lc_log_basic (LOG_ERR, "Reading input failed - %s", strerror(errno));
	    err = 1;
	    goto bail;
	} 
        total_out_size += n;
    }

    *input  = data; 
    *input_len = total_out_size;
    lc_log_debug (jnpr_log_level, "Output: Message len: %d, msg: %s", *input_len, *input);
    data = NULL; /* data should not be freed unless bailing for error */

bail:
    safe_free(data);
    return err;
}

static int
agentd_send_response(agentd_context_t *context,int connfd)
{
    int err = 0;
    int n = 0;
    int sent = 0;
    xmlDocPtr resp_doc = NULL;
    xmlChar *resp_buf = NULL;
    int resp_buf_len = 0;
    agentd_cgi_message * resp = NULL;
    int total_resp_len = 0;

    switch(context->req_type) {
	case agentd_req_config:
	case agentd_req_rest_put:
	case agentd_req_rest_post:
	case agentd_req_rest_delete:
            resp_doc = agentd_mfc_resp_create (context);
            bail_null(resp_doc);
	    break;
	case agentd_req_get:
	case agentd_req_stats:
	case agentd_req_operational:
            resp_doc = agentd_mfc_resp_create (context);
            bail_null(resp_doc);
            err = agentd_mfc_resp_set_data (context, resp_doc);
            bail_error (err);
	    break;
	default:
	    lc_log_basic(LOG_NOTICE, "Unknown req-type => %d", context->req_type);
            set_ret_code(context, 1);
            set_ret_msg (context, "Unable to parse request data");
            resp_doc = agentd_mfc_resp_create(context);
	    bail_null(resp_doc); 
	    break;
    }
    xmlDocDumpFormatMemory (resp_doc, &resp_buf, &resp_buf_len,1);
/*    lc_log_debug (jnpr_log_level, "Response to MFCd: %s", resp_buf); */
    total_resp_len = sizeof(agentd_cgi_message) + resp_buf_len;
    resp = (agentd_cgi_message *) malloc (total_resp_len);
    if (resp == NULL) {
        lc_log_basic (LOG_ERR, "Error allocating memory for agentd response");
        err = 1;
        goto bail;
    }
    bzero (resp, total_resp_len);
    resp->hdr.data_len = resp_buf_len;
    memcpy(resp->mfc_data, resp_buf, resp_buf_len);

    while (sent < total_resp_len) {
        n = write(connfd, resp+sent, total_resp_len-sent);
        if (n <= 0) {
            err = errno;
            lc_log_basic (LOG_ERR, "Sending response failed with err: %d", err);
	    goto bail;
        }
        sent += n;
    }
    lc_log_debug(jnpr_log_level, "Response sent %d/%d - data len: %d",
		sent, total_resp_len, resp_buf_len);

bail:
    if (resp) {
        free (resp);
        resp = NULL;
    }
    if (resp_buf) {
        xmlFree (resp_buf);
    }
    if (resp_doc) {
        xmlFreeDoc (resp_doc);
    }
    return err;
}

static int
agentd_binding_add_changes(agentd_binding_array *changes,
	agentd_binding_array *source, int arr_index, int flag)
{
    int err = 0, arr_len = 0, i = 0;
    agentd_binding *dup_binding = NULL, *src_binding = NULL;

    arr_len = agentd_binding_array_length_quick(source);
    for (i = arr_index; i < arr_len; i++) {
	src_binding  = agentd_binding_array_get_quick(source, i);
	bail_null(src_binding);

	err = agentd_binding_dup(src_binding, &dup_binding);
	bail_error_null(err, dup_binding);

	dup_binding->flags |= flag;
	err = agentd_binding_array_append_takeover(changes, &dup_binding);
	bail_error(err);
    }
bail:
    /* in case there is error, free memory, else it will be NULL */
    agentd_binding_free(&dup_binding);
    return err;
}

static void init_signal_handling(void)
{
    struct sigaction action_cleanup;
    memset(&action_cleanup, 0, sizeof(struct sigaction));
    action_cleanup.sa_handler = exitclean;
    action_cleanup.sa_flags = 0;
    sigaction(SIGTERM, &action_cleanup, NULL);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);
    signal(SIGINT, exitclean);
}

static void exitclean(int signal_num)
{
    int err = 0;
    lc_log_basic(jnpr_log_level, "SIGNAL: %d received for agentd\n",
	    signal_num);
    catcher(signal_num);
    err = agentd_deinit_modules();
    if (err) {
        lc_log_basic (LOG_ERR, "agentd_deinit_modules() returned error");
    }
    err = agentd_unload_modules();
    if (err) {
        lc_log_basic (LOG_ERR, "agentd_unload_modules() returned error");
    }
};

int agentd_log_timestamp (int req_type, const char * msg){
    int err = 0;
    lt_time_ms curr_time = 0;
    char *time_str = NULL;

    curr_time = lt_curr_time_ms();
    err = lt_datetime_ms_to_str (curr_time, true, &time_str);
    bail_error (err);

    if (req_type == agentd_req_none && (env_prov_timestamp || env_rpc_timestamp)) {
        lc_log_basic(LOG_NOTICE, "%s: %s", msg, time_str);
    } else if (req_type == agentd_req_config && env_prov_timestamp) {
        lc_log_basic(LOG_NOTICE, "PROVISIONING- %s: %s", msg, time_str);
    } else {
        if (env_rpc_timestamp) {
            lc_log_basic(LOG_NOTICE, "RPC- %s: %s", msg, time_str);
        }
    }   

bail: 
    safe_free(time_str);
    return err;
}

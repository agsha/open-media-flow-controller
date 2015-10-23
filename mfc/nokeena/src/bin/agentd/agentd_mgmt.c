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
#include <glib.h>
#include "agentd_conf_omap.h"
#include "agentd_op_cmds_base.h"

/*
    1050 Config request Failed
    1051 Unable to find Translational entry
    1052 lookup failed for operational command
*/

extern int jnpr_log_level;
extern int ign_fail;

mfc_agentd_nd_trans_tbl_t mfc_agentd_nd_trans_tbl[] =
{
#include "translation/rules_http_instance.inc"
#include "translation/rules_http_instance_delete.inc"
#include "translation/rules_origin_map.inc"
#include "translation/rules_origin_map_delete.inc"
#include "translation/rules_custom_entries.inc"
#include "translation/rules_interface.inc"
#include "translation/rules_interface_delete.inc"
};


static int main_loop(void);
static int agentd_mgmt_process(agentd_context_t *context, int init_value);
static int agentd_mgmt_event_registration_init(void);
static int daemon_handle_signal(int signum, short ev_type, void *data, lew_context *ctxt);
static int agentd_mgmt_handle_session_close(int fd, fdic_callback_type cbt,
                              void *vsess, void *gsec_arg);
static int agentd_mgmt_func(void * arg);
static int agentd_process_request(agentd_context_t *context, agentd_binding_array *change_list,
    tstring **ret_msg, int *ret_code);

static int agentd_process_config_request(agentd_context_t *context,
    agentd_binding_array *change_list, int *ret_code);

static int agentd_lookup_translation(agentd_context_t *context, const char *pattern,
    trans_entry_t **node_details, int *ret_code);
static int init_agentd_translation_table(void);
static int agentd_handle_diff_binding(
        agentd_context_t *context, agentd_binding *change,
    trans_entry_t *node_details, int *ret_code);

static int agentd_handle_pre_commit(agentd_context_t *context,
        agentd_binding_array *change_list);

static int agentd_handle_post_commands(agentd_context_t *context, uint32 *ret_code,
    tstring **ret_msg);

static int agentd_handle_post_bindings(agentd_context_t *context,
    agentd_binding *binding);

static int agentd_copy_post_cmds(agentd_context_t *context);

static int agentd_set_req_msg_append_bindings_takeover (agentd_context_t * context, bn_request * req);

static int agentd_handle_post_commit (agentd_context_t *context);

static int agentd_insert_special_bindings (agentd_context_t * context, agentd_binding_array *change_list);

const char        * program_name = "jnpr-agentd";
lew_context       * agentd_lew = NULL;
md_client_context * agentd_mcc = NULL;
tbool               agentd_exiting = false;
static pthread_mutex_t upload_mgmt_agentd_lock;
int                 update_from_db_done = 0;
GHashTable * ht;

/* Definition of structure to map the xpath pattern in the configuration to the translation pattern.
*/
typedef struct _agentd_cfg_path_pattern_map_s {
    const char *xpath_pattern; /* xpath pattern to search for in the config xml */
    const char *trans_pattern; /* corresponding translation pattern to be added to the change list bindings */
} agentd_cfg_path_pattern_map_t;

/* List of translation patterns to be added to the change list forcible as a special case.
   This list is processed by agentd_insert_special_bindings( ). 
   If the xpath_pattern in the list is found in the cfg xml, then the trans_pattern will be added to the 
   list of bindings to be processed, irrespective of whether the config has changed or not
*/

static agentd_cfg_path_pattern_map_t agentd_spl_bindings_list[] = {
{"//system/aaa/authentication/login/default/method", "mfc-cluster*systemaaaauthenticationlogindefault"},
};

///////////////////////////////////////////////////////////////////////////////
static int main_loop(void)
{
    int err = 0;
    lc_log_debug(LOG_DEBUG, _("main_loop () - before lew_dispatch"));

    err = lew_dispatch (agentd_lew, NULL, NULL);
    bail_error(err);
bail:
    return err;

}

/* List of signals server can handle */
static const int signals_handled[] = {
    SIGHUP, SIGINT, SIGPIPE, SIGTERM, SIGUSR1
};

#define num_signals_handled (int)(sizeof(signals_handled) / sizeof(int))

/* Libevent handles for server */
static lew_event *event_signals[num_signals_handled];

void catcher(int sig);

void catcher(int sig)
{
    switch(sig) {
	case SIGHUP:
	case SIGPIPE:
	    break;
	case SIGINT:
	case SIGTERM:
	    agentd_mgmt_initiate_exit();
	    break;
	case SIGKILL:
	    break;
	default:
	    break;
    }
}


///////////////////////////////////////////////////////////////////////////////
static int agentd_mgmt_process(agentd_context_t *context, int init_value)
{
    int err = 0;
    uint32 i = 0;

    /* UNUSED */
    init_value = init_value;

    err = lc_log_init(program_name, NULL, LCO_none,
	    LC_COMPONENT_ID(LCI_AGENTD),
	    LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
    bail_error(err);

    err = lew_init(&agentd_lew);
    bail_error(err);

    /*
     * Register to hear about all the signals we care about.
     * These are automatically persistent, which is fine.
     */
    /* register all the signals */
    for (i = 0; i < num_signals_handled; ++i) {
        err = lew_event_reg_signal
            (agentd_lew, &(event_signals[i]), signals_handled[i],
             daemon_handle_signal, NULL);
        bail_error(err);
    }


    err = agentd_mgmt_event_registration_init();
    bail_error(err);

    /*Init the  translation table here*/
    err = init_agentd_translation_table();
    if(err) {
	lc_log_basic(LOG_NOTICE, _("Translation table init failed"));
    }



bail:
    if (err) // registration failed
    {
	/* Safe to call all the exits */
	agentd_mgmt_initiate_exit ();

	/* Ensure we set the flag back */
	agentd_exiting = false;
    }

    /* Enable the flag to indicate config init is done */
    update_from_db_done = 1;

    return err;
}


static int
daemon_handle_signal(int signum, short ev_type, void *data, lew_context *ctxt)
{
    int err = 0;

    lc_log_basic(LOG_INFO, "Received %s", lc_signal_num_to_name(signum));

    switch (signum) {
    case SIGHUP:
    case SIGINT:
    case SIGTERM:
        err = agentd_mgmt_initiate_exit();
        bail_error(err);

    case SIGPIPE:
        /* Currently ignored */
        break;

    case SIGUSR1:
        /* Currently ignored */
        break;

    default:
        lc_log_basic(LOG_WARNING, I_("Got unexpected signal %s"),
                     lc_signal_num_to_name(signum));
        break;
    }

 bail:
    return(err);
}

///////////////////////////////////////////////////////////////////////////////
static int agentd_mgmt_event_registration_init(void)
{
    int err = 0;
    mdc_wrapper_params *mwp = NULL;

    err = mdc_wrapper_params_get_defaults(&mwp);
    bail_error(err);

    mwp->mwp_lew_context = agentd_lew;
    mwp->mwp_gcl_client = gcl_client_agentd;

    mwp->mwp_session_close_func = agentd_mgmt_handle_session_close;

    err = mdc_wrapper_init(mwp, &agentd_mcc);
    bail_error(err);

    err = lc_random_seed_nonblocking_once();
    bail_error(err);


    /*We wont be interested in config changes from mgmtd
     * We will trigger the change from here
     */

bail:
    mdc_wrapper_params_free(&mwp);
    if (err) {
	lc_log_debug(LOG_ERR,
		_("Unable to connect to the management daemon"));
    }
    return err;
}



///////////////////////////////////////////////////////////////////////////////
static int agentd_mgmt_handle_session_close(
	int fd,
	fdic_callback_type cbt,
	void *vsess,
	void *gsec_arg)
{
    int err = 0;
    gcl_session *gsec_session = vsess;

    if ( agentd_exiting) {
	goto bail;
    }

    lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; exiting"));
    pthread_mutex_lock (&upload_mgmt_agentd_lock);
    err = agentd_mgmt_initiate_exit();
    pthread_mutex_unlock (&upload_mgmt_agentd_lock);
    bail_error(err);


bail:
    return err;
}

int agentd_mgmt_initiate_exit(void)
{
    int err = 0;
    int i;

    if (agentd_exiting) {
	goto bail;
    }
    agentd_exiting = true;

    err = mdc_wrapper_disconnect(agentd_mcc, false);
    bail_error(err);

    err = lew_escape_from_dispatch(agentd_lew, true);
    bail_error(err);

bail:
    return err;
}

static int agentd_mgmt_func(void * arg)
{
    int err = 0;
    agentd_context_t *context = arg;

    int start = 1;
    /* Connect/create GCL sessions, initialize mgmtd i/f
     */
    agentd_exiting = false;
    if (start == 1) {
	/* initialize the gcl session */
	err = agentd_mgmt_process(context, 1);
	bail_error(err);
    }


bail:
    return err;
}



int
agentd_mgmt_init(agentd_context_t *context)
{
    int err = 0;

    err = agentd_mgmt_func(context);

    return err;
}


    int
agentd_handle_cfg_request(agentd_context_t *context, agentd_binding_array *change_list)
{
    int err = 0;
    int i = 0;
    int ret_code = 0;
    tstring *ret_msg = NULL;

    bail_null(context);
    bail_null(change_list);

    err = agentd_process_request(context, change_list, &ret_msg, &ret_code);
    bail_error(err);

    if(ret_code) {
	lc_log_basic(jnpr_log_level, _("changes has failed with error %d"), ret_code);
	goto bail;
    }


bail:
    return err;
}

static int
agentd_process_request(agentd_context_t *context,
	agentd_binding_array *change_list,tstring **ret_msg,  int *ret_code)
{
    int err = 0;
    int i;

    /*
     * Based on the type of message the req message creation will change
     * Who will provide this info the change list or the context?
     */

    switch(context->req_type) {
	case agentd_req_config:
	case agentd_req_rest_post:
	case agentd_req_rest_put:
	case agentd_req_rest_delete:
	    {
		err = agentd_array_cleanup();
		bail_error(err);
		err = agentd_process_config_request(context, change_list, ret_code);
		bail_error(err);
	    }
	    break;
	case agentd_req_get:
	    {
		/* get command handling*/
	    }
	    break;
	case agentd_req_rest_get:
	case agentd_req_operational:
	    {
		/*operational command handling*/
		err = agentd_process_oper_request(context, ret_code);
		bail_error(err);
	    }
	    break;
	default:
	    {
		set_ret_msg(context, "Unknown request type");
	    }
    }

bail:
    return err;
}


static int
agentd_process_config_request(agentd_context_t *context,
	agentd_binding_array *change_list, int *ret_code)
{
    int err = 0;
    bn_request *req = NULL;
    int i;
    agentd_binding *change = NULL;
    trans_entry_t *node_details = NULL;
    tstring *ret_msg = NULL;
    int num_changes =  0;
    uint32 code = 0;
    char  pattern[1048] = {0};
    temp_buf_t buffer = {0};


    err = agentd_log_timestamp (agentd_req_config, "Constructing mgmtd message-START");
    complain_error(err);
    /* Any change in only the order of configured items will be lost during sorting, 
       (eg: "authentication login default method" configuration).
       Those bindings should be forcibly added to the change_list, before processing 
       the bindings in the change_list.
    */ 
    err = agentd_insert_special_bindings (context, change_list);
    bail_error (err); 

    num_changes = agentd_binding_array_length_quick(change_list);
    if(num_changes) {
        if (!context->reset_bindings) {
            err = bn_binding_array_new (&context->reset_bindings);
            bail_error (err);
        }
        if (!context->mod_bindings) {
            err = bn_binding_array_new (&context->mod_bindings);
            bail_error (err);
        }
        if (!context->del_bindings) {
            err = bn_binding_array_new (&context->del_bindings);
            bail_error (err);
        }
	/*Do the loop of the change_list and  append the lookup
	 * details to the req message
	 */
	for(i = 0; i < num_changes; i++){
	    change = agentd_binding_array_get_quick(change_list, i);
	    if(change) {
		/*check for delete flag, if set then append delete keyword to it*/
		if(change->flags & AGENTD_CFG_CHANGE_DELETION){
		    snprintf(pattern, sizeof(pattern), "delete%s", change->pattern);
		}else {
		    snprintf(pattern, sizeof(pattern), "%s", change->pattern);
		}

		err = agentd_lookup_translation(context, pattern, &node_details, ret_code);
			/* TODO - check this. Function never returns error */
		if(err) {
		    if (ign_fail == 1) {
			lc_log_basic (LOG_NOTICE, "Ignoring failed lookup for [ %s ]", pattern);
			continue;
		    }
		    /*For now till translation table is filled*/
		    lc_log_basic(LOG_NOTICE, _("Failed lookup for [ %s ]"), pattern);
		    goto cleanup;
		}
		//bail_error(err);
		if (node_details == NULL) {
		    if (ign_fail == 1) {
			lc_log_basic (LOG_NOTICE, "Ignoring missing node details for [ %s ]", pattern);
			continue;
		    }
		    /* TODO: make this a error */
		    lc_log_debug(LOG_NOTICE, "node-details for pattern %s not found", pattern);
		    set_ret_code(context, TRANS_LKUP_FAILED);
		    snprintf(buffer, sizeof(buffer),
			    "Node details are missing for  %s", change->pattern);
		    set_ret_msg(context, buffer);
		    goto cleanup;
		}
		err = agentd_handle_diff_binding(context, change,
			node_details, ret_code);
		bail_error(err);
	    }else {
		continue;
	    }
	}

	err = agentd_handle_pre_commit(context,change_list);
	bail_error(err);

	/*Allocate for the set request message*/
	err = bn_set_request_msg_create(&req, 0);
	bail_error_null(err, req);

        /* Note: Adding bindings to the request should be done just before sending
           the request to mgmtd */
        err = agentd_set_req_msg_append_bindings_takeover (context, req);
        bail_error (err);

	bn_request_msg_dump(NULL, req, jnpr_log_level, "SET-REQ");
        err = agentd_log_timestamp (agentd_req_config, "Constructing mgmtd message-END");
        complain_error(err);

	/*Send it to mgmtd*/
        err = agentd_log_timestamp (agentd_req_config, "Send mgmtd message-START");
        complain_error(err);
	err = mdc_send_mgmt_msg(agentd_mcc, req, false, NULL, &code, &ret_msg);
	bail_error(err);
        err = agentd_log_timestamp (agentd_req_config, "Send mgmtd message-END");
        complain_error(err);

	if (code) {
	    /* message has failed, goto bail */
	    lc_log_basic(LOG_NOTICE, "msg from mgmtd: %d(%s)",
		    code, ts_str_maybe_empty(ret_msg));
	    goto bail;
	}

	lc_log_basic(LOG_INFO, "msg from mgmtd: %d(%s)",
		code, ts_str_maybe_empty(ret_msg));

        err = agentd_log_timestamp (agentd_req_config, "Post commit handling-START");
        complain_error(err);
	err = agentd_handle_post_commands(context, &code, &ret_msg);
	bail_error(err);
	if (code) {
	    /* message has failed, goto bail */
	    lc_log_basic(LOG_NOTICE, "msg(post) from mgmtd: %d(%s)",
		    code,ts_str_maybe_empty(ret_msg));
	    goto bail;
	}
	lc_log_basic(LOG_INFO, "msg(post) from mgmtd: %d(%s)",
		code,ts_str_maybe_empty(ret_msg));

        /* Handle post commit actions */ 
        err = agentd_handle_post_commit(context);
        bail_error(err);
        err = agentd_log_timestamp (agentd_req_config, "Post commit handling-END");
        complain_error(err);
    }

bail:
    if (err) {
	set_ret_code(context, 1);
	set_ret_msg(context, "Internal error occured, Please check logs at MFC");
    } else {
	set_ret_code(context, code);
	set_ret_msg(context, ts_str_maybe_empty(ret_msg));
    }
cleanup:
    ts_free(&ret_msg);
    bn_request_msg_free(&req);
    bn_binding_array_free(&context->reset_bindings);
    context->reset_bindings = NULL;
    bn_binding_array_free(&context->mod_bindings);
    context->mod_bindings = NULL;
    bn_binding_array_free(&context->del_bindings);
    context->del_bindings = NULL;
    return err;

}

int set_ret_msg(agentd_context_t *context, const char *message)
{

    int err;

    bail_null(context);
    bail_null(message);

    lc_log_basic(jnpr_log_level," Last ret_msg: %s", context->ret_msg);

    snprintf(context->ret_msg, sizeof(context->ret_msg), "%s",message);

bail:
    return err;
}

int set_ret_code(agentd_context_t *context, int ret_code)
{

    int err = 0;
    if (context->ret_code != 0) {
	/* value is already set, don't reset it */
    } else {
	context->ret_code = ret_code;
    }
    return err;

}
static int
agentd_lookup_translation(agentd_context_t *context, const char *pattern, trans_entry_t **node_details, int *ret_code)
{
    int err = 0;
    *node_details = NULL;
    temp_buf_t buffer = {0};

    //  bail_null_error(ht, "Tranlsation table is not initialized");
    /* This will change to GNU hash table search*/
    *node_details = (trans_entry_t *)g_hash_table_lookup(ht, pattern);
    if(!(*node_details)) {
	lc_log_basic(LOG_DEBUG, "Unable find translation for the cmd %s", pattern);
	set_ret_code(context, TRANS_LKUP_FAILED);
	snprintf(buffer, sizeof(buffer),
		"Translation entry not found for %s", pattern);
	set_ret_msg(context, buffer);
	return *ret_code;
    }
bail:
    return err;
}

static int init_agentd_translation_table(void)
{
    uint32 i = 0;
    lc_log_debug(jnpr_log_level, "initing the trans table");


    ht = g_hash_table_new(g_str_hash, g_str_equal);

    if(!ht)
    {
	lc_log_debug(jnpr_log_level, "Error :initing the trans table");
	return -1;
    }

    for(i = 0; i < ND_TBL_SIZE; i++)
    {
	const  trans_entry_t *node_details = NULL;
	char *cmd = (char *) mfc_agentd_nd_trans_tbl[i].cmd;
	node_details = (trans_entry_t *)g_hash_table_lookup(ht, cmd);
	if(node_details) {
	    //Duplicate entry
	    lc_log_basic(LOG_NOTICE, _("Duplicate entry for pattern:[%s]\n"), cmd);
	}
	trans_entry_t      *trans_entry = &(mfc_agentd_nd_trans_tbl[i].trans_entry);
	g_hash_table_insert(ht, cmd, trans_entry);
    }


    lc_log_debug(jnpr_log_level, "Done :initing the trans table");
    return 0;
}


static int
agentd_handle_diff_binding(agentd_context_t *context, agentd_binding *change,
	trans_entry_t *node_details, int *ret_code)
{
    int err = 0;
    cust_obj_t *custom_entry = NULL;

    switch (node_details->trans_entry_type) {
	case CUST_ENTRY :
	    custom_entry = (cust_obj_t *)&node_details->trans_item;
	    bail_require(custom_entry);
	    bail_require(custom_entry->func);

	    err = custom_entry->func(context, change, custom_entry->arg, NULL);
	    bail_error(err);
	    break;

	case ND_ENTRY:
	    err = agentd_fill_request_msg(context, change,
		    &node_details->trans_item.mgmt_nd_data_lst, ret_code);
	    bail_error(err);
	    break;

	default:
	    /*not handling other type as of now */
	    lc_log_basic(LOG_NOTICE, "unknown entry-type recvd: %d",
		    node_details->trans_entry_type);
	    break;
    }
bail:
    return err;

}

static int agentd_handle_pre_commit(agentd_context_t *context, agentd_binding_array *change_list)
{
    int err = 0;

    lc_log_debug(jnpr_log_level, "Working on pre_commit.. %ld", (unsigned long) context->config_diff);
    if ((context->conf_flags & AF_REGEN_SMAPS_XML) || (context->conf_flags & AF_DELETE_SMAP_MEMBERS))
    {
        err = agentd_regen_smap_xml(context);
        bail_error(err);
    }
    if (context->conf_flags & AF_SET_AUTH_ORDER) {
        err = agentd_configure_login_method (context);
        bail_error (err);
    }

bail:
    return err;
}

int
agentd_mgmt_save_config(agentd_context_t *context)
{
    int err = 0;
    uint32 ret_code = 0;
    tstring *ret_msg = NULL;

    err = mdc_send_action(agentd_mcc, &ret_code, &ret_msg,
	    "/mgmtd/db/save");
    bail_error(err);

    if (ret_code) {
	lc_log_basic(LOG_WARNING, "saving config failed (%d: %s)",
		ret_code, ts_str_maybe_empty(ret_msg));
    }

bail:
    ts_free(&ret_msg);
    return err;
}

static int agentd_handle_post_commands(agentd_context_t *context, uint32 *ret_code,
	tstring **ret_msg)

{
    int err = 0, j = 0, i = 0;
    uint32_t code = 0;
    tstring *msg = NULL;
    bn_request *req = NULL;
    agentd_binding *abinding = NULL;
    char pattern[1024] = {0};
    char *pattern_ptr = NULL;

    if (context->num_post_cmds == 0 ) {
	/* no commands to handle */
	goto bail;
    }

    if (ret_code) {
	*ret_code = 0;
    }
    if (ret_msg) {
	*ret_msg = NULL;
    }

    /* not handling more than 8 sequencing */
    while(j++ < 8) {
	if (context->num_post_cmds == 0 ) {
	    /* no more commands to handle */
	    break;
	}

	lc_log_debug(jnpr_log_level, "POST iteration: %d", j);

	err = bn_set_request_msg_create(&req, 0);
	bail_error_null(err, req);

	/* making a copy post commands */
	agentd_copy_post_cmds(context);

	for (i = 0; i < context->num_handle_post_cmds; i++) {
	    /* iterate over all the commands and handle them */
	    abinding = context->handle_post_cmds[i];
	    if (abinding == NULL) {
		continue;
	    }
	    err = agentd_handle_post_bindings(context, abinding);
	    bail_error(err);
	}

        /* Note: Adding bindings to the request should be done just before sending
           the request to mgmtd */
        err = agentd_set_req_msg_append_bindings_takeover (context, req);
        bail_error (err);

	bn_request_msg_dump(NULL, req, jnpr_log_level, "POST-SET-REQ");
	ts_free(&msg);
	err = mdc_send_mgmt_msg(agentd_mcc, req, false, NULL, &code, &msg);
	bail_error(err);

	if (code) {
	    /* somthing failed */
	    break;
	}
    }

    *ret_code = code;
    *ret_msg = msg;
    msg = NULL;

bail:
    ts_free(&msg);
    bn_request_msg_free(&req);
    return err;
}

static int
agentd_handle_post_bindings(agentd_context_t *context,
	agentd_binding *abinding)
{
    int err = 0;
    char *pattern_ptr = abinding->pattern;
    uint32_t flags = abinding->flags;


    agentd_binding_dump(jnpr_log_level, "POST_CMD", abinding);
    /* following MUST be refactored */
    if (0 == strcmp(pattern_ptr,PREPEND_TRANS_STR"servicehttpinstance*")
	    && (flags & AGENTD_CFG_CHANGE_DELETION)) {
	err = agentd_namespace_delete(context, abinding, NULL, NULL);
	bail_error(err);

/*    } else if (0 == strcmp(pattern_ptr,
		PREPEND_TRANS_STR"service-elementsresource-pool*instance-listinstance*")
	    && (flags & AGENTD_CFG_CHANGE_DELETION)) {
	err = agentd_rp_delete_instance(context, abinding, NULL, NULL);
	bail_error(err);

    } else if (0 == strcmp(pattern_ptr,
		PREPEND_TRANS_STR"service-elementsresource-pool*instance-listinstance*")
	    && (flags & AGENTD_CFG_CHANGE_ADDITION)) {
*/ /* TODO - chk if this post handling is needed now */
    } else if (0 == strcmp(pattern_ptr,
		PREPEND_TRANS_STR"servicehttpinstance*pre-stageftp-user*password*")
	    && (flags & AGENTD_CFG_CHANGE_ADDITION)){
	err = ns_prestage_ftp_password(context, abinding, NULL, NULL);
	bail_error(err);

    } else {
	lc_log_debug(jnpr_log_level, "post binding not handled : %s (%d)", pattern_ptr, flags);
	err = 1;
    }

bail:
    return err;
}

static int
agentd_copy_post_cmds(agentd_context_t *context)
{
    int i = 0;
    for (i = 0; i < context->num_post_cmds; i++) {
	context->handle_post_cmds[i] = context->post_cmds[i];
    }
    context->num_handle_post_cmds = context->num_post_cmds;
    context->num_post_cmds = 0;
    return 0;
}

static int agentd_handle_post_commit(agentd_context_t *context)
{
    int err = 0, i = 0;
    agentd_binding *abinding = NULL;
    void *cb_arg;
    binding_hdlr abinding_hndlr = NULL;

    if (context->num_post_commit_hndlrs == 0 ) {
        /* nothing to handle */
        goto bail;
    }

    for (i = 0; i < context->num_post_commit_hndlrs; i++) {
            /* iterate over all the commands and handle them */
        abinding = context->post_commit_hndlr[i].abinding;
        cb_arg = context->post_commit_hndlr[i].cb_arg;
        abinding_hndlr = context->post_commit_hndlr[i].cb_func;
      
        err = abinding_hndlr(context, abinding, cb_arg);
        if (cb_arg) {
            free(cb_arg);
            context->post_commit_hndlr[i].cb_arg = NULL;
        }
        bail_error(err);
    }
    context->num_post_commit_hndlrs = 0;

bail:
    return err;
}

int
agentd_save_post_handler (agentd_context_t * context,
    agentd_binding *abinding, void *cb_arg, int cb_arg_len, binding_hdlr cb_func) {
    int err = 0;

    bail_null (context);
    bail_null (cb_func);
    if (context->num_post_commit_hndlrs >= AGENTD_MAX_POST_HANDLERS) {
        lc_log_basic (LOG_ERR, "Cannot save post handler. Array is full");
        return ENOMEM;
    }

    context->post_commit_hndlr[context->num_post_commit_hndlrs].abinding = abinding;
    if (cb_arg && cb_arg_len) {
        context->post_commit_hndlr[context->num_post_commit_hndlrs].cb_arg = malloc(cb_arg_len +1);
        if (context->post_commit_hndlr[context->num_post_commit_hndlrs].cb_arg != NULL) {
            bzero (context->post_commit_hndlr[context->num_post_commit_hndlrs].cb_arg, cb_arg_len +1);
            memcpy(context->post_commit_hndlr[context->num_post_commit_hndlrs].cb_arg,
                    cb_arg, cb_arg_len);
        } else {
            lc_log_basic (LOG_ERR, "Cannot save post handler. malloc failed");
            return ENOMEM;
        }
    }
    context->post_commit_hndlr[context->num_post_commit_hndlrs].cb_func = cb_func;
    context->num_post_commit_hndlrs++;

bail:
    return err;
}

/************** Functions used for constructing XML response to mfcd ***************/

/* Creates the xml document for constructing response to mfc.
   Sets the root & header element */
xmlDocPtr agentd_mfc_resp_create (agentd_context_t * context) {
    xmlNodePtr rootnode = NULL;
    xmlDocPtr mfcRespDoc = NULL;
    xmlNodePtr header = NULL;
    xmlNodePtr status = NULL;
    xmlNodePtr code = NULL;
    xmlNodePtr message = NULL;
    str_value_t status_str = {0};
    int err = 1;

    bail_null (context);
    mfcRespDoc = xmlNewDoc (BAD_CAST "1.0");
    bail_null (mfcRespDoc);
    rootnode = xmlNewNode(NULL, BAD_CAST AGENTD_STR_MFC_RESPONSE);
    bail_null (rootnode);
    xmlDocSetRootElement (mfcRespDoc, rootnode);

    /* Add header node */
    header = xmlNewNode(NULL, BAD_CAST AGENTD_STR_HEADER);
    bail_null (header);
    xmlAddChild (rootnode, header);
    status = xmlNewNode(NULL, BAD_CAST AGENTD_STR_STATUS);
    bail_null (status);
    xmlAddChild (header, status);
    code = xmlNewNode(NULL, BAD_CAST AGENTD_STR_CODE);
    bail_null (code);
    snprintf(status_str, sizeof(status_str), "%d", context->ret_code);
    xmlNodeSetContent(code, BAD_CAST status_str);
    xmlAddChild (status, code);
    message = xmlNewNode (NULL, BAD_CAST AGENTD_STR_MESSAGE);
    bail_null (message);
    xmlNodeSetContent(message, BAD_CAST context->ret_msg);
    xmlAddChild (status, message);
    err = 0;
bail:
    if (err) {
        if (mfcRespDoc) {
            xmlFreeDoc (mfcRespDoc);
            mfcRespDoc = NULL;
        }
    }
    
    return mfcRespDoc;
}

int agentd_mfc_resp_open_container (agentd_context_t * context, unsigned int node_name) {
    xmlNodePtr parent = NULL;
    xmlNodePtr newnode = NULL;
    int err = 1;

    bail_null (context);
    parent = context->resp_data.pnode;
    if (parent == NULL) {
        context->resp_data.data = xmlNewNode (NULL, BAD_CAST AGENTD_STR_DATA);
        parent = context->resp_data.data;
    } 
    bail_null (parent);
    newnode = xmlNewNode(NULL,  BAD_CAST odl_juniper_media_flow_controller_tags[node_name].xt_name);
    bail_null (newnode);
    xmlAddChild (parent, newnode);
    context->resp_data.pnode = newnode;
    err = 0;
bail:
    return err;
} 

/* Adds an element node and its content under the given parent node. */
int agentd_mfc_resp_add_element (agentd_context_t * context, unsigned int  node_name,
                                        const char * node_content) {
    xmlNodePtr parent = NULL;
    xmlNodePtr child = NULL;
    int err = 1;

    bail_null (context);
    parent = context->resp_data.pnode;
    bail_null (parent);

    if (node_content) { /* Allow node to be created for zero-length content */
        child = xmlNewNode(NULL, BAD_CAST odl_juniper_media_flow_controller_tags[node_name].xt_name);
        bail_null (child);
        xmlNodeSetContent (child, BAD_CAST node_content);
        xmlAddChild(parent, child);
    } else {
        lc_log_debug (LOG_INFO, "Node content is NULL");
    }
    err = 0;
bail:
    return err;
}

int agentd_mfc_resp_close_container (agentd_context_t * context, unsigned int node_name) {
    xmlNodePtr new_pnode = NULL;
    int err = 1;

    bail_null (context); 
    new_pnode = context->resp_data.pnode->parent;
    bail_null(new_pnode);/* This cannot happen */
    context->resp_data.pnode = new_pnode;
    err = 0;
bail:
    return err;
}

/* Adds "data" element to response document */
int agentd_mfc_resp_set_data (agentd_context_t * context, xmlDocPtr mfcRespDoc) {
    xmlNodePtr root = NULL;
    int err = 1;

    bail_null (context);
    bail_null (mfcRespDoc);
    if (context->resp_data.data != NULL) {
        root = xmlDocGetRootElement (mfcRespDoc);
        bail_null (root);
        xmlAddChild (root, context->resp_data.data);
    }
    err = 0;
bail:
    return err;
}
/***********************************************************************************************/

static int agentd_set_req_msg_append_bindings_takeover (agentd_context_t * context, bn_request * req) {
    int num_bindings = 0, i = 0;
    bn_binding * binding = NULL;
    int err = 0;

    /* Bindings with "reset" op goes first into the set-request */
    num_bindings = bn_binding_array_length_quick (context->reset_bindings);
    for (i=0; i<num_bindings; i++) {
        err = bn_binding_array_remove_no_shift (context->reset_bindings, i, &binding);
        bail_error_null (err, binding);
        err = bn_set_request_msg_append_binding_takeover (req, bsso_reset, 0, &binding, NULL);
        bail_error (err);
    }
    bn_binding_array_compact_empty_slots (context->reset_bindings);
    /* Bindings with "modify" op goes next into the set-request */
    num_bindings = bn_binding_array_length_quick (context->mod_bindings);
    for (i=0; i<num_bindings; i++) {
        err = bn_binding_array_remove_no_shift (context->mod_bindings, i, &binding);
        bail_error_null (err, binding);
        err = bn_set_request_msg_append_binding_takeover (req, bsso_modify, 0, &binding, NULL);
        bail_error (err);
    }
    bn_binding_array_compact_empty_slots (context->mod_bindings);
    /* Bindings with "delete" op goes last into the set-request */
    num_bindings = bn_binding_array_length_quick (context->del_bindings);
    for (i=0; i<num_bindings; i++) {
        err = bn_binding_array_remove_no_shift (context->del_bindings, i, &binding);
        bail_error_null (err, binding);
        err = bn_set_request_msg_append_binding_takeover (req, bsso_delete, 0, &binding, NULL);
        bail_error (err);
    }
    bn_binding_array_compact_empty_slots (context->mod_bindings);
bail:
    return err;
}
           
int agentd_register_cmds_array (mfc_agentd_nd_trans_tbl_t * trans_arr){
    int err = 0, i = 0;
    const  trans_entry_t *node_details = NULL;
    char * cmd = NULL;
   
    bail_null (trans_arr); 
    bail_null(ht);

    lc_log_debug (jnpr_log_level, "agentd_register_cmds_array called");
    cmd = (char *) trans_arr[i].cmd;
    while (cmd != NULL) {
        err = agentd_register_cmd(&trans_arr[i]);
        bail_error (err);
        cmd = (char *)trans_arr[++i].cmd;
    }

bail:
    return err;
}

int agentd_register_cmd (mfc_agentd_nd_trans_tbl_t * trans_details) {
    int err = 0;
    const  trans_entry_t *node_details = NULL;
    char * cmd = NULL;

    bail_null(ht);
    bail_null(trans_details);

    cmd = (char *) trans_details->cmd;
    bail_null(cmd);

    node_details = (trans_entry_t *)g_hash_table_lookup(ht, cmd);

    if(node_details) {
        //Duplicate entry
        lc_log_basic(LOG_NOTICE, _("Duplicate entry for pattern:[%s]\n"), cmd);
    }
    g_hash_table_insert(ht, cmd, &(trans_details->trans_entry));

bail:
    return err;
}

/* This function adds bindings that were lost due to sorting, in the change_list.
*/
static int agentd_insert_special_bindings (agentd_context_t * context, agentd_binding_array *change_list){
    int err = 0, i = 0, num_spl_bindings=0;
    temp_buf_t xpath_pattern = {0};
    node_name_t node_name = {0};
    xmlXPathObjectPtr xpathObj = NULL;
    struct arg_values arg_vals;

    bail_null (change_list);

    num_spl_bindings = sizeof(agentd_spl_bindings_list)/sizeof(agentd_cfg_path_pattern_map_t);
    lc_log_debug (jnpr_log_level, "Number of spl bindings: %d", num_spl_bindings);

    for (i=0; i<num_spl_bindings; i++) {
        snprintf(xpath_pattern, sizeof(xpath_pattern), "%s",
                          agentd_spl_bindings_list[i].xpath_pattern);
        lc_log_debug (jnpr_log_level, "Xpath pattern: %s", xpath_pattern);
        xpathObj = xmlXPathEvalExpression((const xmlChar *) xpath_pattern, context->xpathCtx);
        if (xpathObj && xpathObj->nodesetval->nodeNr) {
            /* Frame the pattern and add it to the change list */
            bzero (&arg_vals, sizeof(arg_vals));
            snprintf (node_name, sizeof(node_name), "/agentd/dummy_binding/%d", i);
            lc_log_debug (jnpr_log_level, "Xpath found node. Adding trans pattern: %s", 
                                           agentd_spl_bindings_list[i].trans_pattern);
            err = agentd_convert_cfg(xpathObj->nodesetval->nodeTab[0],node_name, 
                  agentd_spl_bindings_list[i].trans_pattern, 
                  &arg_vals, 0, 0, change_list);
            bail_error (err);
        }
        if(xpathObj) {
            xmlXPathFreeObject(xpathObj);
            xpathObj = NULL;
        }
   }   
bail:
    if(xpathObj) {
        xmlXPathFreeObject(xpathObj);
    }

    return err;
}

/* This function returns the TM string that maps to the passed JUNOS string. */

const char * agentd_util_get_tm_str (agentd_string_map *str_map, const char * junos_str) {
    uint32 i = 0;
    int err = 0;

    if (!str_map) {
        lc_log_debug (jnpr_log_level, "Null map passed");
        return NULL;
    }
    if (!junos_str) {
        lc_log_debug (jnpr_log_level, "Null string passed");
        return NULL;
    }

    while (str_map[i].tm_str) {
        if (!strcmp(junos_str, str_map[i].junos_str)) {
            lc_log_debug (jnpr_log_level, "TM string \'%s\' maps to JunOS string \'%s\'",
                                            str_map[i].tm_str, junos_str);
            break;
        }
        i++;
    }
    return str_map[i].tm_str;
}


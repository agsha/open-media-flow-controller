/*
 *
 * Filename:  watchdog_mgmt.c
 * Date:      2010/03/25
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-2009 Ankeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include "md_client.h"
#include "mdc_wrapper.h"
#include "watchdog_main.h"
#include "random.h"
#include "proc_utils.h"
#include "string.h"

// #define DELAY_QUERIES
#define MAX_QUERIES 1000

#define	NS_DEFAULT_PORT		"80"
#define	ALL_STR			"all"

lew_event *watchdog_query_timers[MAX_QUERIES];
int watchdog_next_query_idx = 0;
static const char network_config_prefix[]   = "/nkn/nvsd/network/config";
extern int watchdog_set_watcdog_ns_origin(const char *intf);

extern char *wd_nvsd_pid;
/* ------------------------------------------------------------------------- */
/** Local prototypes & definitions
 */
md_client_context *watchdog_mcc = NULL;

static int watchdog_mgmt_handle_session_close(int fd, fdic_callback_type cbt,
                                        void *vsess, void *gswatchdog_arg);

static int watchdog_mgmt_handle_event_request(gcl_session *sess,
                                        bn_request **inout_request, void *arg);

static int watchdog_mgmt_handle_query_request(gcl_session *sess,
                                        bn_request **inout_query_request,
                                        void *arg);

static int watchdog_mgmt_handle_action_request(gcl_session *sess,
                                         bn_request **inout_action_request,
                                         void *arg);
static int delete_uri_action_hdlr( const char *ns_name, const char *uri_pattern,
					tstring **ret_output);

static int ns_get_disknames( tstr_array **ret_labels, tbool hide_display);

/* ------------------------------------------------------------------------- */
int
watchdog_mgmt_init(void)
{
    int err = 0;
    mdc_wrapper_params *mwp = NULL;

    err = mdc_wrapper_params_get_defaults(&mwp);
    bail_error(err);

    mwp->mwp_lew_context = watchdog_lwc;
    mwp->mwp_gcl_client = gcl_client_watchdog;
    mwp->mwp_session_close_func = watchdog_mgmt_handle_session_close;
    mwp->mwp_query_request_func = watchdog_mgmt_handle_query_request;
    mwp->mwp_action_request_func = watchdog_mgmt_handle_action_request;
    mwp->mwp_event_request_func = watchdog_mgmt_handle_event_request;

    err = mdc_wrapper_init(mwp, &watchdog_mcc);
    bail_error(err);

    err = lc_random_seed_nonblocking_once();
    bail_error(err);

    /*
     * Register to receive config changes.  We care about:
     *   1. Any of our own configuration nodes.
     *   2. Any of our own monitoring nodes.
     */
    err = mdc_send_action_with_bindings_str_va
        (watchdog_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 10,
	 "event_name", bt_name, mdc_event_dbchange,
         "match/1/pattern", bt_string, "/nkn/nvsd/namespace/actions/**",
     /* Will not be used for now */    
	// 
	// "match/3/pattern", bt_string, "/web/httpd/http/port");
	/* Listen to nodes that are specific to watchdog as well */
	"match/2/pattern", bt_string, "/nkn/watch_dog/config/**",
	"match/3/pattern", bt_string, "/nkn/nvsd/network/config/threads",
	"match/4/pattern", bt_string, "/pm/process/nvsd/liveness/hung_count",
        "match/5/pattern", bt_string, "/nkn/nvsd/network/config/**",
	"match/6/pattern", bt_string, "/nkn/nvsd/namespace/mfc_probe/**",
	"match/7/pattern", bt_string, "/web/httpd/**",
	"match/8/pattern", bt_string, "/pm/failure/liveness/grace_period",
	"match/9/pattern", bt_string, "/nkn/nvsd/system/config/coredump" );

    bail_error(err);

    /*Regrister here for an event*/
    err = mdc_send_action_with_bindings_str_va
            (watchdog_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	    "event_name", bt_name, "/net/interface/events/link_up");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va
            (watchdog_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	    "event_name", bt_name, "/net/interface/events/link_down");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va
            (watchdog_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	    "event_name", bt_name, "/pm/events/process_launched");
    bail_error(err);

    //TODO
    /* Register for /pm/events/process_launched available in eucalyptus - update 5 and get pid
     * to be used for memory leak detection
     */

 bail:
    mdc_wrapper_params_free(&mwp);
    if (err) {
        lc_log_debug(LOG_ERR,
                     _("Unable to connect to the management daemon"));
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
watchdog_mgmt_handle_session_close(int fd, fdic_callback_type cbt,
                             void *vsess, void *gswatchdog_arg)
{
    int err = 0;
    gcl_session *gswatchdog_session = vsess;

    /*
     * If we're exiting, this is expected and we shouldn't do anything.
     * Otherwise, it's an error, so we should complain and reestablish
     * the connection.
     */
    if (watchdog_exiting) {
        goto bail;
    }

    lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; exiting"));
    err = watchdog_initiate_exit();
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
watchdog_mgmt_handle_event_request(gcl_session *sess,
                             bn_request **inout_request,
                             void *arg)
{
    int err = 0;
    bn_msg_type msg_type = bbmt_none;
    bn_binding_array *old_bindings = NULL, *new_bindings = NULL;
    tstring *event_name = NULL;
    bn_binding_array *bindings = NULL;
    bn_binding_array *network_bindings = NULL;
    uint32 num_callbacks = 0, i = 0;
    void *data = NULL;
    tbool rechecked_licenses = false;
    tstring *t_modulename = NULL;
    tbool t_loaded = false;


    bail_null(inout_request);
    bail_null(*inout_request);

    err = bn_request_get(*inout_request, &msg_type, NULL, false, &bindings,
	    NULL, NULL);
    bail_error(err);

    bail_require(msg_type == bbmt_event_request);

    err = bn_event_request_msg_get_event_name(*inout_request, &event_name);
    bail_error_null(err, event_name);

    lc_log_basic(LOG_INFO, "Received event: %s", ts_str(event_name));

    if (ts_equal_str(event_name, mdc_event_dbchange, false)) {
	err = mdc_event_config_change_notif_extract
	    (bindings, &old_bindings, &new_bindings, NULL);
	bail_error(err);
	err = bn_binding_array_foreach(new_bindings, watchdog_config_handle_change,
		&rechecked_licenses);
	bail_error(err);
    }
    else if (ts_equal_str(event_name, "/net/interface/events/link_up", false)) {
	tstring *t_intf = NULL;
	tstring *t_name = NULL;

	err = bn_binding_array_get_first_matching_value_tstr
	    (bindings, "/net/interface/state/*", 0, NULL, &t_name, &t_intf);
	bail_error_null(err, t_intf);

	lc_log_basic(LOG_NOTICE, "MFC_Probe: recieved a link up event for intf %s", ts_str(t_intf));

	if(watchdog_config.mgmt_intf && (strcmp(ts_str(t_intf), watchdog_config.mgmt_intf) == 0)) {
	    watchdog_set_watcdog_ns_origin(ts_str(t_intf));
	}
	ts_free(&t_intf);
	ts_free(&t_name);
    }
    else if (ts_equal_str(event_name, "/net/interface/events/link_down", false)) {
	tstring *t_intf = NULL;
	tstring *t_name = NULL;

	err = bn_binding_array_get_first_matching_value_tstr
	    (bindings, "/net/interface/state/*", 0, NULL, &t_name, &t_intf);
	bail_error_null(err, t_intf);

	lc_log_basic(LOG_NOTICE, "MFC_Probe: recieved a link down event for intf %s", ts_str(t_intf));

	if(watchdog_config.mgmt_intf &&  (strcmp(ts_str(t_intf), watchdog_config.mgmt_intf) == 0)) {
	    watchdog_config.has_mgmt_intf = false;
	}
	ts_free(&t_intf);
	ts_free(&t_name);

    }
    else if (ts_equal_str(event_name, "/pm/events/process_launched", false)) {
	tstring *t_name = NULL;
	tstring *t_value = NULL;

	err = bn_binding_array_get_first_matching_value_tstr
	    (bindings, "/pm/monitor/process/*", 0, NULL, &t_name, &t_value);
	bail_error_null(err, t_value);


	if(strcmp(ts_str(t_value), "nvsd" ) == 0) {
	    watchdog_config_query();
	    /* set the var to test for Up time */
	    watchdog_config.is_nvsd_rst = true;
	}
	ts_free(&t_value);
	ts_free(&t_name);

    }
    else {
	lc_log_debug(LOG_WARNING, I_("Received unexpected event %s"),
		ts_str(event_name));
    }

bail:
    bn_binding_array_free(&bindings);
    bn_binding_array_free(&old_bindings);
    bn_binding_array_free(&new_bindings);
    bn_binding_array_free(&network_bindings);
    ts_free(&event_name);
    ts_free(&t_modulename);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
watchdog_mgmt_handle_query_request_async(int fd, short event_type, void *data,
                                   lew_context *ctxt)
{
    int err = 0;
    bn_request *req = data;

    bail_null(req);
    err = mdc_dispatch_mon_request
        (mdc_wrapper_get_gcl_session(watchdog_mcc), req, watchdog_mon_handle_get,
	 NULL, watchdog_mon_handle_iterate, NULL);
    bail_error(err);

    bn_request_msg_free(&req);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
watchdog_mgmt_handle_query_request(gcl_session *sess,
                             bn_request **inout_query_request, void *arg)
{
    int err = 0;
    lt_dur_ms ms_to_wait = 0;
    bn_request *req_copy = NULL;
    lew_event **timer = NULL;

    bail_null(inout_query_request);
    bail_null(*inout_query_request);

#ifdef DELAY_QUERIES
    /*
     * We'll leak the memory of this timer; it's just for testing.
     * The request will be freed by the async handler.
     */
    ms_to_wait = 10000 * lc_random_get_fraction();
    req_copy = *inout_query_request;
    *inout_query_request = NULL;
    if (watchdog_next_query_idx == MAX_QUERIES) {
        lc_log_basic(LOG_WARNING, _("Exceeded maximum allowable number "
                                    "of queries!  Exiting ungracefully..."));
        exit(1);
    }
    timer = &(watchdog_query_timers[watchdog_next_query_idx++]);
    *timer = NULL;
    err = lew_event_reg_timer(watchdog_lwc, timer,
                              watchdog_mgmt_handle_query_request_async,
                              req_copy, ms_to_wait);
    bail_error(err);
    lc_log_basic(LOG_NOTICE, "Waiting %" PRIu64 " ms before responding to "
                 "query request", ms_to_wait);
#else
    err = mdc_dispatch_mon_request(sess, *inout_query_request,
                                   watchdog_mon_handle_get,
                                   NULL, watchdog_mon_handle_iterate, NULL);
    bail_error(err);
#endif

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int
watchdog_mgmt_handle_action_request(gcl_session *sess,
                              bn_request **inout_action_request, void *arg)
{
    int err = 0;
    bn_msg_type req_type = bbmt_none;
    bn_binding_array *bindings = NULL;
    tstring *action_name = NULL;
    tstring *t_namespace = NULL;
    tstring *t_uripattern = NULL;
    tstring *ret_output = NULL;

    /* Standard Samara logic first */
    bail_null(inout_action_request);
    bail_null(*inout_action_request);

    err = bn_request_get
        (*inout_action_request, &req_type, NULL, true, &bindings, NULL, NULL);
    bail_error(err);
    bail_require(req_type == bbmt_action_request);
    
    err = bn_action_request_msg_get_action_name(*inout_action_request,
                                                &action_name);
    bail_error(err);

    lc_log_basic(LOG_INFO, "Received action %s", ts_str(action_name));

    /* INVALIDATE/PURGE URI(s) action handler */
    if (ts_equal_str (action_name, "/nkn/nvsd/namespace/actions/delete_uri", false)) {

    	/* Get the namespace field */
    	err = bn_binding_array_get_value_tstr_by_name
        	(bindings, "namespace", NULL, &t_namespace);
    	bail_error(err);

    	/* Get the uri field */
    	err = bn_binding_array_get_value_tstr_by_name
        	(bindings, "uri-pattern", NULL, &t_uripattern);
    	bail_error(err);

	if (!t_namespace || !t_uripattern) {
	    /* Send response message */
            err = bn_response_msg_create_send
                	(sess, bbmt_action_response,
			bn_request_get_msg_id(*inout_action_request),
                 	0,
			"error: namespace and/or uri-pattern not valid",
			0);
    	    bail_error(err);
	}


	/* Call the delete function */
	err = delete_uri_action_hdlr(ts_str(t_namespace), ts_str(t_uripattern),
					&ret_output);

	/* Send response message */
        err = bn_response_msg_create_send
                	(sess, bbmt_action_response,
			bn_request_get_msg_id(*inout_action_request),
                 	0, ts_str(ret_output), 0);
    	bail_error(err);
    }
    else {
        lc_log_debug(LOG_WARNING, I_("Got unexpected action %s"), 
                     ts_str(action_name));
        err = bn_response_msg_create_send
            (sess, bbmt_action_response, bn_request_get_msg_id(*inout_action_request),
             1, _("error : action not yet supported"), 0);
        goto bail;
    }

 bail:
    ts_free(&t_uripattern);
    ts_free(&t_namespace);
    ts_free(&action_name);
    ts_free(&ret_output);
    bn_binding_array_free(&bindings);
    return(err);
}

/* ------------------------------------------------------------------------- */
int
delete_uri_action_hdlr(
    	const char *ns_name,
    	const char *uri_pattern,
	tstring **ret_output)
{
    int err = 0;
    char *node_name = NULL;
    char *uri_prefix_str = NULL;
    char *uri_prefix_str_esc = NULL;
    const char *proxy_domain = ALL_STR;
    const char *proxy_domain_port = NS_DEFAULT_PORT;
    tstring *ns_uid = NULL;
    tstring *ns_domain = NULL;
    tstring *ns_proxy_mode = NULL;
    tstring *uri_prefix = NULL;
    tstr_array *cmd_params = NULL;
    int32 ret_status = 0;
    char *ret_string = NULL;


    /* First do sanity test on namespace name and uri/all/pattern */
    if (!ns_name || !uri_pattern)
    {
    	ret_string = smprintf("error: unable to find matching namespace and uri-pattern");
    	goto bail;
    }

    /* Get the proxy-mode of the namespace */
    err = mdc_get_binding_tstr_fmt(watchdog_mcc, NULL, NULL, NULL,
				&ns_proxy_mode, NULL,
				"/nkn/nvsd/namespace/%s/proxy-mode",
				ns_name);
    bail_error(err);

    /* Get the uri-prefix of the namespace */
    node_name = smprintf("/nkn/nvsd/namespace/%s/match/uri/uri_name", ns_name);
    bail_null(node_name);

    err = mdc_get_binding_tstr (watchdog_mcc, NULL, NULL, NULL,
			&uri_prefix, node_name, NULL);
    bail_error(err);
    safe_free(node_name);

    /* Check if uri-prefix has been defined */
    if (!uri_prefix) {
	ret_string = smprintf("error: no uri-prefix defined for this namespace");
	goto bail;
    }

    err = ts_detach(&uri_prefix, &uri_prefix_str, NULL);
    bail_error(err);

    /* Check if it is "all" if not check the uri-prefix */
    if ((strcmp(uri_pattern, "all")) &&
    		(!lc_is_prefix(uri_prefix_str, uri_pattern, false)))
    {
	    ret_string = smprintf("error: uri-prefix does not match that of this namespace");
	    goto bail;
    }

    /* Get the domain for the namespace */
    err = bn_binding_name_escape_str(uri_prefix_str, &uri_prefix_str_esc);
    bail_error_null(err, uri_prefix_str_esc);
    node_name = smprintf("/nkn/nvsd/namespace/%s/domain/host", ns_name);
    bail_null(node_name);

    err = mdc_get_binding_tstr(watchdog_mcc, NULL, NULL, NULL, 
    			&ns_domain, node_name, NULL);
    bail_error_null(err, ns_domain);

    /* Get the UID for the namespace */
    node_name = smprintf("/nkn/nvsd/namespace/%s/uid", ns_name);
    bail_null(node_name);

    err = mdc_get_binding_tstr(watchdog_mcc, NULL, NULL, NULL, 
    			&ns_uid, node_name, NULL);
    bail_error_null(err, ns_uid);

    /* Invoke the shell script for deleting the objects */
    err = lc_launch_quick(&ret_status, ret_output, 6, 
			"/opt/nkn/bin/cache_mgmt_del.sh",
			ns_name, ts_str(ns_uid), ts_str(ns_domain), uri_pattern, proxy_domain);

bail:
    /* Check if we had any error that we have not set the error string */
    if (err && !ret_string) {
        ret_string = smprintf("error: operation failed, please check system log for more details");
    }

    /* If ret_string is not null then we have error string */
    if (ret_string) {
        err = ts_new_str_takeover(ret_output, &ret_string, -1, -1);
    }
    ts_free(&ns_uid);
    ts_free(&ns_domain);
    safe_free(node_name);
    safe_free(uri_prefix_str);
    tstr_array_free(&cmd_params);
    return err;

} /* end of cli_ns_object_list_delete_cmd_hdlr() */


/* ------------------------------------------------------------------------- */
static int 
ns_get_disknames(
        tstr_array **ret_labels,
        tbool hide_display)
{
    int err = 0;
    char *node_name = NULL;
    const char *label = NULL;
    const char *t_diskid = NULL;
    tstr_array_options opts;
    tstr_array *labels_config = NULL;
    uint32 i = 0, num_labels = 0;
    tstr_array *labels = NULL;
    tbool t_cache_enabled = false;

    bail_null(ret_labels);
    *ret_labels = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&labels, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(watchdog_mcc,
            NULL, NULL, &labels_config, 
            "/nkn/nvsd/diskcache/config/disk_id", NULL);
    bail_error_null(err, labels_config);

    /* For each disk_id get the Disk Name */
    i = 0;
    t_diskid = tstr_array_get_str_quick (labels_config, i++);
    while (NULL != t_diskid)
    {
    	tstring *t_diskname = NULL;

	/* Now check if the disk is cacheable only then we are interested */
    	node_name = smprintf ("/nkn/nvsd/diskcache/config/disk_id/%s/cache_enabled", t_diskid);

    	/* Now get the cache_enabled flag value */
	err = mdc_get_binding_tbool (watchdog_mcc, NULL, NULL, NULL,
				&t_cache_enabled, node_name, NULL);
	bail_error(err);
	safe_free(node_name);

	/* If cache not enabled then skip this disk */
	if (!t_cache_enabled) 
	{
    		t_diskid = tstr_array_get_str_quick (labels_config, i++);
		continue;
	}

    	node_name = smprintf ("/nkn/nvsd/diskcache/monitor/disk_id/%s/diskname", t_diskid);
    	/* Now get the disk_name */
	err = mdc_get_binding_tstr (watchdog_mcc, NULL, NULL, NULL,
				&t_diskname, node_name, NULL);
	bail_error(err);
	safe_free(node_name);

    	//err = tstr_array_append_str(labels, ts_str(t_diskname));
    	err = tstr_array_insert_str_sorted(labels, ts_str(t_diskname));
    	bail_error(err);

    	t_diskid = tstr_array_get_str_quick (labels_config, i++);
    }

    *ret_labels = labels;
    labels = NULL;
bail:
    safe_free(node_name);
    tstr_array_free(&labels_config);
    return err;
} /* end of cli_ns_get_disknames() */


/* End of watchdog_mgmt.c */

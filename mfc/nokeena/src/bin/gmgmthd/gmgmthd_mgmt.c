/*
 *
 * Filename:  gmgmthd_mgmt.c
 * Date:      2010/03/25
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2010 Juniper Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include "time.h"
#include "md_client.h"
#include "mdc_wrapper.h"
#include "gmgmthd_main.h"
#include "random.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "clish_module.h"
#include "string.h"
#include "libnknmgmt.h"
#include "nkn_mgmt_defs.h"
#include <pthread.h>
#include "fcntl.h"
#include <ctype.h>
#include "ttime.h"
extern unsigned int jnpr_log_level;
// #define DELAY_QUERIES
#define MAX_QUERIES 1000

#define	NS_DEFAULT_PORT		"80"
#define	ALL_STR			"all"

#define SMALL_STR      32
#define MEDIUM_STR     256
#define LARGE_STR      1024

#define CPU0_SENSOR    "CPU0 below Tmax"
#define CPU1_SENSOR    "CPU1 below Tmax"

#define PS_SENSOR      "Power Supply sensor"
#define TEMP_SENSOR    "Temperature sensor"
#define FAN_SENSOR     "Fan sensor"

static int
gmgmthd_bkup_file(void);


typedef enum en_proxy_type
{
        REVERSE_PROXY = 0,
        MID_TIER_PROXY = 1,
        VIRTUAL_PROXY = 2
} en_proxy_type;

lew_event *gmgmthd_query_timers[MAX_QUERIES];
int gmgmthd_next_query_idx = 0;
extern int gmgmthd_set_watcdog_ns_origin(const char *intf);
static pthread_t ipmi_thread;

static void *ipmi_mgmt_thread_handler_func(void *arg);
int ipmi_thread_init(void);

static const char network_config_prefix[]   = "/nkn/nvsd/network/config";

/* ------------------------------------------------------------------------- */
/** Local prototypes & definitions
 */
md_client_context *gmgmthd_mcc = NULL;
md_client_context *gmgmthd_timer_mcc = NULL;

char *
get_extended_uid (
        const char *ns_uid,
        const char *proxy_domain,
        const char *proxy_domain_port);

static int gmgmthd_mgmt_handle_session_close(int fd, fdic_callback_type cbt, 
                                        void *vsess, void *gsgmgmthd_arg);

static int gmgmthd_mgmt_handle_event_request(gcl_session *sess,
                                        bn_request **inout_request, void *arg);

static int gmgmthd_mgmt_handle_query_request(gcl_session *sess,
                                        bn_request **inout_query_request,
                                        void *arg);

static int gmgmthd_mgmt_handle_action_request(gcl_session *sess,
                                         bn_request **inout_action_request,
                                         void *arg);
static int delete_uri_action_hdlr( const char *ns_name, const char *uri_pattern,
                       const char *proxy_domain, const char *proxy_domain_port,
	               tbool pinned, tstring **ret_output);

static int ns_get_disknames( tstr_array **ret_labels, tbool hide_display);

static int handle_shutdown_event(bn_binding_array *);
int unmount_nfs(
	const bn_binding_array *bindings,
	uint32 idx, const bn_binding *binding,
	const tstring *name, const tstr_array *name_components,
	const tstring *name_last_part,
	const tstring *value,
	void *callback_data);

prefetch_timer_data_t   prefetch_timers[MAX_PREFETCH_JOBS];

extern int gmgmthd_watch_nvsd_up_context_init(void);
extern int gmgmthd_status_timer_context_allocate(lt_dur_ms  poll_frequency);

/* ------------------------------------------------------------------------- */
int
gmgmthd_mgmt_init(void)
{
    int err = 0;
    int i = 0;
    mdc_wrapper_params *mwp = NULL;

    err = mdc_wrapper_params_get_defaults(&mwp);
    bail_error(err);

    mwp->mwp_lew_context = gmgmthd_lwc;
    mwp->mwp_gcl_client = gcl_client_gmgmthd;
    mwp->mwp_session_close_func = gmgmthd_mgmt_handle_session_close;
    mwp->mwp_query_request_func = gmgmthd_mgmt_handle_query_request;
    mwp->mwp_action_request_func = gmgmthd_mgmt_handle_action_request;
    mwp->mwp_event_request_func = gmgmthd_mgmt_handle_event_request;

    err = mdc_wrapper_init(mwp, &gmgmthd_mcc);
    bail_error(err);

    /* we are able to connect to mgmtd, set cpu-led to green */
    if(is_pacifica) {
	lc_log_basic(LOG_INFO, "we are able to connect to mgmtd, set cpu-led to green");

	err = set_gpio_led(CPU_LED, GPIO_GREEN);
	bail_error(err);
    }

    err = lc_random_seed_nonblocking_once();
    bail_error(err);

    /*
     * Register to receive config changes.  We care about:
     *   1. Any of our own configuration nodes.
     *   2. Any of our own monitoring nodes.
     */
    err = mdc_send_action_with_bindings_str_va
        (gmgmthd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 9,
	 "event_name", bt_name, mdc_event_dbchange,
         "match/1/pattern", bt_string, "/nkn/nvsd/namespace/actions/**",
	 /* Will not be used for now */    
	// "match/3/pattern", bt_string, "/web/httpd/http/port");
	/* Listen to nodes that are specific to watchdog as well */
	"match/2/pattern", bt_string, "/nkn/nvsd/network/config/threads",
       "match/3/pattern", bt_string, "/nkn/nvsd/network/config/**",
        "match/4/pattern", bt_string, "/nkn/nvsd/network/config/syn-cookie/half_open_conn",
	"match/5/pattern", bt_string, "/nkn/prefetch/config/*/status",
	"match/6/pattern", bt_string, "/nkn/debug/log/all/level",
	"match/7/pattern", bt_string, "/nkn/debug/log/ps/gmgmthd/level",
	"match/8/pattern", bt_string, "/nkn/nvsd/diskcache/config/**");

     bail_error(err);

    /*code to allcate the memory for prefetch timers*/
    for (i = 0; i < MAX_PREFETCH_JOBS; ++i) {
        memset(&(prefetch_timers[i]), 0, sizeof(prefetch_timer_data_t));
        prefetch_timers[i].index = i;
        prefetch_timers[i].context_data = &(prefetch_timers[i]);
    }
    /* Registering for restart event 
     * as prefetch needs to know when nvsd restarts
     */
    err = mdc_send_action_with_bindings_str_va
            (gmgmthd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	    "event_name", bt_name, "/pm/events/proc/restart");
    bail_error(err);

    /* registering as a interested party for /pm/events/shutdown event */
    err = mdc_send_action_with_bindings_str_va
        (gmgmthd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name, "/pm/events/shutdown");

    /*registering for nvsd process restart*/
    err = mdc_send_action_with_bindings_str_va
	(gmgmthd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name, "/pm/events/process_launched");
    bail_error(err);

    /*registering for nvsd process restart*/
    err = mdc_send_action_with_bindings_str_va
	(gmgmthd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name, "/pm/events/process_terminated");
    bail_error(err);

 
    /*initialize the nvsd restart timer*/
    err = gmgmthd_watch_nvsd_up_context_init();
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
gmgmthd_mgmt_handle_session_close(int fd, fdic_callback_type cbt, 
                             void *vsess, void *gsgmgmthd_arg)
{
    int err = 0;
    gcl_session *gsgmgmthd_session = vsess;

    /*
     * If we're exiting, this is expected and we shouldn't do anything.
     * Otherwise, it's an error, so we should complain and reestablish 
     * the connection.
     */
    if (gmgmthd_exiting) {
        goto bail;
    }
    /* Lost connection to mgmtd, Set the CPU led to Orange*/
    if(is_pacifica) {
	lc_log_basic(LOG_INFO, "Lost connection to mgmtd, Set the CPU led to Orange");

	err = set_gpio_led(CPU_LED, GPIO_ORANGE);
	bail_error(err);
    }


    lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; exiting"));
    err = gmgmthd_initiate_exit();
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
gmgmthd_mgmt_handle_event_request(gcl_session *sess, 
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
    tstring *t_bn_name = NULL;
    tstring *t_bn_value = NULL;


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

        err = bn_binding_array_foreach(new_bindings, gmgmthd_config_handle_change,
                                       &rechecked_licenses);
        bail_error(err);
        err = bn_binding_array_foreach(old_bindings, gmgmthd_config_handle_delete,
                                       &rechecked_licenses);
        bail_error(err);
	err = mgmt_log_cfg_handle_change(old_bindings, new_bindings,
		"/nkn/debug/log/ps/gmgmthd/level");
	bail_error(err);
    } else if (ts_equal_str(event_name, "/pm/events/proc/restart", false)) {
	tstring *t_name = NULL;
	tstring *t_value = NULL;

	err = bn_binding_array_get_first_matching_value_tstr
	    (bindings, "/pm/monitor/process/*", 0, NULL, &t_name, &t_value);
	bail_error_null(err, t_value);


	if(strcmp(ts_str(t_value), "nvsd" ) == 0) {
	    gmgmthd_config_query();
	}
	ts_free(&t_value);
	ts_free(&t_name);

    } else if (ts_equal_str(event_name, "/pm/events/process_launched", false)) {
	tstring *t_name = NULL;
	tstring *t_value = NULL;

	err = bn_binding_array_get_first_matching_value_tstr
	    (bindings, "/pm/monitor/process/*", 0, NULL, &t_name, &t_value);
	bail_error_null(err, t_value);

	if(strcmp(ts_str(t_value), "nvsd" ) == 0) {
	    /*Add the timer function here*/
	    /* Restart of nvsd, Set the app led to Orange(2) */
	    if (is_pacifica) {
		lc_log_basic(LOG_INFO, "Restart of nvsd, Set the app led to Orange(2)");

		err = set_gpio_led(APP_LED, GPIO_ORANGE);
		bail_error(err);
	    }

	    err =  gmgmthd_status_timer_context_allocate(10000);
	    //	    gmgmthd_disk_query();
	}
	ts_free(&t_value);
	ts_free(&t_name);

    } else if (ts_equal_str(event_name, "/pm/events/process_terminated", false)) {

	err = bn_binding_array_get_first_matching_value_tstr
	    (bindings, "/pm/monitor/process/*", 0, NULL, &t_bn_name, &t_bn_value);
	bail_error_null(err, t_bn_value);

	if(strcmp(ts_str(t_bn_value), "nvsd" ) == 0) {
	    /*Add the timer function here*/
	    /* Restart of nvsd, Set the app led to red(1) */
	    if (is_pacifica) {
		lc_log_basic(LOG_INFO, "NVSD terminated, Set the app led to Red(1)");

		err = set_gpio_led(APP_LED, GPIO_RED);
		bail_error(err);
	    }

	}
	ts_free(&t_bn_value);
	ts_free(&t_bn_name);

    }


    else if (ts_equal_str(event_name, "/pm/events/shutdown", false)) {
	/* handle the pm shutdown event */
	handle_shutdown_event(bindings);
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
    ts_free(&t_bn_value);
    ts_free(&t_bn_name);
    return(err);
}

/*
 * this function is called when we receive pm shutdown event. We also receive
 * binding which tells us the tyep of shutdown event i.e. reload, reload_force,
 * halt, pm exit. We only handle the reload, reload force and halt.
 * if event is one of these, we iterate over all the NFS mount nodes and
 * trigger the unmount script for each of them.
 * NOTE: we are not handling the return status of unmount script, even if it
 * fails. We don't have control right now. Correct solution may be to trigger
 * script again if the script fail, (2 times should be fine).
 */
static int
handle_shutdown_event(bn_binding_array *bindings)
{
    int err = 0; unsigned int num_nfs = 0;
    tstring *shutdown_type = NULL;

    err = gmgmthd_bkup_file();
    bail_error(err);

    /* Get the shutdown_type */
    err = bn_binding_array_get_value_tstr_by_name
	(bindings, "shutdown_type", NULL, &shutdown_type);
    bail_error(err);

    /*
     * don't unmount in cases of other than reboot, force reboot and halt,
     * e.g pm exit.
     */
    if (ts_equal_str(shutdown_type, "reboot", false)
	    || ts_equal_str(shutdown_type, "halt", false)
	    || ts_equal_str(shutdown_type, "reboot_force", false)) {

	/* call unmount_nfs for each NFS node */
	err = mdc_foreach_binding(gmgmthd_mcc,
		"/nkn/nfs_mount/config/*",
		NULL,
		unmount_nfs,
		NULL,
		&num_nfs);
	bail_error(err);
    }

bail:
    ts_free(&shutdown_type);
    return (err);
}

static int
gmgmthd_bkup_file(void)
{
    int err = 0;
    str_value_t gz_ts = {0};
    lf_cleanup_context *cleanup_ctxt = NULL;
    uint64_t n_bytes_deleted = 0;
    int32_t ret_status = 0;
    tstring *ret_output = NULL;


    lt_time_sec cur_time = lt_curr_time();

    if(!is_pacifica)
	goto bail;

    err = lf_ensure_dir("/nkn/log_bk", S_IRWXU);
    bail_error(err);

    snprintf(gz_ts, sizeof(gz_ts), "/nkn/log_bk/log_bk_%d.tar.gz",
	    cur_time);

    // tar -zcvf test5.tar.gz /var/log/*
    err = lc_launch_quick_status(&ret_status, &ret_output,
				    true, 4,
				    "/bin/tar", "-zcvf",
				    gz_ts, "/log/varlog");
    bail_error(err);

    if(ret_status) {
	lc_log_basic(LOG_NOTICE, "Error archiving logs: %s",
			ts_str(ret_output));
	goto bail;
    } else {
	lc_log_basic(LOG_NOTICE, "backing up logs at %s",
			gz_ts);
    }

    err = lf_cleanup_get_context("/nkn/log_bk/", &cleanup_ctxt);
    bail_error(err);

    err = lf_cleanup_leave_n_files(cleanup_ctxt, "log_bk", 10, &n_bytes_deleted);
    bail_error(err);

    if(n_bytes_deleted)
	lc_log_basic(LOG_NOTICE, "Caping number of file to 10,Clean up %lu bytes", 
			n_bytes_deleted);

bail:
    ts_free(&ret_output);
    return err;

}

/* Called for each NFS mount point, triggers the unmount script */
int
unmount_nfs(
	const bn_binding_array *bindings,
	uint32 idx, const bn_binding *binding,
	const tstring *name, const tstr_array *name_components,
	const tstring *name_last_part,
	const tstring *value,
	void *callback_data)
{
    int err = 0, code = 0;

    err = lc_launch_quick_status(&code, NULL, false, 2,
	    "/opt/nkn/bin/mgmt_nfs_umount.sh",
	    ts_str(name_last_part));

    bail_error(err);

    /* XXX - not handling return status from the script */
bail:
    return(err);
}
/* ------------------------------------------------------------------------- */
static int 
gmgmthd_mgmt_handle_query_request_async(int fd, short event_type, void *data,
                                   lew_context *ctxt)
{
    int err = 0;
    bn_request *req = data;

    bail_null(req);
    err = mdc_dispatch_mon_request
        (mdc_wrapper_get_gcl_session(gmgmthd_mcc), req, gmgmthd_mon_handle_get,
         NULL, gmgmthd_mon_handle_iterate, NULL);
    bail_error(err);
    
    bn_request_msg_free(&req);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
gmgmthd_mgmt_handle_query_request(gcl_session *sess,
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
    if (gmgmthd_next_query_idx == MAX_QUERIES) {
        lc_log_basic(LOG_WARNING, _("Exceeded maximum allowable number "
                                    "of queries!  Exiting ungracefully..."));
        exit(1);
    }
    timer = &(gmgmthd_query_timers[gmgmthd_next_query_idx++]);
    *timer = NULL;
    err = lew_event_reg_timer(gmgmthd_lwc, timer,
                              gmgmthd_mgmt_handle_query_request_async,
                              req_copy, ms_to_wait);
    bail_error(err);
    lc_log_basic(LOG_NOTICE, "Waiting %" PRIu64 " ms before responding to "
                 "query request", ms_to_wait);
#else
    err = mdc_dispatch_mon_request(sess, *inout_query_request,
                                   gmgmthd_mon_handle_get,
                                   NULL, gmgmthd_mon_handle_iterate, NULL);
    bail_error(err);
#endif

 bail:
    return(err);
}

/* ----------------------------------------------------------------------- */
char *
get_extended_uid (
        const char *ns_uid,
        const char *proxy_domain,
        const char *proxy_domain_port)
{
        char convert_domain[300]; // should be more than enough
        const char *p2 = NULL;
        char *p1 = NULL;
        char *ret_uid_str = NULL;
        int totlen;

        /* Sanity test */
        if (!proxy_domain) return strdup (ns_uid) ;

        if (!proxy_domain_port)
                proxy_domain_port = NS_DEFAULT_PORT;

        /* tolowercase and append port number with a colon as seperator*/
        p1 = convert_domain;
        p2 = (char *)proxy_domain;
        totlen = strlen (proxy_domain);
        while (totlen) {
                *p1 = tolower(*p2);
                p1++; p2++; totlen--;
        }
        *p1 = ':'; // Add the port seperator color
        p1++;

        p2 = proxy_domain_port;
        totlen = strlen (proxy_domain_port);
        while (totlen) {
                *p1 = *p2;
                p1++; p2++; totlen--;
        }
        *p1 = '\0';
        totlen = strlen (convert_domain);

        /* Append the domain:port to the uid */
        /* Allocate the return string : UID + _ + domain:port + '\0' */
        ret_uid_str = calloc (strlen (ns_uid) + 1 + strlen (convert_domain) + 1,
                                sizeof (char));
        sprintf (ret_uid_str, "%s_", ns_uid);
        strncat (ret_uid_str, convert_domain, strlen(convert_domain));

        return (ret_uid_str);

} /* end of get_extended_uid () */



/* ------------------------------------------------------------------------- */
int
gmgmthd_mgmt_handle_action_request(gcl_session *sess,
                              bn_request **inout_action_request, void *arg)
{
    int err = 0;
    bn_msg_type req_type = bbmt_none;
    bn_binding_array *bindings = NULL;
    tstring *action_name = NULL;
    tstring *t_namespace = NULL;
    tstring *t_uripattern = NULL;
    tstring *t_proxydomain = NULL;
    tstring *t_proxyport = NULL;
    tbool t_pinned = false;
    tstring *t_streamname = NULL;
    tstring *ret_output = NULL;
    tstring *t_urlfile = NULL;
    tstring *t_time = NULL;
    tstring *t_date = NULL;
    tstring *t_options = NULL;
    int32 status = 0;
    char *bn_mfp_status = NULL;
    uint32 ret_err=0;
    tstring *msg = NULL;
    const bn_binding *binding = NULL;

    lt_time_sec ret_inpdaytimesec;
    lt_time_sec curr_time;
    lt_time_sec inp_timesec;
    lt_time_sec rem_time;
    lt_date today_date;
    lt_time_sec sys_daytimesec;

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

        /* Get the proxy-domain field */
        err = bn_binding_array_get_binding_by_name
                (bindings, "proxy-domain", &binding);
        bail_error(err);
        if(binding) {
            err = bn_binding_get_tstr
                (binding, ba_value, bt_string, NULL, &t_proxydomain);
            bail_error(err);
        } else {
            err = ts_new_str(&t_proxydomain, "");
            bail_error(err);
        }
        /* Get the proxy-port field */
        err = bn_binding_array_get_binding_by_name
                (bindings, "proxy-port", &binding);
        bail_error(err);
        if(binding) {
            err = bn_binding_get_tstr
                (binding, ba_value, bt_string, NULL, &t_proxyport);
            bail_error(err);
        } else {
            err = ts_new_str(&t_proxyport, "");
            bail_error(err);
        }

        /* Get the pinned field */
        err = bn_binding_array_get_binding_by_name
                (bindings, "pinned", &binding);
        bail_error(err);
        if(binding) {
            err = bn_binding_get_tbool
                (binding, ba_value, NULL, &t_pinned);
            bail_error(err);
        }


	if (!t_namespace || !t_uripattern) {
		/* Send responsr message */
            err = bn_response_msg_create_send
                	(sess, bbmt_action_response,
			bn_request_get_msg_id(*inout_action_request),
                 	1,
			"error: namespace and/or uri-pattern not valid",
			0);
    	    bail_error(err);
	}


	/* Call the delete function */
	err = delete_uri_action_hdlr(ts_str(t_namespace), ts_str(t_uripattern),
                        ts_str(t_proxydomain), ts_str(t_proxyport), t_pinned,
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
    ts_free(&t_streamname);
    ts_free(&action_name);
    ts_free(&ret_output);
    ts_free(&t_proxydomain);
    ts_free(&t_proxyport);
    bn_binding_array_free(&bindings);
    return(err);
}

/* ------------------------------------------------------------------------- */
int
delete_uri_action_hdlr(
    	const char *ns_name,
    	const char *uri_pattern,
        const char *proxy_domain,
        const char *proxy_domain_port,
        tbool pinned,
	tstring **ret_output)
{
    int err = 0;
    node_name_t node_name;
    char *uri_prefix_str = NULL;
    char *uri_prefix_str_esc = NULL;
    tstring *ns_uid = NULL;
    tstring *ns_domain = NULL;
    tstring *ns_proxy_mode = NULL;
    tstring *uri_prefix = NULL;
    tstr_array *cmd_params = NULL;
    int32 ret_status = 0;
    int32 ret_errstr = 0;
    char ret_string[1024];
    en_proxy_type proxy_type = REVERSE_PROXY;
    tbool domain_port_flag = true;
    uint64_t port;
    char c_proxy_domain[1024] = ALL_STR;
    char c_proxy_domain_port[32] = NS_DEFAULT_PORT;
    char *ns_uid_str = NULL;

    /* First do sanity test on namespace name and uri/all/pattern */
    if (!ns_name || !uri_pattern)
    {
    	snprintf(ret_string,sizeof(ret_string),"error: unable to find matching namespace and uri-pattern");
        ret_errstr = 1;
    	goto bail;
    }

    if((strncmp(proxy_domain,"",1)==0) &&
           (strncmp(proxy_domain_port,"",1)==0)) {
        domain_port_flag = false;
    }

    /* Get the proxy-mode of the namespace */
    err = mdc_get_binding_tstr_fmt(gmgmthd_mcc, NULL, NULL, NULL,
				&ns_proxy_mode, NULL,
				"/nkn/nvsd/namespace/%s/proxy-mode",
				ns_name);
    bail_error(err);

    /* Set a flag to indicate if the namespace is reverse proxy */
    if (ts_equal_str (ns_proxy_mode, "reverse", true))
        proxy_type = REVERSE_PROXY;
    else if (ts_equal_str (ns_proxy_mode, "mid-tier", true))
        proxy_type = MID_TIER_PROXY;
    else
        proxy_type = VIRTUAL_PROXY;

    /* Now check if we had a domain and port in the case of mid-tier */
    if ((REVERSE_PROXY == proxy_type) && (domain_port_flag)) {
        snprintf(ret_string,sizeof(ret_string),"error: domain and port should not be given for reverse proxy-mode");
        ret_errstr = 1;
        goto bail;
    }

    /* Get the uri-prefix of the namespace */
    snprintf(node_name,sizeof(node_name),"/nkn/nvsd/namespace/%s/match/uri/uri_name", ns_name);

    err = mdc_get_binding_tstr (gmgmthd_mcc, NULL, NULL, NULL,
			&uri_prefix, node_name, NULL);
    bail_error(err);

    /* Check if uri-prefix has been defined */
    if (!uri_prefix) {
	snprintf(ret_string,sizeof(ret_string),"error: no uri-prefix defined for this namespace");
        ret_errstr = 1;
	goto bail;
    }

    err = ts_detach(&uri_prefix, &uri_prefix_str, NULL);
    bail_error(err);

    if (strncmp(proxy_domain_port,"",1)!=0) {
        port = strtoul(proxy_domain_port, NULL,10);
        if(port > 65535 || port < 1) {
            snprintf(ret_string,sizeof(ret_string),"Port value should be between 0 and 65536");
            ret_errstr = 1;
            goto bail;
        }
        strlcpy(c_proxy_domain_port, proxy_domain_port,sizeof(c_proxy_domain_port)-1);
    }

    if (strncmp(proxy_domain,"",1)!=0) {
        strlcpy(c_proxy_domain,proxy_domain,sizeof(c_proxy_domain)-1);
    }

    /* Get the domain for the namespace */
    err = bn_binding_name_escape_str(uri_prefix_str, &uri_prefix_str_esc);
    bail_error_null(err, uri_prefix_str_esc);
    snprintf(node_name,sizeof(node_name),"/nkn/nvsd/namespace/%s/domain/host", ns_name);

    err = mdc_get_binding_tstr(gmgmthd_mcc, NULL, NULL, NULL, 
    			&ns_domain, node_name, NULL);
    bail_error_null(err, ns_domain);

    /* Get the UID for the namespace */
    snprintf(node_name,sizeof(node_name),"/nkn/nvsd/namespace/%s/uid", ns_name);

    err = mdc_get_binding_tstr(gmgmthd_mcc, NULL, NULL, NULL, 
    			&ns_uid, node_name, NULL);
    bail_error_null(err, ns_uid);

    if(domain_port_flag) {
        /*Retrieve the namespace uid for this domain port*/
        ns_uid_str = get_extended_uid (ts_str(ns_uid), c_proxy_domain,
                                        c_proxy_domain_port);
    }else{
        ns_uid_str = strdup (ts_str(ns_uid));
    }

    /* Invoke the shell script for deleting the objects */
    /* "Usage: cache_mgmt_del <namespace> <uid> <domain> {all|<pattern> } {all|<search domain>} */
    if(!pinned) {
        err = lc_launch_quick(&ret_status, ret_output, 6, 
			"/opt/nkn/bin/cache_mgmt_del.py",
			ns_name,ns_uid_str, ts_str(ns_domain), uri_pattern, c_proxy_domain);
    } else {
        err = lc_launch_quick(&ret_status, ret_output, 7,
                        "/opt/nkn/bin/cache_mgmt_del.py",
                        ns_name,ns_uid_str, ts_str(ns_domain), uri_pattern, c_proxy_domain, "pin");
    }

bail:
    /* Check if we had any error that we have not set the error string */
    if (err && !ret_errstr) {
        snprintf(ret_string,sizeof(ret_string),"error: operation failed, please check system log for more details");
        ret_errstr = 1;
    }

    if (ret_errstr) {
        //err = ts_new_str_takeover(ret_output, strdup(ret_string), -1, -1);
        err = ts_new_or_replace_str(ret_output,ret_string);
    }
    ts_free(&ns_uid);
    ts_free(&ns_domain);
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

    err = mdc_get_binding_children_tstr_array(gmgmthd_mcc,
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
	err = mdc_get_binding_tbool (gmgmthd_mcc, NULL, NULL, NULL,
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
	err = mdc_get_binding_tstr (gmgmthd_mcc, NULL, NULL, NULL,
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

/* ------------------------------------------------------------------------- *
 * Purpose : The timer thread is going to be a GCL client and since we do not
 * have thread support within GCL, we are using another GCL client for the timer
 * thread. 
 * (A little over kill but cannot think of a quicker way at this time - Ram)
 */
int
gmgmthd_mgmt_timer_thread_init(void)
{
    int err = 0;
    mdc_wrapper_params *mwp = NULL;

    err = mdc_wrapper_params_get_defaults(&mwp);
    bail_error(err);

    mwp->mwp_lew_context = gmgmthd_timer_lwc;
    mwp->mwp_gcl_client = gcl_client_gmgmthd_timer;
    mwp->mwp_session_close_func = gmgmthd_mgmt_handle_session_close;
    mwp->mwp_query_request_func = gmgmthd_mgmt_handle_query_request;

    err = mdc_wrapper_init(mwp, &gmgmthd_timer_mcc);
    bail_error(err);

 bail:
    mdc_wrapper_params_free(&mwp);
    if (err) {
        lc_log_debug(LOG_ERR, 
                     _("Unable to connect to the management daemon"));
    }
    return(err);
}

int
ipmi_thread_init(void)
{
    int err = 0;

        if (pthread_create(&ipmi_thread, NULL,
                        ipmi_mgmt_thread_handler_func, NULL)) {
                lc_log_basic(LOG_ERR, "Failed to create IPMI gmgmthd thread.");
                return 1;
        }

 bail:
    return(err);

}


static int ipmi_set_cpu_threshold(int cpu_thresh)
{
    uint32 err = 0;
    int32 status = 0;
    char thresh[MEDIUM_STR] = {0};

    snprintf(thresh, sizeof(thresh), "%d", cpu_thresh);

    err = lc_launch_quick_status(&status, NULL, false, 6,
                  "/usr/bin/ipmitool", "sensor", "thresh",
                  CPU0_SENSOR, "ucr", thresh);
    bail_error(err);

    err = lc_launch_quick_status(&status, NULL, false, 6,
                  "/usr/bin/ipmitool", "sensor", "thresh",
                  CPU1_SENSOR, "ucr", thresh);
    bail_error(err);
bail:
    return err;
}

static void *
ipmi_mgmt_thread_handler_func(void *arg)
{
    int fp;
    char buffer[LARGE_STR];
    int length;
    bn_request *req = NULL;
    int err = 0;
    fd_set fd;
    char *sensor_thresh = NULL;
    int cpu_threshold = 10;
    int temp_thresh = 0;


    memset(buffer,'\0',LARGE_STR);
    fp = open("/var/log/local4.pipe", O_RDONLY);
    if(fp == -1) {
       lc_log_basic(LOG_ERR,"Failed to open pipe for read");
       return NULL;
    }

    sensor_thresh = (char *)getenv("SENSOR");
    if(sensor_thresh){
        sscanf(sensor_thresh, "%d", &temp_thresh);
        if((temp_thresh <= 0) || (temp_thresh >50)){
            lc_log_basic(LOG_WARNING, "Configured Threshold %d is not in allowed threshold range of 1 to 50",temp_thresh);
        }
        else {
            cpu_threshold = temp_thresh;
        }
    }
    if(ipmi_set_cpu_threshold(cpu_threshold)!=0){
        lc_log_basic(LOG_CRIT, "CPU threshold setting failed");
    }
    
    while(1) {
        //length = read(fp, buffer, 1024);
        FD_SET(fp,&fd);
        err = select(fp+1, &fd, NULL, NULL, NULL);
        if((err)&&(FD_ISSET(fp,&fd))) {
           length = read(fp,buffer,LARGE_STR-1);
            if((length > 0)&&(strstr(buffer,"ipmievd")!=NULL)) {
            if(strstr(buffer, FAN_SENSOR) != NULL) {
              if(strstr(buffer, "Lower Critical going low") != NULL) {
                char *t = NULL;
                int reading, thresh;
                t = strstr(buffer, "Reading");
                if(t){
                  sscanf(t, "Reading %d < Threshold %d %*s", &reading, &thresh);
                }
                if(reading < thresh){
                err = bn_event_request_msg_create(&req,
                         "/nkn/ipmi/events/fanfailure");
                err = mdc_send_mgmt_msg(gmgmthd_mcc, req, false, NULL,
                        NULL, NULL);
                }
                else {
                err = bn_event_request_msg_create(&req,
                         "/nkn/ipmi/events/fanok");
                err = mdc_send_mgmt_msg(gmgmthd_mcc, req, false, NULL,
                        NULL, NULL);
                }
              }
            } else if((strstr(buffer,PS_SENSOR)!=NULL) &&
                ((strstr(buffer,"Predictive Failure Asserted")!=NULL) ||
                  (strstr(buffer,"Device Absent")!=NULL))) {
                  char sensor_name[MEDIUM_STR] = {0};
                  int32 status = 0;
                  tstring *ret_output = NULL;
                  char *msg = NULL;
                  char *pch = NULL;

                  msg = strstr(buffer, PS_SENSOR);
                  pch = strchr(msg, '-');
                  
                  if(pch != NULL){
                      strlcpy(sensor_name, msg+strlen(PS_SENSOR)+1, pch-msg-strlen(PS_SENSOR)-1);
                  }

                  // The syslog doesnt have the status of PS. So querying using ipmitool
                  err = lc_launch_quick_status(&status, &ret_output, false, 4,
                               "/usr/bin/ipmitool", "sensor", "get",
                               sensor_name);
                  if(err){
                      lc_log_basic(LOG_ERR,"Failed getting Power sensor status");
                      continue;
                  }

                  if((strstr(ts_str(ret_output),"Predictive Failure Asserted")!=NULL)||
                     (strstr(ts_str(ret_output),"Device Absent")!=NULL)) {

                    err = bn_event_request_msg_create(&req,
                         "/nkn/ipmi/events/powerfailure");

                    err = mdc_send_mgmt_msg(gmgmthd_mcc, req, false, NULL,
                        NULL, NULL);
            
                    //Sending a local4 syslog to avoid syslog purge
                    syslog(LOG_LOCAL4, "Power supply failure notified");

                  }else if((strstr(ts_str(ret_output),"Predictive Failure Deasserted")!=NULL) ||
                           (strstr(ts_str(ret_output),"Device Present")!=NULL))  {

                    err = bn_event_request_msg_create(&req,
                         "/nkn/ipmi/events/powerok");
  
                    err = mdc_send_mgmt_msg(gmgmthd_mcc, req, false, NULL,
                        NULL, NULL);

                    syslog(LOG_LOCAL4, "Power supply restore notified");

                  }
                  ts_free(&ret_output);
            } else if(strstr(buffer, TEMP_SENSOR) != NULL) {
                if(strstr(buffer, "Upper Critical going high") != NULL) {
                    char *t = NULL;
                    int reading, thresh;
                    char reading_str[MEDIUM_STR] = {0};
                    char thresh_str[MEDIUM_STR] = {0};
                    t = strstr(buffer, "Reading");
                    if(t){
                        sscanf(t, "Reading %d > Threshold %d %*s", &reading, &thresh);
                    }
                    if(reading > thresh){
                        err = bn_event_request_msg_create(&req,
                                         "/nkn/ipmi/events/temp_ok");
                        bail_error(err);
                    }
                    else {
                        err = bn_event_request_msg_create(&req,
                                         "/nkn/ipmi/events/temp_high");
                        bail_error(err);
                    }
                    snprintf(reading_str, sizeof(reading_str), "%d", reading); 
                    snprintf(thresh_str, sizeof(thresh_str), "%d", thresh);
                    err = bn_event_request_msg_append_new_binding (req, 0, "/nkn/monitor/temp-curr",
                                                                        bt_uint32, (char *)&reading_str, NULL);
                    bail_error(err);

                    err = bn_event_request_msg_append_new_binding (req, 0, "/nkn/monitor/temp-thresh",
                                                                        bt_uint32, (char *)&thresh_str, NULL);
                    bail_error(err);

                    if(strstr(buffer, CPU0_SENSOR)!= NULL){
                        err = bn_event_request_msg_append_new_binding (req, 0, "/nkn/monitor/temp-sensor",
                                                                        bt_string, CPU0_SENSOR, NULL);
                        bail_error(err);
                        err = mdc_send_mgmt_msg(gmgmthd_mcc, req, false, NULL, NULL, NULL);
                        bail_error(err);
                    } else if(strstr(buffer, CPU1_SENSOR)!= NULL){
                        err = bn_event_request_msg_append_new_binding (req, 0, "/nkn/monitor/temp-sensor",
                                                                        bt_string, CPU1_SENSOR, NULL);
                        bail_error(err);
                        err = mdc_send_mgmt_msg(gmgmthd_mcc, req, false, NULL, NULL, NULL);
                        bail_error(err); 
                    }
               }
            }
           } else if (length == 0){
            //Socket closed at the other end
	    close(fp);
            fp = open("/var/log/local4.pipe", O_RDONLY);
            if(fp == -1) {
               lc_log_basic(LOG_NOTICE,"Failed to open pipe for read");
               return NULL;
            }
            FD_ZERO(&fd);
            continue;
            }
            bn_request_msg_free(&req);
        }
        memset(buffer,'\0',1024);
        FD_CLR(fp,&fd);
    }
bail:
    close(fp);
    return NULL;
}


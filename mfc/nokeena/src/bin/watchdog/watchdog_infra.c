/*
 * Filename:  watchdog_watchdog.c
 * Date:      2011/07/06
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2010-2011 Ankeena Networks, Inc.  
 * All rights reserved.
 *
 */

#include "md_client.h"
#include "bnode.h"
#include "watchdog_main.h"
#include <proc_utils.h>
#include "nkn_cntr_api.h"
#include "network.h"
#include "watchdog.h"
#include "file_utils.h"
#include "nkn_mgmt_defs.h"

/* Hold counters */
typedef union counter_val {
    int32 counter32;
    int64 counter64;
    uint32 counteru32;
    uint64 counteru64;
} counter_val_t;

/* Hold the cntr name and values that wer sampled,
 * r1 = t-x, r2 = t,
 * where x is sampling time
 */
typedef struct {
    const char *cntr_name;
    counter_val_t r1;
    counter_val_t r2;
} mfc_probe_cntr_t;

extern char *wd_nvsd_pid;
/*===========================Probe Helper Macros===============================*/

#define  BEGIN_PROBE_CHECK_FUNC(func_name) \
    static tbool func_name(watchdog_cb_arg_t *cb_arg) \
    { \
	tbool ret_val = false; \
	watchdog_cb_arg_t *watchdog_cb  = (watchdog_cb_arg_t *)cb_arg;

#define PROBE_CNTR(name,cntr_name) \
	static mfc_probe_cntr_t name = {cntr_name, {0}, {0}}; \
	name.r1.counteru64 = name.r2.counteru64; \
	name.r2.counteru64 = nkncnt_get_uint64(cntr_name); \
	PROBE_LOG(cntr_name": r1/r2: %lu/%lu",name.r1.counteru64,name.r2.counteru64)

#define PROBE_CNTR_DIFF(name) \
	diff_uint64(name.r1, name.r2)

#define PROBE_CNTR_RET_R1(name) \
	(name.r1.counteru64)

#define PROBE_CNTR_RET_R2(name) \
	(name.r2.counteru64)

#define PROBE_SET_RETVAL(val) \
	(ret_val = val)

#define PROBE_LOG_ONCE(fmt,...)  \
	snprintf(watchdog_cb->ret_msg, sizeof(watchdog_cb->ret_msg), "MFC_Probe:"fmt,##__VA_ARGS__); \

#define PROBE_LOG(fmt,...)  \
	lc_log_basic(probe_dbg_msg, "MFC_Probe:"fmt,##__VA_ARGS__);

#define PROBE_LOG_NOTICE(fmt, ...) \
	lc_log_basic(LOG_NOTICE, "MFC_Probe:"fmt,##__VA_ARGS__);

#define END_PROBE_CHECK_FUNC(func_name) \
	return ret_val; \
    }


#define PROBE_CHECK(name, func_name, thresh,freq, action, enabled, gdb_cmd)                      \
{ func_name, name, gdb_cmd, false, action, enabled, 0, thresh, freq, {0, {0}, NULL}, NULL},

/*===========================Public Declarations===============================*/

/* tmr_hdlr called for each check/alarm function */
int
watchdog_watchdog_tmr_hdlr(int fd, short event_type,
	void *data, lew_context *ctxt);
int
watchdog_watchdog_gdb_file_init(void);

tbool
is_nvsd_hung(void);

tbool
is_nvsd_preread_hung(void);

//extern int preread_prev_state;

int probe_dbg_msg = LOG_DEBUG;

void
check_nvsd_hung(int i, tbool *is_nvsd_dead);

/* set/unset timer for wd alarms*/
int
watchdog_toggle_poll_timer(tbool set);


tbool diff_uint32(counter_val_t r1, counter_val_t r2);
tbool diff_uint64(counter_val_t r1, counter_val_t r2);

nvsd_live_check_t* watchdog_get_nvsd_alarm(const char *name);

tbool
wd_is_grace_prd_elapsed(const char *process,  int64_t *ret_uptime);

int
wd_run_script(const char *abs_path, const tstr_array *argv,
		char **str_ret_output );

int
wd_proc_get_param(const char *proc_file, const char *field, char **value);

int
wd_get_mem_usage(const char *process_name, uint32_t *usage);
/*===========================Local Declarations===============================*/
/*Static Declarations */


static int
watchdog_write_gdb_file(const char *gdb_str, const char *file_name);

static tbool
send_http_request(const char *url, const char *domain, char **ret_msg);

static void
watchdog_rst_thesh_cntr(void);

static int
send_probe_request(int *ret, char *ret_err_msg, int size);

static tbool
is_network_thread_dead_lock(char *ret_msg, int str_size);

/* current PM Hung count number */
static uint32 MFC_PROBE_running_hung_count;


/* counters for storing diff of cod counters
 * This can be moved to the counter table*/
static uint64_t network_thread_counter_val_r1[MAX_EPOLL_THREADS];
static uint64_t network_thread_counter_val_r2[MAX_EPOLL_THREADS];


/* Intended call back function for each of the entry in the counter table
 * As of now most the of the call backs are for calculating the diff alone
 */


/* ------------------Comparator Functions---------------------------------- */
//TODO
// Need to change the comparator functions
/* Could have had the below as macro ##
 * but anyways its all going to take same amount of space
 * in the code segement
 */
/*===========================Public Definitions===============================*/


tbool
diff_uint32(counter_val_t r1, counter_val_t r2)
{

    lc_log_basic(probe_dbg_msg, "counter r2= %u, r1 = %u",
	    r2.counteru32, r1.counteru32);
    return ((r2.counteru32 - r1.counteru32) > 0);
}

tbool
diff_uint64(counter_val_t r1, counter_val_t r2)
{
    lc_log_basic(probe_dbg_msg,"counter r2= %lu, r1 = %lu",
	    r2.counteru64, r1.counteru64);

    if((r2.counteru64 - r1.counteru64) > 0)
	return true;
    return false;
}
/* -----------End of Comparator Functions--------------------------------- */


/* new Changes */




/* last hung count index,Used in scenarios where hung count is z and 
   one hung function gives true for y and 
   other check function returns true z-y.
   This shouldn't restart the nvsd as a single check function didn't return
   true for all the requests configured in the hung count
 */
//static int check_function_hung_index = -1;

/* Include Watch-Dog alarms/check function definitions */
#define PROBE_FUNC
#include "watchdog.inc"
#undef PROBE_FUNC


/* Include watch-Dog alarm/Check function declaration */
nvsd_live_check_t checks[] = {
#define PROBE_CHECKS
#include "watchdog.inc"
#undef PROBE_CHECKS
};

#define NVSD_CHECKS_MAX   sizeof(checks)/sizeof(nvsd_live_check_t)



nvsd_live_check_t* watchdog_get_nvsd_alarm(const char *name) {
    uint32_t i = 0;

    for(i = 0 ; i < NVSD_CHECKS_MAX; i++) {
	if(strcmp(name, checks[i].name) == 0) {
	    return &checks[i];

	}

    }
    return NULL;
}

/*
 *Function called for each watchdog checks based on check index.
 *is_nvsd_dead return value is used to check whether nvsd is alive or not
 */
void
check_nvsd_hung(int i, tbool *is_nvsd_dead)
{
    int err;
	
    PROBE_LOG("Ret val for alarm %s is %d", checks[i].name, checks[i].ret_status);
    /* even if one check function says its a failure announce failure locally */


    if(checks[i].threshold_counter == 0 && checks[i].ret_status)
	lc_log_basic(LOG_NOTICE,"%s", checks[i].cb_arg.ret_msg);

    *is_nvsd_dead |= checks[i].ret_status;

    if(*is_nvsd_dead) {
	checks[i].threshold_counter++;

	/* Don't announce failure till we exceed the threshold */
	if(checks[i].threshold_counter <= checks[i].threshold ) {

	   PROBE_LOG("Supressing failure as we havent excceded the threshold");
	   *is_nvsd_dead = !*is_nvsd_dead;
	}

	/* Dont announce restart,isf log */
	if(*is_nvsd_dead && checks[i].wd_action == WD_ACTION_LOG) {
	   *is_nvsd_dead = !*is_nvsd_dead;
	}

    } else {
	/*Reset the counters here ,only consecutive failures from the sahe check function should restart NVSD*/

	if(checks[i].threshold_counter >= 1) {
	    lc_log_basic(LOG_NOTICE,"MFC_Probe: Recovering from %s", checks[i].name);
	    checks[i].threshold_counter = 0;
	    MFC_PROBE_running_hung_count = 0;
	}

    }


    /* If is_nvsd_dead is true here,we have crossed the threshold */
    if(*is_nvsd_dead) {
	MFC_PROBE_running_hung_count++;
	lc_log_basic(LOG_NOTICE, "MFC_Probe:Suspecting a soft-lockup(%s), %s",
		     checks[i].name, checks[i].cb_arg.ret_msg + strlen("MFC_Probe:"));
	if(MFC_PROBE_running_hung_count >= watchdog_config.hung_count) {
	    int32_t status = 0;
		
	PROBE_LOG("resetting the counters");

	err = mdc_send_event_with_bindings_str_va
		(watchdog_mcc, NULL, NULL, "/nkn/nvsd/notify/nvsd_stuck", 1,
		"failed_check", bt_string, checks[i].name);
	//bail_error(err);

	/* take watchdog bt only when coredump is not enabled */
	if(watchdog_config.is_gdb_ena && !watchdog_config.coredump_enabled) {
	    char *ret_output = NULL;

	    tstr_array *argv = NULL;

	    err = tstr_array_new_va_str(&argv, NULL, 1,
		  checks[i].name);
	    bail_error_null(err, argv);

	    err = wd_run_script("/opt/nkn/bin/wd_gdb.sh",
				argv, &ret_output);
	    safe_free(ret_output);
	    tstr_array_free(&argv);
	    bail_error(err);

	}
	/* Reset the threshold counters */
	watchdog_rst_thesh_cntr();

	/* Reset the running hung count as well */
	MFC_PROBE_running_hung_count = 0;

	}
	return;
    }

bail:
    return;
}
/* Function to determine if NVSD is hung
 * go through the list of check functions,
 * see if they had exceeded the threshold values
 * and if they crossed threshold value advertise
 * that nvsd is hung
 */
tbool
is_nvsd_hung(void)
{
    tbool is_nvsd_dead = false;
    uint32_t i = 0;

    for( i = 0; i < NVSD_CHECKS_MAX ; i++) {
	 check_nvsd_hung(i, &is_nvsd_dead);
	 if (is_nvsd_dead)
		return is_nvsd_dead;	 
    }
    return is_nvsd_dead;
}

/* Function to determine if NVSD preread is hung
 * Check only preread counters
 */
tbool
is_nvsd_preread_hung(void)
{
    tbool is_nvsd_dead = false;

    check_nvsd_hung(5, &is_nvsd_dead);
    return is_nvsd_dead;	 
}

tbool
wd_is_grace_prd_elapsed(const char *process, int64_t *ret_uptime)
{
    int err = 0;
    tbool grace_period_elapsed = false;
    char proc_uptime[256] = {0};
    tstring *ts_uptime = NULL;
    int64_t up_time = 0;

    snprintf(proc_uptime, sizeof(proc_uptime),
	    "/pm/monitor/process/%s/uptime", process);

    err = mdc_get_binding_tstr(watchdog_mcc, NULL,
				NULL, NULL,
				&ts_uptime, proc_uptime, NULL);
    bail_error_null_quiet(err, ts_uptime);

    if(ts_length(ts_uptime))
	up_time = atol(ts_str(ts_uptime));

    if( up_time && (up_time > (watchdog_config.grace_period * 1000)))
	grace_period_elapsed = true;

    *ret_uptime = up_time;
bail:
    ts_free(&ts_uptime);
    return grace_period_elapsed;

}

int
wd_run_script(const char *abs_path, const tstr_array *argv, char **str_ret_output )
{

	int err = 0;
	uint32 param_count = 0;
	uint32 i;
	tstr_array *parts = NULL;
	tstr_array *proc_parts = NULL;
	lc_launch_params *lp = NULL;
	lc_launch_result *lr = NULL;

	lr = malloc(sizeof(*lr));
	bail_null(lr);
	memset(lr, 0 ,sizeof(*lr));

	err = lc_launch_params_get_defaults(&lp);
	bail_error_null(err, lp);

	/* Set the nice level */
	lp->lp_priority = -2;

	/* num of params */
	param_count = tstr_array_length_quick(argv);

	err = ts_new_str(&(lp->lp_path), abs_path);
	bail_error(err);

	err = tstr_array_new(&(lp->lp_argv), 0);
	bail_error(err);

	err = tstr_array_insert_str(lp->lp_argv, 0, abs_path);
	bail_error(err);

	for(i = 1; i < param_count + 1 ; i++)
	{
		const char *param_value = NULL;

		param_value = tstr_array_get_str_quick(argv , i-1);

		if(!param_value)
			break;

		err = tstr_array_insert_str(lp->lp_argv, i,
				param_value); 

	}

	lp->lp_wait = true;
	lp->lp_env_opts = eo_preserve_all;
	lp->lp_log_level = LOG_INFO;

	lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
	lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;

	err = lc_launch(lp, lr);
	bail_error(err);

	if(lr->lr_exit_status == -1) {
		lc_log_basic(LOG_DEBUG, "unable to get the proc status");

	} else {

		lc_log_basic(LOG_DEBUG, "got some output");
		lc_log_basic(LOG_DEBUG, "%s",lr->lr_captured_output ? ts_str(lr->lr_captured_output): "");
		if (lr->lr_captured_output)
		   *str_ret_output = strdup(ts_str(lr->lr_captured_output));

	}

bail :
	lc_launch_params_free(&lp);
	lc_launch_result_free_contents(lr);
	return err;

}

int
watchdog_watchdog_gdb_file_init(void)
{
	int err = 0;
	uint32_t i = 0;
	for( i = 0; i < NVSD_CHECKS_MAX ; i++) {

		err = watchdog_write_gdb_file(checks[i].gdb_cmd,
				checks[i].name);
		bail_error(err);
	}
bail:
	return err;
}

static int
watchdog_write_gdb_file(const char *gdb_str, const char *file_name)
{
	int err = 0;
	uint32_t i = 0;
	char bat_file[64] = {0};
	tbool ret_test = false;

	snprintf(bat_file, sizeof(bat_file), "/vtmp/%s", file_name);

	bail_null(gdb_str);
	bail_null(file_name);

	err = lf_test_exists (bat_file, &ret_test);
	bail_error(err);

	if(ret_test) {
		err = lf_remove_file(bat_file);
		bail_error(err);
	}

	err = lf_write_file(bat_file, 0666, gdb_str, true, strlen(gdb_str));
	bail_error(err);

	if(err) {
	    lc_log_basic(LOG_NOTICE, "Error Writing cmd file");
	}

bail:
	return err;

}

int
watchdog_toggle_poll_timer(tbool set)
{
    int err = 0;
    uint32_t i = 0;

    for( i = 0; i < NVSD_CHECKS_MAX ; i++) {

	if(set) {
	    checks[i].cb_arg.alarm_idx = i;
	    err = lew_event_reg_timer(watchdog_lwc,
		    &checks[i].watchdog_timer,
		    watchdog_watchdog_tmr_hdlr,
		    (void *) &checks[i] ,
		    checks[i].poll_time);
	    bail_error(err);
	} else {
	    err = lew_event_cancel(watchdog_lwc, &checks[i].watchdog_timer);
	    bail_error(err);
	}
    }
bail:
    return err;
}

int
watchdog_watchdog_tmr_hdlr(int fd, short event_type,
	void *data, lew_context *ctxt) {
    int err = 0;

    nvsd_live_check_t *alarms = (nvsd_live_check_t*)data;

    lc_log_basic(probe_dbg_msg,"Alarm function (%s)called", alarms->name);

    if(alarms->check_function && alarms->enabled)
	alarms->ret_status = alarms->check_function(&(alarms->cb_arg));

    /*get the return  message and store in check functrion itself */

    err = lew_event_reg_timer(watchdog_lwc,
	    &alarms->watchdog_timer,
	    watchdog_watchdog_tmr_hdlr,
	    (void *) alarms ,
	    alarms->poll_time);
    bail_error(err);

bail:
    return 0;
}


int
wd_proc_get_param(const char *proc_file, const char *field, char **value)
{
	int err = -1;
	tstr_array *ret_lines = NULL;
	uint32_t i = 0;
	tstr_array *parts = NULL;

	err = lf_read_file_lines(proc_file, &ret_lines);
	//bail_error(err);

	if(field && ret_lines
		&& (tstr_array_length_quick(ret_lines) > 0)) {

		for(i = 0; i < tstr_array_length_quick(ret_lines); i++) {
						err = ts_tokenize_str(tstr_array_get_str_quick(ret_lines, i),
					':', '\\', '"', ttf_ignore_leading_separator, &parts);
			bail_error(err);

			if( parts && tstr_array_get_str_quick(parts,0)
					&& !strcmp(tstr_array_get_str_quick(parts,0), field)) {
				*value = strdup(tstr_array_get_str_quick(parts,1));
				err = 0;
				break;
			}
			tstr_array_free(&parts);
		}
	}

bail:
	tstr_array_free(&parts);
	tstr_array_free(&ret_lines);
	return err;
}

int
wd_get_mem_usage(const char *process_name, uint32_t *usage)
{
	int pid = 0;
	str_value_t proc_file = {0};
	char *mem_usage = NULL;
	tstring *t_pid = NULL;
	unsigned int proc_mem_usage = 0;
	int err = 0;

	if(strcmp(process_name, "nvsd") == 0 && wd_nvsd_pid) {
		snprintf(proc_file, sizeof(proc_file),
		    "/proc/%s/status", wd_nvsd_pid);
	} else {
		lc_log_basic(LOG_NOTICE, "Bad process name");
	}

	err = wd_proc_get_param(proc_file, "VmRSS", &mem_usage);
//	complain_error(err);

	/* Not able to read the proc
	 * possible that PID that we got is wrong(old/changed)
	 * retry,if we dont dont get after retry
	 * dont process alarm
	 */

	 err = mdc_get_binding_tstr(watchdog_mcc, 0, 0, 0, &t_pid, "/pm/monitor/process/nvsd/pid", NULL);
	 bail_error(err);

	 if(t_pid)
	 {
	    safe_free(wd_nvsd_pid);
	    wd_nvsd_pid = strdup(ts_str(t_pid));

	 }

	if(strcmp(process_name, "nvsd") == 0 && wd_nvsd_pid) {
		snprintf(proc_file, sizeof(proc_file),
		    "/proc/%s/status", wd_nvsd_pid);
	} else {
		lc_log_basic(LOG_NOTICE, "Bad process name");
	}
	safe_free(mem_usage);

	err = wd_proc_get_param(proc_file, "VmRSS", &mem_usage);
	/* not checking for error here as the caller
	 * will handle based on value/result arg
	 */

	//bail_error(err);

	(mem_usage) ? (void) sscanf(mem_usage, "\t %u kB", usage) : (void) 0;
bail:
	safe_free(mem_usage);
	ts_free(&t_pid);
	return err;
}

/*===========================Local Definitions===============================*/
static tbool
is_network_thread_dead_lock(char *ret_msg, int str_size)
{
    int err = 0;
    bn_binding *binding = NULL;
    uint32 code = 0;
    uint32 num_network_threads = 0;
    uint32 i = 0;
    tbool is_network_thread_stuck = false;
    tbool retval = false;
    char counter_network_thread[64] = {0};

    num_network_threads = nkncnt_get_uint32("glob_network_num_threads");

    lc_log_basic(probe_dbg_msg,"is_MFC_network_thread_dead_lock %u",num_network_threads);

    for(i = 0; i < num_network_threads; i++) {

	snprintf(counter_network_thread, sizeof(counter_network_thread),"monitor_%u_network_thread", i);

	network_thread_counter_val_r2[i] = nkncnt_get_uint64(counter_network_thread);

	if(!(network_thread_counter_val_r2[i] - network_thread_counter_val_r1[i]) ) {
	    //Some network thread is stuck
	    snprintf( ret_msg + strlen(ret_msg), str_size, " %u: %lu/%lu,",
		    i, network_thread_counter_val_r2[i], network_thread_counter_val_r1[i] );
	    is_network_thread_stuck = true ;
	}
	network_thread_counter_val_r1[i] = network_thread_counter_val_r2[i];

    }
    if(is_network_thread_stuck) {
	lc_log_basic(probe_dbg_msg, "MFC_PROBE:Network thread got stuck");
	retval = true;
    }

bail:
    bn_binding_free(&binding);
    return retval;
}

/* Send a wget to an object matching the
 * Probe namespace which has the apache server as the origin,
 * the request is setn with no wait,
 * and we wait for some time assuming it would complete
 * and give nt status, else if it take to long
 * kill and reap the child 
 */
static int
send_probe_request(int *ret, char *ret_err_msg, int str_size)
{
    char apache_url[STR_TMP_MAX] = {0};
    *ret = 0;
    char *ret_msg = NULL;
    /* if htpd is not enabled or probe namespace is disabled return true here */
    if(!watchdog_config.is_httpd_enabled || !watchdog_config.is_httpd_listen_enabled
	    || !watchdog_config.has_mgmt_intf || !watchdog_config.nvsd_listen_port) {
	*ret = 1;
	return 0;
    }

    lc_log_basic(probe_dbg_msg, "MFC_PROBE:Probe URI is %s", watchdog_config.probe_uri);
    if(send_http_request(watchdog_config.probe_uri, watchdog_config.domain, &ret_msg)) {
	*ret = 1;
    } else {
	/* check if httpd is working */
	//lc_log_basic(LOG_NOTICE, "%s", ret_msg);
	snprintf(apache_url, sizeof(apache_url),"http://%s:%s"PROBE_OBJ, watchdog_config.probe_o_host
		,watchdog_config.probe_o_port);
	snprintf(ret_err_msg, str_size, "%s", ret_msg);
	safe_free(ret_msg);
	if(!send_http_request(apache_url, watchdog_config.mgmt_ip, &ret_msg)) {
	    lc_log_basic(LOG_NOTICE, "HTTPD is not running!!!");
	    snprintf(ret_err_msg, str_size, "%s", ret_msg);
	    *ret = 1;
	}
    }
    safe_free(ret_msg);
    return 0;
}

static tbool
send_http_request(const char *url,const char *domain, char **ret_msg)
{

    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;
    pid_t ret_pid;
    int ret_status = 0;
    int err = 0;
    tbool ret = false;

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0 ,sizeof(*lr));

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    err = ts_new_str(&(lp->lp_path), PYTHON_PATH);
    bail_error(err);
    err = tstr_array_new(&(lp->lp_argv), 0);
    bail_error(err);

    err = tstr_array_insert_str(lp->lp_argv, 0, PYTHON_PATH);
    bail_error(err);


    err = tstr_array_insert_str(lp->lp_argv, 1, PROBE_SCRIPT);
    bail_error(err);

    err= tstr_array_insert_str(lp->lp_argv, 2, url);
    bail_error(err);

    err= tstr_array_insert_str(lp->lp_argv, 3, domain);
    bail_error(err);

    lp->lp_wait = false;

    lp->lp_env_opts = eo_preserve_all ;

    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    err = lc_launch(lp, lr);
    bail_error(err);


    lc_wait(lr->lr_child_pid, 1, 3000, &ret_pid, &ret_status);
    lc_log_basic(probe_dbg_msg, "sending http request: Process exited with status %d:%d retpid =%d:%d\n",
	    lr->lr_exit_status,ret_status,lr->lr_child_pid,ret_pid);
    err = lc_launch_complete_capture(lr);
    bail_error(err);
    if(ret_pid == -1) {
	lc_log_basic(LOG_DEBUG, "Now Kill and reap the child");
	err = lc_kill_and_reap(lr->lr_child_pid, SIGTERM, 150, SIGKILL);
	if( err == lc_err_not_found) {
	    err = 0;
	    lc_log_basic(probe_dbg_msg, "Unable to reap the process");
	}
	ret = false;
    } else {
	lc_log_basic(LOG_DEBUG, "ret value is %s",ts_str(lr->lr_captured_output));
	if(lr->lr_captured_output != NULL && ts_equal_str(lr->lr_captured_output, "Success\n", false)) {

	    ret = true;
	    lc_log_basic(probe_dbg_msg, "MFC_Probe: Probe request returned SUCCESS");
	}
    }

    if(lr->lr_captured_output) {
	*ret_msg = strdup(ts_str(lr->lr_captured_output));
    }
bail :
    lc_launch_params_free(&lp);
    if(lr) {
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }
    return ret;

}

/* Reset the counters used for caluculating diff */
static void
watchdog_rst_thesh_cntr(void)
{
    uint32_t i = 0;
    for(i = 0; i < NVSD_CHECKS_MAX; i++) {
	checks[i].threshold_counter = 0;
    }
    memset(network_thread_counter_val_r1, 0,sizeof(network_thread_counter_val_r1));
    memset(network_thread_counter_val_r2, 0,sizeof(network_thread_counter_val_r2));

}


/*===========================End of watchdog_watchdog.c===============================*/


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <glib.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/prctl.h>
#include <sys/epoll.h>
#include "nkn_cse.h"
#include "nkn_debug.h"
#include "cse_mgmt.h"
#include "nkn_cue.h"
#include "nkn_stat.h"
#include "nkn_mem_counter_def.h"

/* external variables */
/* dynamic log level, from libmgmtlog library */
extern int jnpr_log_level;
extern void config_and_run_counter_update(void);

/* global variables */
/* mgmt thread name */
static GThread	*mgmt_thread;
AO_t glob_crawler_in_progress_cnt;
int mod_crawler = 1;
/* queue and mutex for timer */
TAILQ_HEAD(timer_head, crawler_context) g_cse_timerq_head;

pthread_mutex_t g_cse_timerq_mutex = PTHREAD_MUTEX_INITIALIZER;

int32_t nkn_cse_check_if_next_trigger_time_reached(crawler_context_t *obj);
void nkn_cse_set_next_payload_fd(crawler_context_t *obj);
int32_t nkn_cse_update_master_del_asx_list(crawler_context_t *obj);
void nkn_cse_close_payload_fd(crawler_context_t *obj);
void nkn_cse_try_send_change_lists(crawler_context_t *obj);
int32_t nkn_cse_handle_prev_task(crawler_context_t *obj);
void nkn_cse_generate_script_action_arg(char *buf, crawler_context_t *obj);
void nkn_cse_remove_deleted_crawler_dir(crawler_context_t *obj);
void nkn_cse_get_uriwise_stats(crawler_context_t *obj);
void nkn_cse_reset_counters(crawler_context_t *obj);
void nkn_cse_reset_crawling_status(crawler_context_t *obj);
time_t nkn_cse_get_expiry(const char *source_type, const char *dest_type,
	crawler_context_t *obj);
/*
 * Maximum refresh interval = 30 days.
 */
#define MAX_REFRESH_INTERVAL 24*60*60*30
#define MAX_POST_PAYLOAD 1000
#define MAX_STATUS_LEN 32

char query_status_message[CCP_MAX_STATUS+1][MAX_STATUS_LEN] = {
    "UNKNOWN",
    "OK",
    "NOT_STARTED",
    "IN_PROGRESS",
    "INVALID",
    "NOT_OK"
};

int
cse_mgmt_start_thread(void)
{
    int err = 0 ;

    prctl(PR_SET_NAME, "cse-mgmt", 0, 0, 0);

    err = cse_init_mgmt();
    bail_error(err);

bail:
    //if we are out of cse_init_mgmt(), means, we are suppose to exit 
    exit(0);
    return err;
}

int
cse_mgmt_thread_init(void)
{
    GError  *thread_err = NULL;
    int err = 0;

    mgmt_thread = g_thread_create((GThreadFunc) cse_mgmt_start_thread,
	    NULL, TRUE, &thread_err);
    if (NULL == mgmt_thread) {
	lc_log_basic(LOG_NOTICE,
		"CSED management thread initialization failed");
	g_error_free(thread_err);
	err = 1;
    }

    return err;
}


int
main(void)
{
    int err = 0;

    jnpr_log_level = LOG_NOTICE;
    g_thread_init(NULL);
    TAILQ_INIT(&g_cse_timerq_head);
    DBG_CRAWL(" Crawler invoked");

    /* Initialize counters */
    config_and_run_counter_update();

    cue_init();

    nkn_ccp_client_init((char *)"cse", (char *)"cde", MAX_CRAWLERS);

    err = cse_mgmt_thread_init();
    bail_error(err);

    //timer_unit_test();
    nkn_cse_timer_routine();

bail:
    return(err);
}

/*
foreach entry in timer_q
check if config status is ACTIVE and crawling status is not CRAWL_IN_PROGRESS
1. get next_trigger_time
2. check if current time crossed next trigger time
3. set the next trigger time for this config
4. set the crawling status to CRAWL_IN_PROGRESS
4. Trigger a job routine for this config
5. go to next entry in the timer_q
6. If end reached 
	    go to first entry.
*/
void
nkn_cse_timer_routine()
{

    crawler_context_t *obj = NULL;
    uint64_t cur_time;
    int32_t err;

    GThreadPool *crawler_thread_pool;
    g_thread_pool_set_default_stack_size(256*1024);
    crawler_thread_pool =
	g_thread_pool_new((GFunc) nkn_cse_job_routine, obj,
					MAX_CRAWLERS, TRUE, NULL);

    g_thread_pool_set_max_threads
	((GThreadPool *) crawler_thread_pool, MAX_CRAWLERS, NULL);

    while (mod_crawler) {

	pthread_mutex_lock(&g_cse_timerq_mutex);
	TAILQ_FOREACH(obj, &g_cse_timerq_head, timer_entries) {

	    if (obj->crawling_activity_status == CRAWL_CTXT_DELETED) {
		err = nkn_cse_handle_prev_task(obj);
		if (err == -1)
		    continue;
		// free obj only after the conn_obj is cleaned.
		if (obj->conn_obj == NULL) {
		     char counter_str[512];
		     snprintf(counter_str, 512,
			     "glob_cse_%s_total_crawls", obj->cfg.name);
		     nkn_mon_delete(counter_str, NULL);
		     snprintf(counter_str, 512,
			     "glob_cse_%s_crawl_fail_cnt", obj->cfg.name);
		     nkn_mon_delete(counter_str, NULL);
		     snprintf(counter_str, 512,
			     "glob_cse_%s_schedule_miss_cnt", obj->cfg.name);
		     nkn_mon_delete(counter_str, NULL);
		     snprintf(counter_str, 512,
			     "glob_cse_%s_force_termination_cnt",
			    obj->cfg.name);
		     nkn_mon_delete(counter_str, NULL);
		     snprintf(counter_str, 512,
			     "glob_cse_%s_origin_down_cnt", obj->cfg.name);
		     nkn_mon_delete(counter_str, NULL);

		    nkn_cse_remove_deleted_crawler_dir(obj);
		    TAILQ_REMOVE(&g_cse_timerq_head, obj, timer_entries);
		    DBG_LOG(MSG, MOD_CSE, "Crawler cxt %s is deleted",
			    obj->cfg.name);
		    free(obj);
		    continue;
		}
	    }

	    err = nkn_cse_check_if_valid_schedule(obj);
	    if (err == -1) {
		continue;
	    }
	    cur_time = get_cur_time();

	    if (cur_time >= obj->next_trigger_time &&
			(cur_time < obj->cfg.stop_time ||
			    obj->cfg.stop_time == 0)) {

		switch (obj->cfg.status) {
		    case ACTIVE:
			switch (obj->crawling_activity_status) {
			   case CRAWL_IN_PROGRESS:
				/*
			 	* Before proceeding with the next scheduling,
			 	* check the status of the prev task
			 	* if task is completed, cleanup the structure 
			 	* else wait until the task is abrted.
			 	*/
				obj->stats.num_schedule_misses++;
				obj->stats.num_failures++;
				err = nkn_cse_handle_prev_task(obj);
				if (err == -1)
			    	    continue;

			    case NOT_SCHEDULED:
				if (obj->in_job_routine) {
				    DBG_LOG(MSG, MOD_CSE, "[%s] prev job thread"
					    " is still active",
					    obj->cfg.name);
				    continue;
				} else {
				    err = nkn_cse_handle_prev_task(obj);
				    if (err == -1)
					continue;
				    nkn_cse_set_next_trigger_time(obj,
				    	obj->cfg.refresh_interval);
				    if (AO_load(&glob_crawler_in_progress_cnt)
				    	>= MAX_INPROGRESS_CRAWLER_CNT) {
				        DBG_LOG(MSG, MOD_CSE,
				    	"Already %d crawlers are in progress,"
				    	"Cannot start crawling for the crawler"
				    	" %s", MAX_INPROGRESS_CRAWLER_CNT,
				    	obj->cfg.name);
				        continue;
				    }
				    obj->crawl_start_time = cur_time;
				    obj->crawl_end_time = 0;
				    obj->crawling_activity_status =
				    		    CRAWL_IN_PROGRESS;
				    AO_fetch_and_add1(
				    	&glob_crawler_in_progress_cnt);
				    nkn_cse_reset_counters(obj);
				    obj->stats.num_total_crawls++;
				    obj->crawl_not_run = 0;
				    DBG_CRAWL(" Crawler \"%s\" Started",
				    	    obj->cfg.name);
				    
				    DBG_LOG(MSG, MOD_CSE, "[%s] Job triggered",
				    	obj->cfg.name);
				    obj->in_job_routine = 1;
				    g_thread_pool_push(
				    	(GThreadPool *)crawler_thread_pool,
				    	obj, NULL);
				    break;
				}

			    default:
			        break;
				
			}
			break;

		    case INACTIVE:
			nkn_cse_set_next_trigger_time(obj,
				    obj->cfg.refresh_interval);
		        break;

		    default:
		        break;
		}
	    } else {
		/*
		 * Either trigger time is not reached or
		 * stop time reached.
		 * If stop time is reached
		 *	abrt the prev ongoing task
		 *  if session is initiated or prev. sub task is completed
		 *  try sending a new task.
		 */
		if (obj->cfg.stop_time !=0 && cur_time > obj->cfg.stop_time) {
		    if (obj->crawling_activity_status != CRAWL_CTXT_DELETED) {
			err = nkn_cse_handle_prev_task(obj);
			if (err == -1)
			    continue;
			obj->crawling_activity_status = CRAWL_CTXT_STOPPED;
			DBG_LOG(MSG, MOD_CSE, "Crawler cxt %s is stopped\n",
				obj->cfg.name);
		    }
		} else {
		    nkn_cse_try_send_change_lists(obj);
		}
	    }
	}
	pthread_mutex_unlock(&g_cse_timerq_mutex);
	sleep(1);
    }
}

void
nkn_cse_set_next_trigger_time(crawler_context_t *obj,
			      uint32_t refresh_interval)
{
    int64_t cur_time;
    cur_time = get_cur_time();
    /*
     * If refresh-interval = 0, allow to crawl only once.
     */
    if (obj->cfg.refresh_interval == 0) {
	obj->next_trigger_time = cur_time + MAX_REFRESH_INTERVAL;
    } else {
	//refresh_interval is in mins
	obj->next_trigger_time = (((cur_time - obj->cfg.start_time)/
			(60 * refresh_interval) + 1) *
			(refresh_interval * 60)) + obj->cfg.start_time;
    }

}

#if 0
void
timer_unit_test()
{
    crawler_context_t *obj;

    int i;

    for (i = 0; i < MAX_CRAWLERS; i++) {
	obj = (crawler_context_t *) nkn_calloc_type(1, sizeof (crawler_context_t),
						    mod_cse_ctxt);
	if(!obj) {
	    perror("malloc failed");
	    exit(EXIT_FAILURE);
	}
	obj->crawling_activity_status = NOT_SCHEDULED;
	sprintf(obj->cfg.name, "%s_%d", "crawler", i);
	sprintf(obj->cfg.url, "%s", "http://10.157.42.19:/wget_test1/");
	obj->cfg.start_time	    = get_cur_time() + i*2;
	obj->cfg.stop_time	    = obj->cfg.start_time + 10000;
	obj->cfg.refresh_interval  = 10;
	strcpy(obj->cfg.accept_extn_list[0].extn_name,"a");
	obj->cfg.accept_extn_list[0].skip_preload = 1;
	obj->cfg.accept_extn_list[0].extn_len =
	    strlen(obj->cfg.accept_extn_list[0].extn_name);

	strcpy(obj->cfg.accept_extn_list[1].extn_name,"b");
	obj->cfg.accept_extn_list[1].skip_preload = 1;
	obj->cfg.accept_extn_list[1].extn_len =
	    strlen(obj->cfg.accept_extn_list[1].extn_name);

	strcpy(obj->cfg.accept_extn_list[2].extn_name,"c");
	obj->cfg.accept_extn_list[2].extn_len =
	    strlen(obj->cfg.accept_extn_list[2].extn_name);

	strcpy(obj->cfg.accept_extn_list[3].extn_name,"wmv");
	obj->cfg.accept_extn_list[3].extn_len =
	    strlen(obj->cfg.accept_extn_list[3].extn_name);

	strcpy(obj->cfg.accept_extn_list[4].extn_name,"e");
	obj->cfg.accept_extn_list[4].extn_len =
	    strlen(obj->cfg.accept_extn_list[4].extn_name);

	strcpy(obj->cfg.accept_extn_list[5].extn_name,"f");
	obj->cfg.accept_extn_list[5].extn_len =
	    strlen(obj->cfg.accept_extn_list[5].extn_name);

	strcpy(obj->cfg.accept_extn_list[6].extn_name,"g");
	obj->cfg.accept_extn_list[6].extn_len =
	    strlen(obj->cfg.accept_extn_list[6].extn_name);

	strcpy(obj->cfg.accept_extn_list[7].extn_name,"h");
	obj->cfg.accept_extn_list[7].extn_len =
	    strlen(obj->cfg.accept_extn_list[7].extn_name);

	strcpy(obj->cfg.accept_extn_list[8].extn_name,"i");
	obj->cfg.accept_extn_list[8].extn_len =
	    strlen(obj->cfg.accept_extn_list[8].extn_name);
	obj->num_accept_extns = 9;

	obj->cfg.extn_action_list[i].action_type
	    = ACTION_AUTO_GENERATE;
	strcpy(obj->cfg.extn_action_list[i].action.auto_gen.source_type, "wmv");
	strcpy(obj->cfg.extn_action_list[i].action.auto_gen.dest_type, "asx");
	obj->num_extn_actions = 1;
	obj->cfg.status = ACTIVE;
	obj->next_trigger_time = obj->cfg.start_time + 5;
	TAILQ_INSERT_TAIL(&g_cse_timerq_head, obj, timer_entries);
    }
}
#endif

uint64_t
get_cur_time(void)
{
    struct timeval tim;
    gettimeofday(&tim, NULL);
    uint64_t time_in_sec;

    time_in_sec = tim.tv_sec + (tim.tv_usec/1000000.0);

    return(time_in_sec);
}

void
nkn_cse_job_routine(crawler_context_t *obj)
{
    const char *script_path = SCRIPT_PATH;
    const char *script_name = SCRIPT_NAME;
    char buf[256];
    char *cp;
    FILE *fp;
    char script_command[PATH_MAX+256];
    char script_args[4096];
    char *temp = NULL;

    if (!obj)
	return;

    nkn_cse_generate_script_args((char *)script_args, obj);

    /*
     * Launch the script using popen.
     * the script puts the pid of the wget on to the screen.
     * read the pid and store it in crawler_context obj.
     */
    sprintf(script_command, "perl %s/%s %s", script_path, script_name,
					    script_args);

    fp = popen(script_command, "r");
    if (fp == NULL) {
	DBG_LOG(SEVERE, MOD_CSE,
		"[%s] popen failure while triggering the script",
			    obj->cfg.name);
    } else {
	while (fgets(buf, sizeof(buf), fp)) {
	    if (strcasestr(buf, "crawler_pid:")) {
		obj->wget_status = CSE_WGET_TRIGGERED;
		cp = strchr(buf, ':');
		obj->crawler_pid = (cp)? atoi(cp + 2): -1;
		if (obj->crawler_pid == -1)
		    assert(0);
		DBG_LOG (MSG, MOD_CSE, "[%s] wget_pid: %d", obj->cfg.name,
			    obj->crawler_pid);
	    } else if (strcasestr(buf, "crawl completed")) {
		DBG_LOG (MSG, MOD_CSE,
		    "[%s] crawl_completed", obj->cfg.name);
		pthread_mutex_lock(&obj->crawler_ctxt_mutex);
		if (obj->wget_status != CSE_WGET_TERMINATED) {
		    obj->wget_status = CSE_WGET_CLEAN_TERMINATED;
		    obj->crawler_pid = 0;
		    if (obj->conn_obj)
			assert(0);
		    obj->conn_obj = nkn_ccp_client_start_new_session(
			    nkn_cse_status_callback_func,
			    nkn_cse_cleanup_func, obj,
			    obj->cfg.name);
		    pthread_mutex_unlock(&obj->crawler_ctxt_mutex);
		    break;
		} else {
		    pthread_mutex_unlock(&obj->crawler_ctxt_mutex);
		}
	    } else if (strcasestr(buf, "Origin is down")) {
		if (obj->wget_status != CSE_WGET_TERMINATED)
		    obj->wget_status = CSE_WGET_NOT_CLEAN_TERMINATED;
		obj->crawler_pid = 0;
		obj->stats.num_failures++;
		obj->stats.num_origin_down_cnt++;
		/*
		 * count has to be decreased since wget is not killed
		 * in this case. also conn_obj will not be alloted 
		 */
		syslog(LOG_ERR, "Crawl %s failed: Origin server down",
			obj->cfg.name);
	    } else if (strcasestr(buf, "WGET_URL_NOT_FOUND")) {
		if (obj->wget_status != CSE_WGET_TERMINATED)
		    obj->wget_status = CSE_WGET_NOT_CLEAN_TERMINATED;
		obj->crawler_pid = 0;
		obj->stats.num_failures++;
		syslog(LOG_ERR, "Crawl %s failed: base-url not found",
			obj->cfg.name);
	    } else if (strcasestr(buf, "WGET_RESP_HDR_NOT_FOUND")) {
		if (obj->wget_status != CSE_WGET_TERMINATED)
		    obj->wget_status = CSE_WGET_NOT_CLEAN_TERMINATED;
		obj->crawler_pid = 0;
		obj->stats.num_failures++;
		syslog(LOG_ERR, "Crawl %s failed: No response from origin",
			obj->cfg.name);
	    } else if (strcasestr(buf, "WGET_INVALID_STATUS_CODE")) {
		if (obj->wget_status != CSE_WGET_TERMINATED)
		    obj->wget_status = CSE_WGET_NOT_CLEAN_TERMINATED;
		obj->crawler_pid = 0;
		obj->stats.num_failures++;
		temp = strrchr(buf, '_');
		syslog(LOG_ERR, "Crawl %s failed: Invalid response code %s"
			 "from origin.", obj->cfg.name,
			 temp+1);
	    } else if (strcasestr(buf, "crawl partially completed")) {
		/*
		 * This case arises when wget is forcibly killed.
		 */
		if (obj->wget_status != CSE_WGET_TERMINATED)
		    obj->wget_status = CSE_WGET_NOT_CLEAN_TERMINATED;
		obj->crawler_pid = 0;
		syslog(LOG_ERR, "Crawl %s failed: Crawl partially completed",
			obj->cfg.name);
	    } else if (strcasestr(buf, "WGET_READ_ERROR")) {
		/*
		 * The crawler pid need not be set here because, after the 
		 * below errors either of the above errors will come.
		 */
		syslog(LOG_ERR, "Crawl %s failed: Read error for http request",
			obj->cfg.name);
	    } else if (strcasestr(buf, "WGET_NO_DATA_RECEIVED")) {
		syslog(LOG_ERR, "Crawl %s failed: No data received",
			obj->cfg.name);
	    }
	}
	pclose(fp);
    }

    if (obj->crawling_activity_status == CRAWL_CTXT_DELETED) {
	nkn_cse_remove_deleted_crawler_dir(obj);
    } else {
	/*
	* send the change lists to cde
	*/
	if (obj->wget_status == CSE_WGET_NOT_CLEAN_TERMINATED)
	    nkn_cse_reset_crawling_status(obj);
    }
    obj->in_job_routine = 0;
}

int8_t
nkn_cse_stop_prev_crawl_instance(crawler_context_t *obj)
{
    FILE *fp;
    char command[20];

    if (obj->wget_status == CSE_WGET_TRIGGERED) {
	obj->wget_status = CSE_WGET_TERMINATED;
	sprintf(command, "pkill -9 -P %d", obj->crawler_pid);
	fp = popen(command, "r");
	if (fp == NULL) {
	    DBG_LOG(SEVERE, MOD_CSE,
		"[%s] Popen failure while killing crawl [pid: %d] instance",
			    obj->cfg.name, obj->crawler_pid);
	    return(-1);
	} else  {
	    DBG_LOG(ERROR, MOD_CSE, "[%s] Crawl interval reached,"
		    "crawl still in progress", obj->cfg.name);
	    syslog(LOG_ERR, "[%s] Crawl interval reached,"
		    "crawl still in progress", obj->cfg.name);
	    syslog(LOG_ERR, "[%s] Wget forcibly terminated, Crawl not completed",
		    obj->cfg.name);
	    obj->stats.num_wget_kills++;
	    obj->stats.num_failures++;
	    pclose(fp);
	    /*
	     * Pkill only kills the wget process, the perl script also has to
	     * be killed.
	     */
	    sprintf(command, "kill -9 %d", obj->crawler_pid);
	    fp = popen(command, "r");
	    if (fp == NULL) {
		DBG_LOG(SEVERE, MOD_CSE,
			"[%s] Popen failure while killing crawl [pid: %d] "
			"instance",
			obj->cfg.name, obj->crawler_pid);
		return(-1);
	    } else  {
		pclose(fp);
	    }
	}
    } else {
	DBG_LOG(MSG, MOD_CSE, "[%s] wget not terminated even though "
		    "crawler_pid is not 0, wget_status: %d",
		    obj->cfg.name, obj->wget_status);
    
    }
    obj->crawler_pid = 0;
    return(0);
}

void
nkn_cse_remove_deleted_crawler_dir(crawler_context_t *obj)
{
    char command[200];
    FILE *fp;

    sprintf(command, "rm -rf %s/%s",
	    OUTPUT_PATH, obj->cfg.name);
    fp = popen(command, "r");
    if (fp == NULL) {
	DBG_LOG(SEVERE, MOD_CSE, "[%s] Popen failure while removing"
		"workspace of the delete crawler", obj->cfg.name);
    } else  {
	pclose(fp);
    }
}

void
nkn_cse_generate_script_args(char *script_args, crawler_context_t *obj)
{
    int i;
    char temp[MAX_ACCEPT_EXTNS*MAX_ACCEPT_EXTN_STR_SIZE];
    char temp1[4096];

    temp[0] = '\0';
    sprintf(script_args, "%s ", obj->cfg.name);
    sprintf (temp1, "\"%s\" ", obj->cfg.url);
    strcat(script_args, temp1);

    sprintf (temp1, " %d ", obj->cfg.link_depth);
    strcat(script_args, temp1);

    for (i = 0; i< obj->num_accept_extns; i++) {
	strcat(temp, obj->cfg.accept_extn_list[i].extn_name);
	strcat(temp, ", ");
    }

    if (obj->num_accept_extns) {
	sprintf (temp1, "\"%s\" ", temp);
	strcat(script_args, temp1);
    } else {
	sprintf (temp1, "\"\" ");
	strcat(script_args, temp1);
    }
    strcat(script_args, OUTPUT_PATH);
    sprintf (temp1, " \"%s\" ", obj->cfg.client_domain);
    strcat(script_args, temp1);

    if (obj->cfg.xdomain_fetch) {
	sprintf (temp1, "\"-H\" ");
	strcat(script_args, temp1);
    } else {
	sprintf (temp1, "\"\" ");
	strcat(script_args, temp1);
    }


    /*
     * send action list also as argument
     * It is the last argument.
     */
    temp1[0] = '\0';
    if (obj->num_extn_actions)
	nkn_cse_generate_script_action_arg(temp1, obj);
    strcat(script_args, temp1);

}

int32_t
nkn_cse_status_callback_func(void *ptr)
{
    crawler_context_t *obj =(crawler_context_t *)ptr;
    ccp_client_session_ctxt_t *conn_obj;
    int err;
    int64_t cur_time;

    pthread_mutex_lock(&obj->crawler_ctxt_mutex);
    conn_obj = obj->conn_obj;
    DBG_LOG(MSG, MOD_CSE, "[%s] query status rxd: %s\n",
	    obj->cfg.name, query_status_message[conn_obj->query_status]);
    switch (conn_obj->query_status) {
	case CCP_NOT_OK:
	case CCP_INVALID:
	case CCP_OK:
	    /*
	     * Check if ack for delete action
	     * update master_send_list file by doing diff
	     * between master_dellist
	     * and proc_dellistfile.
	     */
	    if (obj->payload_file_type == DELETE) {
		err = nkn_cse_update_master_del_list(obj);
		/*
		 * TODO: need to take action on err = -1
		 */
	    }
	    if (obj->payload_file_type == DELETE_ASX) {
		err = nkn_cse_update_master_del_asx_list(obj);
		/*
		 * TODO: need to take action on err = -1
		 */
	    }
	    err = nkn_cse_check_if_next_trigger_time_reached(obj);
	    if (err == -1) {
		conn_obj->status = TASK_COMPLETED;
	        DBG_LOG(MSG, MOD_CSE,
		    "[%s] New trigger time reached, finishing current task",
		     obj->cfg.name);
	    } else {
		err = nkn_cse_form_post_message(obj);
		while (err == -1 && obj->payload_file_type != NONE) {
		    nkn_cse_set_next_payload_fd(obj);
		    err = nkn_cse_form_post_message(obj);
		}
		if (err == -1) {
		    conn_obj->status = TASK_COMPLETED;
		    DBG_LOG(MSG, MOD_CSE,
			    "[%s] All change lists submitted\n",
			    obj->cfg.name);
		} else {
		    DBG_LOG(MSG, MOD_CSE,
			    "[%s] Started New Payload", obj->cfg.name);
		    obj->conn_obj->status = POST_TASK;
		    nkn_ccp_client_remove_from_waitq (obj->conn_obj);
		}
	    }
	    break;

	case CCP_NOT_STARTED:
	case CCP_IN_PROGRESS:
	    err = nkn_cse_check_if_next_trigger_time_reached(obj);
	    if (err == -1) {
		conn_obj->status = ABORT_TASK;
	        DBG_LOG(MSG, MOD_CSE,
		    "[%s] New trigger time reached, aborting current task",
		     obj->cfg.name);
	    } else {
		cur_time = get_cur_time();
		conn_obj->wait_time =
		    ((obj->next_trigger_time - cur_time)<
		     DEFAULT_WAITING_TIME)?
		    (obj->next_trigger_time - cur_time): DEFAULT_WAITING_TIME;
		conn_obj->status = SEND_QUERY;
	    }
	    break;

	default:
	    DBG_LOG(MSG, MOD_CSE, "[%s] Invalid query status rxd\n",
		    obj->cfg.name);
	    goto handle_err;
    }
    if (conn_obj->query_status == CCP_OK ||
	conn_obj->query_status == CCP_NOT_OK || 
	conn_obj->query_status == CCP_INVALID)
    	nkn_cse_get_uriwise_stats(obj);
    pthread_mutex_unlock(&obj->crawler_ctxt_mutex);
    return(1);
handle_err:
    if (conn_obj->query_status == CCP_OK ||
	conn_obj->query_status == CCP_NOT_OK || 
	conn_obj->query_status == CCP_INVALID)
    	nkn_cse_get_uriwise_stats(obj);
    pthread_mutex_unlock(&obj->crawler_ctxt_mutex);
    return(-1);
}

/*
 * Form an xml payload by reading data using the payload_fd in the
 * crawler_ctxt.
 * PROCDELLIST USAGE:
 * The list of url entries which are transffered as a part of current
 * payload will be stored under procdellist.txt or procdellistasx.txt.
 * The masterdellist.xml and masterdellistasx.xml will be updated using
 * these files upon receiving CCP_OK for query.
 */
int32_t
nkn_cse_form_post_message(crawler_context_t *obj)
{
    int32_t num_urls = 0;
    int32_t err;
    char line[4096];
    cse_url_struct_t urlnode;
    FILE *procdelfp = NULL;
    int generate_asx_flag = 0;
    time_t expiry;

    if (obj->payload_file_type == NONE)
	return(-1);

    if (obj->payload_file_type == DELETE_ASX) {
	sprintf (line, "%s/%s/%s", OUTPUT_PATH, obj->cfg.name,
		    "procdellistasx.txt");
	procdelfp = fopen(line, "w");
	if (!procdelfp) {
	    DBG_LOG(MSG, MOD_CSE, "[%s] Failed to create file for writing",
		    obj->cfg.name);
	    goto handle_err;
	}
    }

    if (obj->payload_file_type == DELETE) {
	sprintf (line, "%s/%s/%s", OUTPUT_PATH, obj->cfg.name,
		    "procdellist.txt");
	procdelfp = fopen(line, "w");
	if (!procdelfp) {
	    DBG_LOG(MSG, MOD_CSE, "[%s] Failed to create file for writing",
		    obj->cfg.name);
	    goto handle_err;
	}
    }

    err = nkn_ccp_client_start_new_payload
	    (obj->conn_obj, obj->payload_file_type);
    if (err == -1)
	goto handle_err;

    err = nkn_ccp_client_insert_expiry
	    (&obj->conn_obj->xmlnode, obj->next_trigger_time);
    if (err == -1)
	goto handle_err;

    err = nkn_ccp_client_set_cachepin(&obj->conn_obj->xmlnode);
    if (err == -1)
	goto handle_err;

    err = nkn_ccp_client_add_client_domain(&obj->conn_obj->xmlnode,
					    obj->cfg.client_domain);
    if (err == -1)
	goto handle_err;

    while (num_urls < MAX_POST_PAYLOAD) {

	if (fgets(line, sizeof(line), obj->payload_fd)) {
	    if (obj->payload_file_type == DELETE ||
		    obj->payload_file_type == DELETE_ASX) {
		fprintf (procdelfp, "%s", line);
	    }

	    memset (&urlnode, 0, sizeof(cse_url_struct_t));
	    err = nkn_cse_get_url_attributes(line, &urlnode);
	    if (err == -1) {
		if (urlnode.url) {
		    free(urlnode.url);
		    urlnode.url = NULL;
		}
		if (urlnode.domain) {
		    free(urlnode.domain);
		    urlnode.domain = NULL;
		}
		if (urlnode.etag) {
		    free(urlnode.etag);
		    urlnode.etag = NULL;
		}
	        continue;
	    }

	    err = nkn_cse_check_preload_eligibility(urlnode.url, obj);
	    if (err == -1) {
		if (urlnode.url) {
		    free(urlnode.url);
		    urlnode.url = NULL;
		}
		if (urlnode.domain) {
		    free(urlnode.domain);
		    urlnode.domain = NULL;
		}
		if (urlnode.etag) {
		    free(urlnode.etag);
		    urlnode.etag = NULL;
		}
	        continue;
	    }

	    if (obj->payload_file_type == DELETE_ASX||
			obj->payload_file_type == ADD_ASX) {
		generate_asx_flag = 1;
		expiry = nkn_cse_get_expiry((const char *)"wmv",
				(const char *)"asx", obj);
		if (expiry == -1) {
		    DBG_LOG(ERROR, MOD_CSE,
			    "Action for asx autogeneration not found");
		    continue;
		}
	    } else {
		generate_asx_flag = 0;
		expiry = ((obj->cfg.expiry_mechanism_type ==
				EXP_TYPE_ORIGIN_RESPONSE) ||
			  (obj->cfg.refresh_interval == 0))?
			   0: obj->next_trigger_time;
	    }
	    err = nkn_ccp_client_insert_url_entry(&obj->conn_obj->xmlnode,
			urlnode.url, expiry, 1,
			generate_asx_flag, urlnode.etag, urlnode.size,
			urlnode.domain);

	    // free the urlnode
	    if (urlnode.url) {
	        free(urlnode.url);
	        urlnode.url = NULL;
	    }
	    if (urlnode.domain) {
		free(urlnode.domain);
		urlnode.domain = NULL;
	    }
	    if (urlnode.etag) {
	        free(urlnode.etag);
	        urlnode.etag = NULL;
	    }
	    if (err == -1)
		goto handle_err;
	} else {
	    if (num_urls == 0)
		goto handle_err;
	    break;
	}
	num_urls++;
    }
    err = nkn_ccp_client_insert_uri_count(&obj->conn_obj->xmlnode, num_urls);
    if (err == -1)
	goto handle_err;

    err = nkn_ccp_client_end_payload(obj->conn_obj);
    if (err == -1)
	goto handle_err;

    if (procdelfp)
	fclose(procdelfp);
    return (err);
handle_err:
    if (procdelfp)
	fclose(procdelfp);
    return(-1);
}

int32_t
nkn_cse_set_payload_fd(crawler_context_t *obj, change_list_type_t payload_type)
{
    struct stat st;
    char buf[256];

    nkn_cse_close_payload_fd(obj);

    switch (payload_type) {
	case DELETE_ASX:
	    sprintf(buf,"%s/%s/%s",
		    OUTPUT_PATH, obj->cfg.name, DELLISTASX_FILENAME);
	    break;

	case ADD_ASX:
	    sprintf(buf,"%s/%s/%s",
		    OUTPUT_PATH, obj->cfg.name, ADDLISTASX_FILENAME);
	    break;

	case DELETE:
	    sprintf(buf,"%s/%s/%s",
		    OUTPUT_PATH, obj->cfg.name, DELLIST_FILENAME);
	    break;

	case ADD:
	    sprintf(buf,"%s/%s/%s",
		    OUTPUT_PATH, obj->cfg.name, ADDLIST_FILENAME);
	    break;

	case NONE:
	    return(-1);
    }

    if (stat(buf, &st) == 0) {
	if (st.st_size == 0) {
	    DBG_LOG(MSG, MOD_CSE,
		    "[%s] file is empty: %s\n", obj->cfg.name, buf);
	    return(-1);
	}
        obj->payload_fd = fopen(buf, "r");
	if (!obj->payload_fd){
	    DBG_LOG(ERROR, MOD_CSE,
		    "[%s] Failed to open file %s", obj->cfg.name, buf);
	    return -1;
	}
	obj->payload_file_type = payload_type;
        return(1);
    } else {
	DBG_LOG(MSG, MOD_CSE,
		"[%s] file not available: %s\n", obj->cfg.name, buf);
        return(-1);
    }
}

void
nkn_cse_cleanup_func(void *ptr)
{
    crawler_context_t *obj = (crawler_context_t *)ptr;
    if (obj) {
	nkn_cse_reset_crawling_status(obj);
	pthread_mutex_lock(&obj->crawler_ctxt_mutex);
	if (obj->conn_obj) {
	    nkn_ccp_client_delete_epoll_event(obj->conn_obj->sock);
	    close(obj->conn_obj->sock);
	    if (obj->conn_obj->xmlnode.xmlbuf) {
		xmlFree(obj->conn_obj->xmlnode.xmlbuf);
		obj->conn_obj->xmlnode.xmlbuf = NULL;
	    }
	    if (obj->conn_obj->xmlnode.doc) {
		xmlFreeDoc(obj->conn_obj->xmlnode.doc);
		obj->conn_obj->xmlnode.doc = NULL;
	    }
	    obj->conn_obj->ptr = NULL;
	    free(obj->conn_obj);
	    obj->conn_obj = NULL;
	}
	pthread_mutex_unlock(&obj->crawler_ctxt_mutex);
    }
}

void
nkn_cse_reset_crawling_status(crawler_context_t *obj)
{
    pthread_mutex_lock(&obj->crawler_ctxt_mutex);
    if(obj->crawling_activity_status == CRAWL_IN_PROGRESS ||
	(obj->crawling_activity_status == CRAWL_CTXT_DELETED &&
	 obj->inprogress_before_delete)){
	AO_fetch_and_sub1(&glob_crawler_in_progress_cnt);
	DBG_CRAWL(" Crawler \"%s\" Completed", obj->cfg.name);
	obj->crawl_end_time = get_cur_time();
	obj->inprogress_before_delete = 0;
    }

    if (obj->crawling_activity_status != CRAWL_CTXT_DELETED &&
	obj->crawling_activity_status != CRAWL_CTXT_STOPPED) {
        obj->crawling_activity_status = NOT_SCHEDULED;
	obj->wget_status = CSE_WGET_TERMINATED;
    }
    pthread_mutex_unlock(&obj->crawler_ctxt_mutex);
}

int32_t
nkn_cse_get_url_attributes(char *line, cse_url_struct_t *urlnode)
{
    char *p, *p1;
    int uri_len;
    /*
     * get uri name;
     */
     p = strstr(line, "<url>");
     if (!p) {
	DBG_LOG(MSG, MOD_CSE, "<url> tag is missing %s", line);
	goto handle_err;
     } else {
	p1 = strstr(line, "</url>");
	if (!p1) {
	    DBG_LOG(MSG, MOD_CSE, "</url> tag is missing %s", line);
	    goto handle_err;
	} else {
	    uri_len = p1-(p+5)+1;
	    urlnode->url = (char *) nkn_calloc_type (uri_len, 1, mod_cse_url);
	    if (!urlnode->url) {
	        DBG_LOG(SEVERE, MOD_CSE, "Calloc failed");
	        goto handle_err;
	    } else {
		memcpy (urlnode->url, p+5, p1-(p+5));
	    }
	    if (uri_len > MAX_URI_SIZE) {
		DBG_LOG(ERROR, MOD_CSE,"Url exceeded MAX_URI_SIZE(%d),"
			"Skipping url: %s", MAX_URI_SIZE, urlnode->url);
		goto handle_err;
	    }
	}
     }

    /*
     * get domain ;
     */
     p = strstr(p1+5, "<domain>");
     if (!p) {
	DBG_LOG(MSG, MOD_CSE, "<domain> tag is missing");
	goto handle_err;
     } else {
	p1 = strstr(p, "</domain>");
	if (!p1) {
	    DBG_LOG(MSG, MOD_CSE, "</domain> tag is missing");
	    goto handle_err;
	} else {
	    urlnode->domain = (char *)nkn_calloc_type(
		    p1-(p+8)+1, 1, mod_cse_url);
	    if (!urlnode->domain) {
	        DBG_LOG(SEVERE, MOD_CSE, "Calloc failed");
	        goto handle_err;
	    } else {
		memcpy (urlnode->domain, p+8, p1-(p+8));
	    }
	}
     }


    /*
     * get etag ;
     */
     p = strstr(p1+8, "<etag>");
     if (!p) {
	DBG_LOG(MSG, MOD_CSE, "<etag> tag is missing");
	goto handle_err;
     } else {
	p1 = strstr(p, "</etag>");
	if (!p1) {
	    DBG_LOG(MSG, MOD_CSE, "</etag> tag is missing");
	    goto handle_err;
	} else {
	    urlnode->etag = (char *)nkn_calloc_type(
		    p1-(p+6)+1, 1, mod_cse_etag);
	    if (!urlnode->etag) {
	        DBG_LOG(SEVERE, MOD_CSE, "Calloc failed");
	        goto handle_err;
	    } else {
		memcpy (urlnode->etag, p+6, p1-(p+6));
	    }
	}
     }

    /*
     * get size ;
     */
     p = strstr(p1+6, "<cl>");
     if (!p) {
	DBG_LOG(MSG, MOD_CSE, "<cl> tag is missing");
	goto handle_err;
     } else {
	p = p + 4;
	while (*p == ' ') p++;
	urlnode->size = atol(p);
     }
     return 1;
handle_err:
     return -1;
}

int 
cse_exit_cleanup(void)
{
    return 0;
}

int32_t
nkn_cse_update_master_del_list(crawler_context_t *obj)
{
    /*
     * do  diff between masterdellist and procdellist and generate
     * masterdellist
     */

    FILE *fp;
    char command[256];
    char masterfilename[256];
    char tempfilename[256];
    char procdellistfilename[256];
    char buf[4096];
    sprintf (tempfilename,
	    "%s/%s/%s", OUTPUT_PATH, obj->cfg.name, "temp");
    FILE *tempFP;
    tempFP = fopen (tempfilename, "w");
    if (tempFP == NULL) {
	DBG_LOG(ERROR, MOD_CSE,
		"[%s] Error while creating temp file", obj->cfg.name);
	return -1;
    }
    sprintf (masterfilename,
	    "%s/%s/%s", OUTPUT_PATH, obj->cfg.name, DELLIST_FILENAME);
    sprintf (procdellistfilename,
	    "%s/%s/%s", OUTPUT_PATH, obj->cfg.name, "procdellist.txt");

    sprintf (command, "diff %s %s", masterfilename, procdellistfilename);
    fp = popen(command, "r");
    if (fp == NULL) {
	DBG_LOG(ERROR, MOD_CSE,
		"[%s] Error while creating updated master delete list",
			    obj->cfg.name);
	goto handle_err;
    } else {
	char *ptr;
	ptr = fgets(buf, sizeof(buf), fp);
	if (ptr) {
	    while (ptr) {
		char *cp = buf;
		if (*cp == '<') {
		    cp =strstr(buf, "<url>");
		    if (cp) {
			while (*cp == ' ') {
			    cp++;
			}
			fprintf (tempFP, "%s", cp);
		    }
		}
		ptr = fgets(buf, sizeof(buf), fp);
	    }
	    fclose(tempFP);
	    pclose(fp);

	    sprintf (command, "mv %s %s", tempfilename, masterfilename);
	    fp = popen(command, "r");
	    if (fp == NULL) {
		DBG_LOG(ERROR, MOD_CSE,
			"[%s] Error while creating updated master delete list",
			    obj->cfg.name);
		goto handle_err;
	    }
	    pclose(fp);
	} else {
	    DBG_LOG(MSG, MOD_CSE,
		    "[%s] No diff found between masterdellist "
		    "and procdellist files", obj->cfg.name);
	    unlink(masterfilename);
	}
	return 0;
    }
handle_err:
	return -1;
}

int32_t
nkn_cse_update_master_del_asx_list(crawler_context_t *obj)
{
    /*
     * do  diff between masterdellist and procdellist and generate
     * masterdellist
     */

    FILE *fp;
    char command[256];
    char masterfilename[256];
    char tempfilename[256];
    char procdellistfilename[256];
    char buf[4096];
    FILE *tempFP;

    sprintf (tempfilename,
	    "%s/%s/%s", OUTPUT_PATH, obj->cfg.name, "temp");
    tempFP = fopen (tempfilename, "w");
    if (tempFP == NULL) {
	DBG_LOG(ERROR, MOD_CSE,
	    "[%s] Error while creating temp file", obj->cfg.name);
	return -1;
    }
    sprintf (masterfilename,
	    "%s/%s/%s", OUTPUT_PATH, obj->cfg.name, DELLISTASX_FILENAME);
    sprintf (procdellistfilename,
	    "%s/%s/%s", OUTPUT_PATH, obj->cfg.name, "procdellistasx.txt");

    sprintf (command, "diff %s %s", masterfilename, procdellistfilename);
    fp = popen(command, "r");
    if (fp == NULL) {
	DBG_LOG(ERROR, MOD_CSE,
	    "[%s] Error while creating updated master delete list",
	    obj->cfg.name);
	goto handle_err;
    } else {
	char *ptr;
	ptr = fgets(buf, sizeof(buf), fp);
	if (ptr) {
	    while (ptr) {
		char *cp = buf;
		if (*cp == '<') {
		    cp =strstr(buf, "<url>");
		    if (cp) {
			while (*cp == ' ') {
			    cp++;
			}
			fprintf (tempFP, "%s", cp);
		    }
		}
		ptr = fgets(buf, sizeof(buf), fp);
	    }
	    fclose(tempFP);
	    pclose(fp);

	    sprintf (command, "mv %s %s", tempfilename, masterfilename);
	    fp = popen(command, "r");
	    if (fp == NULL) {
		DBG_LOG(ERROR, MOD_CSE,
			"[%s] Error while creating updated master delete"
			" asx list", obj->cfg.name);
		goto handle_err;
	    }
	    pclose(fp);
	} else {
	    DBG_LOG(MSG, MOD_CSE,
		"[%s] No diff found between masterdellist and procdellist"
		" asx files", obj->cfg.name);
	    unlink(masterfilename);
	}
	return 0;
    }
handle_err:
	return -1;
}

int32_t
nkn_cse_check_if_next_trigger_time_reached(crawler_context_t *obj)
{
    uint64_t cur_time;
    if (obj->crawling_activity_status == CRAWL_CTXT_DELETED ||
	    obj->crawling_activity_status == CRAWL_CTXT_STOPPED)
	return -1;

    cur_time = get_cur_time();
    if (cur_time > obj->next_trigger_time - 1) {
	if (obj->crawling_activity_status
		== CRAWL_IN_PROGRESS) {
	    DBG_LOG(ERROR, MOD_CSE, "[%s] Crawl interval reached,"
		"crawl still in progress", obj->cfg.name);
	    syslog(LOG_ERR, "[%s] Crawl interval reached,"
		                    "crawl still in progress", obj->cfg.name);
	}
	return -1;
    } else {
	return 0;
    }
}

void
nkn_cse_set_next_payload_fd(crawler_context_t *obj)
{
    int err;
    char buf[256];

    switch (obj->payload_file_type) {
        case DELETE_ASX:
            DBG_LOG(MSG, MOD_CSE,
		"[%s] ASX delete list sent\n", obj->cfg.name);
            DBG_LOG(MSG, MOD_CSE,
		"[%s] Trying to send overall delete list\n", obj->cfg.name);
            err = nkn_cse_set_payload_fd(obj, DELETE);
            if (err == -1) {
		DBG_LOG(MSG, MOD_CSE,
			"[%s] Error opening overall delete list file\n",
			obj->cfg.name);
		DBG_LOG(MSG, MOD_CSE,
			"[%s] Trying to send ASX add list\n", obj->cfg.name);
		err = nkn_cse_set_payload_fd(obj, ADD_ASX);
		if (err == -1) {
		    DBG_LOG(MSG, MOD_CSE,
			"[%s] Error opening ASX add list file\n",
			obj->cfg.name);
		    DBG_LOG(MSG, MOD_CSE,
			"[%s] Trying to send overall add list\n",
			obj->cfg.name);
		    err = nkn_cse_set_payload_fd(obj, ADD);
		    if (err == -1) {
			DBG_LOG(MSG, MOD_CSE,
				"[%s] Error opening add list file\n",
				obj->cfg.name);
			obj->payload_file_type = NONE;
		    }
		}
            }
            break;

        case DELETE:
            DBG_LOG(MSG, MOD_CSE,
		    "[%s] Overall delete list sent\n", obj->cfg.name);
            DBG_LOG(MSG, MOD_CSE,
		"[%s] Trying to send ASX add list\n", obj->cfg.name);
            err = nkn_cse_set_payload_fd(obj, ADD_ASX);
            if (err == -1) {
                DBG_LOG(MSG, MOD_CSE,
		    "[%s] Error opening ASX add list file\n", obj->cfg.name);
                DBG_LOG(MSG, MOD_CSE,
		    "[%s] Trying to send overall add list\n", obj->cfg.name);
                err = nkn_cse_set_payload_fd(obj, ADD);
                if (err == -1) {
		    DBG_LOG(MSG, MOD_CSE,
			"[%s] Error opening add list file\n", obj->cfg.name);
		    obj->payload_file_type = NONE;
                }
            }
            break;

        case ADD_ASX:
            DBG_LOG(MSG, MOD_CSE,
		    "[%s] ASX add list sent\n", obj->cfg.name);
            DBG_LOG(MSG, MOD_CSE,
		    "[%s] Trying to send overall add list\n", obj->cfg.name);
            err = nkn_cse_set_payload_fd(obj, ADD);
            if (err == -1) {
		DBG_LOG(MSG, MOD_CSE,
		    "[%s] Error opening overall add list file\n",
		    obj->cfg.name);
		obj->payload_file_type = NONE;
            }
	    /*
	     * Remove the addlistasx file after transferred because if action
	     * is removed, we may endup sending the old list again and again.
	     */
	    snprintf(buf, 256, "%s/%s/%s", OUTPUT_PATH, obj->cfg.name,
		    ADDLISTASX_FILENAME);
	    unlink(buf);
            break;

        case ADD:
            DBG_LOG(MSG, MOD_CSE, "[%s] Overall add list sent\n",
		    obj->cfg.name);
	    nkn_cse_close_payload_fd(obj);
            obj->payload_file_type = NONE;
	    snprintf(buf, 256, "%s/%s/%s", OUTPUT_PATH, obj->cfg.name,
		    ADDLIST_FILENAME);
	    unlink(buf);
            break;

        case NONE:
            break;
    }
}

int32_t
nkn_cse_check_preload_eligibility(char *url, crawler_context_t *obj)
{
    int i;
    char *ptr;
    ptr = basename(url);
    if (!ptr)
	return 0;

    for (i=0; i< MAX_ACCEPT_EXTNS; i++)
    {
	if (obj->cfg.accept_extn_list[i].skip_preload) {
	    if (!strcasecmp(
		ptr + (strlen(ptr) - obj->cfg.accept_extn_list[i].extn_len),
		obj->cfg.accept_extn_list[i].extn_name)) {
		return -1;
	    }
	}
    }
    return 0;
}

int32_t
nkn_cse_check_if_valid_schedule(crawler_context_t *obj)
{
    uint64_t cur_time;
    int temp;
    if (obj->cfg.start_time == 0) {
	return -1;
    }

    cur_time = get_cur_time();

    if (obj->cfg.start_time <= cur_time) {
	if (obj->cfg.refresh_interval) {
	    if (!obj->next_trigger_time ||
		    (obj->next_trigger_time > cur_time)) {
		temp = ((cur_time - obj->cfg.start_time) %
			(60 * obj->cfg.refresh_interval));
		if (temp) {
		    obj->next_trigger_time =
			(((cur_time - obj->cfg.start_time)/
			(60 * obj->cfg.refresh_interval) + 1) *
			(obj->cfg.refresh_interval * 60)) +
			    obj->cfg.start_time;
		} else {
		    obj->next_trigger_time = cur_time;
		}
	    }
	} else {
	    if (!obj->next_trigger_time)
		obj->next_trigger_time = cur_time + MAX_REFRESH_INTERVAL;
	}
    } else if (obj->cfg.start_time > cur_time) {
	obj->next_trigger_time = obj->cfg.start_time;
    }

    if (cur_time < obj->cfg.stop_time &&
	    obj->crawling_activity_status == CRAWL_CTXT_STOPPED) {
	if (obj->cfg.start_time <= cur_time) {
	    if (obj->cfg.refresh_interval) {
		obj->next_trigger_time =
		    (((cur_time - obj->cfg.start_time)/
		    (60 * obj->cfg.refresh_interval) + 1) *
		    (obj->cfg.refresh_interval * 60)) + obj->cfg.start_time;
	    } else {
		obj->next_trigger_time = cur_time + MAX_REFRESH_INTERVAL;
	    }
	}
	obj->crawling_activity_status = NOT_SCHEDULED;
    }
    return 0;
}

void
nkn_cse_close_payload_fd(crawler_context_t *obj)
{
    if (obj->payload_fd) {
	fclose(obj->payload_fd);
	obj->payload_fd = NULL;
    }
}

void
nkn_cse_try_send_change_lists(crawler_context_t *obj)
{
    int err;
    if (obj->conn_obj &&
        obj->crawling_activity_status != CRAWL_CTXT_STOPPED &&
        obj->crawling_activity_status != CRAWL_CTXT_DELETED) {

        switch (obj->conn_obj->status) {
            /*
            * session created for sending the files,
            * start the task.
            */
            case SESSION_INITIATED:
		err = nkn_cse_set_payload_fd(obj, DELETE_ASX);
		if (err == -1) {
		    err = nkn_cse_set_payload_fd(obj, DELETE);
		    if (err == -1) {
			err = nkn_cse_set_payload_fd(obj, ADD_ASX);
			if (err == -1) {
			    err = nkn_cse_set_payload_fd(obj, ADD);
			    if (err == -1) {
				nkn_cse_cleanup_func(obj);
				break;
			    }
			}
		    }
		}
		err = nkn_cse_form_post_message(obj);
		while (err == -1 && obj->payload_file_type != NONE) {
		    nkn_cse_set_next_payload_fd(obj);
		    err = nkn_cse_form_post_message(obj);
		}
		if (err == -1) {
		    DBG_LOG(MSG, MOD_CSE, "[%s] All change lists submitted\n",
			    obj->cfg.name);
			nkn_cse_cleanup_func(obj);
		} else {
		    obj->conn_obj->status = POST_TASK;
		}
		break;

	    default:
		break;
        }
    }
}

int32_t
nkn_cse_handle_prev_task(crawler_context_t *obj)
{
    int err;
    pthread_mutex_lock(&obj->crawler_ctxt_mutex);
    if (obj->crawler_pid)
	nkn_cse_stop_prev_crawl_instance(obj);
    if (obj->conn_obj) {
        err = nkn_ccp_client_check_if_task_completed(obj->conn_obj);
	pthread_mutex_unlock(&obj->crawler_ctxt_mutex);
        if (err == -1) {
	   return -1;
	} else {
	    nkn_cse_cleanup_func(obj);
	}
    } else {
	/*
	 * the wget_ststus has to be set within the lock to avoid race with
	 * job routine.
	 */
	 if(obj->crawling_activity_status == CRAWL_IN_PROGRESS ||
		 (obj->crawling_activity_status == CRAWL_CTXT_DELETED &&
		  obj->inprogress_before_delete)){
	     AO_fetch_and_sub1(&glob_crawler_in_progress_cnt);
	     DBG_CRAWL(" Crawler \"%s\" Completed", obj->cfg.name);
	     obj->crawl_end_time = get_cur_time();
	     obj->inprogress_before_delete = 0;
	 }

	if (obj->crawling_activity_status != CRAWL_CTXT_DELETED &&
		obj->crawling_activity_status != CRAWL_CTXT_STOPPED) {
	    obj->crawling_activity_status = NOT_SCHEDULED;
	    obj->wget_status = CSE_WGET_TERMINATED;
	}
	pthread_mutex_unlock(&obj->crawler_ctxt_mutex);
    }
    return 0;
}

void
nkn_cse_generate_script_action_arg(char *buf, crawler_context_t *obj)
{
    int i;
    char temp[1024];
    for (i = 0; i< obj->num_extn_actions; i++) {
	switch (obj->cfg.extn_action_list[i].action_type) {
	    case ACTION_AUTO_GENERATE:
		sprintf(temp, " \"action: auto_generate, src: %s, dest: %s\" ",
			obj->cfg.extn_action_list[i].action.auto_gen.source_type,
			obj->cfg.extn_action_list[i].action.auto_gen.dest_type);
		strcat(buf,temp);
		break;
	    default:
		break;
	}
    }
}

void
nkn_cse_get_uriwise_stats(crawler_context_t *obj)
{
    url_t *ptr;
    ptr = obj->conn_obj->urlnode;
    while (ptr) {
       if (ptr->action_type == CCP_ACTION_ADD) {
           if (ptr->status == -1) {
	       if (ptr->errcode != NKN_MM_CUM_COD_ALREADY_PRESENT)
		   obj->stats.num_add_fails++;
           } else {
	       obj->stats.num_adds++;
	   }
       } else if (ptr->action_type == CCP_ACTION_DELETE) {
           if (ptr->status == -1) {
               obj->stats.num_delete_fails++;
           } else {
	       obj->stats.num_deletes++;
	   }
       }
       ptr = ptr->next;
    }
}

void
nkn_cse_reset_counters(crawler_context_t *obj)
{
    DBG_LOG(MSG, MOD_CSE, "[%s] counters:\nnum_adds: %ld\n"
           "num_deletes: %ld\nnum_add_fails: %ld\nnum_delete_fails: %ld\n",
           obj->cfg.name, obj->stats.num_adds, obj->stats.num_deletes,
           obj->stats.num_add_fails, obj->stats.num_delete_fails);

    obj->stats.num_adds                = 0;
    obj->stats.num_deletes      = 0;
    obj->stats.num_add_fails    = 0;
    obj->stats.num_delete_fails = 0;
}

time_t
nkn_cse_get_expiry(const char *source_type, const char *dest_type,
		    crawler_context_t *obj)
{
    int i;
    for (i = 0; i< obj->num_extn_actions; i++) {
	if (strcasestr(
	    obj->cfg.extn_action_list[i].action.auto_gen.source_type, source_type) &&
	    strcasestr(
	    obj->cfg.extn_action_list[i].action.auto_gen.dest_type, dest_type)) {
		return (obj->cfg.extn_action_list[i].action.auto_gen.expiry +
			    obj->crawl_start_time);
	}
    }
    return -1;
}

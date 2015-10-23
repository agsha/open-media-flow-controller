
#include <pthread.h>
#include <unistd.h>
#include <sys/queue.h>
#include <sys/prctl.h>
#include <libgen.h>
#include "common.h"
#include "random.h"

#include "lgd_worker.h"
#include "md_client.h"
#include "mdc_wrapper.h"
#include "file_utils.h"
#include "proc_utils.h"
#include "nknlog_pub.h"

char hostname[1024];
int hostname_len;

extern gcl_session *sess;
extern pthread_mutex_t upload_mgmt_log_lock;
static const char * logexportpath = LOG_EXPORT_HOME;
static const char * logpath = "/log/varlog/nkn";
/*---------------------------------------------------------------------------*/
TAILQ_HEAD(tqhead, lgd_work_elem) lgd_worker_head;
struct tqhead *lgd_worker_headp;
pthread_t   lgd_worker_task;

//pthread_mutex_t	    lgd_work_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t	    lgd_wq_ready = PTHREAD_COND_INITIALIZER;
pthread_mutex_t	    lgd_wq_lock_ready = PTHREAD_MUTEX_INITIALIZER;

/*---------------------------------------------------------------------------*/
#define	LOCK_WORKQ()				\
    do {					\
	pthread_mutex_lock(&lgd_wq_lock_ready);	\
    } while(0)

#define UNLOCK_WORKQ()				\
    do {					\
	pthread_mutex_unlock(&lgd_wq_lock_ready);	\
    } while(0)

/*---------------------------------------------------------------------------*/
static void *lgd_worker(void *arg);
static int lgd_worker_queue_purge(void);

int lgd_worker_deinit(void);
int lgd_worker_init(void);
/*---------------------------------------------------------------------------*/
int lgd_worker_init(void)
{
    int err = 0;

    /* Create a queue */
    TAILQ_INIT(&lgd_worker_head);

    /* Create the worker task and wait on the queue */
    err = pthread_create(&lgd_worker_task, NULL, lgd_worker,
	    (void *) "logd-worker-1");
    if (err) {
	lc_log_debug(LOG_NOTICE, "faile to create task");
	bail_error(err);
    }

bail:
    return err;
}

/*---------------------------------------------------------------------------*/
int lgd_worker_deinit(void)
{
    int err = 0;

    if (pthread_mutex_trylock(&lgd_wq_lock_ready) < 0) {
	if (errno == EBUSY) {
	    pthread_cond_destroy(&lgd_wq_ready);
	}
    }

bail:
    return err;
}

/*---------------------------------------------------------------------------*/
static void *lgd_worker(void *arg)
{
    int err = 0;
    struct lgd_work_elem *elem = NULL;

    err = prctl(PR_SET_NAME, (char *)(arg));
    if (err) {
	lc_log_debug(LOG_NOTICE, "failed to set task name");
	bail_error(err);
    }

    gethostname(hostname, sizeof(hostname)-1);
    hostname_len = strlen(hostname);

    lc_log_basic(LOG_INFO, "worker task starting..");

    while (1) {
	/* Check if we need to exit */
	if (lgd_exiting) {
	    break;
	}
	LOCK_WORKQ();
	elem = TAILQ_FIRST(&lgd_worker_head);
	if (elem) {
	    /* Wait for a work queue element */
	    lc_log_basic(LOG_INFO, "Waking lgd_worker()");

	    /* Remove the head */
	    TAILQ_REMOVE(&lgd_worker_head, elem, lwe_entries);
	    UNLOCK_WORKQ();


	    lc_log_debug(LOG_DEBUG, "working on element id: %10u",
		    elem->lwe_id);
	    if (elem->lwe_wrk != NULL) {
		err = elem->lwe_wrk(elem->lwe_wrk_arg);
	    }else {
		/* work elem not defined a function.. what do we do??
		 */
		lc_log_debug(LOG_NOTICE,
			"Duh.. work element with no work function..");
	    }

	    /* Free the element */
	    free(elem);
	    continue;
	}
	pthread_cond_wait(&lgd_wq_ready, &lgd_wq_lock_ready);
	UNLOCK_WORKQ();
    } /* while (1) */

    /* Clean the work queue */
    err = lgd_worker_queue_purge();
    bail_error(err);

bail:
    complain_error(err);
    lc_log_basic(LOG_NOTICE, "worker task exiting..");
    return NULL;
}


/* Create a new work element */
int lgd_lw_work_new(struct lgd_work_elem **ret_elem)
{
    int err = 0;

    bail_null(ret_elem);

    *ret_elem = calloc(sizeof(struct lgd_work_elem), 1);
    bail_null(*ret_elem);

    (*ret_elem)->lwe_id = lc_random_get_uint32();

bail:
    return err;
}


/* Add a work element to the queue */
int lgd_lw_work_add(struct lgd_work_elem *elem)
{
    int err = 0;

    bail_null(elem);

    lc_log_debug(LOG_DEBUG, "Queueing work element : %10u", elem->lwe_id);

    LOCK_WORKQ();
    TAILQ_INSERT_TAIL(&(lgd_worker_head), elem, lwe_entries);
    UNLOCK_WORKQ();
    pthread_cond_signal(&lgd_wq_ready);

bail:
    return err;
}


static int lgd_worker_queue_purge(void)
{
    struct lgd_work_elem *elem = NULL;


    LOCK_WORKQ();

    TAILQ_FOREACH(elem, &lgd_worker_head, lwe_entries) {
	TAILQ_REMOVE(&lgd_worker_head, elem, lwe_entries);

	free(elem);
    }

    UNLOCK_WORKQ();

    return 0;
}
// msg to gzip the files
int post_msg_to_gzip(struct uploadInfo *p_uploadInfo)
{
    int err = 0;
    struct lgd_work_elem *ret_elem = NULL;

    err = lgd_lw_work_new(&ret_elem);
    bail_null(ret_elem);
    ret_elem->lwe_wrk = (void *)lgd_file_deflate;
    ret_elem->lwe_wrk_arg = (void *)p_uploadInfo;
    err = lgd_lw_work_add(ret_elem);
    bail_error(err);

bail:
    if(err){
	safe_free(ret_elem);
    }
    return err;
}
// msg to upload the files to the specified url
int post_msg_to_upload(struct uploadInfo *p_uploadInfo)
{
    int err = 0;
    struct lgd_work_elem *ret_elem = NULL;

    err = lgd_lw_work_new(&ret_elem);
    bail_error(err);
    ret_elem->lwe_wrk = (void *)lgd_file_upload;
    ret_elem->lwe_wrk_arg = (void *)p_uploadInfo;
    err = lgd_lw_work_add(ret_elem);
    bail_error(err);

bail:
    if(err){
	safe_free(ret_elem);
    }
    return err;
}
// msg to move the file to logexport directory
int post_msg_to_mv_to_logexport( struct uploadInfo *p_uploadInfo)
{
    int err = 0;
    struct lgd_work_elem *ret_elem = NULL;
    if(p_uploadInfo == NULL)
    {
	return -1;
    }
    err = lgd_lw_work_new(&ret_elem);
    bail_error(err);
    ret_elem->lwe_wrk = (void *)mv_file_to_logexport;
    ret_elem->lwe_wrk_arg = (void *)p_uploadInfo;
    err = lgd_lw_work_add(ret_elem);
    bail_error(err);

bail:
    if(err){
	safe_free(ret_elem);
    }
    return err;
}
// CB - gzip
void lgd_file_deflate(struct uploadInfo *p_uploadInfo)
{
    char gzip_file[MAX_FILE_SZ] = { 0 };

    char *bname = NULL, *b = NULL;
    char *dname = NULL, *d = NULL;

    b = basename(bname = strdup(p_uploadInfo->fullfilename));
    d = dirname(dname = strdup(p_uploadInfo->fullfilename));

    if((p_uploadInfo->logtype == TYPE_CRAWLLOG) ||
	    (p_uploadInfo->logtype == TYPE_ACCESSLOG)){
	/* post mv msg before upload function as by the time we upload and
	   then move file to logexport the file is closed and new file is opened */

	snprintf(gzip_file, MAX_FILE_SZ, "/bin/gzip %s/.%s", d, b);
    }
    else{
	snprintf(gzip_file, MAX_FILE_SZ, "/bin/gzip %s/%s", d, b);
    }
    system(gzip_file);
    post_msg_to_mv_to_logexport(p_uploadInfo);
    safe_free(bname);
    safe_free(dname);
}
// CB - upload
int lgd_file_upload( uploadInfo *p_uploadInfo)
{
    int err = 0;
    bn_request *action_req = NULL;
    int msg_id = 0;
    char rurl[MAX_FILE_SZ] = {0};
    char l_file_path[MAX_FILE_SZ] = {0};
    int ret = 0;
    char that_file[MAX_FILE_SZ];
    char *actual_file = NULL;

    char *bname = NULL, *b = NULL;

    b = basename(bname = strdup(p_uploadInfo->fullfilename));
    if(p_uploadInfo->remote_url != NULL){

	pthread_mutex_lock(&upload_mgmt_log_lock);
	if(p_uploadInfo->logtype == TYPE_ACCESSLOG){
	    snprintf(that_file, MAX_FILE_SZ, "%s_%s_%s.gz", hostname,
		    p_uploadInfo->profile, p_uploadInfo->filename);

	    snprintf(l_file_path, MAX_FILE_SZ, "%s/%s_%s_%s.gz",
		    logexportpath,
		    hostname, p_uploadInfo->profile, b);
	}

	else if(p_uploadInfo->logtype == TYPE_CRAWLLOG){
	    snprintf(that_file, MAX_FILE_SZ, "%s_%s.gz", hostname,
		    p_uploadInfo->filename);

	    snprintf(l_file_path, MAX_FILE_SZ, "%s/%s_%s.gz",
		    logexportpath,
		    hostname, b);
	}
	else{
	    snprintf(that_file, MAX_FILE_SZ, "%s_%s.gz", hostname,
		    p_uploadInfo->filename);

	    snprintf(l_file_path, MAX_FILE_SZ, "%s/%s.gz",
		    logpath, b);
	}
	snprintf(rurl, MAX_FILE_SZ, "%s/%s",  p_uploadInfo->remote_url, that_file);

	err = bn_action_request_msg_create(&action_req, "/file/transfer/upload");
	bail_error_null(err, action_req);

	err = bn_action_request_msg_append_new_binding(action_req, 0, "local_path",
		bt_string, l_file_path, NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding(
		action_req,
		0,
		"remote_url", bt_string,  rurl, NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding(
		action_req,
		0,
		"password", bt_string, p_uploadInfo->password?:"", NULL);
	bail_error(err);

	// Send the message out to mgmtd
	err = bn_request_msg_send(sess, action_req);
	bail_error(err);
	msg_id = bn_request_get_msg_id(action_req);
	lc_log_basic(LOG_INFO, "!!!!msg_id:%d", msg_id);
	p_uploadInfo->req_id = msg_id;
	cp_hashtable_put(ht_uploadInfo, &msg_id, p_uploadInfo);
	pthread_mutex_unlock(&upload_mgmt_log_lock);
    }
bail:

    if(err){
	pthread_mutex_unlock(&upload_mgmt_log_lock);
    }
    bn_request_msg_free(&action_req);
    safe_free(bname);
    safe_free(p_uploadInfo->profile);
    safe_free(p_uploadInfo->fullfilename);
    safe_free(p_uploadInfo->filename);
    safe_free(p_uploadInfo->remote_url);
    safe_free(p_uploadInfo->password);
    p_uploadInfo->req_id = 0;
    p_uploadInfo->logtype = 0;
    safe_free(p_uploadInfo);

    return err;
}
// CB - move file to logexport
int mv_file_to_logexport(struct uploadInfo *p_uploadInfo)
{
    char this_file[MAX_FILE_SZ];
    int ret = 0;
    int err = 0;
    char dest_name[MAX_FILE_SZ] = {0};
    tbool is_exist = false;
    char *bname = NULL, *b = NULL;
    char *dname = NULL, *d = NULL;

    if((p_uploadInfo->logtype == TYPE_CRAWLLOG) ||
	    (p_uploadInfo->logtype == TYPE_ACCESSLOG )){
	b = basename(bname = strdup(p_uploadInfo->fullfilename));
	d = dirname(dname = strdup(p_uploadInfo->fullfilename));

	snprintf(this_file, MAX_FILE_SZ, "%s/.%s.gz", d, b);

	err = lf_test_exists(this_file, &is_exist);
	bail_error(err);

	if(p_uploadInfo->logtype == TYPE_ACCESSLOG){
	    snprintf(dest_name, MAX_FILE_SZ, "%s/%s_%s_%s.gz",
		    logexportpath, hostname,p_uploadInfo->profile, b);
	}
	else{
	    snprintf(dest_name, MAX_FILE_SZ, "%s/%s_%s.gz",
		    logexportpath, hostname, b);
	}
	/*Support for Logexport feature,move the files to logexport directory */
	err = lf_move_file(this_file, dest_name);
	if(err){
	    lc_log_basic(LOG_NOTICE, "failed to move the file %s to %s (reason: %d)",
		    this_file, dest_name, err);
	    err = 0;
	    goto bail;
	}

	if(p_uploadInfo->logtype == TYPE_ACCESSLOG){
	    lc_log_basic(LOG_DEBUG,
		    "log-export.NOTICE:(%d) file %s_%s_%s.gz"
		    " moved to logexport directory\n",
		    CODE_MOVE_LOG,
		    hostname, p_uploadInfo->profile, b);
	}
	else{

	    lc_log_basic(LOG_DEBUG,
		    "log-export.NOTICE:(%d) file %s_%s.gz"
		    " moved to logexport directory\n",
		    CODE_MOVE_LOG,
		    hostname, b);
	}
    }
    else {
	goto bail;
    }

bail:

    post_msg_to_upload(p_uploadInfo);
    safe_free(dname);
    safe_free(bname);
    return err;
}

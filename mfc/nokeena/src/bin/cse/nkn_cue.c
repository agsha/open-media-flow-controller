/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains implementation of Cache Update Engine for media management
 *
 * Author: Jeya ganesh babu
 *
 */
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_mediamgr_api.h"
#include "nkn_am_api.h"
#include "nkn_cod.h"
#include "nkn_cue.h"
#include "nkn_ccp_server.h"
#include "http_header.h"
#include "nkn_iccp_cli.h"

static pthread_mutex_t cue_xfer_mutex;
static pthread_mutex_t cue_hash_table_mutex;
static pthread_cond_t cue_thread_cond;
static pthread_t cue_thread_t;
TAILQ_HEAD(nkn_cue_xfer_list_t, ccp_client_data) nkn_cue_xfer_list;
int cue_proc_interval = 1;
static GHashTable *nkn_cue_hash_table = NULL;
AO_t nkn_max_tasks[CCP_MAX_ACTION];
uint64_t glob_cue_max_parallel_tasks_reached;

/*
 * CUE callback - called for every post message from CSE
 */
void cue_callback(ccp_client_data_t *objp)
{
    if(objp->command == CCP_POST) {
	pthread_mutex_lock(&cue_xfer_mutex);;
	TAILQ_INSERT_TAIL(&nkn_cue_xfer_list, objp, entries);
	pthread_cond_signal(&cue_thread_cond);
	pthread_mutex_unlock(&cue_xfer_mutex);;
    }
}

/* 
 * Cleanup url entry when the task is complete or there
 * is and abort
 */
static void
cue_cleanup_url_entry(ccp_urlnode_t *url_entry, int errcode)
{

    if(errcode != NKN_MM_CIM_NVSD_CONN_RESET)
	g_hash_table_remove(nkn_cue_hash_table, url_entry->url);

    /* If buf version expired, then it means new object is present
	* in the origin. Need to fetch again. Set the status to 0,
	* so it will be automatically done by the CUE thread */
    if((errcode == NKN_BUF_VERSION_EXPIRED) ||
	    (errcode == NKN_MM_CIM_NVSD_CONN_RESET)) {
	url_entry->status = 0;
    } else {
	url_entry->errcode = errcode;
	if(errcode)
	    url_entry->status = CCP_NOT_OK;
	else
	    url_entry->status = CCP_OK;
    }
    AO_fetch_and_sub1(&nkn_max_tasks[url_entry->action]);
}

/* task complete - invoked from iccp when an object is ingested
 * or deleted
 */
void
cue_task_complete(void *ptr)
{
    iccp_info_t *iccp_info = (iccp_info_t *)ptr;
    ccp_urlnode_t *url_entry = NULL;

    pthread_mutex_lock(&cue_hash_table_mutex);
    url_entry = (ccp_urlnode_t *)g_hash_table_lookup(nkn_cue_hash_table,
							iccp_info->uri);
    if(url_entry) {
	pthread_mutex_lock(&url_entry->entry_mutex);
	cue_cleanup_url_entry(url_entry, iccp_info->errcode);
	pthread_mutex_unlock(&url_entry->entry_mutex);
    }

    pthread_mutex_unlock(&cue_hash_table_mutex);
    return;
}


/* 
 * CUE init - Currently integrated to CSE
 */
int
cue_init()
{
    int ret = 0;
    pthread_condattr_t a;

    /* This is for CUE <-> nvsd communication over ICCP
     */
    iccp_cli_init(cue_task_complete);

    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);

    pthread_cond_init(&cue_thread_cond, &a);

    TAILQ_INIT(&nkn_cue_xfer_list);
    ret = pthread_mutex_init(&cue_xfer_mutex, NULL);
    if(ret < 0) {
        DBG_LOG(SEVERE, MOD_CUE, "CUE xfer mutex not created. ");
        return ret;
    }

    ret = pthread_mutex_init(&cue_hash_table_mutex, NULL);
    if(ret < 0) {
        DBG_LOG(SEVERE, MOD_CUE, "CUE hash table mutex not created. ");
        return ret;
    }

    nkn_cue_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
    if(!nkn_cue_hash_table)
	ret = -1;
    if(ret < 0) {
        DBG_LOG(SEVERE, MOD_CUE, "CUE hash table not created. ");
        return ret;
    }

    /* This is for the CSE <-> CUE communication
     */
    nkn_ccp_server_init(cue_callback, (char *)"cue", (char *)"cse");
    if((ret = pthread_create(&cue_thread_t, NULL,
			    cue_thread, NULL))) {
        DBG_LOG(SEVERE, MOD_CUE,"CUE thread not created");
        return ret;
    }

    return ret;
}

static gboolean
cue_reset_callback(gpointer key __attribute((unused)),
		   gpointer value,
		   gpointer user_data __attribute((unused)))
{
    if(value) {
	cue_cleanup_url_entry((ccp_urlnode_t *)value,
				NKN_MM_CIM_NVSD_CONN_RESET);
    }
    return 1;

}


static void
cue_reset(void)
{
    pthread_mutex_lock(&cue_hash_table_mutex);
    g_hash_table_foreach_remove(nkn_cue_hash_table,
				cue_reset_callback,
				NULL);
    pthread_mutex_unlock(&cue_hash_table_mutex);
}

/* 
 * Whole task cleanup - Called during abort from CSE
 */
static void
cue_task_cleanup(ccp_client_data_t *objp)
{
    ccp_urlnode_t *url_entry = NULL;
    pthread_mutex_lock(&cue_hash_table_mutex);
    url_entry = objp->url_entry;
    while(url_entry) {
	pthread_mutex_lock(&url_entry->entry_mutex);

	if(g_hash_table_lookup(nkn_cue_hash_table, url_entry->url)) {
	    cue_cleanup_url_entry(url_entry, NKN_MM_CIM_ABORT);
	}
	pthread_mutex_unlock(&url_entry->entry_mutex);
	url_entry = url_entry->next;
    }
    pthread_mutex_unlock(&cue_hash_table_mutex);
}

/* 
 * Main CUE thread 
 */
void *
cue_thread(void *arg __attribute((unused)))
{
    struct timespec   abstime;
    ccp_client_data_t *objp, *tmp_objp;
    ccp_urlnode_t *url_entry = NULL;
    int ret = 0;
    int cleanup = 0;
    int task_fail = 0;
    iccp_info_t iccp_info;
    ccp_urlnode_t *eurl_entry;
    int timeout = 1;
    int reset_status = 0;

    while(1) {
	timeout = 1;
	reset_status = iccp_return_and_reset_cim_status();
	if(reset_status) {
	    cue_reset();
	}
	pthread_mutex_lock(&cue_xfer_mutex);
	objp = TAILQ_FIRST(&nkn_cue_xfer_list);
	pthread_mutex_unlock(&cue_xfer_mutex);
proc_next_entry:;
	cleanup = 0;
	url_entry = NULL;
	if(objp) {
	    switch(objp->command) {
		case CCP_POST:
		case CCP_QUERY:
		    if(objp->command == CCP_QUERY &&
			    !(objp->command_ack)) {
			task_fail = 0;
			/* Check for all the URI entries
			 * If all the entries have status as OK/NOT_OK
			 * the objp can be cleaned up and
			 * final status can be returned back to CSE
			 */
			if(objp->query_status == CCP_OK ||
				objp->query_status == CCP_NOT_OK) {
			} else {
			    url_entry = objp->url_entry;
			    while(url_entry) {
				if((!url_entry->status) ||
					url_entry->status == CCP_NOT_STARTED ||
					url_entry->status == CCP_IN_PROGRESS)
				    break;
				if(url_entry->status == CCP_NOT_OK)
				    task_fail = 1;
				url_entry = url_entry->next;
			    }
			}
			tmp_objp = NULL;
			if(!url_entry) {
			    if(task_fail)
				objp->query_status = CCP_NOT_OK;
			    else
				objp->query_status = CCP_OK;
			    /* Remove from list, task is complete */
			    pthread_mutex_lock(&cue_xfer_mutex);
			    tmp_objp = TAILQ_NEXT(objp, entries);
			    TAILQ_REMOVE(&nkn_cue_xfer_list, objp, entries);
			    pthread_mutex_unlock(&cue_xfer_mutex);
			    objp->command_ack = 1;
			    nkn_ccp_server_handle_response(objp);
			    objp = tmp_objp;
			    goto proc_next_entry;
			}
			objp->command_ack = 1;
			nkn_ccp_server_handle_response(objp);
		    } else /* POST REQUST */ {
			if(!objp->query_status) {
			    objp->command_ack = 1;
			    objp->wait_time = 1 * objp->count;
			    objp->query_status = CCP_NOT_STARTED;
			    nkn_ccp_server_handle_response(objp);
			}
		    }
		    url_entry = objp->url_entry;
		    /* For each entry check the status and trigger a task
		     * to nvsd
		     */
		    while(url_entry) {
			ret = 0;
			if(!url_entry->status) {
			    url_entry->status = CCP_NOT_STARTED;
			}
			if(objp->action == CCP_ACTION_ADD ||
			    objp->action == CCP_ACTION_DELETE) {
			    if(url_entry->status == CCP_NOT_STARTED) {

				url_entry->action = objp->action;
				/* Send only a max of 100 tasks to NVSD based on
				 * the action. Do not set the timeout, so we can
				 * wait for 1sec before retrying again
				 */
				if(AO_load(&nkn_max_tasks[url_entry->action]) >
					    MAX_ICCP_PARALLEL_TASKS) {
				    glob_cue_max_parallel_tasks_reached++;
				    break;
				}
				pthread_mutex_init(&url_entry->entry_mutex,
							NULL);
				pthread_mutex_lock(&cue_hash_table_mutex);
				pthread_mutex_lock(&url_entry->entry_mutex);

				iccp_info.command	= url_entry->action;
				iccp_info.uri		= url_entry->url;
				iccp_info.expiry	= url_entry->expiry;
				iccp_info.data		= url_entry->data;
				iccp_info.data_len	= url_entry->data_len;
				iccp_info.etag		= url_entry->etag;
				iccp_info.content_len   = url_entry->size;
				iccp_info.cache_pin	= url_entry->cache_pin;
				iccp_info.client_domain = url_entry->domain;
				iccp_info.crawl_name    = objp->crawl_name;
				ret = iccp_send_task(&iccp_info);
				if(!ret) {
				    objp->query_status = CCP_IN_PROGRESS;
				    url_entry->status  = CCP_IN_PROGRESS;
				    eurl_entry = g_hash_table_lookup(nkn_cue_hash_table,
							url_entry->url);
				    if(eurl_entry) {
					cue_cleanup_url_entry(eurl_entry,
						NKN_MM_CUM_COD_ALREADY_PRESENT);
				    }
				    g_hash_table_insert(nkn_cue_hash_table,
							url_entry->url,
							url_entry);
				    AO_fetch_and_add1(&nkn_max_tasks[url_entry->action]);
				}

				pthread_mutex_unlock(&url_entry->entry_mutex);
				pthread_mutex_unlock(&cue_hash_table_mutex);
				timeout = 0;
				/* Break after a single success full entry 
				 * to try out other tasks, probably other crawls
				 * Setting the timeout to 0, so we wont wait for
				 * 1sec. If there are no other task, the current
				 * one will be tried for the next url_entry
				 */
				break;
			    }
			} else {
			    cleanup = 1;
			    break;
			}
			if(!ret) {
			    url_entry = url_entry->next;
			}
		    }
		    break;
		case CCP_ABRT:
		default:
		    cleanup = 1;
		    break;
	    }
	    if(cleanup) {
		cue_task_cleanup(objp);
		pthread_mutex_lock(&cue_xfer_mutex);
		tmp_objp = TAILQ_NEXT(objp, entries);
		TAILQ_REMOVE(&nkn_cue_xfer_list, objp, entries);
		pthread_mutex_unlock(&cue_xfer_mutex);
		nkn_ccp_server_handle_response(objp);
		objp = tmp_objp;
	    } else {
		pthread_mutex_lock(&cue_xfer_mutex);
		objp = TAILQ_NEXT(objp, entries);
		pthread_mutex_unlock(&cue_xfer_mutex);
	    }
	    goto proc_next_entry;
	}
	if(!timeout)
	    continue;
        clock_gettime(CLOCK_MONOTONIC, &abstime);
        abstime.tv_sec += cue_proc_interval;
        abstime.tv_nsec = 0;
	pthread_mutex_lock(&cue_xfer_mutex);
	pthread_cond_timedwait(&cue_thread_cond, &cue_xfer_mutex, &abstime);
	pthread_mutex_unlock(&cue_xfer_mutex);
    }
}


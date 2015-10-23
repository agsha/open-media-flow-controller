#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <glib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <ares.h>
#include <sys/epoll.h>

#include "nvsd_mgmt.h"
#include "nkn_defs.h"
#include "nkn_sched_api.h"
#include "nkn_debug.h"
#include "server.h"
#include "network.h"
#include "nkn_mgmt_agent.h"
#include "nkn_cfg_params.h"
#include "nkn_log_strings.h"
#include "nkn_trace.h"
#include "nkn_am_api.h"
#include "ssp.h"
#include "nkn_namespace.h"
#include "nkn_mediamgr_api.h"
#include "nkn_cl.h"
#include "nkn_mediamgr_api.h"
#include "nkn_rtsched_api.h"
#include "network_hres_timer.h"
#include "nkn_compressmgr.h"
#undef __FD_SETSIZE
#define __FD_SETSIZE MAX_GNM 

NKNCNT_DEF(compress_task_called, AO_t, "", "total compress task")
NKNCNT_DEF(compress_task_completed, AO_t, "", "total compress task")
NKNCNT_DEF(compress_task_failed, AO_t, "", "total compress tasks failed")
NKNCNT_DEF(compress_servfail, AO_t, "", "total compress serv failures")
NKNCNT_DEF(compress_channel_reinit, AO_t, "", "total times compress challe re-inited")

nkn_mutex_t compressreq_mutex; 			//Multiple threads to handle compress tasks
nkn_mutex_t comressreq_mutex;			//Single thread to handle incoming DNS responses
pthread_t compressthr[MAX_CM_THREADS];		
pthread_t comressthr;
pthread_cond_t compressreq_cond = PTHREAD_COND_INITIALIZER;
nkn_task_id_t compress_task_arr[MAX_CM_TASK_ARR];	//Task queue in which the scheduler puts compress tasks
volatile int compress_task_arr_tail;
volatile int compress_task_arr_head;

int http_cfg_fo_use_compress;

static int isam_task_arrFull( void )
{
    	if ((compress_task_arr_tail < compress_task_arr_head)
        	&& (compress_task_arr_head - compress_task_arr_tail) == 1)
	{
        	return 1;
	}
    	if ((compress_task_arr_tail == (MAX_CM_TASK_ARR - 1))
        	&& (compress_task_arr_head == 0))
	{
        	return 1;
	}
    	return 0;
}

static int isam_task_arrCnt(void )
{
    	if (compress_task_arr_head < compress_task_arr_tail)
	{
        	return compress_task_arr_tail - compress_task_arr_head;
	}
    	else
	{
        	if (compress_task_arr_head == compress_task_arr_tail)
		{
            		return 0;
		}
        	else
		{
            		return (compress_task_arr_tail + MAX_CM_TASK_ARR -
                    		compress_task_arr_head);
		}
    	}
}

/*Function to lookup compress data locally and call client, if not found*/
static void compress_mgr_fetch_compress(compress_msg_t *data)
{
	UNUSED_ARGUMENT(data);
	int copylen = 0;
	int cplen = 0;
	compress_msg_t *cp = (compress_msg_t *)data;
	char * outp;
	int ret = 0;
	int out_len =0;
	int i  =0;
	uint32_t outcopylen;
	uint32_t outcplen;
	uint32_t offset;
#if 1
	copylen = cp->comp_in_len;
	outp = (char *)nkn_malloc_type( 2 * CM_MEM_PAGE_SIZE * cp->in_iovec_cnt, mod_compress_msg_t);
	char * p = outp;
	out_len = CM_MEM_PAGE_SIZE * 2 * cp->in_iovec_cnt;
	if(!cp->comp_init) {
		//ocon->comp_end = 0;
		cp->strm->zalloc = Z_NULL;
                cp->strm->zfree = Z_NULL;
                cp->strm->opaque = Z_NULL;
		if(cp->comp_type == HRF_ENCODING_GZIP) {
		    ret = deflateInit2(cp->strm,
                        Z_DEFAULT_COMPRESSION,
                        Z_DEFLATED,
                        MAX_WBITS +16,  /* supress zlib-header */
                        8,
                        Z_DEFAULT_STRATEGY);
		} else {	
		    ret = deflateInit2(cp->strm,
                        Z_DEFAULT_COMPRESSION,
                        Z_DEFLATED,
                        MAX_WBITS ,  /* supress zlib-header */
                        8,
                        Z_DEFAULT_STRATEGY);
	    }

                if (ret != Z_OK) {
		       cp->comp_err = -1;
			glob_compress_task_failed++;
			free(outp);
                       return ;
		}

                cp->comp_init = 1;
          }

	  for (i = 0 ; i < (int)cp->in_iovec_cnt; i++) {

	      cplen = 0;
	      if(cp->strm->avail_in > 0) {
		cplen = cp->strm->avail_in;
	      } else {
		cplen =  copylen > cp->in_content[i].iov_len ?  cp->in_content[i].iov_len : copylen;
	      } 
	      
	      cp->strm->next_in = (Bytef *)cp->in_content[i].iov_base + cp->strm->avail_in;
              cp->strm->avail_in = cplen;
	      cp->strm->avail_out = out_len;
              cp->strm->next_out = (Bytef *)p;
	      ret = deflate(cp->strm, Z_SYNC_FLUSH);
	      if(ret < 0 ) {

		cp->comp_err = ret;
		glob_compress_task_failed++;

		free(outp);
        	return;
	      }

	      if(cp->strm->avail_in != 0) {
		i--;
		copylen-=(cplen - cp->strm->avail_in);
		continue;
	      }
	
	      copylen-=cplen;	
	      p += (out_len - cp->strm->avail_out);
	      out_len = cp->strm->avail_out;

	    if(copylen == 0)
		break;
	 }

	if(copylen != 0) {
	    cp->comp_err = -1;
	    glob_compress_task_failed++;
	    free(outp);
	    return;

	}

 	if((unsigned )cp->strm->total_in == (unsigned)cp->tot_expected_in) {
	    cplen = cp->strm->avail_out;
	    cp->strm->avail_out = out_len;
            cp->strm->next_out = (Bytef *)p;
	    ret = deflate(cp->strm, Z_FINISH);
	    if(ret < 0 ) {
		    cp->comp_err = ret;
		    glob_compress_task_failed++;
		    free(outp);
		    return;
	    }

            (void)deflateEnd(cp->strm);
	    glob_compress_task_completed++;
		
	    cp->comp_end = 1;
	}

	outcopylen = 2 * cp->in_iovec_cnt * CM_MEM_PAGE_SIZE - cp->strm->avail_out + cp->comp_offset;
	p = outp;
	while(outcopylen > 0) {
	    offset = cp->comp_out % CM_MEM_PAGE_SIZE;
	    outcplen = outcopylen > CM_MEM_PAGE_SIZE-offset ? CM_MEM_PAGE_SIZE-offset : outcopylen;
	    int a_index = cp->out_iovec_cnt;
	    if(offset == 0 && outcplen >= CM_MEM_PAGE_SIZE) {
		cp->out_bufs[a_index] = cp->alloc_buf(&cp->out_content[a_index].iov_len);
		cp->out_content[a_index].iov_base = cp->buf2data(cp->out_bufs[a_index]);
		cp->out_iovec_cnt++;
  		if(cp->comp_offset) {
		    memcpy(cp->out_content[a_index].iov_base, cp->comp_buf, cp->comp_offset);
		    offset = cp->comp_offset;
		    cp->comp_offset = 0;
		}

		memcpy(cp->out_content[a_index].iov_base + offset, p, outcplen-offset);
		p+=(outcplen - offset);
	    } else if (cp->out_iovec_cnt > 0 && offset > 0) { 
		memcpy(cp->out_content[a_index].iov_base+offset, p, outcplen);
		p+=(outcplen);
	    } else if (cp->comp_end == 0){
		memcpy(cp->comp_buf + cp->comp_offset, p, outcplen-cp->comp_offset);
		cp->comp_offset = outcplen;
		p+=(outcplen - cp->comp_offset);

	    } else  {
		cp->out_bufs[a_index] = cp->alloc_buf(&cp->out_content[a_index].iov_len);
		cp->out_content[a_index].iov_base = cp->buf2data(cp->out_bufs[a_index]);
		cp->out_iovec_cnt++;
    
		if(cp->comp_offset) {
		    memcpy(cp->out_content[a_index].iov_base, cp->comp_buf, cp->comp_offset);
		    offset = cp->comp_offset;
		    cp->comp_offset = 0;
	   
		}

		memcpy(cp->out_content[a_index].iov_base + offset, p, outcplen-offset);
		p+=(outcplen - offset);
	    }
		
	    outcopylen-=outcplen;
	    cp->comp_out += outcplen;
	}


	cp->comp_in_processed += cp->comp_in_len;
	//cp->comp_out = cp->comp_max_out_len - cp->strm->avail_out; 
#endif
	free(outp);
        return ;
}

/*Auth worker threads call this to process compress requests*/
static void compress_mgr_thread_input(nkn_task_id_t id)
{
        struct nkn_task     *ntask = nkn_task_get_task(id);
        assert(ntask);
       compress_msg_t *data = (compress_msg_t*)nkn_task_get_data(id);
	//omcon_t *ocon = nkn_task_get_private(TASK_TYPE_ORIGIN_MANAGER, id);
        compress_mgr_fetch_compress(data);
        nkn_task_set_private(TASK_TYPE_COMPRESS_MANAGER, id, (void *)data);
	nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
}


/*Scheduler input function*/
void compress_mgr_input(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	isam_task_arrFull();
#if 1
        nkn_task_set_state(id, TASK_STATE_EVENT_WAIT);
        struct nkn_task     *ntask = nkn_task_get_task(id);
        assert(ntask);
	//compress_msg_t *data = (compress_msg_t*)nkn_task_get_data(id);
	{
		NKN_MUTEX_LOCK(&compressreq_mutex);
		if (isam_task_arrFull()) {
			nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT,
						TASK_STATE_RUNNABLE);
			NKN_MUTEX_UNLOCK(&compressreq_mutex);
			return;
		}
		compress_task_arr[compress_task_arr_tail] = id;
		if (compress_task_arr_tail == (MAX_CM_TASK_ARR - 1))
		{
			compress_task_arr_tail = 0;
		}
		else
		{
			compress_task_arr_tail++;
		}
		pthread_cond_signal(&compressreq_cond);
		NKN_MUTEX_UNLOCK(&compressreq_mutex);
	}
#endif
	return;
}

/*Dummy output function for scheduler*/
void compress_mgr_output(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return;
}

/*Dummy output function for scheduler*/
void compress_mgr_cleanup(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return;
}

/*This is the function for worker compress threads, which pick task from compress q*/
void *compress_mgr_input_handler_thread(void *arg)
{
	UNUSED_ARGUMENT(arg);
     	nkn_task_id_t taskid;
     	prctl(PR_SET_NAME, "nvsd-compress-input-work", 0, 0, 0);
     	while (1)
     	{
                NKN_MUTEX_LOCK(&compressreq_mutex);
                if (isam_task_arrCnt() < 1) 
		{
                        pthread_cond_wait(&compressreq_cond, &compressreq_mutex.lock);
                        NKN_MUTEX_UNLOCK(&compressreq_mutex);
                }
 		else
		{
                        taskid = compress_task_arr[compress_task_arr_head];
                        if ( compress_task_arr_head == (MAX_CM_TASK_ARR - 1))
                                compress_task_arr_head = 0;
                        else
                                compress_task_arr_head++ ;
                        NKN_MUTEX_UNLOCK(&compressreq_mutex);
                        compress_mgr_thread_input(taskid);
                }
     	}
}

/* Init function to start compression module threads. 
*/
int compress_mgr_init(uint32_t init_flags)
{
	NKN_MUTEX_INIT(&compressreq_mutex, NULL, "compressreq_mutex");
	UNUSED_ARGUMENT(init_flags);
	/***********************************************************
	 * We do not need this for now, but will eventually need
	 * to handle the compress taks, key management etcc..
	 ***********************************************************/
	if (1) {
	    int32_t i;
	    for (i=0; i<MAX_CM_THREADS; i++) {
        	DBG_LOG(MSG, MOD_COMPRESS,
			"Thread %d created for Compress Manager",i);
                if(pthread_create(&compressthr[i], NULL,
				  compress_mgr_input_handler_thread, (void *)&i) != 0) {
		    DBG_LOG(SEVERE, MOD_COMPRESS,
			    "Failed to create compressmgr threads %d",i);
		    DBG_ERR(SEVERE, "Failed to create compressmgr threads %d",i);
		    return -1;
                }
	    }
	}

        return 0;

}



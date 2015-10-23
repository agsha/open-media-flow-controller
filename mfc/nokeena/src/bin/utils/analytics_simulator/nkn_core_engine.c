/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains code which implements the Core modeler
 *
 * Author: Hrushikesh Paralikar
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <sys/time.h>
#include <atomic_ops.h>
#include "log_parser.h"
#include "config_parser.h"
#include "nkn_core_engine.h"
#include "nkn_global_defs.h"
#include "nkn_bm_api.h"



#define MAX_DATA_RECEIVE 2097152

uint64_t nkn_cur_ts;
int nkn_assert_enable = 0;



FILE *log_fp = NULL;

TAILQ_HEAD(hotness_bucket_t, uri_entry_s)
    hotness_bucket[MAX_HOT_VAL];
uint64_t num_hot_objects[MAX_HOT_VAL];

BM_data bm_register_module;
AM_data am_register_module;
DM_data dm_register_module;

pthread_t schedule_get_thread;


//main intitalization function, intialises all the globala variables as well as
//the AM, BM , and DM modules. Also starts up the logger and the scheduler
//threads
static void CE_init()
{

	int rc = 0;
	g_thread_init(NULL);
	pthread_t signal_handler;

	sigemptyset (&signal_mask);
	sigaddset (&signal_mask, SIGSEGV);
	sigaddset (&signal_mask, SIGINT);
	rc = pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
	if(rc != 0) {

	    printf("Error:sigmask\n");
	    return;
	}

	rc = pthread_create (&signal_handler, NULL, signal_handler_fn, NULL);
	TAILQ_INIT(&sched_queue);
	scheduler_stop_processing = 0;

	AO_store(&queue_busy,0);
	AO_store(&nkn_cur_ts,0);

	pthread_mutex_init(&logger_mutex, NULL);
	pthread_cond_init(&logger_cond, NULL);

	put_in_sched_queue_ptr = put_in_sched_queue;
	schedule_get_ptr = schedule_get;

	pthread_mutex_init(&sched_queue_mutex, NULL);
	pthread_mutex_init(&queue_busy_mutex, NULL);
	pthread_mutex_init(&bufferpool_queue_mutex, NULL);
	pthread_mutex_init(&ingestion_protection_mutex, NULL);
	pthread_mutex_init(&synch_mutex, NULL);
	pthread_cond_init(&synch_cv, NULL);
	pthread_mutex_init(&ingest_synch_mutex, NULL);
	pthread_cond_init(&ingest_synch_cv, NULL);
	pthread_cond_init(&queue_busy_cond, NULL);
        BM_init(&bm_register_module);
	AM_init(&am_register_module);
	DM_init(&dm_register_module);
	if (schedule_get_ptr) {
		pthread_create(&schedule_get_thread, NULL,
			(void*)(schedule_get_ptr),NULL);

	}

	pthread_create(&logger, NULL, (void*)(logger_thread), NULL);
	log_memory = fopen("log_mem","w");

}


/* process_logs: reads off the access logs one entry at a time and passes on
 * that to the scheduler
 */
static void process_logs(char *log_file)
{
    uint64_t total_data = 0;
    uri_entry_t *obj=calloc(1,sizeof(uri_entry_t));
    memset(obj->uri_name,0,sizeof(obj->uri_name));
    int i,rc;
   // DM_put_data_t *put_data;
    struct timespec timetosleep;
    struct timeval cur_time;


    while(!get_next_entry(log_file,obj->uri_name,&obj->content_len,&obj->resp,
            &obj->start_offset, &obj->end_offset, &obj->curr_provider,
	    &obj->cacheable, &obj->last_access_time, &obj->partial)) {
	  /* Runtime Statistics */
        if (!(line_no % 1000000))
            fprintf(log_fp, "Cache_hit_ratio = %ld\n", cache_hit_ratio);
        if (!obj->uri_name[0])
            continue;
	total_num_requests++;

	/*gettimeofday(&cur_time,NULL);
	timetosleep.tv_sec = cur_time.tv_sec;
	timetosleep.tv_nsec = (cur_time.tv_usec/1000 + (1000));
    */
	if (total_num_requests == 1) {
	//   nkn_cur_ts = obj->last_access_time * DELAY_GRANULARITY;
	      AO_store(&nkn_cur_ts, (obj->last_access_time * DELAY_GRANULARITY));


	  }


	 if(obj->resp != 200 && obj->resp !=206) {

	    total_data += obj->content_len;
	    free(obj);
	    obj = calloc(1,sizeof(uri_entry_t));
	    memset(obj->uri_name,0,sizeof(obj->uri_name));
	    continue;

	}

	if (obj->cacheable == 0) {

	    //increment total_bytes_delivered.
	    free(obj);
	    obj = calloc(1,sizeof(uri_entry_t));
	    memset(obj->uri_name,0,sizeof(obj->uri_name));
	    continue;


	}

	total_num_cacheable_entries++;

	if (obj->content_len == 0) {

	    free(obj);
	    obj = calloc(1,sizeof(uri_entry_t));
	    memset(obj->uri_name,0,sizeof(obj->uri_name));
	    continue;
	}


	//if (total_num_requests == 1) {
	 //   nkn_cur_ts = obj->last_access_time * DELAY_GRANULARITY;
	//    AO_store(&nkn_cur_ts, obj->last_access_time * DELAY_GRANULARITY);
	//}

	obj->entry_num = total_num_requests;

	//Testing disk inggestion...
	//if(obj->entry_num <= 2)
	/*if(obj->entry_num < 2)
	{
	    DM_put_data_t *put_data;
	    put_data = (DM_put_data_t*)calloc(sizeof(DM_put_data_t), 1);
	    if(obj->entry_num == 1) { 
		put_data->request_offset = 0;
		put_data->length = 2097152;
	    }
	    else
	    {
		 put_data->request_offset = 2097152;
		 put_data->length = 2097152;
	    }
	     strncpy(put_data->uri_name, obj->uri_name, MAX_URI_LEN);
	     put_data->tier_type = 2; //SATA
	     put_data->decoded_hotness = rand() % 65536;
	     CE_DM_put(put_data);
	     ingest_count++;
	     free(obj);
	     obj=calloc(1,sizeof(uri_entry_t));
	     memset(obj->uri_name,0,sizeof(obj->uri_name));
	     continue;
	}*/
	/*
	else
	{
	    free(obj);

	    obj=calloc(1,sizeof(uri_entry_t));
	    memset(obj->uri_name,0,sizeof(obj->uri_name));
	    continue;

	}*/

	/*//Testing partial ingest
	if (obj->entry_num == 1)
	{
	    obj->offline_ingest = 1;

	}
	*/

	obj->end_offset = obj->start_offset + obj->content_len;

	if (obj->cacheable) {
	total_data += (obj->end_offset - obj->start_offset);
	//total_data += obj->content_len;
	}
	obj->request_offset = obj->start_offset;

	if (obj->content_len >= ssd_min_size) {

	    ingests_expected++;
	}

	//select tier: 
	select_tier(obj, size_based_selection);

	//send the entry to the scheduler, but only if its access time is <=
	//current time or the request queue is empty.
	while(1) {
	    //pthread_mutex_lock(&current_time_mutex);
	    pthread_mutex_lock(&queue_busy_mutex);
	    if (AO_load(&nkn_cur_ts) >= (obj->last_access_time * DELAY_GRANULARITY) ||
		    queue_busy == 0) {
		//unlock
		if( (obj->last_access_time * DELAY_GRANULARITY) > nkn_cur_ts) {
		 //   nkn_cur_ts = obj->last_access_time * DELAY_GRANULARITY;
		    AO_store(&nkn_cur_ts,
			    (obj->last_access_time * DELAY_GRANULARITY));
		}

		//queue_busy++;
		AO_fetch_and_add1(&queue_busy);
		pthread_mutex_unlock(&queue_busy_mutex);
		//pthread_mutex_unlock(&current_time_mutex);

		if(put_in_sched_queue_ptr) {

		    put_in_sched_queue_ptr(obj);
		}
		break; //to process next object.



	    }else {
		gettimeofday(&cur_time,NULL);
		timetosleep.tv_sec = cur_time.tv_sec;
		timetosleep.tv_nsec = (cur_time.tv_usec/1000 + (1000));

		pthread_mutex_unlock(&queue_busy_mutex);
		//pthread_mutex_unlock(&current_time_mutex);

		rc = pthread_cond_timedwait(&queue_busy_cond, &queue_busy_mutex,
			&timetosleep);
		//printf("RC: %d \n",rc);
		pthread_mutex_unlock(&queue_busy_mutex);


	    }
	}
    obj=calloc(1,sizeof(uri_entry_t));
    memset(obj->uri_name,0,sizeof(obj->uri_name));
    }
    free(obj);  //This free is for the last obj that was created to hold the 
    //last +1 th entry but exited the while loop as there is no last +1th entry.


    scheduler_stop_processing = 1;
    pthread_join(schedule_get_thread,0);



    while(1) {

	    if (bm_stop_processing == 1 && am_stop_processing == 1 &&
		    ingestion_thread_done == 1) {

		bm_register_module.BM_cleanup();
		am_register_module.AM_cleanup();
		break;
	    }
    }
    //printf("TOTAL DATA REQUESTED %ld\n",total_data);
    fprintf(log_fp, "\nTotal_bytes_delivered = %ld\n", total_bytes_delivered);
    fprintf(log_fp, "Total_bytes_in_ram = %ld\n", total_bytes_in_ram);
    fprintf(log_fp, "Total_bytes_in_am = %ld\n",total_bytes_in_am);
    fprintf(log_fp, "Total_num_requests = %ld\n", total_num_requests);
    fprintf(log_fp, "Total_num_dup_requests = %ld\n", total_dup_requests);
    fprintf(log_fp, "Total_num_cacheable_entries = %ld\n",
	    total_num_cacheable_entries);
    fprintf(log_fp, "Total_dup_bytes_delivered = %ld\n",
	    total_dup_bytes_delivered);
    fprintf(log_fp, "Bytes in disks -> \n");
    for(i=0;i<number_of_disks;i++) {

	fprintf(log_fp,"Total bytes in disk %d = %ld \n",
		i+1,total_bytes_in_disks[i]);
    }
    fprintf(log_fp, "Bytes from disks -> \n");
    for(i=0;i<number_of_disks;i++) {
	fprintf(log_fp,"Total bytes from disk %d = %ld \n",
		i+1,total_bytes_from_disks[i]);
    }
    fprintf(log_fp, "ingests_to_do %ld \n", ingests_to_do);
    fprintf(log_fp, "ingests_done %ld \n", ingests_done);
    fprintf(log_fp, "ingests_expected %ld \n", ingests_expected);
    fprintf(log_fp, "-----------------------------\n");
    fprintf(log_fp, "Cache_Hit_Ratio = %ld\n", cache_hit_ratio);
    fprintf(log_fp, "-----------------------------\n");

}


void put_in_sched_queue(uri_entry_t *object)
{
        pthread_mutex_lock(&sched_queue_mutex);
        TAILQ_INSERT_TAIL(&sched_queue, object, sched_queue_entries);
	pthread_mutex_unlock(&sched_queue_mutex);

}



/* the scheduler thread 
 */
void schedule_get(void *arg)
{

    printf(" BM THread spawned \n");
    uri_entry_t *requested_object = NULL;
    am_transfer_data_t *transfer_obj;
    DM_stat_resp_t *stat_resp = NULL;
    //DM_put_data_t *put_data = NULL;
    uint64_t length = 0,start = 0,end = 0, buffers_required = 0;
    DM_get_resp_t * get_resp = NULL;
    pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
    while(1) {

	//exit condition
        if (scheduler_stop_processing) {
	    pthread_mutex_lock(&sched_queue_mutex);
	    if (TAILQ_EMPTY(&sched_queue)) {
		pthread_mutex_unlock(&sched_queue_mutex);
		bm_stop_processing = 1;
		printf("scheduler exiting.. \n");
		//pthread_exit(0);
		return;
	    }
	    pthread_mutex_unlock(&sched_queue_mutex);
        }

	pthread_mutex_lock(&sched_queue_mutex);
        if (!TAILQ_EMPTY(&sched_queue)) {
            requested_object = NULL;
	    requested_object = TAILQ_FIRST(&sched_queue);
            TAILQ_REMOVE(&sched_queue, requested_object,
			sched_queue_entries);
            pthread_mutex_unlock(&sched_queue_mutex);

	    start = requested_object->start_offset;
	    end = requested_object->end_offset;
	    length = requested_object->content_len;
	    //set the length of data to be requested.
	    if ((end - requested_object->request_offset) > MAX_DATA_RECEIVE) {
	        requested_object->request_length = MAX_DATA_RECEIVE;

	    }
	    else {

		requested_object->request_length = end -
		    requested_object->request_offset;
	    }
/*
 //PULL INGEST CODE : DO NOT DELETE

	    if (AO_load(&requested_object->offline_ingest) == 1 &&
		    requested_object->partial == 1) {
		//partial and offline ingest are set
		//when this object is being ingested offline and
		//was a partial fetch so we dont know the length. Always try 
		//to get MAX_DATA_RECEIVE in that case

		requested_object->request_length = MAX_DATA_RECEIVE;

	    }
	    if (AO_load(&requested_object->offline_ingest) == 1) {

		//simulate an offline ingest.
		//do a bm_get, 
		//if 2MB present, do dmput,
		//else ingest fails.
		//once ingestion complete, reset offline_ingest and 
		//delete the uri object.
		bm_register_module.BM_get(requested_object);
		if (requested_object->response_length !=
			requested_object->request_length ) {

		    //ingest has failed!
		}
		else {

		    put_data = (DM_put_data_t*)calloc(sizeof(DM_put_data_t), 1);
		    put_data->tier_type = requested_object->tier_type;
		    put_data->request_offset = requested_object->request_offset;
		    put_data->length = requested_object->request_length;
		    strncpy(put_data->uri_name, requested_object->uri_name,
			    (MAX_URI_LEN-1));
		    put_data->hotness = requested_object->hotness;
		    put_data->decoded_hotness =
			am_decode_hotness(&requested_object->hotness);
		    CE_DM_put(put_data);
		    free(put_data);
		    requested_object->response_length = 0;
		    //deref the fetched buffers
		    bm_register_module.deref_fetched_buffers(requested_object);

		}
		    continue;
	    }
//PULL INGEST CODE ENDS
	    */


	    //call BM_get to get requested length of data from the buffer cache,
	    //if the data is not present in the cache, call dm_stat to lookup
	    //for the data in the disks and if present call DM_get to get the 
	    //data.
	    //Later, send the data acquired to the am module via am_inc_num_hits
	    //for hotness calculation as well as ingestion
	    bm_register_module.BM_get(requested_object);
	    if (requested_object->response_length !=0) {
		//attrbuff will be set only if get is successfull(resp_len !=0)
		//set the bytes fetched from cache;
		(requested_object->attrbuff)->bytes_from_cache +=
		    requested_object->response_length;
		//set the time of hit
		(requested_object->attrbuff)->object_ts = AO_load(&nkn_cur_ts);
		requested_object->bm_put = 0;
	    }
	    //bm_register_module.deref_fetched_buffers(requested_object);
	    if (!requested_object->duplicate_request &&
		    requested_object->response_length !=0 ) {
		requested_object->duplicate_request = 1;
		total_dup_requests++;
	    }
	    //nkn_cur_ts += bm_get_fetch_delay;
	    //AO_fetch_and_add(&nkn_cur_ts,bm_get_fetch_delay);
	    CE_inc_cur_time(bm_get_fetch_delay);
	    //pthread_mutex_unlock(&current_time_mutex);
	    requested_object->total_fetched += requested_object->response_length;

		//restore parameters

		//requested_object->start_offset = requested_object->end_offset;
		//requested_object->end_offset = end;
		//requested_object->content_len = length - MAX_DATA_RECEIVE;	


	    if (requested_object->bm_get_failure) {
		//reset bm_get_failure
		requested_object->bm_get_failure = 0;
		if(requested_object->response_length == 0) {
		    requested_object->request_length =
			requested_object->request_length -
			    requested_object->response_length;

		    //DM_stat plugin
		    //If DM_stat success, DM_get pulgin.
		    //else BM_put
		    stat_resp = (DM_stat_resp_t *)calloc(sizeof(DM_stat_resp_t),1);
		    copy_to_stat_resp(requested_object,stat_resp);
		    dm_register_module.DM_stat(stat_resp);
		    if (!(stat_resp->failure)) {

			//DM_get plugin...

			get_resp = (DM_get_resp_t *)calloc(sizeof(DM_get_resp_t),1);
			strncpy(get_resp->uri_name, stat_resp->uri_name,
				strlen(stat_resp->uri_name));

			get_resp->request_offset =
			    (requested_object->request_offset/DATA_BUFFER_SIZE)*
				    DATA_BUFFER_SIZE;

			get_resp->request_length = stat_resp->block_size;

			get_resp->physid = stat_resp->physid;
			buffers_required = get_resp->request_length/
			    DATA_BUFFER_SIZE;
			/*if (requested_object->request_length%DATA_BUFFER_SIZE>0) {

			    buffers_required += 1;
			}*/
			if (small_read == 1) {

			    //request only 256K (small_read_size).
			    //buffers required  = 8
			  //if (requested_object->request_length>=small_read_size) {
			    buffers_required = small_read_size/ DATA_BUFFER_SIZE;
			    get_resp->request_length = small_read_size;
			  //}


		       }

		       if ( (requested_object->request_length <
			       get_resp->request_length) &&
			       ((requested_object->request_offset+
				requested_object->request_length)>=
				requested_object->end_offset)) {
			    //in case of partial data present and this is
			    //the request for the last part of the object;

			    get_resp->request_length =
				requested_object->request_length;
			    buffers_required =
				get_resp->request_length/DATA_BUFFER_SIZE;
			    if(get_resp->request_length%DATA_BUFFER_SIZE != 0) {
				buffers_required +=1;
			    }
		       }

		       bm_register_module.get_buffers_from_BM(buffers_required,
			       requested_object, get_resp->buffers);
		       get_resp->attrbuff = requested_object->attrbuff;

		       dm_register_module.DM_get(get_resp);

		       if (!(get_resp->failure)) {
		       requested_object->request_offset =
			   requested_object->request_offset +
				requested_object->request_length;


			requested_object->response_length =
			    requested_object->request_length;
			(requested_object->attrbuff)->object_ts =
			    AO_load(&nkn_cur_ts);


			total_dup_bytes_delivered +=
			    requested_object->response_length;

			total_bytes_delivered +=
			    requested_object->response_length;
		       bm_register_module.return_buffers_to_client(buffers_required,
			requested_object, get_resp->buffers);
		       }
		       else {
			    nkn_buf_t *temp_attrbuf = NULL;
			    int i = 0;
			    temp_attrbuf = ((get_resp->buffers[0])->attrbuf);
			    for(i = 0; i< buffers_required; i++)
			    {
				((get_resp->buffers[i])->attrbuf)->use--;
				pthread_mutex_lock(&bufferpool_queue_mutex);
				makeBufferAvailable(get_resp->buffers[i]);
				pthread_mutex_unlock(&bufferpool_queue_mutex);


			    }
			    if(temp_attrbuf->hotness == 0 && temp_attrbuf->ref == 0) {
				pthread_mutex_lock(&bufferpool_queue_mutex);
				calculateHotnessBucket(temp_attrbuf);
				pthread_mutex_unlock(&bufferpool_queue_mutex);
			    }
			    requested_object->response_length = 0;
			    free(get_resp);
			    goto BMPUT;
		       }


		       free(get_resp);
		       requested_object->bm_put = 0;
		    }
		    else {
BMPUT:
			bm_register_module.BM_put(requested_object);
			requested_object->bm_put = 1;
		    }
		    free(stat_resp);
		    //set the time of hit 
		    (requested_object->attrbuff)->object_ts = AO_load(&nkn_cur_ts);
		    //bm_register_module.deref_fetched_buffers(requested_object);
		    requested_object->total_fetched += requested_object->response_length;
		    //Also no need to modify requested_object->request_offset, 
		    //since the next task would be to BM_get from requested 
		    //offset till the end;

		    //Modify the length to remaining bytes to be fetched 
		    //and add task to the queue(after bm_put).
		    requested_object->request_length =
			requested_object->request_length -
			    requested_object->response_length;
		    //reset bm_get_failure
		    //requested_object->bm_get_failure = 0;
		}
	    }
	   else {
		if (requested_object->request_length !=
			requested_object->response_length) {
		    //requested_object->request_length = 
		    //requested_object->request_length - 
		    //requested_object->response_length;

		    requested_object->request_length =
			requested_object->request_length -
			    requested_object->response_length;

		}
	    }

	    if(requested_object->write_size != 0) {
		//in case of size based selection when requested len is 
		//less than the min size required for ingestion, then do not
		//pass the obj to AM.
		if (am_register_module.am_inc_num_hits) {
		     pthread_mutex_lock(&ingestion_protection_mutex);
		     //lock to ensure race doesnt happen between the am
		     //ingestion thread while freeing the extent and over here
		     //while incrementing extent count.
		    if(((((AO_load(&nkn_cur_ts) -
				((requested_object->attrbuff)->last_send)))
			> (3*DELAY_GRANULARITY)) &&
			    requested_object->first_request == 0) ||
			(requested_object->bm_put == 1 &&
			    requested_object->first_request == 0) ||
		        (requested_object->bm_put == 1 &&
			(AO_load(&((requested_object->attrbuff)->ingest_in_progress)) == 1))) {

			AO_fetch_and_add1(&((requested_object->attrbuff)->ref));
			if ( AO_load(&((requested_object->attrbuff)->ref)) == 1)
			{
			    //remove the attrib buffer from the tailq;
			    TAILQ_REMOVE(&attrbufferpool_queues[(requested_object->attrbuff)->flist], (requested_object->attrbuff), attrbuffpoolentry);

			}
			(requested_object->attrbuff)->last_send =
			    AO_load(&nkn_cur_ts);
			transfer_obj =
		       (am_transfer_data_t*)calloc(sizeof(am_transfer_data_t),1);
			if (requested_object->first_request == 0) {
			    (requested_object->attrbuff)->num_hits++;
			    requested_object->first_request = 1;
			}
			copy_to_transfer_object(requested_object, transfer_obj);
			ref_buffers_and_copy(requested_object, transfer_obj);
			am_register_module.am_inc_num_hits(transfer_obj);

			//Synchronisation code 
			//pthread_mutex_unlock(&ingestion_protection_mutex);
			//pthread_mutex_lock(&synch_mutex);

			//pthread_cond_wait(&synch_cv, &synch_mutex);

			//pthread_mutex_unlock(&synch_mutex);

			//pthread_mutex_lock(&ingestion_protection_mutex);
			//Synchronisation code end
		    }
		    else {
			if (requested_object->first_request == 0) {
			    (requested_object->attrbuff)->num_hits++;
			    //requested_object->am_inc_num_hits = 1;
			    requested_object->first_request = 1;
			}
		    }
		    pthread_mutex_unlock(&ingestion_protection_mutex);
		}

	    }
	    bm_register_module.deref_fetched_buffers(requested_object);

	    requested_object->bm_put = 0;

	    if (requested_object->request_offset >=
		    requested_object->end_offset) {
                //data retrieval complete.
		if( requested_object->total_fetched !=
			(end - requested_object->start_offset)) {
		    printf(" fetched: %ld, total %ld,num: %ld\n",
			    requested_object->total_fetched,
				end - requested_object->start_offset,
				    requested_object->entry_num);
		    printf("WRONG FETCH : OBJ uri %s, strt: %ld, end:%ld,\n",
			    requested_object->uri_name,
				requested_object->start_offset,
				    requested_object->end_offset);

		}

		pthread_mutex_lock(&queue_busy_mutex);
		//queue_busy--;
		AO_fetch_and_sub1(&queue_busy);
		pthread_mutex_unlock(&queue_busy_mutex);

		free(requested_object);
                entries_done++;
		cache_hit_ratio = (total_dup_bytes_delivered * 100) /
		    (total_bytes_delivered);
		continue;

            }

	    pthread_mutex_lock(&sched_queue_mutex);
	    TAILQ_INSERT_TAIL(&sched_queue, requested_object,
		    sched_queue_entries);
	    pthread_mutex_unlock(&sched_queue_mutex);

	    //}


	}
	else
	    pthread_mutex_unlock(&sched_queue_mutex);
    }


}


void CE_inc_cur_time(uint64_t increment) {


    AO_fetch_and_add(&nkn_cur_ts, increment);

/* //Synchronisation code
    pthread_mutex_lock(&ingest_synch_mutex);
    while ((AO_load(&nkn_cur_ts) > AO_load(&ingest_done_time)) &&
	    AO_load(&ingest_done_time) !=0 ) {


	pthread_cond_wait(&ingest_synch_cv, &ingest_synch_mutex);


    }
    pthread_mutex_unlock(&ingest_synch_mutex);
    //Synchronisation code end
*/



}



void derefbuffer(nkn_buf_t *buffer)
{
    AO_fetch_and_sub1(&(buffer->ref));
    assert((AO_load(&(buffer->ref))) >=0);

    assert((AO_load(&(buffer->ref))) <1800000);
    if((AO_load(&(buffer->ref))) == 0) {

	pthread_mutex_lock(&bufferpool_queue_mutex);
	calculateHotnessBucket(buffer);
	pthread_mutex_unlock(&bufferpool_queue_mutex);
    }
}


void release_data_buffer(nkn_buf_t *present_buffer)
{

    if (AO_load(&(present_buffer->ref))==0) {

	free(present_buffer->uri_name);
	AO_fetch_and_sub1(&((present_buffer->attrbuf)->ref));
	//derefbuffer(present_buffer->attrbuf);
	(present_buffer->attrbuf)->buffer_count--;
	pthread_mutex_lock(&bufferpool_queue_mutex);
	TAILQ_REMOVE(&bufferpool_queues[present_buffer->flist],
	    present_buffer, bufferpoolentry);
	makeBufferAvailable(present_buffer);
	pthread_mutex_unlock(&bufferpool_queue_mutex);
    }
}




/* function to set the required parameters of the transfer obj used to transfer
 * the data to AM
 */
void copy_to_transfer_object(uri_entry_t *requested_object ,
	am_transfer_data_t *transfer_obj )
{
    //int i = 0;
    strncpy(transfer_obj->uri_name, requested_object->uri_name,
	    strlen(requested_object->uri_name));
    transfer_obj->attrbuff = requested_object->attrbuff;
    transfer_obj->provider = (requested_object->attrbuff)->provider;
    transfer_obj->num_hits = (requested_object->attrbuff)->num_hits;
    transfer_obj->hotness = (requested_object->attrbuff)->hotness;
transfer_obj->bytes_from_cache = (requested_object->attrbuff)->bytes_from_cache;
    transfer_obj->object_ts = (requested_object->attrbuff)->object_ts;
    transfer_obj->bm_put = requested_object->bm_put;
    transfer_obj->entry_num = requested_object->entry_num;
    transfer_obj->tier_type = requested_object->tier_type;
    transfer_obj->write_size = requested_object->write_size;
    //reset the bytes from cache for next iteration.
    (requested_object->attrbuff)->bytes_from_cache = 0;
    if (requested_object->partial != 1)
    {
	//required for offline(pull) ingest
	transfer_obj->length = requested_object->content_len;

    }


}

void ref_buffers_and_copy(uri_entry_t *requested_object,
	am_transfer_data_t *transfer_obj)
{

    int i = 0;
    if ((requested_object->bm_put == 1) &&
	    (AO_load(&ingest_count) < max_parallel_ingests ||
		AO_load(&((requested_object->attrbuff)->ingest_in_progress))) &&
	    ((AO_load(&buffers_held) + requested_object->buffer_fetch_count) <
	     max_buffers_hold))
    {
	for(i =0; i<requested_object->buffer_fetch_count; i++) {

	transfer_obj->buffers[i] = requested_object->data_bufs[i];
	AO_fetch_and_add1(&((transfer_obj->buffers[i])->ref));
	assert(AO_load(&((transfer_obj->buffers[i])->ref)) != 1);

	}
	transfer_obj->buffer_count = requested_object->buffer_fetch_count;
	//(transfer_obj->attrbuff)->ingest_in_progress = 1;
	AO_fetch_and_add(&buffers_held,transfer_obj->buffer_count);
	AO_fetch_and_add1(&((requested_object->attrbuff)->extent_count));
	assert(AO_load(&((requested_object->attrbuff)->extent_count))!=0);
    }

    else {
	transfer_obj->buffer_count = 0; //buffers obtained from bm_get or dm_get
    //indicates to am that no ingestion is to be done. do only hotness update
	requested_object->bm_put = 0;
	transfer_obj->bm_put = 0;
    }
}


void copy_to_stat_resp(uri_entry_t *requested_object,DM_stat_resp_t *stat_resp)
{
    strncpy(stat_resp->uri_name, requested_object->uri_name,
	    strlen(requested_object->uri_name));
    stat_resp->request_offset = requested_object->request_offset;
    stat_resp->request_length = requested_object->request_length;
    stat_resp->failure = 1;

}

void update_hotness(am_object *object)
{




}

void CE_DM_put(DM_put_data_t *put_resp)
{

    dm_register_module.DM_put(put_resp);

}


void CE_DM_attribute_update(attr_update_transfer_t *obj)
{

    dm_register_module.DM_attribute_update(obj);


}




void select_tier(uri_entry_t *object, int size_based_selection)
{

    if (size_based_selection == 0) {


	if (number_of_SATAs != 0) {

	    object->tier_type = TYPE_SATA;
	    object->write_size = sata_block_size;
	}
	else if (number_of_SASs != 0) {

	    object->tier_type = TYPE_SAS;
	    object->write_size = sas_block_size;
	}
	else if (number_of_SSDs != 0) {

	    object->tier_type = TYPE_SSD;
	    object->write_size = ssd_block_size;
	}
	else {
	    assert(0);

	}

    }
    else {

	if ((object->content_len >= ssd_min_size) &&
		(object->content_len < sas_min_size)) {

	    object->tier_type = TYPE_SSD;
	    object->write_size = ssd_block_size;

	}
	else if ((object->content_len >= sas_min_size) &&
		    (object->content_len < sata_min_size)) {

	    object->tier_type = TYPE_SAS;
	    object->write_size = sas_block_size;

	}
	else if (object->content_len >= sata_min_size) {

		object->tier_type = TYPE_SATA;
		object->write_size = sata_block_size;
	}


    }

}

int main(int argc, char **argv)
{
    char config_file[255];
    char log_file[255];
    char accesslog_file[255];
    memset(config_file,0,sizeof(config_file));
    memset(log_file,0,sizeof(log_file));
    memset(accesslog_file,0,sizeof(accesslog_file));

    int c;
    FILE *config_fp = NULL;

    while(1) {
        c = getopt(argc, argv, "c:a:l:");
        if(c < 0)
            break;
        switch(c) {
            case 'c':
                strcpy(config_file, optarg);
                break;
            case 'a':
                strcpy(accesslog_file, optarg);
                break;
            case 'l':
                strcpy(log_file, optarg);
                break;
            default:
                printf("Invalid argument\n");
                printf("Usage: analytics_simulator -c <config file> -a <accesslog> -l <log file>\n");
                break;
        }
    }
    if (config_file[0]  == '\0' || accesslog_file[0] == '\0') {
        printf("Usage: analytics_simulator -c <config file> -a <accesslog> -l <log file>\n");
        return 0;
    }


//    /*Initiliase all the modules*/
//    CE_init();


    printf("Config file = %s\n", config_file);
    config_fp = fopen(config_file, "r");
    if (config_fp == NULL) {
        printf("Unable to open config file %s\n", config_file);
        return 0;
    }


    /* Parse the config file */

    printf("Parsing Config file \"%s\".....", config_file);
    parse_config(config_fp);
    fclose(config_fp);
    printf("Done\n");

     /*Initiliase all the modules*/
    CE_init();



    if (log_file[0]) {
        log_fp = fopen(log_file, "w");
        if (log_fp == NULL) {
            printf("Unable to open log file %s\n", log_file);
            return 0;
        }
        printf("log file = %s\n", log_file);
    } else {
        log_fp = stdout;
        printf("log file = stdout\n");
    }

    printf("accesslog file = %s\n", accesslog_file);
    process_logs(accesslog_file);

    fclose(log_fp);
    fclose(log_memory);
	//pthread_join(om_main_thread,0);
    //scheduler_stop_processing = 1;
    //pthread_join(schedule_get_thread,0);
    return 0;

}

void*  my_calloc(int size, char calldetails[100])
{
    void *ptr = NULL;

    ptr = (void*)calloc(size,1);
    assert(ptr);

    //fflush(log_memory);
    return ptr;
}

void my_free(void*ptr, char calldetails[100])
{


    //fprintf(log_memory,"%s , %p \n",calldetails,ptr);
    //fflush(log_memory);
    free(ptr);

}



void logger_thread(void *arg)
{

    int rc = 0;
    int i = 0;
    struct timespec timetosleep;
    struct timeval cur_time;
    //gettimeofday(&cur_time,NULL);

    //metosleep.tv_sec = cur_time.tv_sec + 1;
    //sleep of 1 microsecond before checking next entry can be processed or not.
    //metosleep.tv_nsec = (cur_time.tv_usec/1000 + (100000));


    while(1)
    {
	gettimeofday(&cur_time,NULL);
	timetosleep.tv_sec = cur_time.tv_sec + 1;
	timetosleep.tv_nsec = (cur_time.tv_usec/1000 + (100000));
	pthread_mutex_lock(&logger_mutex);


	printf("Current Time = %ld, Cache_Hit_Ratio = %ld, entries done %ld\n",
	    AO_load(&nkn_cur_ts),cache_hit_ratio, entries_done);
	printf("buffers held %ld, //el ingests %ld\n", AO_load(&buffers_held),
		AO_load(&ingest_count));

	/*printf(" cur time %ld, Ingestdonetime %ld \n", (AO_load(&nkn_cur_ts)),
	    AO_load(&ingest_done_time));*/

	for(i=0;i<number_of_disks;i++) {
             printf("Total bytes in  disk %d = %ld \n",
		                       i+1,total_bytes_in_disks[i]);
        }

	rc = pthread_cond_timedwait(&logger_cond,&logger_mutex,&timetosleep);

	pthread_mutex_unlock(&logger_mutex);






    }



}


void *signal_handler_fn(void *arg)
{

    int ret = 0;
    int signal_caught = 0;
    ret = sigwait(&signal_mask, &signal_caught);

    switch(signal_caught)
    {

	case SIGSEGV: fclose(log_memory);
		      break;

	default:
	    printf("Unexpected signal %d\n",signal_caught);

	break;
    }
    exit(0);
}









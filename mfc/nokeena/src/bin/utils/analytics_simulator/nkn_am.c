/*
* Copyright 2010 Juniper Networks, Inc
*
* This file contains code which implements the  manager
*
* Author: Hrushikesh Paralikar
*
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <math.h>
#include <sys/prctl.h>
#include <strings.h>
#include <pthread.h>
#include "nkn_am_api.h"
#include "nkn_bm_api.h"
#include "nkn_global_defs.h"



unsigned long glob_am_bytes_based_hotness_threshold = 2097152;
int glob_am_bytes_based_hotness = 0;
uint64_t am_stop_processing = 0;
uint64_t ingestion_thread_done = 0;
void am_inc_num_hits(am_transfer_data_t  *transfered_obj)
{

    //add the task to the am_task_queue;


    pthread_mutex_lock(&am_task_queue_mutex);
    TAILQ_INSERT_TAIL(&am_task_queue, transfered_obj, am_task_queue_entry);
    pthread_mutex_unlock(&am_task_queue_mutex);




}

//unused
void am_delete()
{



}
void copytoAMobject(am_transfer_data_t *transfered_obj,
	am_object_data_t* am_object)
{

    strncpy(am_object->uri_name, transfered_obj->uri_name,
	    strlen(transfered_obj->uri_name));

    am_object->provider = transfered_obj->provider;
    am_object->num_hits = transfered_obj->num_hits;
    am_object->hotness = transfered_obj->hotness;
    am_object->bytes_from_cache = transfered_obj->bytes_from_cache;
    am_object->object_ts = transfered_obj->object_ts;
    am_object->tier_type = transfered_obj->tier_type;
    am_object->write_size = transfered_obj->write_size;
    am_object->length = transfered_obj->length;
    am_object->partial = transfered_obj->partial;
}


/* The AM main thread: takes the objects transferred from the core engine
 * (scheduler thread) and does the hotness update. Also sends the object to the 
 * ingestion thread if the object is supposed to be ingested
 */
void AM_main(void *arg)
{

    am_transfer_data_t *transfered_object = NULL;
    am_object_data_t *am_object = NULL, *temp = NULL;
    attr_update_transfer_t * update_transfer_obj = NULL;
    uint64_t hotness_increment = 0;
    uint64_t bytes_from_cache = 0;
    nkn_buf_t *attrbuff = NULL;
    //am_extent_t *extent = NULL;
    pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
    while(1) {

	if(bm_stop_processing == 1) {
	    pthread_mutex_lock(&am_task_queue_mutex);
	    if(TAILQ_EMPTY(&am_task_queue)) {
		pthread_mutex_unlock(&am_task_queue_mutex);
		am_stop_processing = 1;
		printf("AM main exiting.. \n");
		//pthread_exit(0);
		return;
	    }
	    pthread_mutex_unlock(&am_task_queue_mutex);
	}


	pthread_mutex_lock(&am_task_queue_mutex);
	if(!TAILQ_EMPTY(&am_task_queue)) {

	    transfered_object = TAILQ_FIRST(&am_task_queue);
	    TAILQ_REMOVE(&am_task_queue, transfered_object,am_task_queue_entry);

	    pthread_mutex_unlock(&am_task_queue_mutex);
	    attrbuff = transfered_object->attrbuff;




	    //hash table lookup to check if object is already present or not
	    //if not, make entry into hash table.

	    am_object = g_hash_table_lookup(am_hash_table,
				    transfered_object->uri_name);

	    if (transfered_object->bm_put == 1 &&
		    AO_load(&(attrbuff->ingest_in_progress)) == 1) {

		assert(am_object);

		goto ingest_begin;
	    }
	    if (transfered_object->bm_put == 1 &&
		     AO_load(&(attrbuff->ingest_in_progress)) == 0) {
		AO_fetch_and_add1(&((attrbuff)->ref));
		//unref this after ingestion complete.
		AO_store(&(attrbuff->ingest_in_progress),1);
		//ingest_count++;
		AO_fetch_and_add1(&ingest_count);
	    }

	    if (am_object == NULL) {
		am_object = (am_object_data_t *)calloc(sizeof(am_object_data_t),1);
		copytoAMobject(transfered_object, am_object);
		//attrbuff = transfered_object->attrbuff;
		//free(transfered_object);
		g_hash_table_insert(am_hash_table,am_object->uri_name,
		    am_object);


		TAILQ_INSERT_TAIL(&ingest_queues[am_object->tier_type-1],
			am_object, ingest_queues_entry);
		am_object->offline_ingest_queue = FIFO_LIFO_QUEUE;
	    }
	    else {

		if (am_object->provider == PROVIDER_ORIGIN) {
		    TAILQ_REMOVE(&am_origin_provider_queue, am_object,
			am_origin_provider_entry);
		}
		else {
		    TAILQ_REMOVE(&am_disk_provider_queue, am_object,
			am_disk_provider_entry);
		}

		total_bytes_in_am -= sizeof(am_object)+am_extra_struct_size;
	    }
	    /*
	    extent = (am_extent_t *)calloc(sizeof(am_extent_t),1);
	    extent->object = am_object;
	    copy_to_am_extent(transfered_object, extent);
	    pthread_mutex_lock(&am_ingest_queue_mutex);
	    TAILQ_INSERT_TAIL(&am_ingest_queue, extent, am_ingest_queue_entry);
	    pthread_mutex_unlock(&am_ingest_queue_mutex);
	    */
	    //create_and_add_extent(transfered_object, am_object);

	    //free(transfered_object);

	    if (am_object->provider == PROVIDER_ORIGIN) {
		//provider is origin, put in origin queue.
		 if( (total_bytes_in_am +sizeof(am_object)+am_extra_struct_size)
			 > am_size){
		     if(!TAILQ_EMPTY(&am_origin_provider_queue)) {
			temp = TAILQ_FIRST(&am_origin_provider_queue);
			 while(1) {
			    assert(temp);
			    //temp should never be NULL
			    if ((AO_load(&(temp->in_ingestion)) == 0) &&
				    (AO_load(&(temp->offline_ingest_on)) == 0)){
				TAILQ_REMOVE(&am_origin_provider_queue, temp,
				    am_origin_provider_entry);
				g_hash_table_remove(am_hash_table,
					temp->uri_name);
				total_bytes_in_am -=
				    (sizeof(temp)+am_extra_struct_size);
				free(temp);
				break;
			    }
			    temp = TAILQ_NEXT(temp, am_origin_provider_entry);
			}
		     }
		}


		TAILQ_INSERT_TAIL(&am_origin_provider_queue, am_object,
		    am_origin_provider_entry);
		total_bytes_in_am += sizeof(am_object)+am_extra_struct_size;

	    }
	    else {
		//provider is disk, put in disk queue.
		if( (total_bytes_in_am +sizeof(am_object)+am_extra_struct_size)
		    > am_size){
		    if(!TAILQ_EMPTY(&am_disk_provider_queue)) {

			temp = TAILQ_FIRST(&am_disk_provider_queue);
			while(1) {
			    assert(temp);
			    //temp should never be NULL
			    if ((AO_load(&(temp->in_ingestion)) == 0) &&
				    (AO_load(&(temp->offline_ingest_on)) == 0)){

				TAILQ_REMOVE(&am_disk_provider_queue, temp,
				    am_disk_provider_entry);
				g_hash_table_remove(am_hash_table,
					temp->uri_name);
				total_bytes_in_am -=
				    (sizeof(temp)+am_extra_struct_size);
				free(temp);
				break;
			    }
			    temp = TAILQ_NEXT(temp, am_disk_provider_entry);
			}

		    }

		}
		TAILQ_INSERT_TAIL(&am_disk_provider_queue, am_object,
		    am_disk_provider_entry);
		total_bytes_in_am += sizeof(am_object)+am_extra_struct_size;

	    }

	    bytes_from_cache = am_object->bytes_from_cache;
	    if(glob_am_bytes_based_hotness) {

		//calculate hotness_increment based on bytes returned from 
		//cache and global_am_bytes_threshold
		if (bytes_from_cache>glob_am_bytes_based_hotness_threshold)
		{
		    hotness_increment = ((double)bytes_from_cache /
			   (double)glob_am_bytes_based_hotness_threshold);

		}
	    }

	    /*attrbuff->hotness =  am_update_hotness(&am_object->hotness,
		am_object->num_hits,hotness_increment,(am_object->object_ts/DELAY_GRANULARITY),0);*/

	    attrbuff->hotness =  am_update_hotness(&attrbuff->hotness,
		    attrbuff->num_hits,(attrbuff->object_ts/
			DELAY_GRANULARITY), hotness_increment, 0);
	    //attribute update in DM

	    am_object->hotness = attrbuff->hotness;
	    am_object->num_hits = attrbuff->num_hits;
	    am_object->object_ts = attrbuff->object_ts;
	    update_transfer_obj =
		(attr_update_transfer_t *)calloc(sizeof(attr_update_transfer_t),1);
	    update_transfer_obj->hotness = am_object->hotness;
	    update_transfer_obj->decoded_hotness =
		am_decode_hotness(&am_object->hotness);
	    strncpy(update_transfer_obj->uri_name, am_object->uri_name,
		    (MAX_URI_LEN-1));
	    CE_DM_attribute_update(update_transfer_obj);
	    free(update_transfer_obj);
	    //reset hotness_increment for next iteration;

	    hotness_increment = 0;
	    bytes_from_cache = 0;

	    if (transfered_object->bm_put != 1) {
		/* //Synchronisation code
		pthread_mutex_lock(&synch_mutex);
		pthread_cond_signal(&synch_cv);
		pthread_mutex_unlock(&synch_mutex);
		  //Synchronisation code end  
		*/
		goto ingest_done;
	    }
ingest_begin:

	    pthread_mutex_lock(&ingestion_protection_mutex);
	    if (AO_load(&(am_object->in_ingestion)) == 0) {

		    AO_store(&(am_object->in_ingestion),1);
	    }
	    pthread_mutex_unlock(&ingestion_protection_mutex);
	    //create an extent from the buffers, and add this extent to
	    //the extent queue of the ingestion thread.
	    create_and_add_extent(transfered_object, am_object);
//	    AO_fetch_and_add1(&((attrbuff)->extent_count));
ingest_done:
	    free(transfered_object);

	    //derefattributebuffer((am_object->attrbuff));
	    /*AO_fetch_and_sub1(&(attrbuff->ref));
	    assert((AO_load(&(attrbuff->ref))) >= 0);

	    if((AO_load(&(attrbuff->ref))) == 0) {

	        calculateHotnessBucket(attrbuff);
	    }*/
	    derefbuffer(attrbuff);
	    attrbuff = NULL;
	    am_object = NULL;
	}
	else
	    pthread_mutex_unlock(&am_task_queue_mutex);

    }


}

/* The ingestion thread: 
 * 1. Reads an extent from the am_ingest_queue and merges the extents together
 *    if an extent representing the same block of data is already present. Also
 *    creates "push objects" and inserts them into the am_write_disk_queue.
 * 2. Reads the push objects from the write_disk_queue and checks for the
 *    extents of the push object. If the extent has been held for more than
 *    buffer_hold_time, then releases the buffers and if the extent has
 *    accumulated a disk block size of data, then adds the extent to the
 *    write_simulate_queue for simulating disk write.
 * 3. Reads extents from the write simulte queue and checks whether the time
 *    is greater than the ingestion simulation time + the time at which 
 *    ingestion was started. if it is, then simulate a disk write.
 */
void AM_ingest(void *arg)
{
    am_extent_t *extent = NULL, *old_extent = NULL, *written_extent = NULL;
    am_push_ingest_t *push_ptr = NULL;
    GList *iterator = NULL;
    int merge = 0;
    int ret = 0;
    int i = 0;
    int exit_value = 0;
    int free_needed = 0;
    DM_put_data_t *put_data;
    pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);

    while(1){


	//exit logic for this thread.

	if (am_stop_processing == 1 && bm_stop_processing == 1) {
	    pthread_mutex_lock(&am_ingest_queue_mutex);
	    if(TAILQ_EMPTY(&am_ingest_queue) &&
		    TAILQ_EMPTY(&am_write_disk_queue) &&
			TAILQ_EMPTY(&write_simulate_queue)) {
		pthread_mutex_unlock(&am_ingest_queue_mutex);
		printf("AM ingest exiting.. \n");
		ingestion_thread_done = 1;
		//pthread_exit(0);
		return;
	    }
	     pthread_mutex_unlock(&am_ingest_queue_mutex);

	}


	pthread_mutex_lock(&am_ingest_queue_mutex);
	if(!TAILQ_EMPTY(&am_ingest_queue)) {

	    extent = TAILQ_FIRST(&am_ingest_queue);
	    TAILQ_REMOVE(&am_ingest_queue, extent, am_ingest_queue_entry);
	    pthread_mutex_unlock(&am_ingest_queue_mutex);
	    if(!((extent->object)->push_ptr)) {

		//this is new object. create new push object for
		//ingestion. and set the am_object push_ptr and
		//add the extent.

		push_ptr=(am_push_ingest_t *)calloc(sizeof(am_push_ingest_t),1);
		push_ptr->am_extent_list = NULL;
		push_ptr->attrbuff = extent->attrbuff;
		//set the parameters for the push_obj
		(extent->object)->push_ptr = push_ptr;
		push_ptr->am_extent_list=g_list_append(push_ptr->am_extent_list,
			extent);

		/*if(AO_load(&((push_ptr->attrbuff)->ingest_in_progress))==0) {
		    AO_store(&((push_ptr->attrbuff)->ingest_in_progress),1);

		    assert(AO_load(&((push_ptr->attrbuff)->ref)) !=0);
		    AO_fetch_and_add1(&((push_ptr->attrbuff)->ref));
		}*/
		assert(AO_load(&((push_ptr->attrbuff)->ingest_in_progress))==1);
		//insert the push object onto the write queue.
		TAILQ_INSERT_TAIL(&am_write_disk_queue, push_ptr,
			am_write_disk_queue_entry);
	    }
	    else
	    {


		//object already exists. obtain object and search for
		//existing extent for merging. If extent not present, this
		//is new extent. Add it to the extent list.
		push_ptr = (extent->object)->push_ptr;
		iterator = g_list_first(push_ptr->am_extent_list);
		while(iterator != NULL)
		{
		    if (extent->start_offset ==
			    ((am_extent_t *)(iterator->data))->start_offset) {

			merge = 1;
			old_extent = ((am_extent_t *)(iterator->data));
			push_ptr->am_extent_list =
			    g_list_remove_link(push_ptr->am_extent_list,
				iterator);
			//free(iterator);
			break;
		    }
		    else{
		    //iterator = g_list_next(iterator);
			iterator = g_list_next(iterator);
		    }
		    //iterator = g_list_first(push_ptr->am_extent_list);
		}
		if (merge == 1) {


		    //merge into new extent
		    //remove old extent from the list and free it up.
		    //add new extent into the list.
		    ret = merge_extents(extent, old_extent);

		    AO_fetch_and_sub1(&((push_ptr->attrbuff)->extent_count));
		    assert(AO_load(&((push_ptr->attrbuff)->extent_count))!=0);
		    free(old_extent);
		}
		else
		{

		    //check if extent with same offset is sent for ingestion.
		    TAILQ_FOREACH(old_extent, &write_simulate_queue,
			    write_simulate_queue_entry) {
			if(!strcmp((extent->attrbuff)->uri_name,
				    (old_extent->attrbuff)->uri_name)) {
			    if (extent->start_offset == old_extent->start_offset) {

				printf(" EXTENT GIVEN FOR INGESTION!!!!!!!!\n");
			    }
			}
		    }

		}
		push_ptr->am_extent_list=g_list_append(push_ptr->am_extent_list,
			extent);
		assert(AO_load(&((push_ptr->attrbuff)->ingest_in_progress))==1);
		//set last update time of extent
		old_extent = NULL;
		merge = 0;
	    }

	    pthread_mutex_lock(&synch_mutex);
	    pthread_cond_signal(&synch_cv);
	    pthread_mutex_unlock(&synch_mutex);

	}
	else
	    pthread_mutex_unlock(&am_ingest_queue_mutex);


	push_ptr = NULL;
	if (!TAILQ_EMPTY(&am_write_disk_queue)) {

	    push_ptr = TAILQ_FIRST(&am_write_disk_queue);
	    TAILQ_REMOVE(&am_write_disk_queue, push_ptr,
		    am_write_disk_queue_entry);

	    //traverse the extent list for the push ptr.
	    //for each extent, if size = 2MB, send it for ingestion.
	    //check how to simulate delay between the 2 conseq writes
	    //ie: cant ingest before the first one is done.
	    //if extent last update time exceeds timeout, free extent.
	    //if object has no extents left, free the object.
	    //if obj not deleted and none of the extents are filled, put it 
	    //back at end of the queue.
	    //also decide tier type for which disk the obj should go on.


	    if ((g_list_length(push_ptr->am_extent_list) == 0)) {
		 assert(AO_load(&((push_ptr->attrbuff)->extent_count)) != 0);
		TAILQ_INSERT_TAIL(&am_write_disk_queue, push_ptr,
		    am_write_disk_queue_entry);


	    //	push_ptr = NULL;
	   //	continue;
		goto write_simulate;
	    }
	    iterator = g_list_first(push_ptr->am_extent_list);
	    while(iterator != NULL)
	    {

		if ( (AO_load(&nkn_cur_ts) -
			((am_extent_t *)(iterator->data))->last_updated) >
			    buffer_hold_time) {
		    //deref buffers
		    //free extent.
		    //
		    //
		    if( ((am_extent_t *)(iterator->data))->length ==
			    ((am_extent_t *)(iterator->data))->write_size) {

		    printf("deleting full extent\n");

		    }
		    for(i = 0; i< MAX_TRANSFERRABLE_BUFFERS; i++)
		    {

		        if (((am_extent_t *)(iterator->data))->buffers[i]!=NULL)
			{

		     derefbuffer(((am_extent_t *)(iterator->data))->buffers[i]);
				//buffers_held--;
				AO_fetch_and_sub1(&buffers_held);
			}
			//((am_extent_t *)(iterator->data))->buffers[i] = NULL;

		    }
		    //(((am_extent_t *)(iterator->data))->object)->push_ptr = NULL;
		    push_ptr->am_extent_list =
			g_list_remove_link(push_ptr->am_extent_list,
			iterator);
		    free_needed = 1;
		    //free((am_extent_t *)(iterator->data));
		    //free(iterator);

		}
		else if (((am_extent_t *)(iterator->data))->length ==
			    ((am_extent_t *)(iterator->data))->write_size){

		    //DM_put; 

		    //deref buffers
		    //free extent.
		    //
		    ((am_extent_t *)(iterator->data))->write_start_time =
			AO_load(&nkn_cur_ts);
		    /*printf("cur time  - last updated = %ld \n",
			   (AO_load(&nkn_cur_ts) -
			    ((am_extent_t *)(iterator->data))->last_updated)); */
		    /*put_data = (DM_put_data_t*)calloc(sizeof(DM_put_data_t), 1);
		    put_data->tier_type =
			((am_extent_t *)(iterator->data))->tier_type;
		    create_dm_put(put_data, (am_extent_t *)(iterator->data));
		    CE_DM_put(put_data);*/
		    //AO_store(&ingest_done_time,
		 //	    (AO_load(&nkn_cur_ts) + dm_put_delay));
		    //(push_ptr->attrbuff)->ingest_in_progress = 0;
		    //free(put_data);
		    //(((am_extent_t *)(iterator->data))->object)->push_ptr = NULL;

		    push_ptr->am_extent_list =
			g_list_remove_link(push_ptr->am_extent_list,
			    iterator);
		    TAILQ_INSERT_TAIL(&write_simulate_queue,
			(am_extent_t *)(iterator->data),
			    write_simulate_queue_entry);
			//free_needed = 1;

		    free_needed = 0;
		}
		if (free_needed == 1) {

		    free_needed = 0;
		    exit_value = free_extent(((am_extent_t *)(iterator->data)));
		    /*
		    AO_fetch_and_sub1(&((push_ptr->attrbuff)->extent_count));
		    if ((g_list_length(push_ptr->am_extent_list) == 0) &&
		     (AO_load(&((push_ptr->attrbuff)->extent_count))== 0)) {


		    (((am_extent_t *)(iterator->data))->object)->push_ptr=NULL;


		       assert(AO_load(&((push_ptr->attrbuff)->ingest_in_progress)) == 1);
		       AO_store(&((push_ptr->attrbuff)->ingest_in_progress),0);
		       derefbuffer(push_ptr->attrbuff);
		       free(push_ptr);
		       push_ptr = NULL;
		       ingest_count--;
		       free((am_extent_t *)(iterator->data));
		       break;
		    }
		    free((am_extent_t *)(iterator->data));*/


		    if(exit_value == 0 )
		    {
			push_ptr = NULL;
			break;

		    }
		    //else , exit_value = 1 , so the terminatong condition was
		    //not met, free this current extent and continue;
		    free((am_extent_t *)(iterator->data));

		}

		iterator = g_list_next(iterator);

		//iterator = g_list_next(iterator);
		//iterator = g_list_first(push_ptr->am_extent_list);
	    }
	    /*
	    if (g_list_length(push_ptr->am_extent_list) == 0 &&
		    (AO_load(&((push_ptr->attrbuff)->extent_count))== 0)) {

		//(push_ptr->attrbuff)->ingest_in_progress = 0;
		assert(AO_load(&((push_ptr->attrbuff)->ingest_in_progress)) == 1);
		AO_store(&((push_ptr->attrbuff)->ingest_in_progress),0);
		derefbuffer(push_ptr->attrbuff);
		free(push_ptr);
		ingest_count--;
		//push_ptr = NULL;
		continue;

	    }
	    else
	    {
		TAILQ_INSERT_TAIL(&am_write_disk_queue, push_ptr,
			am_write_disk_queue_entry);


	    }*/
	    /*if(push_ptr) {

		TAILQ_INSERT_TAIL(&am_write_disk_queue, push_ptr,
		    am_write_disk_queue_entry);

	    }*/

	    if(push_ptr) {
		TAILQ_INSERT_TAIL(&am_write_disk_queue, push_ptr,
		    am_write_disk_queue_entry);
	    }
	    //AO_fetch_and_add(&nkn_cur_ts, 100);
	}

write_simulate:
	    if (!TAILQ_EMPTY(&write_simulate_queue)) {

		written_extent = TAILQ_FIRST(&write_simulate_queue);
		TAILQ_REMOVE(&write_simulate_queue, written_extent,
			write_simulate_queue_entry);

		if ((AO_load(&nkn_cur_ts) - written_extent->write_start_time) >
			dm_put_delay) {

		    put_data = (DM_put_data_t*)calloc(sizeof(DM_put_data_t), 1);
		    put_data->tier_type = written_extent->tier_type;
		    create_dm_put(put_data, written_extent);
		    CE_DM_put(put_data);

		    free(put_data);
		    for(i = 0; i< MAX_TRANSFERRABLE_BUFFERS; i++)
		    {
			if (written_extent->buffers[i]!=NULL)
			{

			    derefbuffer(written_extent->buffers[i]);
		            //buffers_held--;
			    AO_fetch_and_sub1(&buffers_held);
		        }
		    }
		    written_extent->from_ingestion = 1;
		    exit_value = free_extent(written_extent);
		    if(exit_value == 1) {

			free(written_extent);

		    }

		    pthread_mutex_lock(&ingest_synch_mutex);
		    AO_store(&ingest_done_time, 0);
		    pthread_cond_signal(&ingest_synch_cv);
		    pthread_mutex_unlock(&ingest_synch_mutex);
		    ingests_done++;
		}
		else {

		    TAILQ_INSERT_TAIL(&write_simulate_queue,
			written_extent, write_simulate_queue_entry);

		}

	    }
	    if (!TAILQ_EMPTY(&am_ingest_queue) ||
		    !TAILQ_EMPTY(&am_write_disk_queue) ||
		      !TAILQ_EMPTY(&write_simulate_queue)) {
		AO_fetch_and_add(&nkn_cur_ts, 1);
	    }
	    /*
	     //Synchronisation code
	    pthread_mutex_lock(&ingest_synch_mutex);
	    if(AO_load(&ingest_done_time) != 0) {

		    pthread_cond_signal(&ingest_synch_cv);
	    }
	     pthread_mutex_unlock(&ingest_synch_mutex);
	     //Synchronisation code end
	     */
    }
}

int free_extent(am_extent_t *extent)
{
    am_push_ingest_t *push_ptr = NULL;

    pthread_mutex_lock(&ingestion_protection_mutex);
    AO_fetch_and_sub1(&((extent->attrbuff)->extent_count));
    if ((g_list_length((extent->object)->push_ptr->am_extent_list) == 0) &&
	    (AO_load(&((extent->attrbuff)->extent_count))== 0)) {

	push_ptr = (extent->object)->push_ptr;
	(extent->object)->push_ptr=NULL;
	assert(AO_load(&((push_ptr->attrbuff)->ingest_in_progress)) == 1);
	AO_store(&((push_ptr->attrbuff)->ingest_in_progress),0);
	derefbuffer(push_ptr->attrbuff);
	if (extent->from_ingestion == 1) {

	    TAILQ_REMOVE(&am_write_disk_queue, push_ptr,
		am_write_disk_queue_entry);
	}

	free(push_ptr);
	push_ptr = NULL;
	//ingest_count--;
	AO_fetch_and_sub1(&ingest_count);
	//free(extent);
	assert(AO_load(&((extent->object)->in_ingestion)) == 1);
	AO_store(&((extent->object)->in_ingestion), 0);
	my_free(extent,"free:nkn_am.c:645\0");
	pthread_mutex_unlock(&ingestion_protection_mutex);
	return 0; //returned after freeing the extent.

    }
    pthread_mutex_unlock(&ingestion_protection_mutex);
    return 1; //returned without freeing the extent

}

void create_dm_put(DM_put_data_t *put_data, am_extent_t *extent)
{
    put_data->request_offset = extent->start_offset;
    put_data->length = extent->length;
    strncpy(put_data->uri_name, (extent->attrbuff)->uri_name,
	(MAX_URI_LEN-1));

    put_data->hotness = (extent->object)->hotness;
    put_data->decoded_hotness = am_decode_hotness(&((extent->object)->hotness));

}

int merge_extents(am_extent_t* extent, am_extent_t* old_extent)
{


    int i = 0;
    //for each buffer in the extent.
    //merge them..//aligned to 32K boundaries.

    extent->length = 0;

    for(i = 0; i< MAX_TRANSFERRABLE_BUFFERS; i++) {


	if (old_extent->buffers[i] != NULL )
	{

	    if(extent->buffers[i] != NULL) {


		assert(!(strcmp((extent->buffers[i])->uri_name,
				(old_extent->buffers[i])->uri_name)));

		if ((old_extent->buffers[i])->start_offset <
		   (extent->buffers[i])->start_offset) {

		    (extent->buffers[i])->length +=
			(extent->buffers[i])->start_offset -
			    (old_extent->buffers[i])->start_offset;
		    (extent->buffers[i])->start_offset =
			(old_extent->buffers[i])->start_offset;
		}

		if (((old_extent->buffers[i])->start_offset +
			    (old_extent->buffers[i])->length) >
			((extent->buffers[i])->start_offset +
			    (extent->buffers[i])->length)) {

			(extent->buffers[i])->length =
			    ((old_extent->buffers[i])->start_offset +
				(old_extent->buffers[i])->length) -
				    (extent->buffers[i])->start_offset;
		}


		derefbuffer((old_extent->buffers[i]));
		//deref the old buffer and make it available if ref = 0

		release_data_buffer((old_extent->buffers[i]));
	    }
	    else
	    {

		extent->buffers[i] = old_extent->buffers[i];
		old_extent->buffers[i] = NULL;

	    }


	}


	if (extent->buffers[i] != NULL) {

	    extent->length += (extent->buffers[i])->length;
	}

	//extent->last_updated = AO_load(&nkn_cur_ts);
    }

    extent->last_updated = AO_load(&nkn_cur_ts);
    //Synchronisation code
    //pthread_mutex_lock(&ingest_synch_mutex);
    if (extent->write_size == extent->length) {

	//AO_store(&ingest_done_time, (AO_load(&nkn_cur_ts) + dm_put_delay));

        ingests_to_do++;
    }

    //pthread_mutex_unlock(&ingest_synch_mutex);
    //Synchronisation code end
    return 0;


}
/*
void set_write_size(DM_put_data_t *put_data)
{

    if (put_data->tier_type == TYPE_SATA) {

	    write_size = sata_block_size;
    }
    else if (put_data->tier_type == TYPE_SAS) {


	     write_size = sas_block_size;
    }
    else if (put_data->tier_type == TYPE_SSD) {

	     write_size = ssd_block_size;
    }
    else {
	    assert(0);

    }



}*/

void copy_to_am_extent(am_transfer_data_t* transfered_obj, am_extent_t* extent)
{






}
/*
void derefbuffer(nkn_buf_t *buffer)
{
        AO_fetch_and_sub1(&(buffer->ref));
	assert((AO_load(&(buffer->ref))) >=0);

	if((AO_load(&(buffer->ref))) == 0) {

            calculateHotnessBucket(buffer);
	}
}*/



void create_and_add_extent(am_transfer_data_t* transfered_obj,
	am_object_data_t *am_object)
{
    am_extent_t *extent = NULL;
    int start_index = 0;
    int copy_from_index = 0;
    int buffers = 0;
    int i = 0;

    //set start_offset for the extent.
    //find the start index for storing the buffer pointers.
    //start index = first buffer->startoffser mod the block size.
    //store buffers from that index till the end.

    while(buffers < transfered_obj->buffer_count)
    {

	extent = (am_extent_t *)calloc(sizeof(am_extent_t),1);
	extent->object = am_object;
	extent->attrbuff = transfered_obj->attrbuff;
	extent->last_updated = AO_load(&nkn_cur_ts);
	extent->tier_type = transfered_obj->tier_type;
	extent->write_size = transfered_obj->write_size;
	extent->start_offset = ((transfered_obj->buffers[copy_from_index])->start_offset/
	    extent->write_size) * extent->write_size;
	start_index =((transfered_obj->buffers[copy_from_index])->start_offset %
		extent->write_size)/DATA_BUFFER_SIZE;
	for(i = start_index; i < MAX_TRANSFERRABLE_BUFFERS; i++)
	{
	    if (transfered_obj->buffers[copy_from_index] != NULL) {

		extent->buffers[i] = (transfered_obj->buffers[copy_from_index]);
		extent->length +=
		    (transfered_obj->buffers[copy_from_index])->length;
		copy_from_index++;
		buffers++;

	    }
	    else
	    {

		break;
	    }
	}

	if (buffers < transfered_obj->buffer_count) {

	    AO_fetch_and_add1(&((extent->attrbuff)->extent_count));
	    //in case if the 2 buffers being added to the extent being created
	    //are the 64th buff of the first and the 1st buff of the next extent
	    //inc the extent count by 1 more! since 2 extents are being added
	    //for this case 
	}
	copy_to_am_extent(transfered_obj, extent);
	pthread_mutex_lock(&am_ingest_queue_mutex);
	TAILQ_INSERT_TAIL(&am_ingest_queue, extent, am_ingest_queue_entry);
	pthread_mutex_unlock(&am_ingest_queue_mutex);
    }


}

/* the offline ingest thread
 */
void offline_ingest_thread(void *arg)
{

    //am_object_data_t *object = NULL;
    //uri_entry_t *uri_request = NULL;
    pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
    /*
     while(1)
     {
	//if number_of_offline_ingests < 100,
	//take next item with get_next_ingest_entry
	//and add to the scheduler queue to do a BM_get.
    

	if (AO_load(&number_of_offline_ingests) <= 100) {


	    object = get_next_ingest_entry();
	    if (object == NULL) {

		continue;

	    }
	    else if((AO_load(&object->ingest_in_progress)==1)) {
	    
		continue;

	    }
	    else
	    {

		//create uri_entry_t structure, set the params
		uri_request = calloc(1,MAX_URI_LEN);
		strncpy(uri_request->uri_name,object->uri_name,(MAX_URI_LEN-1));
		AO_store(&(uri_entry->offline_ingest),1);
		uri_request->partial = object->partial;
		uri_request->start_offset = 0;
		if (uri_request->partial == 1) {

		    uri_request->end_offset = 0;
		    uri_request->content_len = 0;
		}
		else {

		    uri_request->end_offset = object->length;
		    uri_request->content_len = object->length;
		}
		uri_request->request_offset = uri_request->start_offset;
		uri_request->tier_type = object->tier_type;
		uri_request->write_size = object->write_size;
		uri_request->hotness = object->hotness;

		AO_fetch_and_add1(&number_of_offline_ingests);
		pthread_mutex_lock(&sched_queue_mutex);
		TAILQ_INSERT_TAIL(&sched_queue,uri_request,sched_queue_entries);
		pthread_mutex_unlock(&sched_queue_mutex);
	    }

	}

     }


    */



}

am_object_data_t* get_next_ingest_entry()
{

    am_object_data_t *object = NULL;
    static int tier = 1;
    static int hybrid_fill_tier = 1;
    int i = 0;
    int counter = 0;


    if (offline_ingest_mode != HYBRID_INGEST) {
        for(i = 1; i <= 3; i++) {

	    if (!TAILQ_EMPTY(&ingest_queues[tier-1])) {

		if	(offline_ingest_mode == FIFO_INGEST) {
		    object = TAILQ_FIRST(&ingest_queues[tier-1]);

		}
		else {

		    object = TAILQ_LAST(&ingest_queues[tier-1],
			ingest_queues_head);
		}
		TAILQ_REMOVE(&ingest_queues[tier-1],
			object,ingest_queues_entry);
		tier++;
		if(tier == (TYPE_SATA + 1)) {
		    tier = TYPE_SSD;
		}
		return object;
	    }
	    tier++;
	    if(tier == (TYPE_SATA + 1)) {
		tier = TYPE_SSD;
	    }
	}
	/*
	else if(!TAILQ_EMPTY(&ingest_queues[TYPE_SAS-1])) {

	    if  (offline_ingest_mode == FIFO_INGEST) {
		object = TAILQ_FIRST(&ingest_queues[TYPE_SAS-1]);

	    }
	    else {

		object = TAILQ_LAST(&ingest_queues[TYPE_SAS-1],
			ingest_queues_head);
	    }
	    TAILQ_REMOVE(&ingest_queues[TYPE_SAS-1],object,ingest_queues_entry);
	    return object;


	}
	else if(!TAILQ_EMPTY(&ingest_queues[TYPE_SATA-1])) {

	    if  (offline_ingest_mode == FIFO_INGEST) {
		object = TAILQ_FIRST(&ingest_queues[TYPE_SATA-1]);

	    }
	    else {


		object = TAILQ_LAST(&ingest_queues[TYPE_SATA-1],
			ingest_queues_head);
	    }

	    TAILQ_REMOVE(&ingest_queues[TYPE_SATA-1],object,
		    ingest_queues_entry);
	    return object;
	}*/
	return NULL;
    }
    else
    {

hybrid_mode:
	for(i = 1; i <= 3; i++) {
	    if (!TAILQ_EMPTY(&hybrid_queues[tier-1])) {

		object = TAILQ_FIRST(&hybrid_queues[tier-1]);
		TAILQ_REMOVE(&hybrid_queues[tier-1],object,
			hybrid_queues_entry);


		tier++;
		if(tier == (TYPE_SATA + 1)) {
		    tier = TYPE_SSD;
		}
		return object;

	    }
	     tier++;
	     if(tier == (TYPE_SATA + 1)) {
		tier = TYPE_SSD;
	     }

	}


	//populate the queue and return object 
	for( i = 1; i<= 3; i++) {

	    if (!TAILQ_EMPTY(&hybrid_queues[hybrid_fill_tier-1])) {
		while(counter <1000) {

		    if(!TAILQ_EMPTY(&hybrid_queues[hybrid_fill_tier-1])) {
			object = TAILQ_LAST(&ingest_queues[hybrid_fill_tier-1],
			        ingest_queues_head);
			TAILQ_REMOVE(&ingest_queues[hybrid_fill_tier-1],
			    object,ingest_queues_entry);
			TAILQ_INSERT_TAIL(&hybrid_queues[hybrid_fill_tier-1],object,
			    hybrid_queues_entry);
			counter++;

		    }
		    else {
			break;
		    }
		}
		counter = 0;
		hybrid_fill_tier++;
		if(hybrid_fill_tier == (TYPE_SATA + 1)) {
		    hybrid_fill_tier = TYPE_SSD;
		    goto hybrid_mode;
		}
	    }
	    hybrid_fill_tier++;
	    if(hybrid_fill_tier == (TYPE_SATA + 1)) {

		    hybrid_fill_tier = TYPE_SSD;
	    }


	}
	return NULL;
    }





}
void AM_cleanup()
{
    am_object_data_t *am_object = NULL;
    while(!TAILQ_EMPTY(&am_origin_provider_queue)) {

	am_object = TAILQ_FIRST(&am_origin_provider_queue);
	TAILQ_REMOVE(&am_origin_provider_queue, am_object,
		am_origin_provider_entry);
	free(am_object);


    }
    am_object = NULL;

    while(!TAILQ_EMPTY(&am_disk_provider_queue)) {

	am_object = TAILQ_FIRST(&am_disk_provider_queue);
	TAILQ_REMOVE(&am_disk_provider_queue, am_object,
		am_disk_provider_entry);
	free(am_object);
    }


}


int AM_init(AM_data *am_register_module)
{

      int i = 0;
      am_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
      am_register_module->am_inc_num_hits = am_inc_num_hits;
      am_register_module->am_delete = am_delete;
      am_register_module->AM_cleanup = AM_cleanup;
      TAILQ_INIT(&am_task_queue);
      TAILQ_INIT(&am_disk_provider_queue);
      TAILQ_INIT(&am_origin_provider_queue);
      TAILQ_INIT(&am_ingest_queue);
      TAILQ_INIT(&am_write_disk_queue);
      TAILQ_INIT(&write_simulate_queue);
      for(i = 0;i <3; i++) {

	    TAILQ_INIT(&ingest_queues[i]);
	    TAILQ_INIT(&hybrid_queues[i]);
      }


      pthread_mutex_init(&am_task_queue_mutex, NULL);
      pthread_mutex_init(&am_ingest_queue_mutex, NULL);
      pthread_create(&am_main, NULL, (void*)(AM_main), NULL);
      pthread_create(&am_ingest, NULL, (void*)(AM_ingest), NULL);
      pthread_create(&am_offline_ingest, NULL, (void *)(offline_ingest_thread),
	      NULL);
      return 0;
}

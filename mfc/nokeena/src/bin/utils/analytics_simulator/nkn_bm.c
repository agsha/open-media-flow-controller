/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains code which implements the Buffer manager
 *
 * Author: Hrushikesh Paralikar
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <strings.h>
#include <pthread.h>
#include "nkn_bm_api.h"

uint64_t number_of_buffers;
uint64_t number_of_attr_buffers;
uint64_t bm_stop_processing;

/* BM_get: simulates the buffer cache fetch
 */

int BM_get(uri_entry_t *requested_object)
{

    nkn_buf_t *buffer = NULL, *attr_buffer_present = NULL;
    uint64_t offset = 0, start = 0, end = 0, append_offset = 0;
    uint64_t request_start = 0, remaining_length = 0;
    int partial_success = 0; //flag used to indicate a partial success
    char buff[512];
    char uri_name[21000];
    memset(buff, 0, sizeof(buff));

    memset(uri_name, 0, sizeof(uri_name));

    requested_object->response_length = 0;
    //setting response length to 0 for every request
    remaining_length =  requested_object->request_length;


    //start = requested_object->start_offset;
    start = (requested_object->request_offset / DATA_BUFFER_SIZE) *
	DATA_BUFFER_SIZE;
    request_start = requested_object->request_offset;
    //end = requested_object->end_offset;
    end = requested_object->request_offset+requested_object->request_length;



    for(offset = start;offset < end && requested_object->buffer_fetch_count <
	    NUM_BUFS_RETURNED; offset+= DATA_BUFFER_SIZE) {

	// offset % 32K to determine what to append;
        //append modulo result to uriname and lookup; 
	strncpy(uri_name, requested_object->uri_name,(sizeof(uri_name)-1));
	append_offset = (offset / DATA_BUFFER_SIZE) * DATA_BUFFER_SIZE;

        snprintf(buff, sizeof(buff), "%"PRIu64, append_offset);

	strcat(uri_name,",");
	strcat(uri_name,buff);


	buffer = g_hash_table_lookup(uri_hash_table, uri_name);

	if (!buffer) {
	    //BM_put(requested_object);	

	    //if first request failed, requested_offset should remain.
	    /*requested_object->response_length = 0; //0 if first lookup failed.
	    if (requested_object->request_offset != offset) {
				
		requested_object->response_length = offset -
		    requested_object->request_offset;
		requested_object->request_offset = offset;
				 
	    }*/
	    //requested_object->response_length = ;check what is response length

	    /*if ((end - offset) > DATA_BUFFER_SIZE) {
		requested_object->request_length = DATA_BUFFER_SIZE;
			
	    }
	    else {

                requested_object->request_length = end - offset;
            }*/

	    //AO_fetch_and_add(&nkn_cur_ts,bm_get_failure_delay);
	     CE_inc_cur_time(bm_get_failure_delay);
	    requested_object->bm_get_failure = 1;
	    return requested_object->bm_get_failure;


	}
	else {
	    assert(!strcmp(uri_name, buffer->uri_name));


	    //total success case.
	    if ((request_start >= buffer->start_offset) &&
		(request_start < (buffer->start_offset + buffer->length) ) &&
		((request_start + remaining_length) <=
				    (buffer->start_offset + buffer->length) )) {


		total_dup_bytes_delivered += remaining_length;
		total_bytes_delivered += remaining_length;
		requested_object->request_offset += remaining_length;
		requested_object->response_length += remaining_length;
		remaining_length = 0;
		//requested_object->attrbuff = buffer->attrbuf;

	    }
	    else if ((request_start >= buffer->start_offset) &&
		(request_start < (buffer->start_offset + buffer->length)) &&
		((request_start + remaining_length) >=
						(offset + DATA_BUFFER_SIZE)) &&
		(buffer->start_offset + buffer->length) ==
						(offset + DATA_BUFFER_SIZE)) {

		total_dup_bytes_delivered +=
				    (offset + DATA_BUFFER_SIZE - request_start);
		total_bytes_delivered +=
				    (offset + DATA_BUFFER_SIZE - request_start);
		requested_object->request_offset =
						    offset + DATA_BUFFER_SIZE;
		requested_object->response_length +=
				    (offset + DATA_BUFFER_SIZE - request_start);

		remaining_length -= (offset + DATA_BUFFER_SIZE - request_start);


	    } // success, eg : buff: 4K-32K, requested :4K- 64K
	    else if ( (request_start < buffer->start_offset) &&
		((request_start + remaining_length) >= buffer->start_offset) &&
		((request_start + remaining_length) <=
						(offset + DATA_BUFFER_SIZE))) {


		requested_object->bm_get_failure = 1;

	   } //partial success: some data present buff: 4K - 32K, Req: 2K - 30K
	    else if ( (request_start < buffer->start_offset) &&
		((request_start + remaining_length) >= buffer->start_offset ) &&
                ((request_start + remaining_length) >
					    (offset + DATA_BUFFER_SIZE)))    {

		requested_object->bm_get_failure = 1;

	    }//partial success: some data present buff: 4K-32K, Req: 2K -40K
	    else if ( (request_start >= buffer->start_offset) &&
		((request_start + remaining_length) >
				    (buffer->start_offset + buffer->length)) &&
		( (request_start < (buffer->start_offset + buffer->length)))) {
		total_dup_bytes_delivered += (buffer->start_offset +
						buffer->length - request_start);
		total_bytes_delivered += (buffer->start_offset +
						buffer->length - request_start);

		requested_object->request_offset = (buffer->start_offset +
								buffer->length);
                requested_object->response_length += (buffer->start_offset +
						buffer->length - request_start);

		remaining_length -= (buffer->start_offset + buffer->length -
								request_start);
		partial_success = 1;
	    } //partial success :some data present buff: 0 -10K , Req: 4K- 32K or 4K-40K
	    else if ((request_start>=(buffer->start_offset + buffer->length)) ||
		( (request_start + remaining_length) < buffer->start_offset)) {

		    requested_object->bm_get_failure = 1;
		    //return requested_object->bm_get_failure;

		    //partial_success = 1;
						//do nothing! 
	    }//partial success :some data present buff: 0K-2K Req : 4K-40K or Req : 4K - 32K




	    if (requested_object->bm_get_failure == 1) {
		//add failure delay.
		 //AO_fetch_and_add(&nkn_cur_ts,bm_get_failure_delay);
		  CE_inc_cur_time(bm_get_failure_delay);
		return requested_object->bm_get_failure;
	    }
	    else {
		//add looping delay:delay that simulates a buffer hit.
		// AO_fetch_and_add(&nkn_cur_ts,bm_get_loop_delay);
		 CE_inc_cur_time(bm_get_loop_delay);



	    }

	    //update buffer parameters and store buffer pointer in obj
	    AO_fetch_and_add1(&(buffer->ref));
	    if ( AO_load(&(buffer->ref)) == 1) {

		TAILQ_REMOVE(&bufferpool_queues[buffer->flist],
		        buffer,bufferpoolentry);
	    }
	    requested_object->data_bufs[requested_object->buffer_fetch_count++]=
									buffer;
	    buffer->use++;

	    attr_buffer_present = g_hash_table_lookup(attr_hash_table,
		requested_object->uri_name);
	    if (attr_buffer_present) {
		//attr_buffer_present->ref++;
		//buffer->attrbuf = attr_buffer_present;
		attr_buffer_present->use++;
		attr_buffer_present->use_ts = AO_load(&nkn_cur_ts);
		requested_object->attrbuff  = attr_buffer_present;
		/*if (attr_buffer_present->ref == 0) {
		    TAILQ_REMOVE(&attrbufferpool_queues[attr_buffer_present->flist],
			    attr_buffer_present,attrbuffpoolentry);
		    attr_buffer_present->ref++;
		}*/
	    }
	    else {
		//assert... attrbuff should always be present
		assert(0);
	    }





	    cache_hit_ratio = (total_dup_bytes_delivered * 100) /
						(total_bytes_delivered);
	    if (partial_success) {
		partial_success = 0;
		return 0;
	    }
	    request_start = offset + DATA_BUFFER_SIZE;



	}
		memset(buff, 0, sizeof(buff));
		memset(uri_name, 0, sizeof(uri_name));
    }


        return 0;
}


/* BM_put : simulates an origin fetch
 */
void BM_put(uri_entry_t *requested_object)
{

    //uri_entry_t *objp;
    nkn_buf_t *buffer = NULL ,*present_buffer = NULL, *attr_buffer_present=NULL;
    char buff[512];
    char *uri_name;
    char *attr_uri_name;
    memset(buff, 0, sizeof(buff));
    uint64_t append_offset = 0;
    uint64_t remaining_length = 0;
    int counter = 1,i = 0;
    append_offset = (requested_object->request_offset / DATA_BUFFER_SIZE)
							    * DATA_BUFFER_SIZE;

    remaining_length = requested_object->request_length;
    requested_object->response_length = 0;

    //if this is the first time an origin fetch is being done for an obj
    //then fetch on one buffer , else fetch origin_fetch_number buffers
    if(requested_object->first_origin_fetch)
	counter = origin_fetch_number;
    else
	requested_object->first_origin_fetch = 1;


    for(i = 0;i < counter; i++) {

	if(requested_object->response_length<requested_object->request_length) {

	    //if (requested_object->cacheable) {


                uri_name = (char*)calloc(MAX_URI_LEN, 1);
                strncpy(uri_name, requested_object->uri_name, (MAX_URI_LEN-1));
							//DATA_BUFFER_SIZE;
		snprintf(buff, sizeof(buff), "%"PRIu64, append_offset);

                strcat(uri_name,",");
                strcat(uri_name,buff);


		present_buffer = g_hash_table_lookup(uri_hash_table,uri_name);


		pthread_mutex_lock(&bufferpool_queue_mutex);
		buffer = getFreeBuffer();
		pthread_mutex_unlock(&bufferpool_queue_mutex);
		//gets the buffer by calculating the best of five pools

		//set the parameters for the buffer
		//buffer->ref++;
		AO_fetch_and_add1(&(buffer->ref));
		assert(AO_load(&(buffer->ref)) == 1);

		buffer->uri_name = uri_name;


		attr_buffer_present = g_hash_table_lookup(attr_hash_table,
		    requested_object->uri_name);
		if (!attr_buffer_present) {
		    //insert the entry in the hash;
		    attr_buffer_present = getFreeAttrBuffer();
		    //int len = strlen(requested_object->uri_name);
		    attr_uri_name = (char*)calloc(MAX_URI_LEN,1);
		    strncpy(attr_uri_name,requested_object->uri_name,
			    (MAX_URI_LEN-1));
		    attr_buffer_present->uri_name = attr_uri_name;

		    g_hash_table_insert(attr_hash_table,
			    attr_uri_name,attr_buffer_present);
		}
		else if (AO_load(&(attr_buffer_present->ref)) == 0)
		{
		    TAILQ_REMOVE(&attrbufferpool_queues[attr_buffer_present->flist],
			attr_buffer_present,attrbuffpoolentry);
		    //if(present_buffer == buffer)
		    //	attr_buffer_present->ref++;
		}

		buffer->attrbuf = attr_buffer_present;
		//buffer->uri_name = uri_name;
		attr_buffer_present->use++;
		attr_buffer_present->use_ts = AO_load(&nkn_cur_ts);
		attr_buffer_present->provider = PROVIDER_ORIGIN;

                merge_content(requested_object, present_buffer, buffer,
			&remaining_length,append_offset);



	    requested_object->data_bufs[requested_object->buffer_fetch_count++]
								    = buffer;


	    requested_object->attrbuff = attr_buffer_present;

		//insert at tail of in use buffer pool since its a new buffer;
	    //TAILQ_INSERT_TAIL(&bufferpool_queues[0],buffer,bufferpoolentry);


		g_hash_table_insert(uri_hash_table, uri_name, buffer);



	    //} //end of if(cacheable
	    memset(buff, 0, sizeof(buff));
	    append_offset += DATA_BUFFER_SIZE;
	    requested_object->request_offset = append_offset;
	    requested_object->response_length = requested_object->request_length
							    - remaining_length;

	    present_buffer = NULL;
	    attr_buffer_present = NULL;
	}

	else {
	    break;
	}

    }

    total_bytes_delivered += requested_object->response_length;

}





//merge_content: simulates the fetch, ie: updates the buffer offsets and then
//simulates the RAM store (BM_put) as it merges content of two buffers if one
//was already present representing the same offset.

void  merge_content(uri_entry_t *requested_object, nkn_buf_t *present_buffer,
	nkn_buf_t *buffer, uint64_t *remaining_length, uint64_t append_offset)
{

    if ((requested_object->request_offset + *remaining_length) >=
	    (append_offset+DATA_BUFFER_SIZE)) {

	buffer->start_offset  = requested_object->request_offset;

        buffer->length = (append_offset+DATA_BUFFER_SIZE) -
				requested_object->request_offset ;
        *remaining_length = *remaining_length - (buffer->length);

    }
    else {

        buffer->start_offset = append_offset;


        buffer->length = (requested_object->request_offset +
				    *remaining_length)-append_offset;

	*remaining_length = 0;


    }



     //pthread_mutex_lock(&current_time_mutex);
     //nkn_cur_ts += om_get_delay + bm_put_delay;
     //AO_fetch_and_add(&nkn_cur_ts,om_get_delay + bm_put_delay);
      CE_inc_cur_time(om_get_delay + bm_put_delay);
     //pthread_mutex_unlock(&current_time_mutex);


    buffer->use++; //if new buffer,use = 1, if already present,
		    //then use = presentuse+ 1;(updated below)



    //total_bytes_in_ram += (buffer->length);






    if (present_buffer) {

	if (present_buffer != buffer) {
	    //it may be the case that the buffer which got evicted to make way
	    //for this buffer is present_buffer!! so no need to calculate again.
	    total_bytes_in_ram -= present_buffer->length;


	    if (present_buffer->start_offset < buffer->start_offset ) {

	buffer->length += (buffer->start_offset - present_buffer->start_offset);
		buffer->start_offset = present_buffer->start_offset;

	    }
	    if ((present_buffer->start_offset + present_buffer->length) >
		    (buffer->start_offset +  buffer->length))
		    buffer->length = (present_buffer->start_offset +
			    present_buffer->length) -  buffer->start_offset;
	    buffer->use = present_buffer->use + 1;

	    //buffer->attrbuf = present_buffer->attrbuf;
	    //if(present_buffer->uri_name)
	    //free(present_buffer->uri_name);
	    //present_buffer->attrbuf,attrbuffpoolentry);
	   // free(present_buffer->uri_name);
	    g_hash_table_remove(uri_hash_table, present_buffer->uri_name);

	    /*
	    free(present_buffer->uri_name);
	    TAILQ_REMOVE(&bufferpool_queues[present_buffer->flist],present_buffer,
		     bufferpoolentry);
	    if (present_buffer->ref > 0) {
		printf("BM_put: ref > 0\n");
	    }else
	    {
		printf("BM_put: ref = 0\n");
	    }
	    makeBufferAvailable(present_buffer);*/

	    //derefbuffer(present_buffer);

	     //for ingestion but still existed. so make it available.
	    release_data_buffer(present_buffer);
	    AO_fetch_and_add1(&((buffer->attrbuf)->ref));
	    (buffer->attrbuf)->buffer_count++;
	}
	else {
	    //if present_buffer == buffer,
	    //that means present buffer was evicted to get buffer.
	    //so attrbuffref was decremented while evicting it.
	    //so increment here.
	    AO_fetch_and_add1(&((buffer->attrbuf)->ref));
	    (buffer->attrbuf)->buffer_count++;
	}

    }
    else {

	//(buffer->attrbuf)->ref++;
	 AO_fetch_and_add1(&((buffer->attrbuf)->ref));
	 (buffer->attrbuf)->buffer_count++;
    }
    total_bytes_in_ram += buffer->length;

}



/*calculateHotnessBucket: function which calculates the hotness bucket to which
 * the buffer should go to amongst the 5 levels of buffer pools
 */

void calculateHotnessBucket(nkn_buf_t *buffer)
{






    int i = 0, insert_flag = 0;
    //int level = 0;
    nkn_buf_t *temp;





    if (buffer->bufsize == DATA_BUFFER_SIZE) {


	//buffer->use++;
	buffer->use_ts = AO_load(&nkn_cur_ts);

	/*level =  buffer->flist; //gives the bucket in which the buffer currently is.
	if (level == (HOTNESS_LEVELS - 1)) {

	    TAILQ_REMOVE(&bufferpool_queues[level],buffer,bufferpoolentry);
	    TAILQ_INSERT_TAIL(&bufferpool_queues[level],buffer,bufferpoolentry);
	    //Remove and TAILQ_INSERT at the end of the last level queue.
	}
	else*/ {
	    for(i = 0; i< HOTNESS_LEVELS - 1; i++) {


		//Remove buffer from current queue it is in
		if (insert_flag == 1) {
		    TAILQ_REMOVE(&bufferpool_queues[buffer->flist], buffer,
			bufferpoolentry);
		    insert_flag = 0;
		}
		if (!TAILQ_EMPTY(&bufferpool_queues[i])) {

		    temp = TAILQ_FIRST(&bufferpool_queues[i]);
		    if (buffer->use > temp->use ) {

			//TAILQ_INSERT in i+1th queue and update level.
			buffer->flist = i + 1;
			TAILQ_INSERT_TAIL(&bufferpool_queues[i+1],buffer,
			    bufferpoolentry);
			insert_flag = 1;
		    }
		    else {

			//TAILQ_INSERT in ith queue and update level.
			buffer->flist = i;
			TAILQ_INSERT_TAIL(&bufferpool_queues[i],buffer,
			    bufferpoolentry);
			break;

		    }
		}
		else {

		    //TAILQ_INSERT in this queue. and update level;
		    buffer->flist = i;
		    TAILQ_INSERT_TAIL(&bufferpool_queues[i],buffer,
			bufferpoolentry);
		    break;

		}


	    }
	}

    }
    else {
	insert_flag = 0;
	//for Attribute buffers.
	assert(buffer->ingest_in_progress == 0);
	/*level =  buffer->flist;
	if (level == (HOTNESS_LEVELS - 1)) {
	    TAILQ_REMOVE(&attrbufferpool_queues[level],buffer,
		    attrbuffpoolentry);
	    TAILQ_INSERT_TAIL(&attrbufferpool_queues[level],buffer,
		    attrbuffpoolentry);
	}*/
	//else 
	{
	    for(i = 0; i< HOTNESS_LEVELS - 1; i++) {

		//Remove buffer from current queue it is in
		if (insert_flag == 1) {
		    TAILQ_REMOVE(&attrbufferpool_queues[buffer->flist],buffer,
			attrbuffpoolentry);
		    insert_flag = 0;
		}
		if (!TAILQ_EMPTY(&attrbufferpool_queues[i])) {
		    temp = TAILQ_FIRST(&attrbufferpool_queues[i]);

		    if (buffer->use > temp->use ) {
			buffer->flist = i + 1;
			TAILQ_INSERT_TAIL(&attrbufferpool_queues[i+1],buffer,
				attrbuffpoolentry);
			insert_flag = 1;
		    }
		    else {
			buffer->flist = i;
			TAILQ_INSERT_TAIL(&attrbufferpool_queues[i],buffer,
				attrbuffpoolentry);
			break;
		    }
		}
		else {

		    buffer->flist = i;
		    TAILQ_INSERT_TAIL(&attrbufferpool_queues[i],buffer,
			    attrbuffpoolentry);
		    break;

		}
	    }


	}


    }



}

/* makeBufferAvailable: used when a particular buffer has been freed or can 
 * be now reused for other entries, inserts the buffer into the lowest list
 * of the buffer pool
 */
void makeBufferAvailable(nkn_buf_t *buffer)
{
    //nkn_uol_t id;           /* identity */

     buffer->uri_name = NULL;//pointer to uri which this buffer represents.
     //buffer->buffer = NULL;
     buffer->use_ts = 0;
     //buffer->flags = 0;
     buffer->type = 0;
     buffer->attrbuf = NULL;

     if(buffer->bufsize == DATA_BUFFER_SIZE) {

	//TAILQ_REMOVE(&bufferpool_queues[buffer->flist], buffer,bufferpoolentry);

	buffer->flist = 0;

	TAILQ_INSERT_TAIL(&bufferpool_queues[buffer->flist],buffer,
		bufferpoolentry);

     }
     else {

	//TAILQ_REMOVE(&attrbufferpool_queues[buffer->flist],buffer,
	//	attrbuffpoolentry);

	buffer->flist = 0;

	TAILQ_INSERT_TAIL(&attrbufferpool_queues[buffer->flist],buffer,
		attrbuffpoolentry);
     }
     //buffer->flist0 = 0;
     //buffer->discard_ts = 0;
     //buffer->bufsize = 0;
     //buffer->discard_cnt = 0;
     buffer->start_offset = 0;
     buffer->end_offset = 0;
     buffer->length = 0;
     //buffer->endhit = 0;
     //buffer->ref = 0;
     AO_store(&(buffer->ref),0);
     buffer->use = 0;
     //buffer->create_ts = 0;
     buffer->last_send = 0;
     buffer->num_hits = 0;
     buffer->provider = 0;
     buffer->hotness = 0;
     buffer->bytes_from_cache = 0;
     buffer->object_ts = 0;
     AO_store(&(buffer->ingest_in_progress),0);
     AO_store(&(buffer->extent_count),0);
     buffer->buffer_count = 0;
}


//unused
/*
void  merge_content(uri_entry_t *requested_object, nkn_buf_t *present_buffer,
	nkn_buf_t *buffer, uint64_t *remaining_length, int append_offset)
{


    int partial_success = 0;
    uint64_t request_start = requested_object->request_offset;

     if (present_buffer) {
	if ( (requested_object->request_offset < present_buffer->start_offset)&&
	   ((requested_object->request_offset + *remaining_length) >
		present_buffer->start_offset)) && 
	    (requested_object->data_in_mid == 1) {

		*remaining_length -= (present_buffer->start_offset -
					    requested_object->request_offset);
		total_bytes_in_ram += (present_buffer->start_offset -
					    requested_object->request_offset);
                buffer->length = (present_buffer->start_offset -
		    requested_object->request_offset)+ present_buffer->length;

		buffer->start_offset  = requested_object->request_offset;
		requested_object->data_in_mid == 0;
		return;
	}
	if ((request_start < present_buffer->start_offset) &&
	   ((request_start + remaining_length) >=present_buffer->start_offset)&&
	   ((request_start + remaining_length) <=
					(append_offset + DATA_BUFFER_SIZE))){

	    total_dup_bytes_delivered += ( (request_start + remaining_length -
						present_buffer->start_offset));
	    //total_bytes_delivered += total_dup_bytes_delivered +
	    //			(present_buffer->start_offset - request_start);


	    buffer->start_offset = request_start;
	    buffer->length = remaining_length;
	    //no need to change requested_object->request_offset
	    requested_object->response_length += total_dup_bytes_delivered +
			    (present_buffer->start_offset - request_start);
	    remaining_length -= total_dup_bytes_delivered +
			    (present_buffer->start_offset - request_start);
	    partial_success = 1;
	    //
	} //partial success: some data present buff: 4K - 32K, Req: 2K - 30K
	else if ( (request_start < present_buffer->start_offset) &&
		( (request_start + remaining_length) >=
					    present_buffer->start_offset ) &&
		( (request_start + remaining_length) >
				        (append_offset + DATA_BUFFER_SIZE))) {
	    total_dup_bytes_delivered += (append_offset + DATA_BUFFER_SIZE -
						present_buffer->start_offset);
	    //total_bytes_delivered += (request_start + remaining_length -
	   //					present_buffer->start_offset);

	    buffer->start_offset = request_start;
	    buffer->length = (append_offset + DATA_BUFFER_SIZE) - request_start;
	    //no need to change requested_object->request_offset
	    requested_object->response_length += total_dup_bytes_delivered +
			    (present_buffer->start_offset - request_start);
	    remaining_length -= total_dup_bytes_delivered +
			    (present_buffer->start_offset - request_start);
	    partial_success = 1;
	}//partial success: some data present buff: 4K - 32K, Req: 2K - 30K
	else if ( (request_start >= present_buffer->start_offset) &&
	    ((request_start + remaining_length) >
		    (present_buffer->start_offset + present_buffer->length)) &&
	    ( (request_start <
		    (present_buffer->start_offset + present_buffer->length)))) {
	    total_dup_bytes_delivered += (present_buffer->start_offset +
					present_buffer->length - request_start);
	    //total_bytes_delivered += (present_buffer->start_offset +
	   //				present_buffer->length - request_start);


	    buffer->start_offset = present_buffer->start_offset;
	    if ( (request_start + *remaining_length) >
		    (append_offset + DATA_BUFFER_SIZE)) {
		buffer->length = (append_offset + DATA_BUFFER_SIZE) -
				present_buffer->start_offset;
	    else
		buffer->length = (request_start + *remaining_length) -
			(present_buffer->start_offset + present_buffer->length);


	    requested_object->request_offset =
			(buffer->start_offset + buffer->length);
	    requested_object->response_length +=
			(buffer->start_offset + buffer->length -
								 request_start);

	    remaining_length -=
		(buffer->start_offset + buffer->length - request_start);
	    partial_success = 1;
	    requested_object->data_in_mid = 1;
	} //partial success:some data present buff: 0 -10K ,Req:4K- 32K or4K-40K

	else if ( (request_start >=
		    (present_buffer->start_offset + present_buffer->length)) ||
		( (request_start + remaining_length) <=
						present_buffer->start_offset)) {

		    partial_success = 0;



	}//partial success:some data present buff:0K-2K Req:4K-40K or Req:4K-32K
    }


    if(!partial_success) {
	if ((requested_object->request_offset + *remaining_length) >=
		            (append_offset+DATA_BUFFER_SIZE)) {

	    buffer->start_offset  = requested_object->request_offset;

	    buffer->length = (append_offset+DATA_BUFFER_SIZE) -
			    requested_object->request_offset ;


	    *remaining_length = *remaining_length - (buffer->length);

	}
	else {

	    buffer->start_offset = append_offset;

	    buffer->length = (requested_object->request_offset +
	    *remaining_length)-append_offset;


	    *remaining_length = 0;


	}

	total_bytes_in_ram += (buffer->length);
    }


    if (present_buffer) {

	if (present_buffer->start_offset < buffer->start_offset )
	    buffer->start_offset = present_buffer->start_offset;
	if ((present_buffer->start_offset + present_buffer->length) >
				    (buffer->start_offset +  buffer->length))
	    buffer->length = (present_buffer->start_offset +
				present_buffer->length) -  buffer->start_offset;
    }







}

*/


/*getFreeAttrBuffer: gets a free attribute buffer for use
 also handles eviction in case the new buffer to be allocated was in use.
 */

nkn_buf_t *getFreeAttrBuffer()
{

    int index = 0,i;
    int64_t worst_cost,temp_cost;
    nkn_buf_t * buf = NULL, *temp;

    /*
    if(!TAILQ_EMPTY(&attrbufferpool_queues[0])) {

	buf = TAILQ_FIRST(&attrbufferpool_queues[0]);
	TAILQ_REMOVE(&attrbufferpool_queues[0],buf,attrbuffpoolentry);

    }

    if( buf == NULL ) {
	printf("Error in getting the attr buffer \n");
	return 0;
    }
    else
	return buf;

    */
    for(i = 0; i< HOTNESS_LEVELS; i++) {

	if(!TAILQ_EMPTY(&attrbufferpool_queues[i])) {
	    buf = TAILQ_FIRST(&attrbufferpool_queues[i]);
	    index = i;
	    break;
	}
    }

    //if (buf == NULL)
    assert(buf);
    //	printf("Error: in getfreeAttrbuffer,There should be atleast 1 free\n");


    worst_cost = (buf->use * LRUSCALE) -
	            ((AO_load(&nkn_cur_ts) - buf->use_ts)/DELAY_GRANULARITY);

    for(i = 0; i< HOTNESS_LEVELS; i++) {

	if(!TAILQ_EMPTY(&attrbufferpool_queues[i])) {

	    temp = TAILQ_FIRST(&attrbufferpool_queues[i]);
	    temp_cost = (temp->use * LRUSCALE) -
		((AO_load(&nkn_cur_ts) - temp->use_ts)/DELAY_GRANULARITY);
	    if (temp_cost < worst_cost) {
		worst_cost = temp_cost;
		index = i;

	    }


	}

    }


    buf = TAILQ_FIRST(&attrbufferpool_queues[index]);
    TAILQ_REMOVE(&attrbufferpool_queues[index],buf,attrbuffpoolentry);

    assert(buf->ref == 0);

    if (buf->uri_name != NULL) {
	//if the buffer is in use, evict it!
	 g_hash_table_remove(attr_hash_table,buf->uri_name);


	free(buf->uri_name);
	makeBufferAvailable(buf);
	TAILQ_REMOVE(&attrbufferpool_queues[buf->flist],buf,attrbuffpoolentry);
    }
    return buf;

}



/*getFreeBuffer: gets a free attribute buffer for use
 * also handles eviction in case the new buffer to be allocated was in use
*/
nkn_buf_t * getFreeBuffer()
{
	int index = 0,i;
	int64_t worst_cost,temp_cost;
	nkn_buf_t * buf = NULL, *temp =NULL;


    //=====================================================================//

    //get the first buffer available from the hotness buckets
    for(i = 0; i< HOTNESS_LEVELS; i++) {

	if(!TAILQ_EMPTY(&bufferpool_queues[i])) {
	    buf = TAILQ_FIRST(&bufferpool_queues[i]);
	    index = i;
	    break;
	}
    }

    assert(buf); //buf shouldnt be NULL;

    worst_cost = (buf->use * LRUSCALE) -
	((AO_load(&nkn_cur_ts) - buf->use_ts)/DELAY_GRANULARITY);
     for(i = 0; i< HOTNESS_LEVELS; i++) {

	 if (!TAILQ_EMPTY(&bufferpool_queues[i])) {

	    temp = TAILQ_FIRST(&bufferpool_queues[i]);
	    temp_cost = (temp->use * LRUSCALE) -
		((AO_load(&nkn_cur_ts) - temp->use_ts)/DELAY_GRANULARITY);
	    if (temp_cost < worst_cost) {
		worst_cost = temp_cost;
		index = i;

	    }


	}
     }

    buf = NULL;
    buf = TAILQ_FIRST(&bufferpool_queues[index]);
    TAILQ_REMOVE(&bufferpool_queues[index],buf,bufferpoolentry);

    assert(buf->ref == 0);

    if (buf->uri_name != NULL) {
	//if the buffer is in use, evict it!
	g_hash_table_remove(uri_hash_table,buf->uri_name);
	total_bytes_in_ram -= buf->length;

	//dec refcount for buf->attribute buffer since evicting;;
	AO_fetch_and_sub1(&((buf->attrbuf)->ref));
	(buf->attrbuf)->buffer_count--;

	//if((buf->attrbuf)->ref < 0)
	assert((AO_load(&((buf->attrbuf)->ref)) >= 0));
	//printf("attrib buff ref count < 0\n");
	assert((AO_load(&((buf->attrbuf)->ref))) <1800000);

	free(buf->uri_name);

        if((AO_load(&((buf->attrbuf)->ref))) == 0) {

	    calculateHotnessBucket(buf->attrbuf);

	}
	//derefbuffer(buf->attrbuf);
	makeBufferAvailable(buf);
	TAILQ_REMOVE(&bufferpool_queues[buf->flist],buf,bufferpoolentry);
    }

    return buf;

}

void deref_fetched_buffers(uri_entry_t *requested_object)
{

        int i = 0;
        for(i =0; i<requested_object->buffer_fetch_count; i++) {


            if(requested_object->data_bufs[i]) {
		derefbuffer(requested_object->data_bufs[i]);
	    }


        }

	//AO_fetch_and_add(&nkn_cur_ts,bm_get_fetch_delay);
	CE_inc_cur_time(bm_get_fetch_delay);
	requested_object->buffer_fetch_count = 0;

}


//unused
/*
void BM_main(void *arg)
{

	printf(" BM THread spawned \n");
	uri_entry_t *requested_object;
	while(1) {

		
		if (bm_stop_processing && bm_get_queue.tqh_first == NULL) {

                        pthread_exit(0);

                }
	
		
		if (bm_get_queue.tqh_first != NULL) {
			pthread_mutex_lock(&bm_queue_mutex);
			uri_entry_t * requested_object = bm_get_queue.tqh_first;
			TAILQ_REMOVE(&bm_get_queue, bm_get_queue.tqh_first, bm_get_queue_entries);
			pthread_mutex_unlock(&bm_queue_mutex);
		

			BM_put(requested_object);
		}
	}
	
	//pthread_exit(0);

}*/



/* get_buffers_from_BM: function used to get buffers to be used for reading data
 * off disks via DM_get
 */
void get_buffers_from_BM(uint64_t buffers_required,
	uri_entry_t *requested_object, nkn_buf_t *buffers[MAX_BUFFERS_FETCH_DM])
{
    nkn_buf_t *buffer = NULL, *attr_buffer_present = NULL;
    char *attr_uri_name = NULL;
    int counter = 0;
    //uint64_t append_offset = 0;
    //char buff[512];
    //memset(buff, 0, sizeof(buff));

    //append_offset = (requested_object->request_offset / DATA_BUFFER_SIZE) *
   //							DATA_BUFFER_SIZE;




    attr_buffer_present = g_hash_table_lookup(attr_hash_table,
					requested_object->uri_name);
    if (!attr_buffer_present) {
	//insert the entry in the hash;
	attr_buffer_present = getFreeAttrBuffer();
	//int len = strlen(requested_object->uri_name);
	attr_uri_name = (char*)calloc(MAX_URI_LEN,1);
	strncpy(attr_uri_name,requested_object->uri_name,
	                                             (MAX_URI_LEN-1));
	attr_buffer_present->uri_name = attr_uri_name;
	g_hash_table_insert(attr_hash_table,
	                             attr_uri_name,attr_buffer_present);
    }
    else if (AO_load(&(attr_buffer_present->ref)) == 0)
    {
       TAILQ_REMOVE(&attrbufferpool_queues[attr_buffer_present->flist],
                                     attr_buffer_present,attrbuffpoolentry);
       //if(present_buffer == buffer)
       //  attr_buffer_present->ref++;
       //                                                                  
    }
    requested_object->attrbuff = attr_buffer_present;
    while(buffers_required != 0) {

	pthread_mutex_lock(&bufferpool_queue_mutex);
	buffer = getFreeBuffer();
	pthread_mutex_unlock(&bufferpool_queue_mutex);
	assert(AO_load(&(buffer->ref))== 0);
	buffer->attrbuf = attr_buffer_present;
	buffers[counter] = buffer;
	buffers[counter]->use++;


	attr_buffer_present->use++;
	attr_buffer_present->use_ts = AO_load(&nkn_cur_ts);
	attr_buffer_present->provider = PROVIDER_DM;
	//append_offset += DATA_BUFFER_SIZE;
	buffers_required--;
	counter++;
    }



}


/* return_buffers_to_client: used to simulate the communication after a DM_get
 * to send buffers back to the client.
 * Also stores the buffers into the cache. (only if the buffer was not 
 * previously present in the cache
 */
void return_buffers_to_client(uint64_t buffers_required,
    uri_entry_t *requested_object, nkn_buf_t *buffers[MAX_BUFFERS_FETCH_DM])
{

    uint64_t counter = 0;
    nkn_buf_t *present_buffer = NULL;

    while(counter != buffers_required)
    {

	//total_dup_bytes_delivered += (buffers[counter])->length;
	//total_bytes_delivered += (buffers[counter])->length;
	present_buffer = g_hash_table_lookup(uri_hash_table,
		(buffers[counter])->uri_name);

	if (present_buffer == NULL) {

	    //this is a new buffer
	    //insert into the hash table
	    //insert into requested_obj->databufs
	    //inc ref count
	    //inc attrbuff refcount
	    //inc total bytes in ram
	    g_hash_table_insert(uri_hash_table, buffers[counter]->uri_name,
					 buffers[counter]);

	    requested_object->data_bufs[requested_object->buffer_fetch_count++]=
							    buffers[counter];
	    AO_fetch_and_add1(&((buffers[counter])->ref));
	    assert(AO_load(&((buffers[counter])->ref)) == 1);
	    ((buffers[counter])->attrbuf)->buffer_count++;
	    AO_fetch_and_add1(&(((buffers[counter])->attrbuf)->ref));

	    total_bytes_in_ram += (buffers[counter])->length;

	}
	else {
	    free((buffers[counter])->uri_name);
	    assert(AO_load(&(buffers[counter]->ref)) == 0);
	    makeBufferAvailable(buffers[counter]);

	}
	counter++;
    }





}


/*  init_free_pool: initializes the buffer pool for use
 */
void init_free_pool()
{

    nkn_buf_t *databuf = NULL;
    nkn_buf_t *attrbuf = NULL;
    int i = 0;
    number_of_buffers =((ram_size)/((DATA_BUFFER_SIZE)+(1.2*ATTR_BUFFER_SIZE)));
    number_of_attr_buffers = number_of_buffers * 1.2;
    TAILQ_INIT(&free_databuf_queue);
    TAILQ_INIT(&free_attrbuf_queue);

    for( i = 0; i < HOTNESS_LEVELS; i++) {

        TAILQ_INIT(&bufferpool_queues[i]);
	TAILQ_INIT(&attrbufferpool_queues[i]);

    }

    for(i = 0; i < number_of_buffers ; i++) {

	databuf = (nkn_buf_t *) calloc(sizeof(nkn_buf_t),1);
	//attrbuf = (nkn_buf_t *) calloc(sizeof(nkn_buf_t),1);

	databuf->bufsize = DATA_BUFFER_SIZE;
	//attrbuf->bufsize = ATTR_BUFFER_SIZE;
	//buffs[i] = databuf;
	//attrbuffs[i]=attrbuf;
	//databuf->uri_name = NULL;
	//attrbuf->uri_name = NULL;
	TAILQ_INSERT_TAIL(&free_databuf_queue,databuf,entries);
	//TAILQ_INSERT_TAIL(&free_attrbuf_queue,attrbuf,attrentries);

	TAILQ_INSERT_TAIL(&bufferpool_queues[0],databuf,bufferpoolentry);
	//TAILQ_INSERT_TAIL(&attrbufferpool_queues[0],attrbuf,attrbuffpoolentry);
    }


    for(i = 0; i < number_of_attr_buffers ; i++) {

	attrbuf = (nkn_buf_t *) calloc(sizeof(nkn_buf_t),1);
	attrbuf->bufsize = ATTR_BUFFER_SIZE;
	//attrbuffs[i] = attrbuf;


	TAILQ_INSERT_TAIL(&free_attrbuf_queue,attrbuf,attrentries);
	TAILQ_INSERT_TAIL(&attrbufferpool_queues[0],attrbuf,attrbuffpoolentry);
    }


    max_parallel_ingests = (0.1 * number_of_buffers)/64;
    max_buffers_hold = 0.1 * number_of_buffers;

}


int BM_init(BM_data* register_module)
{
	//TAILQ_INIT(&sched_queue);
	//bm_stop_processing = 0;
    //fp1 = fopen("check2","w");
    init_free_pool();

    register_module->BM_get = BM_get;
    register_module->BM_put = BM_put;
    register_module->BM_cleanup = BM_cleanup;
    register_module->deref_fetched_buffers = deref_fetched_buffers;
    register_module->get_buffers_from_BM = get_buffers_from_BM;
    register_module->return_buffers_to_client = return_buffers_to_client;
    uri_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
    attr_hash_table = g_hash_table_new(g_str_hash, g_str_equal);

    bm_stop_processing = 0;
    printf("BM initialisation done........with number of buffers = %ld, attr_buffers = %ld, ramsize %ld \n",number_of_buffers,number_of_attr_buffers,ram_size);
    return 0;

}

void BM_cleanup()
{
    nkn_buf_t *databuf = NULL, *attrbuf = NULL;
    //nkn_buf_t *attrbuf = NULL;
    int i = 0, j = 0;
    uint64_t buffers_freed = 0;
    uint64_t attr_buffers_freed = 0;

    /* //TEST_CODE
    uint64_t bufferstest = 0;
    nkn_buf_t *temp = NULL, *temp1 = NULL;
    GHashTable *test = g_hash_table_new(g_str_hash, g_str_equal);
    for( temp = TAILQ_FIRST(&free_databuf_queue); temp!=NULL;temp=TAILQ_NEXT(temp,entries)) {

	temp1 = g_hash_table_lookup(test, temp->uri_name);
	if(temp1 == NULL) {
	    g_hash_table_insert(test,temp,temp->uri_name);

	}
	else {
	    assert(0);
	}

	bufferstest++;
    }
    assert(bufferstest == number_of_buffers);
    */
    for( i = 0; i < HOTNESS_LEVELS; i++) {


	for(j = 0; j < number_of_buffers ; j++) {

		if (!TAILQ_EMPTY(&bufferpool_queues[i])) {

		    databuf = TAILQ_FIRST(&bufferpool_queues[i]);
		    buffers_freed++;
		    TAILQ_REMOVE(&bufferpool_queues[i],databuf,bufferpoolentry);
		    if (databuf->uri_name != NULL) {

			(databuf->attrbuf)->buffer_count--;
			derefbuffer(databuf->attrbuf);
			(databuf->attrbuf)->use = 0;
			g_hash_table_remove(uri_hash_table,databuf->uri_name);
			free(databuf->uri_name);
		    }

		    free(databuf);

		}
		else
		    break;

	}
    }
    //if this assert fails, probably there was some problem with 
    //releasing the buffers somewhere since all the buffers should be in 
    //the buffer pool at the end time.
    assert(buffers_freed == number_of_buffers);

     for( i = 0; i < HOTNESS_LEVELS; i++) {


	for(j = 0; j < number_of_attr_buffers ; j++) {

	    if (!TAILQ_EMPTY(&attrbufferpool_queues[i])) {

		attrbuf = TAILQ_FIRST(&attrbufferpool_queues[i]);
		attr_buffers_freed++;
		assert(attrbuf->buffer_count == 0);
		TAILQ_REMOVE(&attrbufferpool_queues[i], attrbuf,
			attrbuffpoolentry);
//		printf("%p\n", attrbuf);

		if (attrbuf->uri_name) {
		    g_hash_table_remove(attr_hash_table, attrbuf->uri_name);
		    free(attrbuf->uri_name);
		}
		free(attrbuf);


	    }
	    else
		break;
	}
    }

    assert(attr_buffers_freed == number_of_attr_buffers);
    //bm_stop_processing = 1;
}

/*void remove_entry(gpointer key, gpointer value, gpointer userdata)
{

    if(key != NULL)
	free(key);
    g_hash_table_remove(uri_hash_table,key);

}
*
*/

//unused
void empty_cache()
{
    //this function may be causing memory leaks
    //
    //
    //
    //

    /*
    for( i = 0; i < HOTNESS_LEVELS; i++) {

	for(j = 0; j < number_of_buffers ; j++) {
		if (!TAILQ_EMPTY(&bufferpool_queues[i])) {

		    databuf = TAILQ_FIRST(&bufferpool_queues[i]);
                    TAILQ_REMOVE(&bufferpool_queues[i],databuf,bufferpoolentry);

                     if (databuf->uri_name != NULL) {

			 AO_fetch_and_sub1(&((databuf->attrbuf)->ref));
			 assert(AO_load(&((databuf->attrbuf)->ref)) >= 0);
			 if (AO_load(&((databuf->attrbuf)->ref)) == 0) {
			     (databuf->attrbuf)->use = 0;
			     calculateHotnessBucket((databuf->attrbuf));
			 }
			 g_hash_table_remove(uri_hash_table,databuf->uri_name);
			 free(databuf->uri_name);
		     }
		     makeBufferAvailable(databuf);
		}
		else
		    break;
	    }
    }*/

    //g_hash_table_foreach(uri_hash_table,remove_entry,NULL);

    //g_hash_table_foreach(attr_hash_table,remove_entry,NULL);

    BM_cleanup();
    init_free_pool();

}

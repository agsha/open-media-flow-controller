/* 
 * Copyright 2010 Juniper Networks, Inc
 *
 * This file contains code which implements the Disk manager
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
#include "nkn_dm2_api.h"



uint64_t block_size;
uint64_t number_of_blocks;
uint64_t number_of_free_blocks;



/* DM_put: simulates a disk write 
 */
void DM_put(DM_put_data_t * put_data)
{
    uint64_t remaining_length = 0;
    //DM_stat_resp_t *stat_resp = NULL;
    int tier_start_index = 0;
    int tier_end_index = 0;
    int i = 0;
    uint64_t max_free_blocks = 0;
    int disk_index = 0;
    uri_head_t *object = NULL;
    dm_extent_t *extent = NULL;
    void *ptr = NULL;
	//GList * iterator = NULL;

    /*stat_resp = (DM_stat_resp_t *)calloc(sizeof(DM_stat_resp_t),1);
    //copy details from put data to the stat_resp struct
    copy_to_stat(stat_resp, put_data);
    DM_stat(stat_resp);
    if (!(stat_resp->failure)) {

	//success
	free(stat_resp);
	return;

    }
    free(stat_resp);*/
    remaining_length = put_data->length;
    if (put_data->tier_type == TYPE_SSD ) {

	tier_start_index = 0;
	tier_end_index = number_of_SSDs;
    }
    else if (put_data->tier_type == TYPE_SAS) {

	tier_start_index = number_of_SSDs;
	tier_end_index = number_of_SSDs + number_of_SASs;
    }
    else {
	tier_start_index = number_of_SSDs + number_of_SASs;
	tier_end_index = number_of_disks;
    }

    for(i = tier_start_index;i < tier_end_index; i++)
    {

        if (disks[i]->number_of_free_blocks > max_free_blocks) {
	   disk_index = i;
	    max_free_blocks = disks[i]->number_of_free_blocks;
	}


    }


    ptr = my_calloc(sizeof(dm_extent_t), "calloc:nkn_dm2.c:134\0");
    //extent = (dm_extent_t *)calloc(sizeof(dm_extent_t),1);
    extent = (dm_extent_t *)ptr;
    extent->start_offset = put_data->request_offset;
    extent->length = put_data->length;
    extent->physid = disk_index;
    //assert(extent->length == disks[disk_index]->block_size);

    //extent length should be equal to block size of the particular
    //tier.
    //
    pthread_mutex_lock(&disklocks[disk_index]);
    object = g_hash_table_lookup(disktables[disk_index], put_data->uri_name);

    if (object == NULL ) {
	object = (uri_head_t *)calloc(sizeof(uri_head_t),1);
	object->uri_extent_list = NULL;
	strncpy(object->uri_name, put_data->uri_name,
	(MAX_URI_LEN-1));

	object->hotness = put_data->hotness;
	object->bucket_number =
	dm2_get_hotness_bucket(put_data->decoded_hotness);
	TAILQ_INSERT_TAIL(&(disks[disk_index]->buckets[object->bucket_number]),
	    object, bucket_entry);
	AO_fetch_and_add1(&block_counter[disk_index][object->bucket_number]);
	object->uri_extent_list =g_list_append(object->uri_extent_list, extent);
	g_hash_table_insert(disktables[disk_index],object->uri_name,object);

    }
    else
    {
	object->uri_extent_list =
	    g_list_append(object->uri_extent_list, extent);
    }
    pthread_mutex_unlock(&disklocks[disk_index]);
    AO_fetch_and_sub1(&(disks[disk_index]->number_of_free_blocks));
    AO_fetch_and_add(&total_bytes_in_disks[disk_index],put_data->length);
    AO_store(&(disks[disk_index]->use_percent),
	    (total_bytes_in_disks[disk_index] * 100) /
		disks[disk_index]->disk_size);
	//write into disk with index disk_index;
	    //disktables[i] , hashtable i corresponds ith disk

	    //g_hash_table_insert
	    //& allocate new extent., add it to linked list for uri_head.

	//}
    //}



}


void copy_to_stat(DM_stat_resp_t * stat_resp, DM_put_data_t * put_data)
{

    strncpy(stat_resp->uri_name, put_data->uri_name,
        strlen(put_data->uri_name));
    stat_resp->request_offset = put_data->request_offset;
    stat_resp->request_length = put_data->length;

}



/* DM_get: simulates a disk read
 */
void DM_get(DM_get_resp_t * get_resp)
{



    char *uri_name = NULL;
    uint64_t append_offset = 0;
    char buff[512];
    int resp_length = 0;
    int counter = 0;
    uri_head_t * object = NULL;
    int lookup_failure = 0;
    GList* iterator = NULL;
    memset(buff, 0, sizeof(buff));
    //append_offset = ((get_resp->request_offset /
//	    disks[get_resp->physid]->block_size) *
//		disks[get_resp->physid]->block_size);

    append_offset = ((get_resp->request_offset /
		DATA_BUFFER_SIZE) * DATA_BUFFER_SIZE);
    pthread_mutex_lock(&disklocks[get_resp->physid]);
    object=g_hash_table_lookup(disktables[get_resp->physid],get_resp->uri_name);
    if (object == NULL) {

	lookup_failure = 1;
    }
    else {

	iterator = g_list_first(object->uri_extent_list);
	while(iterator != NULL) {

	    if ((get_resp->request_offset >=
		    ((dm_extent_t*)(iterator->data))->start_offset) &&
			((get_resp->request_offset+get_resp->length)<=
			    (((dm_extent_t*)(iterator->data))->start_offset +
				((dm_extent_t*)(iterator->data))->length))) {

		lookup_failure = 0;
		break;

	    }
	    else {

		lookup_failure = 1;
	    }
	    iterator = g_list_next(iterator);
	}

    }

    if (lookup_failure == 0) {
	while(resp_length != get_resp->request_length)
	{
	    uri_name = (char*)calloc(MAX_URI_LEN,1);
	    strncpy(uri_name, get_resp->uri_name,(MAX_URI_LEN-1));
	    snprintf(buff, sizeof(buff), "%"PRIu64, append_offset);
	    strcat(uri_name,",");
	    strcat(uri_name,buff);

	    (get_resp->buffers[counter])->uri_name = uri_name;
	    (get_resp->buffers[counter])->attrbuf = get_resp->attrbuff;
	    (get_resp->attrbuff)->hotness = object->hotness;
	    (get_resp->attrbuff)->provider = PROVIDER_DM;

	    memset(buff, 0, sizeof(buff));

	    (get_resp->buffers[counter])->start_offset = append_offset;

	    (get_resp->buffers[counter])->end_offset = append_offset +
							    DATA_BUFFER_SIZE;

	    if (((get_resp->buffers[counter])->end_offset) >
		    (get_resp->request_offset + get_resp->request_length)) {

		(get_resp->buffers[counter])->end_offset =
		((get_resp->request_offset + get_resp->request_length));
	    }
	    (get_resp->buffers[counter])->use_ts = AO_load(&nkn_cur_ts);
	    (get_resp->buffers[counter])->length =
	    (get_resp->buffers[counter])->end_offset -
			(get_resp->buffers[counter])->start_offset;


	    append_offset += DATA_BUFFER_SIZE;

	    resp_length += (get_resp->buffers[counter])->length;
	    counter++;
	}
	get_resp->failure = 0;
	total_bytes_from_disks[get_resp->physid] += get_resp->request_length;
    }
    else {

	get_resp->failure = 1;
    }
    pthread_mutex_unlock(&disklocks[get_resp->physid]);

}
/* DM_stat : simulates a disk lookup
 */
void DM_stat(DM_stat_resp_t * stat_resp)
{
    int i  = 0, index = 0;
    uri_head_t * object = NULL;
    GList * iterator = NULL;

    for(i = 0; i < number_of_disks; i++) {
	//tiered lookup...1st SSDs,the SASs, then SATAs
	//do a lookup on each of the hash tables
	//if found, go ahead to check whether requested data is present or not
	//if yes, return physid/details of disk having data.
	//if lookup fails or data not present , return NULL;
	//NULL return = do a DM_put.
	pthread_mutex_lock(&disklocks[i]);
	object = g_hash_table_lookup(disktables[i], stat_resp->uri_name);
	pthread_mutex_unlock(&disklocks[i]);
	if (object != NULL) {
	    //find if requested data is present or not.
	    //if yes, return physid/details of disk having data.
	    //else set failure = 1
	    pthread_mutex_lock(&disklocks[i]);
	    iterator = g_list_first(object->uri_extent_list);
	    while(iterator != NULL) {

		index = i;
		//for(extent = g_list_first(object->uri_extent_list); extent !=NULL ;
	         //= g_list_next(iterator)) {
	        //check if requiresd data is within extent offset.if yes return the
	        //physid of the disk containing the data.
		//and set failure = 0;
		if (stat_resp->request_offset >= ((dm_extent_t*)(iterator->data))->start_offset &&
		    ((stat_resp->request_offset + stat_resp->request_length)<=
			(((dm_extent_t*)(iterator->data))->start_offset + ((dm_extent_t*)(iterator->data))->length))) {

		    stat_resp->physid = i;
		    stat_resp->block_size = disks[i]->block_size;
		    stat_resp->failure = 0;
		    break;
		}
		else {
		    stat_resp->failure = 1;
		}
		iterator = g_list_next(iterator);
	    }
	    pthread_mutex_unlock(&disklocks[i]);
	    if (stat_resp->failure == 0) {
		break;
	    }

	}
    }
    if (object == NULL)
	stat_resp->failure = 1;

}

/* attribute update: updates the hotness and the bucket the dm_object is in
 */
void DM_attribute_update(attr_update_transfer_t *obj)
{
    int i = 0;
    uri_head_t *object = NULL;

    for( i = 0; i < number_of_disks; i++)
    {

	pthread_mutex_lock(&disklocks[i]);
        object = g_hash_table_lookup(disktables[i], obj->uri_name);
	if (object != NULL)
	{

	    object->hotness = (obj->hotness);
	    TAILQ_REMOVE(&(disks[i]->buckets[object->bucket_number]),
		    object, bucket_entry);
	    AO_fetch_and_sub1(&block_counter[i][object->bucket_number]);
	    object->bucket_number =
		dm2_get_hotness_bucket(obj->decoded_hotness);
	    TAILQ_INSERT_TAIL(&(disks[i]->buckets[object->bucket_number]),
		object, bucket_entry);
	    AO_fetch_and_add1(&block_counter[i][object->bucket_number]);

	}
	pthread_mutex_unlock(&disklocks[i]);
    }



}

/* 'dm2_get_hotness_bucket' helps find a bucket for an object based
 *  on its hotness.
 */

static int
dm2_get_hotness_bucket(int hotness)
{

    int bkt = 0;

    if (hotness < 0)
	assert(0);

    /* The following are the hotness to evict bucket mappings
    * 0-128 -> Individual Buckets
    * 129-255 -> 129
    * 256-511 -> 130 and so on.
    */
     if (hotness <= DM2_UNIQ_EVICT_BKTS)
	return hotness;

     while(hotness >>= 1) /* find MSB! */
	++bkt;

      /* Map the hotness to the corresponding bucket */
      bkt = (bkt + DM2_UNIQ_EVICT_BKTS - DM2_MAP_UNIQ_BKTS + 1);

      return bkt;

}   /* dm2_get_hotness_bucket */



void DM_cleanup()
{
    int i = 0;
    for(i = 0;i< number_of_disks;i++) {

	g_hash_table_foreach(disktables[i], cleantable, &i);
	free(disks[i]);


    }



}

void cleantable(gpointer key, gpointer value, gpointer userdata)
{
    uri_head_t *object = (uri_head_t*)key;
    int *disk_index = (int *)userdata;
    GList *iterator = NULL;
    dm_extent_t *extent = NULL;
    g_hash_table_remove(disktables[*disk_index],object->uri_name);
    while(g_list_length(object->uri_extent_list)!=0) {

	iterator = g_list_first(object->uri_extent_list);
	extent = ((dm_extent_t *)(iterator->data));

	object->uri_extent_list =
            g_list_remove_link(object->uri_extent_list,iterator);
	free(extent);
    }
    free(object);

}

void init_dm_structures()
{
    int i = 0, j = 0;
    if (small_block == 1) {

	block_size = ssd_block_size; //256 Kb block size

    }
    else {

	block_size = sata_block_size; //2Mb block size

    }
    //number_of_blocks = disk_size / block_size;
    //number_of_free_blocks = number_of_blocks;


    for(i = 0;i< number_of_disks;i++) {

	disktables[i] = g_hash_table_new(g_str_hash, g_str_equal);

	disks[i] = (disk_t *)calloc(sizeof(disk_t),1);
	for(j = 0; j < DM2_MAX_EVICT_BUCKETS; j++)
	{
	    TAILQ_INIT(&(disks[i]->buckets[j]));

	}
	if (i < number_of_SSDs) {
	    disks[i]->disk_type = TYPE_SSD;
	    disks[i]->block_size = ssd_block_size;
	    disks[i]->disk_size = ssd_disk_size;
	    disks[i]->number_of_blocks = ssd_disk_size / disks[i]->block_size;
	    AO_store(&(disks[i]->number_of_free_blocks),disks[i]->number_of_blocks);
	}
	else if (i >= number_of_SSDs && (i < number_of_SSDs + number_of_SASs)) {
	    disks[i]->disk_type = TYPE_SAS;
	    disks[i]->block_size = sas_block_size;
	    disks[i]->disk_size = sas_disk_size;
	    disks[i]->number_of_blocks = sas_disk_size / disks[i]->block_size;
	    AO_store(&(disks[i]->number_of_free_blocks),disks[i]->number_of_blocks);
	}
	else {
	    disks[i]->disk_type = TYPE_SATA;
	    disks[i]->block_size = block_size;
	    disks[i]->disk_size = sata_disk_size;
	    disks[i]->number_of_blocks = sata_disk_size / disks[i]->block_size;
	    AO_store(&(disks[i]->number_of_free_blocks),disks[i]->number_of_blocks);


	}

    }

}


/* The eviction thread!!!
 */
void DM_evict(void*arg)
{
    int i = 1;
    int total_use = 0;
    int tier_start_index = 0;
    int tier_end_index = 0;
    uint64_t number_of_blocks = 0; //number of blocks to evict
    pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
    //uint64_t blocks_in_lowest_bucket = 0;
    //uint64_t deleted_blocks = 0;
    //int bkt_number = 0;
    //uint64_t blocks_per_disk = 0;
    //uri_head_t *object = NULL;
    //GList *iterator;
    //int bucket = 0;
    //dm_extent_t *extent = NULL;
    printf("DM_eviction spawned \n");

    while(1) {

	if(ingestion_thread_done == 1) {

	    printf("DM_eviction exiting... \n");
	    DM_cleanup();
	    return;
	}
	for(i = 1;i<= 3;i++) {

	    //calculate total use for ith tier;
	    if( i == 1)
	    {
		if (number_of_SSDs != 0) {

		    total_use = tier_total_use(i);

		    if( total_use >= ssd_high_mark) {

			tier_start_index = 0;
			tier_end_index = number_of_SSDs;
			number_of_blocks =((total_use - ssd_low_mark)*
							number_of_SSDs);
		        evict_data(tier_start_index, tier_end_index,
			    number_of_blocks);
		    }
		}
	    }
	    else if(i == 2) {

		if (number_of_SASs !=0) {
		    total_use = tier_total_use(i);
		    if( total_use >= sas_high_mark) {

			tier_start_index = number_of_SSDs;
			tier_end_index = number_of_SSDs + number_of_SASs;
			number_of_blocks =((total_use - sas_low_mark)*
			    number_of_SASs);

			evict_data(tier_start_index, tier_end_index,
			number_of_blocks);


		    }
		}
	    }
	    else if(i == 3) {

		if (number_of_SATAs != 0) {
		    total_use = tier_total_use(i);
		    if( total_use >= sata_high_mark) {

			tier_start_index = number_of_SSDs + number_of_SASs;
			tier_end_index = number_of_disks;
			number_of_blocks =((total_use - sata_low_mark)*
			    number_of_SATAs *
			    disks[tier_start_index]->number_of_blocks)/ 100;
			evict_data(tier_start_index, tier_end_index,
			    number_of_blocks);
		    }
		}
	    }
	    i++;
	}
    }
}


//void DM_evict(void* arg)
void evict_data(int tier_start_index, int tier_end_index,
	uint64_t number_of_blocks)
{
    int j = 0;
    //int total_use = 0;
    //int tier_start_index = 0;
    //int tier_end_index = 0;
    //uint64_t number_of_blocks = 0; //number of blocks to evict
    uint64_t blocks_in_lowest_bucket = 0;
    uint64_t deleted_blocks = 0;
    int bkt_number = 0;
    uint64_t blocks_per_disk = 0;
    uri_head_t *object = NULL;
    GList *iterator;
    int bucket = 0;
    dm_extent_t *extent = NULL;

    while (deleted_blocks < number_of_blocks) {

	while (blocks_in_lowest_bucket == 0) {

	    for(j = tier_start_index;j < tier_end_index; j++)
	    {

		blocks_in_lowest_bucket +=
		AO_load(&block_counter[j][bkt_number]);
	    }
	    /*printf(" no. of blocks %ld , bkt %d, blks in bkt %ld \n", number_of_blocks,
		    bkt_number, blocks_in_lowest_bucket); */
	    bkt_number++;
	    assert(bkt_number <= (DM2_MAX_EVICT_BUCKETS + 1));
	}
	assert(blocks_in_lowest_bucket);

	if (number_of_blocks < blocks_in_lowest_bucket) {

	    blocks_per_disk = number_of_blocks /
		(tier_end_index - tier_start_index);

	    for(j = tier_start_index;j < tier_end_index; j++)
	    {
		bucket = bkt_number - 1;
		pthread_mutex_lock(&disklocks[j]);
		while (deleted_blocks != blocks_per_disk) {

		    if(!TAILQ_EMPTY(&disks[j]->buckets[bucket])) {
			object = TAILQ_FIRST(&disks[j]->buckets[bucket]);

			iterator = g_list_first(object->uri_extent_list);

			extent = ((dm_extent_t *)(iterator->data));

			object->uri_extent_list =
			   g_list_remove_link(object->uri_extent_list,iterator);

			AO_store(&total_bytes_in_disks[j],
			    (AO_load(&total_bytes_in_disks[j])-extent->length));
			free(extent);
			AO_fetch_and_add1(&(disks[j]->number_of_free_blocks));
			if (g_list_length(object->uri_extent_list)==0) {

			    TAILQ_REMOVE(&disks[j]->buckets[bucket],
					object, bucket_entry);
			    g_hash_table_remove(disktables[j],object->uri_name);
			    AO_fetch_and_sub1(&block_counter[j][object->bucket_number]);
			    free(object);
			}
		    }
		    else {

			bucket++;

		    }
		    deleted_blocks++;
		}
		pthread_mutex_unlock(&disklocks[j]);
		 AO_store(&(disks[j]->use_percent),
		    (AO_load(&total_bytes_in_disks[j]) * 100) /
			disks[j]->disk_size);
		deleted_blocks = 0;
	    }

	    deleted_blocks = number_of_blocks;
	    blocks_in_lowest_bucket = 0;
	    bkt_number--; //since this bkt number had extra blocks
	    //since 3 * blocks per disk might be less than
	    //number_of_blocks.
	}
	else
	{
	    for(j = tier_start_index;j < tier_end_index; j++)
	    {
		bucket = bkt_number - 1;
		pthread_mutex_lock(&disklocks[j]);
		while((!TAILQ_EMPTY(&disks[j]->buckets[bucket])) &&
		    (deleted_blocks < number_of_blocks)) {

		    object =
			TAILQ_FIRST(&disks[j]->buckets[bucket]);

		    TAILQ_REMOVE(&disks[j]->buckets[bucket],
			    object, bucket_entry);
		    g_hash_table_remove(disktables[j], object->uri_name);

		    while(g_list_length(object->uri_extent_list)!=0) {

			iterator = g_list_first(object->uri_extent_list);
			extent = ((dm_extent_t *)(iterator->data));

			object->uri_extent_list =
			   g_list_remove_link(object->uri_extent_list,iterator);
			AO_store(&total_bytes_in_disks[j],
			    (AO_load(&total_bytes_in_disks[j])-extent->length));
			free(extent);
			AO_fetch_and_add1(&(disks[j]->number_of_free_blocks));
			deleted_blocks++;
			if (deleted_blocks == number_of_blocks) {

			    bkt_number--;
			    break;
			}
		    }
		    if (g_list_length(object->uri_extent_list) == 0) {
			AO_fetch_and_sub1(&block_counter[j][object->bucket_number]);
			free(object);
		    }
		}
		pthread_mutex_unlock(&disklocks[j]);

		AO_store(&(disks[j]->use_percent),
		    (AO_load(&total_bytes_in_disks[j]) * 100) /
			 disks[j]->disk_size);
	    }


	}
	blocks_in_lowest_bucket = 0;
    }



}

int tier_total_use(int tier_type)
{
    int tier_start_index = 0, tier_end_index = 0;
    int i = 0;
    int total_use = 0;
    if(tier_type == TYPE_SSD) {

	tier_start_index = 0;
	tier_end_index = number_of_SSDs;
    }
    else if (tier_type == TYPE_SAS) {

	tier_start_index = number_of_SSDs;
	tier_end_index = number_of_SSDs + number_of_SASs;
    }
    else if (tier_type == TYPE_SATA) {

	tier_start_index =  number_of_SSDs + number_of_SASs;
	tier_end_index = number_of_disks;

    }

    for(i = tier_start_index;i < tier_end_index; i++) {

	total_use += AO_load(&(disks[i]->use_percent));


    }
    total_use = total_use / (tier_end_index - tier_start_index);
    return total_use;

}

int DM_init(DM_data* register_module)
{
    int i = 0;

    for(i = 0;i < MAX_DISKS; i++) {

	disktables[i] = NULL;
	disks[i] = NULL;
	pthread_mutex_init(&disklocks[i],NULL);
    }

    init_dm_structures();
    register_module->DM_get = DM_get;
    register_module->DM_put = DM_put;
    register_module->DM_stat = DM_stat;
    register_module->DM_cleanup = DM_cleanup;
    register_module->DM_attribute_update = DM_attribute_update;
    pthread_create(&dm_eviction, NULL, (void*)(DM_evict), NULL);




    return 0;
}

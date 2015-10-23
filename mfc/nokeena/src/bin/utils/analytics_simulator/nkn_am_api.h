/*
*Copyright 2010 Juniper Networks, Inc
*
* This file contains code which implements the Analytics manager
*
* Author: Hrushikesh Paralikar
*
*/


#ifndef _NKN_AM_API_H
#define _NKN_AM_API_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <glib.h>
//#include "nkn_defs.h"
//#include "nkn_attr.h"
//#include "queue.h"
#include <sys/queue.h>
#include "nkn_global_defs.h"
#include "config_parser.h"
//#include "nkn_namespace.h"
//#include "nkn_mm_mem_pool.h"
//#include "nkn_sched_api.h"

#include <pthread.h>


#define AM_MAX_SHORT_VAL    0xFFFF
#define MAX_TRANSFERRABLE_BUFFERS 64
//#define WRITE_SIZE 2097152
#define FIFO_INGEST 1
#define LIFO_INGEST 2
#define HYBRID_INGEST 3


#define FIFO_LIFO_QUEUE 1
#define HYBRID_QUEUE 2

typedef uint64_t nkn_hot_t;
GHashTable *am_hash_table;
pthread_t am_main;
pthread_t am_ingest;
pthread_t am_offline_ingest;
pthread_mutex_t am_task_queue_mutex;
pthread_mutex_t am_ingest_queue_mutex;

int write_size; //data size to accumulate before ingesting;

//typedef struct nkn_buf buffers;

//struct am_object_data am_object;

typedef struct am_transfer_data {

        char uri_name[MAX_URI_LEN];
	nkn_buf_t *attrbuff;
	uint16_t provider;
	uint64_t num_hits;
	nkn_hot_t hotness;
	uint64_t object_ts;
	uint64_t bytes_from_cache;
	nkn_buf_t *buffers[MAX_TRANSFERRABLE_BUFFERS];
	uint64_t buffer_count;
	int bm_put; //if  =0 , only update hotness.
	uint64_t entry_num; //debug purpose
	int tier_type;
	int write_size;
	uint64_t length;
	int partial;
	TAILQ_ENTRY(am_transfer_data) am_task_queue_entry;


}am_transfer_data_t;


typedef struct am_push_ingest {

    TAILQ_ENTRY(am_push_ingest) am_write_disk_queue_entry;
    GList   *am_extent_list;
    nkn_buf_t *attrbuff;
}am_push_ingest_t;



typedef struct am_object_data {

     char uri_name[MAX_URI_LEN];
     //buffers *attrbuff;
     uint16_t provider;
     uint64_t num_hits;
     nkn_hot_t hotness;
     uint64_t object_ts;
     uint64_t bytes_from_cache;
     int tier_type;
     int write_size; //write size depends on tier type
     AO_t in_ingestion;
     AO_t offline_ingest_on; //set to 1 if either of the ingest is in progress.
     int offline_ingest_queue; //indicates whether the object is in fifo/lifo 
			       // queue or hybrid queue.
     //GList   *am_extent_list;
     uint64_t length;
     am_push_ingest_t *push_ptr;
     int partial;
     TAILQ_ENTRY(am_object_data) am_disk_provider_entry;
     TAILQ_ENTRY(am_object_data) am_origin_provider_entry;
     TAILQ_ENTRY(am_object_data) ingest_queues_entry;
     TAILQ_ENTRY(am_object_data) hybrid_queues_entry;
}am_object_data_t;


typedef struct am_extent {

    uint64_t start_offset;
    uint64_t length; //size of data filled.
    nkn_buf_t *attrbuff;
    am_object_data_t *object;
    uint64_t last_updated;
    uint64_t write_start_time;
    nkn_buf_t *buffers[MAX_TRANSFERRABLE_BUFFERS];
    int from_ingestion;
    int tier_type;
    int write_size;
    nkn_hot_t hotness;
    int decoded_hotness;
    TAILQ_ENTRY(am_extent) am_ingest_queue_entry;
    TAILQ_ENTRY(am_extent) write_simulate_queue_entry;
}am_extent_t;



void am_inc_num_hits(am_transfer_data_t *);
void am_delete();
void copytoAMobject(am_transfer_data_t*,am_object_data_t*);
void copy_to_am_extent(am_transfer_data_t*, am_extent_t*);
int merge_extents(am_extent_t*, am_extent_t*);
void create_and_add_extent(am_transfer_data_t*, am_object_data_t *);
//void copytoTransferobject(uri_entry_t*, am_transfer_data_t*);
int AM_init(AM_data*);
void AM_main(void *);
void offline_ingest_thread(void *);
am_object_data_t* get_next_ingest_entry();

void am_encode_hotness(nkn_hot_t *out_hotness, int32_t in_temp);

void am_encode_update_time(nkn_hot_t *out_hotness, time_t object_ts);
uint32_t am_decode_update_time(nkn_hot_t *in_hotness);
int32_t  am_decode_hotness(nkn_hot_t *in_hotness);
void am_encode_interval_check_start(nkn_hot_t *out_hotness, time_t object_ts);
uint32_t am_decode_interval_check_start(nkn_hot_t *in_hotness);
nkn_hot_t am_update_hotness(nkn_hot_t *in_hotness, int num_hits,
	                            time_t object_ts, uint64_t hotness_inc, char *uri);
uint32_t am_get_hit_interval(void);

//void derefbuffer(nkn_buf_t *buffer);
void set_write_size(DM_put_data_t *);
void create_dm_put(DM_put_data_t *, am_extent_t *);
int free_extent(am_extent_t *);
void AM_cleanup();

TAILQ_HEAD(,am_transfer_data) am_task_queue;
TAILQ_HEAD(,am_object_data) am_disk_provider_queue;
TAILQ_HEAD(,am_object_data) am_origin_provider_queue;
TAILQ_HEAD(,am_extent) am_ingest_queue;
TAILQ_HEAD(,am_push_ingest) am_write_disk_queue;
TAILQ_HEAD(,am_extent) write_simulate_queue;


TAILQ_HEAD(ingest_queues_head,am_object_data) ingest_queues[3]; //3 = 1 for each tier type;

// both, ingest_queues and hybrid_queues for offline ingest.

TAILQ_HEAD(,am_object_data) hybrid_queues[3]; //3 = 1 for each tier type;




#endif

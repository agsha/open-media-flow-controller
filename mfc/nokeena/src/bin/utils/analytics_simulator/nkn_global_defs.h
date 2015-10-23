/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * Core modeler header file
 *
 * Author: Hrushikesh Paralikar
 *
 */



#ifndef _NKN_GLOBAL_H
#define _NKN_GLOBAL_H

#include <sys/types.h>
#include <sys/queue.h>
#include <pthread.h>
#include <stdint.h>
#include <atomic_ops.h>
#include <signal.h>

#define MAX_URI_LEN 2048
#define MAX_HOT_VAL 65536
#define DELAY_GRANULARITY 1000000 //microseconds
#define NUM_BUFS_RETURNED 64

#define PROVIDER_DM 1
#define PROVIDER_ORIGIN 2


#define DATA_BUFFER_SIZE (32*1024)
#define ATTR_BUFFER_SIZE (4*1024)

#define MAX_BUFFERS_FETCH_DM 64


#define TYPE_SATA 3
#define TYPE_SSD 1
#define TYPE_SAS 2

#define MAX_DISKS 20

FILE *log_memory;
sigset_t   signal_mask;

/* Statistics */
uint64_t total_bytes_delivered;
uint64_t total_bytes_in_ram;
uint64_t total_bytes_in_disk;
uint64_t total_dup_bytes_delivered;
uint64_t total_num_requests;
uint64_t total_dup_requests;
uint64_t total_num_cacheable_entries;
uint64_t cache_hit_ratio;
//uint64_t origin_fetch_number;
uint64_t total_buffers_in_use;
uint64_t total_bytes_in_am;
uint64_t entries_done;
AO_t total_bytes_in_disks[MAX_DISKS];
uint64_t total_bytes_from_disks[MAX_DISKS];
//uint64_t nkn_cur_ts;
AO_t nkn_cur_ts;

AO_t ingest_done_time; //used for synchronisation between the scheduler and 
		       //the ingestion thread.


uint64_t bm_get_loop_delay;
uint64_t bm_get_fetch_delay;
uint64_t om_get_delay;
uint64_t bm_put_delay;
uint64_t bm_get_failure_delay;

uint64_t ingests_done;
uint64_t ingests_to_do;
uint64_t ingests_expected;

int max_parallel_ingests;
AO_t ingest_count;
AO_t buffers_held;
uint64_t max_buffers_hold;

//indicates that bm has exited, used as exit condition for the AM module.
extern uint64_t bm_stop_processing;
extern uint64_t am_stop_processing;
extern uint64_t ingestion_thread_done;

//uint64_t queue_busy;
AO_t queue_busy;   //atomic variable
/*
pthread_mutex_t OM_queue_mutex;
pthread_cond_t OM_queue_cond;
*/


pthread_mutex_t queue_busy_mutex;
//pthread_mutex_t current_time_mutex;
pthread_cond_t queue_busy_cond;

pthread_mutex_t bufferpool_queue_mutex;

pthread_mutex_t ingestion_protection_mutex;

pthread_mutex_t synch_mutex;
pthread_cond_t synch_cv;

pthread_mutex_t ingest_synch_mutex;
pthread_cond_t ingest_synch_cv;


typedef struct nkn_buf buffers;
typedef struct am_object_data am_object;
typedef struct am_transfer_data am_transfer_data_object;

typedef struct uri_entry_s {
    buffers *data_bufs[NUM_BUFS_RETURNED];
    buffers *attrbuff;   //pointer to the 4K attribute buffer
    char uri_name[MAX_URI_LEN];
    uint64_t start_offset;
    uint64_t end_offset;
    uint64_t content_len;
    time_t   last_access_time;


    uint64_t request_offset; //indicates the offset from which to start fetching
    uint64_t request_length; //indicates the length of data to be fetched
			    //from request_offset


 //   uint64_t response_offset; //unused.
    uint64_t response_code;    //unused.  
    uint64_t response_length;  // used to indicate the number of bytes returned
    uint64_t response_valid;  //unused.

    int first_origin_fetch; //indicates whether the first origin fetch is done.
			    //1 = done;
    //int origin_fetch; // flag used to indicated BM to call BMput; 1 = simulate a OM fetch; 
    int bm_get_failure; //used to indicate whether a BM_get is successfull
			//or a failure. 1 = failure, that means do a DMstat 
			//for remaining data that BM_get did not return;
    int cached; //indicates whether this object is cached or not.
    int duplicate_request; //indicates whether this is a duplicate request.
    int buffer_fetch_count; //the number of buffers fetched 
    uint64_t total_fetched; //the total bytes fetched
    uint64_t entry_num; //the entry number from access log:useful for debugging.
    //int in_queue; //Unused : indicates whether the object is currently being processed
		 //set to one at the start when it is put in the queue.set to 0
		 //when removed from the queue.
    //nkn_hot_t hotness;
    int resp;
    int cacheable; //set if the entry is cacheable 
    int curr_provider;
    //int am_inc_num_hits; //indicates whether am_inc_num_hits has been called 
			 //for this object or not. 1 = already called.
    int first_request;
    //int ingest_in_progress; // dnt update any details(am module) just ingestbuf.
			    //set to 1 when buffers are refcounted after satisfying
			    //condition (10% of total buffs can be refcounted)
			    //else 0.
    int bm_put;	//indicates bm_put done.
    //int ingest_in_progress; //indicates ingest is in prog.

    int tier_type; //select which disk tier the object should go to if ingested.
    int write_size; //write size depends on tier type
    AO_t offline_ingest; //set by AM to indicate that
			 //this is not a normal request,
			 //this is an offline ingest request.
    uint64_t hotness; //set in case of offline ingest;

    int partial; //indicates whether a partial or a complete fetch. 1 = partial
//#define ENTRY_NEW          0x01
//#define ENTRY_IN_RAM       0x02
//#define ENTRY_NOT_INGESTED 0x04
//#define ENTRY_INGESTED     0x08
//#define ENTRY_EVICTED      0x10
//    int state;
//   TAILQ_ENTRY(uri_entry_s) hotness_entries;
    TAILQ_ENTRY(uri_entry_s) sched_queue_entries;
}uri_entry_t;


typedef struct nkn_buf {
    //nkn_uol_t id;           /* identity */
    TAILQ_ENTRY(nkn_buf) entries;
    TAILQ_ENTRY(nkn_buf) attrentries;
    TAILQ_ENTRY(nkn_buf) bufferpoolentry;
    TAILQ_ENTRY(nkn_buf) attrbuffpoolentry;
    char* uri_name;         //pointer to uri which this buffer represents.
    struct nkn_buf *attrbuf;
    //uint64_t use_ts     : 40;
    uint64_t type       :  8; /* type of buffer */
    uint64_t flist      :  8;          /* free list the buffer is on */
    uint64_t bufsize    : 16;
    uint64_t provider	: 32;
    uint32_t start_offset;
    uint32_t end_offset;
    uint32_t length;
    //nkn_provider_type_t provider;   /* provider of the data */
    //int ref;                /* reference count */
    AO_t ref;
    int use;                /* use count - number of hits */
    uint64_t use_ts;
    //uint64_t create_ts;     /* timestamp at which setid is done*/
    uint64_t object_ts; //time at which the object was hit.

    //parameters specific for attr buff,used by AM
    uint64_t last_send;
    uint32_t num_hits;
    uint64_t hotness;
    uint32_t bytes_from_cache;
    AO_t ingest_in_progress; // dnt update any details(am module) just ingestbuf
			    //set to 1 when buffers are refcounted after satisfying
			    //condition (10% of total buffs can be refcounted)
			    //else 0.
    AO_t extent_count;
    uint32_t buffer_count; //testing purpose.
}nkn_buf_t;

//int bm_stop_processing; // to indicate the BM_main thread to stop processing.
typedef struct BM_data {

	int (*BM_get)(uri_entry_t*);
	//void (*BM_main)(void *);
	//void (*BM_put_in_queue)(uri_entry_t*);
	void (*BM_put)(uri_entry_t*);
	void (*BM_cleanup)();
	void (*deref_fetched_buffers)(uri_entry_t*);
	//uint64_t (*getLastSend)(uri_entry_t *);
	//void (*setLastSend)(uri_entry_t *, uint64_t);
	//void (*inc_num_hits)(uri_entry_t *, uint64_t);
	void (*get_buffers_from_BM)(uint64_t, uri_entry_t *, nkn_buf_t *[]);
	void (*return_buffers_to_client)(uint64_t,uri_entry_t *, nkn_buf_t *[]);
}BM_data;



typedef struct AM_data {


    void (*am_inc_num_hits)(am_transfer_data_object *);

    void(*am_delete)(void);

    void(*AM_cleanup)(void);
}AM_data;


typedef struct attr_update_transfer {

    char uri_name[MAX_URI_LEN];
    uint64_t hotness;
    int32_t decoded_hotness;
}attr_update_transfer_t;


typedef struct DM_put_data {

    char uri_name[MAX_URI_LEN];
    uint64_t length;
    uint64_t request_offset;
    int tier_type;
    uint64_t hotness;
    int32_t decoded_hotness;

}DM_put_data_t;

typedef struct DM_stat_resp {


    char uri_name[MAX_URI_LEN];
    uint64_t physid;
    uint64_t request_offset;
    uint64_t request_length;
    int failure; //set 1 when object not present.
    uint64_t block_size; //block size of disk containing required data.

}DM_stat_resp_t;

typedef struct DM_get_resp {

    char uri_name[MAX_URI_LEN];
    uint64_t physid;
    uint64_t request_offset;
    uint64_t request_length; //corresponds to block size or small read size
    int failure;
    uint64_t length; //actual length of data requested
    nkn_buf_t * buffers[MAX_BUFFERS_FETCH_DM];
    nkn_buf_t* attrbuff;
}DM_get_resp_t;

typedef struct DM_data {



    void (*DM_put)(DM_put_data_t *);

    void (*DM_stat)(DM_stat_resp_t *);

    void (*DM_get)(DM_get_resp_t *);

    void (*DM_cleanup)();

    void (*DM_attribute_update)();


}DM_data;





int BM_init(BM_data*);
int AM_init(AM_data*);


int DM_init(DM_data*);

//unused


//global func to update hotness of objects in RAM or Disk
//called by AM when there is a lookup success in its hash table.
void update_hotness(am_object *);

void CE_DM_put(DM_put_data_t *);
void CE_DM_attribute_update(attr_update_transfer_t *);
void CE_inc_cur_time(uint64_t);

void release_data_buffer(nkn_buf_t *);

void empty_cache(); //implemented in nkn_bm.c

void derefbuffer(nkn_buf_t *);
void select_tier(uri_entry_t *object, int size_based_selection);
void* my_calloc(int size, char [100]);
void my_free(void*, char [100]);
#endif

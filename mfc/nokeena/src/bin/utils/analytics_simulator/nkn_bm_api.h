/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains Buffer Manager related defines
 *
 * Author : Hrushikesh Paralikar
 *
 */

#ifndef _NKN_BM_API_H
#define _NKN_BM_API_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <glib.h>
#include <sys/queue.h>
#include "nkn_global_defs.h"
#include "config_parser.h"
#include <atomic_ops.h>


/* Hashtable for the RAM */
GHashTable *uri_hash_table;


//Hashtable for attribute buffers
GHashTable *attr_hash_table;

//#define DATA_BUFFER_SIZE (32*1024)
//#define ATTR_BUFFER_SIZE (4*1024)
#define HOTNESS_LEVELS 5
#define LRUSCALE 20 //used in buffer eviction

//NUMBER_OF_BUFFERS here considers a 1:1 ratio for data:attr buffers
extern uint64_t number_of_buffers;
extern uint64_t number_of_attr_buffers;


TAILQ_HEAD(,nkn_buf) free_databuf_queue;  //free pool for data buffers
TAILQ_HEAD(,nkn_buf) free_attrbuf_queue;  //free pool for attribute buffers:
TAILQ_HEAD(,nkn_buf) bufferpool_queues[HOTNESS_LEVELS];   //levels
TAILQ_HEAD(,nkn_buf) attrbufferpool_queues[HOTNESS_LEVELS];





//nkn_buf_t *BM_get(nkn_uol_t *uol, nkn_buffer_type_t type, int flags);
int BM_get(uri_entry_t*);



void init_free_pool();



nkn_buf_t * getFreeBuffer();
nkn_buf_t * getFreeAttrBuffer();


void  merge_content(uri_entry_t*, nkn_buf_t *, nkn_buf_t *, uint64_t *,uint64_t);
void BM_put(uri_entry_t *);
void BM_cleanup();
void calculateHotnessBucket(nkn_buf_t *);
void makeBufferAvailable(nkn_buf_t *);


//unused
uint64_t getLastSend(uri_entry_t *);

//unused
void setLastSend(uri_entry_t *, uint64_t);

//unused
void inc_num_hits(uri_entry_t *, uint64_t);

void get_buffers_from_BM(uint64_t, uri_entry_t *, nkn_buf_t *[]);

void return_buffers_to_client(uint64_t, uri_entry_t *, nkn_buf_t *[]);

//void remove_entry(gpointer key, gpointer value, gpointer userdata);
//void empty_cache();

//debug pointers
//nkn_buf_t  *buffs[81];
//nkn_buf_t  *attrbuffs[250];
#endif

 /*
  * Copyright 2010 Juniper Networks, Inc
  *
  * This file contains Disk  Manager related defines
  * 
  * Author : Hrushikesh Paralikar
  *
  */

#ifndef _NKN_DM2_API_H
#define _NKN_DM2_API_H

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
#include <atomic_ops.h>

//#define MAX_DISKS 20
//#define TYPE_SATA 3
//#define TYPE_SSD 1
//#define TYPE_SAS 2
//#define SSD_BLOCK_SIZE 262144
//#define SATA_BLOCK_SIZE 2097152

#define DM2_UNIQ_EVICT_BKTS   (128)
#define DM2_MAP_UNIQ_BKTS     (7) // 2^7 = 128
#define DM2_GROUP_EVICT_BKTS  (16 - DM2_MAP_UNIQ_BKTS)
#define DM2_MAX_EVICT_BUCKETS (DM2_GROUP_EVICT_BKTS + DM2_UNIQ_EVICT_BKTS + 1)

extern uint64_t number_of_blocks;
extern uint64_t number_of_free_blocks;
GHashTable *disktables[MAX_DISKS];
pthread_mutex_t disklocks[MAX_DISKS];
pthread_t dm_eviction;

AO_t block_counter[MAX_DISKS][DM2_MAX_EVICT_BUCKETS];

typedef struct uri_head {

    char uri_name[MAX_URI_LEN];
    int bucket_number;
    uint64_t hotness;
    GList   *uri_extent_list;

    TAILQ_ENTRY(uri_head) bucket_entry;
}uri_head_t;

typedef struct disk {
    uint64_t block_size;
    uint64_t number_of_blocks;
    AO_t number_of_free_blocks;
    uint64_t disk_size;
    int disk_type;
    AO_t use_percent;
    TAILQ_HEAD(, uri_head) buckets[DM2_MAX_EVICT_BUCKETS];

}disk_t;

disk_t *disks[MAX_DISKS];

typedef struct dm_extent {

    uint64_t physid;
    uint64_t start_offset;
    uint64_t length;

}dm_extent_t;






void init_dm_free_pool();

void DM_put(DM_put_data_t *);

void DM_stat(DM_stat_resp_t *);

void DM_get(DM_get_resp_t *);

void DM_cleanup();

void DM_attribute_update(attr_update_transfer_t *);
void copy_to_stat(DM_stat_resp_t *, DM_put_data_t *);
void cleantable(gpointer, gpointer, gpointer);

int tier_total_use(int);

static int
dm2_get_hotness_bucket(int hotness);

void evict_data(int, int, uint64_t);
#endif

/*
 * (C) Copyright 2011 Juniper Networks, Inc
 *
 * This file contains header defines of the static single threaded queue
 *
 * Author: Jeya ganesh babu
 *
 */

typedef struct nkn_st_queue_entry {
    struct nkn_st_queue_entry *next;
    struct nkn_st_queue_entry *prev;
    void *ptr;
    int id;
}nkn_st_queue_entry_t;

typedef struct nkn_st_queue {
    nkn_st_queue_entry_t *stq_entry;
    nkn_st_queue_entry_t *tail_entry;
    nkn_st_queue_entry_t *used_list;
    nkn_st_queue_entry_t *free_list;
    uint32_t max_entries;
    uint32_t num_entries;
}nkn_st_queue_t;

nkn_st_queue_t * nkn_st_queue_init(int num_entries);

int nkn_st_queue_insert(nkn_st_queue_t *st_q, void *ptr);

int nkn_st_queue_remove(nkn_st_queue_t *st_q, int id);

uint32_t nkn_st_queue_entries(nkn_st_queue_t *st_q);

void * nkn_st_queue_and_next_id(nkn_st_queue_t *st_q, int id, int *next_id);

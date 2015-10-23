/*
 * (C) Copyright 2011 Juniper Networks, Inc
 *
 * This file contains code which implements a static single threaded queue
 *
 * Author: Jeya ganesh babu
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <math.h>
#include "nkn_assert.h"
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_static_st_queue.h"

/*
 * Initialize a static queue
 */
nkn_st_queue_t *
nkn_st_queue_init(int num_entries)
{
    nkn_st_queue_t        *st_q       = NULL;
    nkn_st_queue_entry_t  *stq_entry = NULL;
    int i;

    st_q = nkn_calloc_type(1, sizeof(nkn_st_queue_t), mod_st_queue);
    if (!st_q) {
	goto ret;
    }

    st_q->stq_entry = nkn_calloc_type(1, sizeof(nkn_st_queue_entry_t) * num_entries,
			   mod_st_queue);
    if (!st_q->stq_entry) {
	free(st_q);
	st_q = NULL;
	goto ret;
    }

    st_q->max_entries = num_entries;

    for (i=0; i<num_entries; i++) {
	stq_entry = &st_q->stq_entry[i];
	if (i == 0)
	    st_q->free_list = stq_entry;
	stq_entry->id = i;
	if (i != (num_entries-1))
	    stq_entry->next = &st_q->stq_entry[i+1];
	else
	    stq_entry->next = NULL;
    }

ret:;
    return st_q;
}

/* 
 * Insert a node to the queue
 * Retval - Node id which should be used during remove operation
 *	  - -1 in case of failure
 */
int
nkn_st_queue_insert(nkn_st_queue_t *st_q, void *ptr)
{
    nkn_st_queue_entry_t *stq_entry = st_q->free_list;

    if (stq_entry) {

	st_q->free_list = stq_entry->next;
	stq_entry->next = NULL;
	stq_entry->ptr  = ptr;

	if (st_q->used_list) {
	    st_q->tail_entry->next = stq_entry;
	    stq_entry->prev        = st_q->tail_entry;
	} else {
	    st_q->used_list = stq_entry;
	    stq_entry->prev = NULL;
	}

	st_q->tail_entry = stq_entry;
	st_q->num_entries++;

	return stq_entry->id;
    }
    return -1;
}

/*
 * Remove a node (based on node id) from the queue
 * Retval - Success (0) /Failure (-1)
 */
int
nkn_st_queue_remove(nkn_st_queue_t *st_q, int id)
{
    nkn_st_queue_entry_t *stq_entry;

    if ((id < 0) || (id > (int)st_q->max_entries))
	return -1;

    stq_entry = &st_q->stq_entry[id];

    if (stq_entry->next)
	stq_entry->next->prev = stq_entry->prev;
    if (stq_entry->prev)
	stq_entry->prev->next = stq_entry->next;

    if (stq_entry == st_q->used_list)
	st_q->used_list = stq_entry->next;
    if (stq_entry == st_q->tail_entry)
	st_q->tail_entry = stq_entry->prev;

    stq_entry->next = st_q->free_list;
    st_q->free_list = stq_entry;

    stq_entry->prev = NULL;

    st_q->num_entries--;

    return 0;
}

/*
 * Return the node ptr for the given id and
 * as well return the next id
 * Retval - Node ptr or NULL if id is invalid, returns the first
 *          Node if the id is -1.
 *        - Valid next id or -1 if the current id is the last.
 */
void *
nkn_st_queue_and_next_id(nkn_st_queue_t *st_q, int id, int *next_id)
{
    nkn_st_queue_entry_t *stq_entry;

    if (id == -1) {
	stq_entry = st_q->used_list;
	goto ret_val;
    }

    if ((id < 0) || (id > (int)st_q->max_entries))
	return NULL;

    stq_entry = &st_q->stq_entry[id];

ret_val:;

    if (stq_entry && stq_entry->next)
	*next_id = stq_entry->next->id;
    else
	*next_id = -1;

    if (stq_entry)
	return stq_entry->ptr;
    else
	return NULL;

}

/* 
 * Returns the number of queue entries
 */
uint32_t
nkn_st_queue_entries(nkn_st_queue_t *st_q)
{
    return st_q->num_entries;
}

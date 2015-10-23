/*
 * (C) Copyright 2008-2011 Juniper Networks, Inc
 *
 * This file contains common definitions for the atomic read/write Queue
 *
 * Author: Jeya ganesh babu
 *
 */

#ifndef NKN_RW_QUEUE_H
#define NKN_RW_QUEUE_H

/* RW Queue structure */
typedef struct nkn_rw_queue {
    void            **rw_q_array;
    volatile int    rd_index;
    volatile int    wr_index;
    uint32_t        mask;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
}nkn_rw_queue_t;

#define ENTRY_PRESENT(X) (X->wr_index - X->rd_index)

/* Init routine */
void * nkn_rw_queue_init(int num_entries);

/* Task specific Routines */
/* Insert Tasks */
int nkn_rw_queue_insert_task(void * restrict ptr,
			     void * restrict element);

/* Get Next task */
void * nkn_rw_queue_get_next_element(void *ptr);

#endif /* NKN_RW_QUEUE_H */

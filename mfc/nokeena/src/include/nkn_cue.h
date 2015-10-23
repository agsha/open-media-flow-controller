/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains CUE declarations
 *
 * Author: Jeya ganesh babu
 *
 */
#ifndef _NKN_CUE_H
#define _NKN_CUE_H

#include "nkn_ccp_server.h"

typedef struct cue_stat_str_t {
    char *crawl_name;
    int id;
    AO_t num_tasks;
    AO_t num_post_tasks;
    AO_t num_abort_tasks;
    AO_t num_query_tasks;
    AO_t num_objects_added;
    AO_t num_objects_deleted;
    AO_t num_objects_add_failed;
    AO_t num_objects_delete_failed;
    AO_t last_task_num_objects_added;
    AO_t last_task_num_objects_deleted;
    AO_t last_task_num_objects_add_failed;
    AO_t last_task_num_objects_delete_failed;
}cue_stat_str_t;

void *cue_thread(void *arg __attribute((unused)));
void cue_callback(ccp_client_data_t *objp);
int cue_init(void);
void cue_task_complete(void *ptr);

#endif /* _NKN_CUE_H */

/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains CIM declarations
 *
 * Author: Jeya ganesh babu
 *
 */
#ifndef _NKN_CIM_H
#define _NKN_CIM_H

#include "nkn_iccp_srvr.h"

/* NVSD crawler statistics structure
 * based on crawl name
 */
typedef struct cim_stat_str_t {
    char *crawl_name;
    int id;
    AO_t num_tasks;
    AO_t num_post_tasks;
    AO_t num_objects_added;
    AO_t num_objects_deleted;
    AO_t num_objects_add_failed;
    AO_t num_objects_delete_failed;
}cim_stat_str_t;

int cim_init(void);
void cim_callback(void *ptr);
void cim_task_complete(nkn_cod_t cod, char *ns_uuid, int errcode,
	iccp_action_type_t action, size_t file_size, time_t expiry);
int cim_get_params(nkn_cod_t cod, time_t *tm, int *cache_pin, int *flags);
#endif /* _NKN_CIM_H */

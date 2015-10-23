/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * Core modeler header file
 *
 * Author: Jeya ganesh babu
 *
 */

#ifndef _SIMULATOR_H
#define _SIMULATOR_H

#include "queue.h"

#define MAX_HOT_VAL 65536

typedef struct uri_entry_s {
    char *uri_name;
    uint64_t start_offset;
    uint64_t end_offset;
    uint64_t content_len;
    time_t   last_access_time;
    nkn_hot_t hotness;
    int	curr_provider;
#define ENTRY_NEW          0x01
#define ENTRY_IN_RAM       0x02
#define ENTRY_NOT_INGESTED 0x04
#define ENTRY_INGESTED     0x08
#define ENTRY_EVICTED      0x10
    int state;
    TAILQ_ENTRY(uri_entry_s) hotness_entries;
}uri_entry_t;

#endif /* _SIMULATOR_H */

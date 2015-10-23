/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * Origin manager header file
 *
 * Author: Hrushikesh Paralikar
 *
 */

#ifndef _NKN_OM_API_H
#define _NKN_OM_API_H

#include "nkn_global_defs.h"



int OM_get(uri_entry_t);
void OM_main(void* arg);

TAILQ_HEAD(get_queue_head,uri_entry_t) om_get_queue;




#endif

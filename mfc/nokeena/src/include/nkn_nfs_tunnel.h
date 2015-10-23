#ifndef _NKN_NFS_TUNNEL_H
#define _NKN_NFS_TUNNEL_H
/*
 * (C) Copyright 2008-2011 Juniper Networks, Inc
 *
 * This file contains symbols necessary to interface with the NFS tunnel module.
 *
 * Author: Jeya ganesh babu
 *
 */

#include "cache_mgr.h"

typedef struct nfst_response_t {
    cache_response_t cr;
    char             *uri;
    char             uri_len;
    int              tunnel_response_code;
}nfst_response_t;

void nfs_tunnel_mgr_input(nkn_task_id_t tid);
void nfs_tunnel_mgr_output(nkn_task_id_t tid);
void nfs_tunnel_mgr_cleanup(nkn_task_id_t tid);
int nkn_nfs_tunnel_handle_returned_content(nkn_task_id_t tid, uint8_t *in_buf,
					int in_buflen);

#endif /* _NKN_NFS_TUNNEL_H */

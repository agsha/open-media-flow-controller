/*============================================================================
 *
 *
 * Purpose: This file defines the private virt_cache_server data structures 
 *          and functions.
 *
 * Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
 *
 *
 *============================================================================*/
#ifndef __VCS_PRIVATE_H__
#define __VCS_PRIVATE_H__

#include <stdio.h>
#include <errno.h>

#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <atomic_ops.h>

#include "nkn_defs.h"
#include "vfs_defs.h"
#include "vcs_dbg.h"
#include "vcs_workq.h"
#include "virt_cache_defs.h"
#include "zvm_api.h"

#define EiB(x) ((1ULL << 60) * (x))

#define VCS_N_CHANNELS      8
#define VCS_CHANNEL_SIZE    EiB(4)

#define VCS_DISK                    "vdx"
#define VCS_MAX_DISK_NAME_STR       (sizeof("vdX")+1)

#define VCS_MAX_CHANNEL_NAME_STR    (sizeof("channel_XX.dat")+1)

#define VCS_MOUNT_DIR               "/nkn/mnt/virt_cache/virt_cache/"
#define VCS_MOUNT_POINT             "/nkn/mnt/virt_cache/virt_cache"

#define USECS(t) (((int64_t)(t)) * 1000 * 1000)

extern struct vfs_funcs_t vfs_funs;

/*
 * LOCK ORDERING: VC -> CHANNEL.
 *      nkn_vcs_lock -> nkn_vc_channel_lock
 */

typedef struct nkn_vc_slot {
    uint64_t  client_id;
    char *    uri;
    uint64_t  file_length;
    FILE *    vfp;
    AO_t      usage_count;
    AO_t      close_flag;
    uint32_t  streaming_read_size;
} nkn_vc_slot_t;

typedef union nkn_vc_slot_handle
{
    uint64_t            value;
    struct {
	uint64_t        ptr : 48;
	uint64_t        nonce : 7;
	uint64_t        unused : 9;
    } fields;
} nkn_vc_slot_handle_t;

typedef struct nkn_vc_channel {
    char *                path;
    char                  disk_name[VCS_MAX_DISK_NAME_STR];
    char                  name[VCS_MAX_CHANNEL_NAME_STR];
    char                  vm_name[VCCMP_MAX_VM_NAME_STR];
    pthread_mutex_t       lock;           /* protects usage count and slots */
    int                   usage_count;
    int                   next_slot;
    nkn_vc_slot_handle_t  vcs_vc_slot_table[VCCMP_SLOTS_PER_CHANNEL];
    uint8_t               id;

    // stats
    AO_t                  total_open_requests_cnt;
    AO_t                  total_stat_requests_cnt;
    AO_t                  total_close_requests_cnt;
    AO_t                  total_open_files_cnt;
    AO_t                  current_open_files_cnt;
    AO_t                  total_bytes_read_cnt;
    AO_t                  total_open_errors_cnt;
    AO_t                  total_stat_errors_cnt;
    AO_t                  total_read_errors_cnt;
    AO_t                  total_open_virt_files_cnt;
    AO_t                  current_open_virt_files_cnt;

} nkn_vc_channel_t;

typedef struct nkn_vc_acceptor {
    pthread_mutex_t   mutex;
    pthread_cond_t    cond;
    int               socket;
    int               port;
} nkn_vcs_acceptor_t;

typedef struct nkn_virt_cache_server {
    nkn_vc_channel_t      vcs_vc_channels[VCS_N_CHANNELS];
    pthread_mutex_t       lock;       /* protects channel assignment/retrieval. */
    nkn_vcs_acceptor_t    acceptor;
    pthread_t             accept_thread;
    pthread_t             main_thread;
    vcs_workq_t           wq;

    // stats
    AO_t                  total_clients_cnt;
    AO_t                  current_clients_cnt;
    AO_t                  total_disconnect_cnt;
    AO_t                  total_lost_cnt;
    AO_t                  total_socket_error_cnt;
    AO_t                  total_connect_requests_cnt;
    AO_t                  total_open_requests_cnt;
    AO_t                  total_stat_requests_cnt;
    AO_t                  total_close_requests_cnt;
    AO_t                  total_open_files_cnt;
    AO_t                  current_open_files_cnt;
    AO_t                  total_bytes_read_cnt;
    AO_t                  total_open_errors_cnt;
    AO_t                  total_stat_errors_cnt;
    AO_t                  total_read_errors_cnt;
} nkn_virt_cache_server_t, nkn_vcs_t;

extern nkn_vcs_t *vc;

typedef struct nkn_vc_client {
    int                       socket;
    int32_t                   result_code;
    vccmp_connect_req_data_t  data;
    nkn_vc_channel_t *        channel;
} nkn_vc_client_t;
 
// typedef struct nkn_virt_cache_server {
//     char *       mountpoint;
//     int          socket;
//     short        port;
//     vcs_workq_t  wq;
// } nkn_virt_cache_server_t;


nkn_vc_channel_t *vcs_channel_find_by_name(
    const char *name);
void vcs_slot_put(
    nkn_vc_channel_t *ch,
    nkn_vc_slot_t *slot,
    uint32_t slot_index);

// inline functions
static inline int vcs_send(
    int s,
    void *r)
{
    unsigned char *p = (unsigned char *)r;
    vccmp_msg_header_t *h = (vccmp_msg_header_t *)r;
    size_t len = h->msg_length;
    int rc;

    while (len > 0) {
	rc = send(s, p, len, 0);
	if (rc == 0) {
	    DBG_VCM3("socket(%d) closed.\n", s);
	    AO_fetch_and_add1(&vc->total_lost_cnt);
	    return 0;
	}
	if (rc < 0) {
	    DBG_VCE("send(%d) failed: %s\n", s, strerror(errno));
	    AO_fetch_and_add1(&vc->total_socket_error_cnt);
	    return -1;
	}
	p += rc;
	len -= rc;
    }

    return h->msg_length;
}

static inline int vcs_recv_msg_header(
    int s,
    vccmp_msg_header_t *h)
{
    int rc, len = sizeof(vccmp_msg_header_t);
    unsigned char *p = (void *)h;

    while (len > 0) {
	rc = recv(s, p, len, 0);
	if (rc == 0) {
	    DBG_VCM3("socket(%d) closed.\n", s);
	    AO_fetch_and_add1(&vc->total_lost_cnt);
	    return 0;
	} else if (rc < 0) {
	    DBG_VCE("recv(%d) failed: %s\n", s, strerror(errno));
	    AO_fetch_and_add1(&vc->total_socket_error_cnt);
	    return -1;
	}
	p += rc;
	len -= rc;
    }

    return h->msg_length;
}

static inline int vcs_recv(
    int s,
    void *buf,
    uint32_t len)
{
    int rc;
    uint32_t size = len;
    unsigned char *p = buf;

    while (len > 0) {
	rc = recv(s, p, len, 0);
	if (rc == 0) {
	    DBG_VCM3("socket(%d) closed.\n", s);
	    AO_fetch_and_add1(&vc->total_lost_cnt);
	    return 0;
	} else if (rc < 0) {
	    DBG_VCE("recv(%d) failed: %s\n", s, strerror(errno));
	    AO_fetch_and_add1(&vc->total_socket_error_cnt);
	    return rc;
	}
	p += rc;
	len -= rc;
    }
    return size;
}

static inline int vcs_so_recvtimeo(
    int s,
    int timeout)
{
    struct timeval tv;
    int rc;

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    rc = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (rc < 0) {
	DBG_VCE("setsockopt(%d, SO_RCVTIMEO) failed: %s\n", s, strerror(errno));
	return -1;
    }
    return 0;
}

static inline int vcs_client_is_connected(
    nkn_vc_client_t *client)
{
    return (client->channel != NULL);
}

#define NKN_DONT_CHECK 0

static inline int vcs_check_msg_header(
    vccmp_msg_header_t *h,
    uint32_t type,
    uint32_t length)
{
    if (h->msg_type != type) {
	DBG_VCE("Invalid message type '%s'(%d), expected '%s'(%d)\n", 
		get_msg_type_string(h->msg_type), h->msg_type,
		get_msg_type_string(type), type);
	return -1;
    }

    if ((length != NKN_DONT_CHECK) && (length != h->msg_length)) {
	DBG_VCE("%s invalid length '%d', expected '%d'\n", 
		get_msg_type_string(type), h->msg_length, length);
	return -1;
    }

    return 0;
}

static inline void vcs_channel_lock(
    nkn_vc_channel_t *ch)
{
    pthread_mutex_lock(&ch->lock);
}

static inline void vcs_channel_unlock(
    nkn_vc_channel_t *ch)
{
    pthread_mutex_unlock(&ch->lock);
}

static inline nkn_vc_slot_t *vcs_slot_from_handle(
    nkn_vc_slot_handle_t handle)
{
    return (nkn_vc_slot_t *)((uint64_t)(handle.fields.ptr));
}

static inline int vcs_inc_slot_index(int cur_slot)
{
    if (cur_slot == (VCCMP_SLOTS_PER_CHANNEL-1))
	return 0;
    return ++cur_slot;
}

/*
 * NB: If the provided slot handle is used in a channel,
 *     then that channel must be locked.
 */
static inline uint8_t vcs_slot_nonce(
    nkn_vc_slot_handle_t handle)
{
    return handle.fields.nonce;
}

static inline nkn_vc_slot_t *vcs_slot_get(
    nkn_vc_channel_t *ch,
    uint32_t slot_index,
    uint8_t nonce)
{
    nkn_vc_slot_t *slot = NULL;
    nkn_vc_slot_handle_t handle;

    if (slot_index >= VCCMP_SLOTS_PER_CHANNEL)
	return NULL;


    handle = ch->vcs_vc_slot_table[slot_index];
    if (handle.fields.nonce != nonce) {
	DBG_VCE("Mis-matched nonce for slot %u handle:%d caller:%d", 
	    slot_index, (int)handle.fields.nonce, (int)nonce);
	return NULL;
    }
    
    slot = vcs_slot_from_handle(handle);
    if (slot != NULL) {
        AO_fetch_and_add1(&slot->usage_count);
    }
    return slot;
}

static inline void vcs_vc_lock(void)
{
    pthread_mutex_lock(&vc->lock);
}

static inline void vcs_vc_unlock(void)
{
    pthread_mutex_unlock(&vc->lock);
}

#endif /* __VCS_PRIVATE_H__ */

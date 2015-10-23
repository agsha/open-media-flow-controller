/*============================================================================
 *
 *
 * Purpose: This file implements the public and private virt_cache_server 
 *          functions.
 *
 * Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
 *
 *
 *============================================================================*/
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <inttypes.h>

#include <unistd.h>
#include <netinet/in.h>
#include <alloca.h>

#include <signal.h>
#include <pthread.h>
#include <poll.h>
#include <sys/mount.h>
#include <sys/prctl.h>

#include "vcs_virt.h"
#include "vcs_net.h"
#include "vcs_uri.h"
#include "vcs_workq.h"
#include "vcs_fuse.h"
#include "vcs_private.h"
#include "nkn_virt_cache_server.h"

#include "nkn_util.h"
#include "nkn_stat.h"

#define VCS_NO_RESPONSE             1
#define VCS_CLOSE_SOCKET            0       // request a graceful shutdown


/* Strings used for monitoring virtcache server */
#define VCS_TOTAL_CLIENTS_CNT		"virtcache.server.total_clients_cnt"
#define VCS_CURRENT_CLIENTS_CNT		"virtcache.server.current_clients_cnt"
#define VCS_TOTAL_DISCONNECT_CNT	"virtcache.server.total_disconnect_cnt"
#define VCS_TOTAL_LOST_CNT		"virtcache.server.total_lost_cnt"
#define VCS_TOTAL_SOCK_ERROR_CNT	"virtcache.server.total_socket_error_cnt"
#define VCS_TOTAL_CONNECT_REQUESTS_CNT	"virtcache.server.total_connect_requests_cnt"
#define VCS_TOTAL_OPEN_REQUESTS_CNT	"virtcache.server.total_open_requests_cnt"
#define VCS_TOTAL_STAT_REQUESTS_CNT	"virtcache.server.total_stat_requests_cnt"
#define VCS_TOTAL_CLOSE_REQUESTS_CNT	"virtcache.server.total_close_requests_cnt"
#define VCS_TOTAL_OPEN_FILES_CNT	"virtcache.server.total_open_files_cnt"
#define VCS_CURRENT_OPEN_FILES_CNT	"virtcache.server.current_open_files_cnt"
#define VCS_TOTAL_BYTES__READ_CNT	"virtcache.server.total_bytes_read_cnt"
#define VCS_TOTAL_OPEN_ERRORS_CNT	"virtcache.server.total_open_errors_cnt"
#define VCS_TOTAL_STAT_ERRORS_CNT	"virtcache.server.total_stat_errors_cnt"
#define VCS_TOTAL_READ_ERRORS_CNT	"virtcache.server.total_read_errors_cnt"

/* Strings used for monitoring virtcache channel */
#define VCS_CHAN_x_TOTAL_OPEN_REQUESTS_CNT_FMT     "virtcache.chan.%d.total_open_requests_cnt"
#define VCS_CHAN_x_TOTAL_STAT_REQUESTS_CNT_FMT     "virtcache.chan.%d.total_stat_requests_cnt"
#define VCS_CHAN_x_TOTAL_CLOSE_REQUESTS_CNT_FMT    "virtcache.chan.%d.total_close_requests_cnt"
#define VCS_CHAN_x_TOTAL_OPEN_FILES_CNT_FMT        "virtcache.chan.%d.total_open_files_cnt"
#define VCS_CHAN_x_CURRENT_OPEN_FILES_CNT_FMT      "virtcache.chan.%d.current_open_files_cnt"
#define VCS_CHAN_x_TOTAL_BYTES__READ_CNT_FMT       "virtcache.chan.%d.total_bytes_read_cnt"
#define VCS_CHAN_x_TOTAL_OPEN_ERRORS_CNT_FMT       "virtcache.chan.%d.total_open_errors_cnt"
#define VCS_CHAN_x_TOTAL_STAT_ERRORS_CNT_FMT       "virtcache.chan.%d.total_stat_errors_cnt"
#define VCS_CHAN_x_TOTAL_READ_ERRORS_CNT_FMT       "virtcache.chan.%d.total_read_errors_cnt"
#define VCS_CHAN_x_TOTAL_OPEN_VIRT_FILES_CNT_FMT   "virtcache.chan.%d.total_open_virt_files_cnt"
#define VCS_CHAN_x_CURRENT_OPEN_VIRT_FILES_CNT_FMT "virtcache.chan.%d.current_open_virt_files_cnt"


#define PTR2FD(p) ((int)((unsigned long)(p)))
#define FD2PTR(fd) ((void *)((unsigned long)(fd)))

nkn_vcs_t virt_cache, *vc = &virt_cache;

nkn_vc_channel_t *vcs_channel_find_by_name(
    const char *name) 
{
    int i;
    nkn_vc_channel_t *ch;

    for (i = 0; i < VCS_N_CHANNELS; i++) {
	ch = &vc->vcs_vc_channels[i];
	if (strcmp(ch->name, name) == 0) {
	    return ch;
	}
    }
    return NULL;
}

static void vcs_get_disk_name_for_channel(nkn_vc_channel_t* ch)
{
    if (ch->usage_count) {
	DBG_VCE("get_disk_name - channel usage count is not 0!\n");
    } else if (ch->disk_name[0] == '\0') {
	strcpy(ch->disk_name, "vdl");
    } else if (ch->disk_name[2] == 'z') {
	ch->disk_name[2] = 'l';
    } else {
	ch->disk_name[2] = (ch->disk_name[2])++;
    }
//     } else {
// 	strcpy(ch->disk_name, VCS_DISK);
//     }
}

/*
 * Returns an pointer to the first empty channel.
 */
static nkn_vc_channel_t* vcs_channel_get_for_vm(const char *vm)
{
    int i;
    nkn_vc_channel_t *ch = NULL;

    vcs_vc_lock();

    for (i = 0; i < VCS_N_CHANNELS; i++) {
	ch = &vc->vcs_vc_channels[i];
	vcs_channel_lock(ch);
	if (ch->usage_count) {
	    if (strncmp(ch->vm_name, vm, VCCMP_MAX_VM_NAME_STR) == 0) {
		goto found;
	    }
	}
	vcs_channel_unlock(ch);

    }

    for (i = 0; i < VCS_N_CHANNELS; i++) {
	ch = &vc->vcs_vc_channels[i];
	vcs_channel_lock(ch);
	if (ch->usage_count == 0) {
	    strncpy(ch->vm_name, vm, sizeof(ch->vm_name)-1);
	    vcs_get_disk_name_for_channel(ch);
	    goto found;
	}
	vcs_channel_unlock(ch);
    }

    vcs_vc_unlock();
    return NULL;

found:
    ch->usage_count++;
    DBG_VCM3("%d: '%s' usage-count:%d\n", ch->id, vm, ch->usage_count);
    vcs_channel_unlock(ch);
    vcs_vc_unlock();
    return ch;
}

static nkn_vc_channel_t *vcs_channel_get(
    uint8_t channel_id)
{
    nkn_vc_channel_t *ch = &vc->vcs_vc_channels[channel_id];

    vcs_channel_lock(ch);
    ch->usage_count++;
    DBG_VCM3("%d: '%s' usage-count:%d\n", 
	      ch->id, ch->vm_name, ch->usage_count);
    vcs_channel_unlock(ch);
    return ch;
}

static inline bool vcs_channel_is_attached(
    nkn_vc_channel_t *ch)
{
    return (ch->vm_name[0] != 0);
}

static void vcs_channel_clear_stats(
    nkn_vc_channel_t *ch)
{
    AO_store(&ch->total_open_requests_cnt, 0);
    AO_store(&ch->total_stat_requests_cnt, 0);
    AO_store(&ch->total_close_requests_cnt, 0);
    AO_store(&ch->total_open_files_cnt, 0);
    AO_store(&ch->current_open_files_cnt, 0);
    AO_store(&ch->total_bytes_read_cnt, 0);
    AO_store(&ch->total_open_errors_cnt, 0);
    AO_store(&ch->total_stat_errors_cnt, 0);
    AO_store(&ch->total_read_errors_cnt, 0);
    AO_store(&ch->total_open_virt_files_cnt, 0);
    AO_store(&ch->current_open_virt_files_cnt, 0);
}

static int vcs_channel_put(
    nkn_vc_channel_t *ch)
{
    int rc;
    int usage_count;

    vcs_vc_lock();
    vcs_channel_lock(ch);
    if (ch->usage_count == 0) {
	DBG_VCE("channel usage count is 0.\n");
    } else {
	ch->usage_count--;
	DBG_VCM3("%d: usage-count:%d\n", ch->id, ch->usage_count);
	if (ch->usage_count == 0) {
	    if (vcs_channel_is_attached(ch)) {
		rc = vcs_virt_detach_device(ch->vm_name, ch->path, ch->disk_name);
		if (rc < 0) {
		    DBG_VCE("Failed to detach disk '%s'.\n", ch->disk_name);
		}
	    }
	    memset(ch->vm_name, 0, sizeof(ch->vm_name));
	}
    }
    usage_count = ch->usage_count;
    vcs_channel_unlock(ch);
    vcs_vc_unlock();
    return usage_count;
    
}

/* Must be called with the channel locked. */
static int
vcs_find_open_slot(
    nkn_vc_channel_t *ch)
{
    int ending_slot = ch->next_slot;

    do {
	ch->next_slot = vcs_inc_slot_index(ch->next_slot);
	if (vcs_slot_from_handle(ch->vcs_vc_slot_table[ch->next_slot]) == NULL) {
	    return ch->next_slot;
	}
    } while (ch->next_slot != ending_slot);

    return -1;
}

static int vcs_do_query_channel(
    nkn_vc_client_t *client,
    uint64_t msg_id,
    uint8_t channel_id)
{
    vccmp_query_channel_rsp_msg_t rsp;
    nkn_vc_channel_t *ch;
    int i, j, n = 0, count = 0;

    if (channel_id >= VCS_N_CHANNELS) {
	client->result_code = EINVAL;
	DBG_VCE("Invalid channel: %u\n", channel_id);
	vccmp_setup_query_channel_rsp_msg(&rsp, msg_id, 
					  client->result_code, 0, NULL);
	return vcs_send(client->socket, &rsp);
    }

    ch = &vc->vcs_vc_channels[channel_id];
    vcs_channel_lock(ch);

    vccmp_setup_query_channel_rsp_msg(&rsp, msg_id, 0, 
				      ch->usage_count, ch->vm_name);

    for (i = 0; i < VCCMP_SLOTS_PER_CHANNEL / 64; i++) {
	uint64_t val = 0;
	for (j = 0; j < 64; j++, n++) {
	    if (vcs_slot_from_handle(ch->vcs_vc_slot_table[n]) != NULL) {
		val |= (1ULL << j);
		count++;
	    }
	}
	rsp.msg_data.bitmask[i] = val;
    }

    DBG_VCM("  channel:       %u\n"
	     "  slots in use:  %d of %u\n"
	     "  VM name:       '%s'\n"
	     "  usage count:   %d\n",
	     channel_id, count, VCCMP_SLOTS_PER_CHANNEL,
	     ch->vm_name, ch->usage_count);

    vcs_channel_unlock(ch);

    return vcs_send(client->socket, &rsp);
}

static int vcs_do_query_slot(
    nkn_vc_client_t *client,
    uint64_t msg_id,
    vccmp_query_slot_req_data_t *data)
{
    int rc;
    vccmp_query_slot_rsp_msg_t rsp, *p = &rsp;
    uint8_t channel_id;
    nkn_vc_channel_t *ch;
    nkn_vc_slot_t *slot;
    uint32_t uri_len = 0;
    int32_t result_code = 0;
    uint8_t nonce;
    char *uri = NULL;
    uint64_t file_length = 0;
    uint64_t client_id = VCCMP_INVALID_CLIENT_ID;

    channel_id = data->channel_id;

    if (channel_id >= VCS_N_CHANNELS) {
	client->result_code = EINVAL;
	vccmp_setup_query_slot_rsp_msg(&rsp, msg_id, client->result_code,
				       0, 0, 0, 0);
	return vcs_send(client->socket, &rsp);
    }

    ch = &vc->vcs_vc_channels[channel_id];
    vcs_channel_lock(ch);
    slot = vcs_slot_from_handle(ch->vcs_vc_slot_table[data->slot_index]);
    if (slot != NULL) {
	uri_len = strlen(slot->uri);
	p = nkn_malloc_type(sizeof(vccmp_query_slot_rsp_msg_t) + uri_len + 1, 
			    mod_vcs_vccmp_query_slot_rsp_msg_t);
	if (p != NULL) {
	    p->msg_data.uri_len = uri_len;
	    uri = vcs_uriput(&p->msg_data.uri_len, slot->uri);
	} else {
	    result_code = ENOMEM;
	}
	file_length = slot->file_length;
	client_id = slot->client_id;
    }
    nonce = vcs_slot_nonce(ch->vcs_vc_slot_table[data->slot_index]);
    vcs_channel_unlock(ch);

    if (result_code != 0) {
	client->result_code = result_code;
	vccmp_setup_query_slot_rsp_msg(&rsp, msg_id, client->result_code,
				       0, 0, 0, 0);
	return vcs_send(client->socket, &rsp);
    }

    vccmp_setup_query_slot_rsp_msg(p, msg_id, 0,
				   nonce, client_id, file_length, uri_len);
    rc = vcs_send(client->socket, p);

    DBG_VCM("slot[%u:%u] %s\n", channel_id, data->slot_index,
	     (uri != NULL) ? uri : "?????");

    if (p != &rsp)
	free(p);

    return rc;			   
}

/*
 * RETURNED VALUE MUST BE free()ED
 */
static char *vcs_path(
    const char *dir,
    const char *file)
{
    char *p;
    int dlen = strlen(dir), size = dlen + strlen(file) + 2;
    
//    p = nkn_malloc_type(size, mod_vcs_path_str);
    p = malloc(size);
    if (p == NULL) {
	DBG_OOM();
	return NULL;
    }

    if ((dlen > 0) && dir[dlen] != '/') {
	sprintf(p, "%s/%s", dir, file);
    } else {
	sprintf(p, "%s%s", dir, file);
    }
    return p;
}

static int vcs_do_connect(
    nkn_vc_client_t *client, 
    vccmp_connect_req_data_t *data)
{
    vccmp_connect_rsp_msg_t rsp;
    nkn_vc_channel_t *ch = NULL;
    int rc;
    
    if (vcs_client_is_connected(client)) {
	client->result_code = EALREADY;
	vccmp_setup_connect_rsp_msg(&rsp, client->result_code, 
	    VCCMP_INVALID_CHANNEL_ID);
	DBG_VCE("socket(%d) already connected.\n", client->socket);
	return vcs_send(client->socket, &rsp);
    }

    if (data->client_id == VCCMP_INVALID_CLIENT_ID) {
	client->result_code = EINVAL;
	vccmp_setup_connect_rsp_msg(&rsp, client->result_code, 
	    VCCMP_INVALID_CHANNEL_ID);
	DBG_VCE("invalid client id.\n");
	return vcs_send(client->socket, &rsp);
    }

    if (data->client_id == client->data.client_id) {
	client->result_code = EINVAL;
	vccmp_setup_connect_rsp_msg(&rsp, client->result_code, 
	    VCCMP_INVALID_CHANNEL_ID);
	DBG_VCE("client doesn't match: (expected %"PRIu64", got %"PRIu64")\n",
		client->data.client_id, data->client_id);
	return vcs_send(client->socket, &rsp);
    }

    if (data->vccmp_major != VCCMP_MAJOR_VER) {
	client->result_code = EINVAL;
	vccmp_setup_connect_rsp_msg(&rsp, client->result_code, 
	    VCCMP_INVALID_CHANNEL_ID);
	DBG_VCE("invalid MAJOR version: (expected %d, got %d)\n",
		VCCMP_MAJOR_VER, data->vccmp_major);
	return vcs_send(client->socket, &rsp);
    }

    if (data->vccmp_major != VCCMP_MINOR_VER) {
	client->result_code = EINVAL;
	vccmp_setup_connect_rsp_msg(&rsp, client->result_code, 
	    VCCMP_INVALID_CHANNEL_ID);
	DBG_VCE("invalid MINOR version: (expected %d, got %d)\n",
		VCCMP_MINOR_VER, data->vccmp_minor);
	return vcs_send(client->socket, &rsp);
    }

    /* FIXME: verify MFC version */

    ch = vcs_channel_get_for_vm(data->vm_name);
    if (ch == NULL) {
	client->result_code = EBUSY;
	vccmp_setup_connect_rsp_msg(&rsp, client->result_code, 
	    VCCMP_INVALID_CHANNEL_ID);
	DBG_VCE("no available channels.\n");
	return vcs_send(client->socket, &rsp);
    }

    rc = vcs_virt_attach_device(data->vm_name, ch->path, ch->disk_name);
    if (rc < 0) {
	vcs_channel_put(ch);
	client->result_code = EFAULT;
	vccmp_setup_connect_rsp_msg(&rsp, client->result_code, 
	    VCCMP_INVALID_CHANNEL_ID);
	DBG_VCE("Failed to attach virtual device: %s\n", strerror(rc));
	return vcs_send(client->socket, &rsp);
    }
    client->data = *data;
    client->channel = ch;

    vcs_channel_clear_stats(ch);

    vccmp_setup_connect_rsp_msg(&rsp, 0, ch->id);
    DBG_VCM("  client:  %"PRIu64"\n"
	     "  channel: %u\n"
	     "  version: %s (%d.%d)\n",
	     data->client_id,
	     ch->id,
	     data->mfc_ver, data->vccmp_major, data->vccmp_minor);
    return vcs_send(client->socket, &rsp);
}

static void vcs_slot_free(
    nkn_vc_slot_t *slot)
{
    if (slot != NULL) {
	if (slot->uri != NULL) {
	    free(slot->uri);
	}
	if (slot->vfp != NULL) {
            vfs_funs.pnkn_fclose(slot->vfp);
	}
	free(slot);
    }
}

/*
 * NB: must be called with channel locked.
 */
static void vcs_slot_close(
    nkn_vc_channel_t *ch,
    uint32_t slot_index)
{
    nkn_vc_slot_handle_t *handle;
    nkn_vc_slot_t *slot;
    uint8_t nonce;

    handle = &ch->vcs_vc_slot_table[slot_index];
    nonce = vcs_slot_nonce(*handle);
    slot = vcs_slot_from_handle(*handle);

    if (slot == NULL) {
	DBG_VCW("slot close called on empty slot.\n");
	return;
    }

    if (AO_load(&slot->usage_count)) {
        AO_store(&slot->close_flag, 1);
	DBG_VCM3("%u: set CLOSE flag\n", slot_index);
    } else {
	handle->value = 0;
	handle->fields.nonce = ++nonce;
	AO_fetch_and_sub1(&vc->current_open_files_cnt);
	AO_fetch_and_sub1(&ch->current_open_files_cnt);
	vcs_slot_free(slot);
	DBG_VCM3("%u: nonce:%u\n", slot_index, nonce);
    }
}

void vcs_slot_put(
    nkn_vc_channel_t *ch,
    nkn_vc_slot_t *slot,
    uint32_t slot_index)
{
    if (slot == NULL) 
	return;

    AO_fetch_and_sub1(&slot->usage_count);
    if (AO_load(&slot->close_flag) && (AO_load(&slot->usage_count) == 0)) {
	vcs_channel_lock(ch);
	vcs_slot_close(ch, slot_index);
	vcs_channel_unlock(ch);
    }
}

/*
 * NB: must be called with channel locked.
 */
static int vcs_channel_close_client_slots(
    nkn_vc_channel_t *ch, 
    uint64_t client_id)
{
    int i, n = 0;
    nkn_vc_slot_t *slot;

    DBG_VCM3(" ID:%"PRIu64"\n", client_id);

    for (i = 0; i < VCCMP_SLOTS_PER_CHANNEL; i++) {
	slot = vcs_slot_from_handle(ch->vcs_vc_slot_table[i]);
	if ((slot != NULL) && (slot->client_id == client_id)) {
	    vcs_slot_close(ch, i);
	    n++;
	}
    }
    return n;
}

static int vcs_do_disconnect(
    nkn_vc_client_t *client, 
    uint8_t channel_id)
{
    uint64_t client_id = client->data.client_id;
    nkn_vc_channel_t *ch;
    int n = 0;

    DBG_VCM("processing disconnect request");
    ch = client->channel;
    if (ch == NULL) {
	DBG_VCW("client not connected.\n");
	ch = vcs_channel_get(channel_id);
    } else {
	if (ch->id != channel_id) {
	    DBG_VCW("channel ID:%d wrong, expected %d\n", channel_id, ch->id);
	    channel_id = ch->id;
	}
    }

    /* you're not paranoid, if they're really out to get you. */
    if (client_id != VCCMP_INVALID_CLIENT_ID) {
	vcs_channel_lock(ch);
	n = vcs_channel_close_client_slots(ch, client_id);
	vcs_channel_unlock(ch);
    }
    

    vcs_channel_put(ch);
    client->channel = NULL; 

    if (n > 0) {
	DBG_VCW("channel[%d]: closed %d slots for client '%"PRIu64"'\n",
		channel_id, n, client_id);
    }
    
    return VCS_CLOSE_SOCKET;
}

static int vcs_do_stat(
    nkn_vc_client_t *client,
    uint64_t msg_id,
    vccmp_stat_req_data_t *data)
{
    vccmp_stat_rsp_msg_t rsp;
    vccmp_stat_data_t stat_data;
    char *uri = ((char*)data) + sizeof(vccmp_stat_data_t);
    int rc;

    rc = vfs_funs.pnkn_vcstat(uri, &stat_data);
    if (rc < 0) {
	client->result_code = errno;
	DBG_VCE("pnkn_vcstat() failed for '%s': %s\n", uri, strerror(errno));
	AO_fetch_and_add1(&vc->total_stat_errors_cnt);
	AO_fetch_and_add1(&client->channel->total_stat_errors_cnt);
	vccmp_setup_stat_rsp_msg(&rsp, msg_id, client->result_code, NULL);
	return vcs_send(client->socket, &rsp);
    }

    vccmp_setup_stat_rsp_msg(&rsp, msg_id, 0, &stat_data);
    return vcs_send(client->socket, &rsp);
}

static nkn_vc_slot_t *vcs_new_slot(
    char* uri)
{
    nkn_vc_slot_t *slot;

    slot = nkn_malloc_type(sizeof(nkn_vc_slot_t), mod_vcs_nkn_vc_slot_t);
    if (slot == NULL) {
	DBG_OOM();
	return NULL;
    }
    memset(slot, 0, sizeof(nkn_vc_slot_t));

    slot->uri = nkn_strdup_type(uri, mod_vcs_strdup_t);
    if (slot->uri == NULL) {
	DBG_OOM();
	free(slot);
	return NULL;
    }

    return slot;
}

/*
 * NB: must be called with channel locked.
 */
static void vcs_slot_set(
    nkn_vc_channel_t *ch,
    nkn_vc_slot_t *slot,
    uint32_t slot_index,
    uint8_t nonce)
{
    nkn_vc_slot_handle_t handle;
    
    handle.value = (uint64_t)slot;
    handle.fields.nonce = nonce;
    ch->vcs_vc_slot_table[slot_index] = handle;
}

static int vcs_do_open(
    nkn_vc_client_t *client,
    uint64_t msg_id,
    vccmp_open_req_data_t *data)
{
    vccmp_open_rsp_msg_t msg;
    nkn_vc_channel_t *ch;
    nkn_vc_slot_t *slot = NULL;
    vccmp_stat_data_t stat_data;
    vccmp_slot_data_t slot_data;
    int slot_index;
    uint8_t nonce = 0;
    char *uri = ((char*)data) + sizeof(vccmp_open_req_data_t);

    if (!vcs_client_is_connected(client)) {
	client->result_code = ENOTCONN;
	DBG_VCE("client not connected.\n");
	goto error;
    }

    ch = client->channel;
    AO_fetch_and_add1(&ch->total_open_requests_cnt);

    slot = vcs_new_slot(uri);
    if (slot == NULL) {
	client->result_code = ENOMEM;
	goto error;
    }
    slot->client_id = client->data.client_id;

    slot->vfp = vfs_funs.pnkn_vcopen(slot->uri, &stat_data);
    if (slot->vfp == NULL) {
	client->result_code = errno;
	DBG_VCE("pnkn_vcopen('%s') failed: %s\n", slot->uri, strerror(errno));
	goto error;
    }
    slot->file_length = stat_data.file_length;
    slot->streaming_read_size = data->streaming_read_size;

    vcs_channel_lock(ch);
    slot_index = vcs_find_open_slot(ch);
    if (slot_index >= 0) {
	nonce = vcs_slot_nonce(ch->vcs_vc_slot_table[slot_index]);	
	vcs_slot_set(ch, slot, slot_index, nonce);

	DBG_VCM("  %s\n"
		 "  channel: %u\n"
		 "  slot:    %u\n"
		 "  nonce:   %u\n",
		 slot->uri, ch->id, slot_index, nonce);
    }
    vcs_channel_unlock(ch);

    if (slot_index < 0) {
	client->result_code = ENOSPC;
	DBG_VCE("no slot available in channel '%d'\n", ch->id);
	goto error;
    }
    
    slot_data.index = slot_index;
    slot_data.nonce = nonce;

    AO_fetch_and_add1(&vc->total_open_files_cnt);
    AO_fetch_and_add1(&vc->current_open_files_cnt);
    AO_fetch_and_add1(&ch->total_open_files_cnt);
    AO_fetch_and_add1(&ch->current_open_files_cnt);
    vccmp_setup_open_rsp_msg(&msg, msg_id, 0, &slot_data, &stat_data);
    return vcs_send(client->socket, &msg);

error:
    if (slot != NULL) {
	vcs_slot_free(slot);
    }
    AO_fetch_and_add1(&vc->total_open_errors_cnt);
    AO_fetch_and_add1(&ch->total_open_errors_cnt);
    vccmp_setup_open_rsp_msg(&msg, msg_id, client->result_code, NULL, NULL);
    return vcs_send(client->socket, &msg);
}

static int vcs_do_close(
    nkn_vc_client_t *client,
    uint32_t slot_index)
{
    nkn_vc_channel_t *ch;

    if (!vcs_client_is_connected(client)) {
	DBG_VCE("client not connected\n");
	return VCS_NO_RESPONSE;
    }

    if (slot_index >= VCCMP_SLOTS_PER_CHANNEL) {
	DBG_VCE("invalid slot index '%d'\n", slot_index);
	return VCS_NO_RESPONSE;
    }

    ch = client->channel;
    AO_fetch_and_add1(&ch->total_close_requests_cnt);
    vcs_channel_lock(ch);
    vcs_slot_close(ch, slot_index);
    vcs_channel_unlock(ch);

    DBG_VCM("  channel: %u\n"
	     "  slot:    %u\n",
	     ch->id, slot_index);

    return VCS_NO_RESPONSE;
}

static void
vcs_close_client(
    nkn_vc_client_t *client,
    int rc)
{
    nkn_vc_channel_t *ch;
    int n;

    DBG_VCM3(" ID:%"PRIu64"\n", client->data.client_id);

    if (rc) {
	DBG_VCW("unexpected client shutdown.\n");
    }

    close(client->socket);
    ch = client->channel;

    if (vcs_client_is_connected(client)) {
	DBG_VCW("client exiting while still connected. channel:%p\n", 
	    client->channel);
	n = vcs_channel_close_client_slots(ch, client->data.client_id);
	if (n > 0) {
	    DBG_VCW("closed %d slots\n", n);
	}
    }
    
    memset(client, 0, sizeof(nkn_vc_client_t));

    if (ch == NULL)
	return;

    vcs_channel_put(ch);
}

static void
vcs_worker(
    void *arg)
{
    int s = PTR2FD(arg), rc;
    vccmp_msg_header_t h;
    unsigned char *p = NULL;
    nkn_vc_client_t client;
    
    memset(&client, 0, sizeof(client));
    client.socket = PTR2FD(arg);

    union {
	vccmp_connect_req_data_t connect;
	vccmp_disconnect_req_data_t disconnect;
	vccmp_close_req_data_t close;
	vccmp_query_channel_req_data_t channel;
	vccmp_query_slot_req_data_t slot;
    } req;

    DBG_VCM3("started worker for socket(%d).\n", s);
    AO_fetch_and_add1(&vc->total_clients_cnt);
    AO_fetch_and_add1(&vc->current_clients_cnt);

    while (1) {
	uint32_t len;
	memset(&h, 0, sizeof(h));

	rc = vcs_recv_msg_header(s, &h);
	if (rc <= 0) {
	    goto error;
	}

	DBG_VCM("--- %"PRIu64" : %s ---\n", 
		 h.msg_id, get_msg_type_string(h.msg_type));

	len = h.msg_length - sizeof(h);
	if (h.msg_type == VCCMPMT_OPEN_REQ || h.msg_type == VCCMPMT_STAT_REQ) {
	    p = nkn_malloc_type(len+1, ((h.msg_type == VCCMPMT_STAT_REQ) ? 
					mod_vcs_stat_req_t : mod_vcs_open_req_t));
	    if (p == NULL) {
		DBG_OOM();
		goto error;
	    }
	    memset(p, 0, len+1);
	} else {
	    memset(&req, 0, sizeof(req));
	    p = (unsigned char *)&req;

	    if (len > sizeof(req)) {
		DBG_VCE("%s: length '%d' too long (max %lu bytes)\n", 
			get_msg_type_string(h.msg_type), len, sizeof(req));
		goto error;
	    }
	}

	rc = vcs_recv(s, p, len);
	if (rc <= 0) {
	    DBG_VCE("Failed to read the request data from socket! error:%d", rc);
	    goto error;
	}

	switch (h.msg_type) {
	case VCCMPMT_CONNECT_REQ:
	    AO_fetch_and_add1(&vc->total_connect_requests_cnt);
	    rc = vcs_do_connect(&client, &req.connect);
	    if (rc <= 0)
		goto error;
	    break;
	case VCCMPMT_DISCONNECT_REQ:
	    AO_fetch_and_add1(&vc->total_disconnect_cnt);
	    rc = vcs_do_disconnect(&client, req.disconnect.channel_id);
	    if (rc <= 0)
		goto error;
	    break;
	case VCCMPMT_QUERY_CHANNEL_REQ:
	    rc = vcs_do_query_channel(&client, h.msg_id, 
				      req.channel.channel_id);
	    break;
	case VCCMPMT_QUERY_SLOT_REQ:
	    rc = vcs_do_query_slot(&client, h.msg_id, &req.slot);
	    break;
	case VCCMPMT_OPEN_REQ:
	    AO_fetch_and_add1(&vc->total_open_requests_cnt);
	    rc = vcs_do_open(&client, h.msg_id, (vccmp_open_req_data_t *)p);
	    break;
	case VCCMPMT_CLOSE_REQ:
	    AO_fetch_and_add1(&vc->total_close_requests_cnt);
	    rc = vcs_do_close(&client, req.close.slot_data.index);
	    break;
	case VCCMPMT_STAT_REQ:
	    AO_fetch_and_add1(&vc->total_stat_requests_cnt);
	    rc = vcs_do_stat(&client, h.msg_id, (vccmp_stat_req_data_t *)p);
	    break;
	default:
	    DBG_VCE("%"PRIu64": invalid message type '%d'\n", 
		    h.msg_id, h.msg_type);
	    goto error;
	}

	if (p && p != (unsigned char *)&req) {
	    free(p);
	    p = NULL;
	}
    }
    // fall-through

error:
    AO_fetch_and_sub1(&vc->current_clients_cnt);
    vcs_close_client(&client, rc);
    if (p && p !=  (unsigned char *)&req)
	free(p);
    return;

}

static void *
vcs_accept_thread(
    void *arg)
{
    int s, rc;
    struct pollfd pfd;
    struct stat stbuf;

    UNUSED_ARGUMENT(arg);
    memset(&stbuf, 0, sizeof(stbuf));

    prctl(PR_SET_NAME, "nvsd-vcsaccpt", 0, 0, 0);

    pfd.fd = vc->acceptor.socket;
    pfd.events = POLLIN;
    pfd.revents = 0;

    // NB: We need to hold off allowing connections until the FUSE mount 
    //     point is up and running. Otherwise, it produces bad state in the
    //     connecting VM, and the only way to clear it is to shut the VM down.
    pthread_mutex_lock(&vc->acceptor.mutex);
    pthread_cond_wait(&vc->acceptor.cond, &vc->acceptor.mutex);
    pthread_mutex_unlock(&vc->acceptor.mutex);
    sleep(1);

    while (1) {
	rc = poll(&pfd, 1, -1);
	if (rc < 0) {
	    continue;
	}

	s = accept(vc->acceptor.socket, NULL, NULL);
	DBG_VCM3("accept: %d\n", s);

	rc = vcs_workq_submit(&vc->wq, vcs_worker, FD2PTR(s));
	if (rc != 0) {
	    DBG_VCE("vcs_workq_submit(): %s\n", strerror(errno));
	    continue;
	}
    }
    return NULL;
}

static void vcs_cleanup_mount_point(void)
{
    struct stat stbuf;
    memset(&stbuf, 0, sizeof(stbuf));

    // Make sure its unmounted.
    if (lstat(VCS_MOUNT_POINT, &stbuf) == -1) {
        umount2(VCS_MOUNT_POINT, 2);
    }
}

static int vcs_setup_mount_point(void)
{
    int rc;
    char* mount_dir = NULL;
    struct stat stbuf;

    memset(&stbuf, 0, sizeof(stbuf));

    mount_dir = alloca(strlen(VCS_MOUNT_DIR + 1));
    strcpy(mount_dir, VCS_MOUNT_DIR);
    rc = nkn_create_directory(mount_dir, 1);
    if (rc < 0) {
        DBG_VCS("Failed to create mount point: %s", mount_dir);
        return -1;
    }

    // Make sure its unmounted.
    if (lstat(VCS_MOUNT_POINT, &stbuf) == -1) {
        umount2(VCS_MOUNT_POINT, 2);
    }

    return 0;
}

static int vcs_cleanup_channel_counters(
    nkn_vc_channel_t *ch)
{
    int rc = 0;
    char name[64];

    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_OPEN_REQUESTS_CNT_FMT, ch->id);
    nkn_mon_delete(name, NULL);
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_STAT_REQUESTS_CNT_FMT, ch->id);
    nkn_mon_delete(name, NULL);
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_CLOSE_REQUESTS_CNT_FMT, ch->id);
    nkn_mon_delete(name, NULL);
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_OPEN_FILES_CNT_FMT, ch->id);
    nkn_mon_delete(name, NULL);
    snprintf(name, sizeof(name), VCS_CHAN_x_CURRENT_OPEN_FILES_CNT_FMT, ch->id);
    nkn_mon_delete(name, NULL);
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_BYTES__READ_CNT_FMT, ch->id);
    nkn_mon_delete(name, NULL);
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_OPEN_ERRORS_CNT_FMT, ch->id);
    nkn_mon_delete(name, NULL);
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_STAT_ERRORS_CNT_FMT, ch->id);
    nkn_mon_delete(name, NULL);
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_READ_ERRORS_CNT_FMT, ch->id);
    nkn_mon_delete(name, NULL);

    return rc;
}

static int vcs_setup_channel_counters(
    nkn_vc_channel_t *ch)
{
    int rc = 0;
    char name[64];

    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_OPEN_REQUESTS_CNT_FMT, ch->id);
    if (!rc)
	rc = nkn_mon_add(name, NULL, &ch->total_open_requests_cnt,
	    sizeof(ch->total_open_requests_cnt));
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_STAT_REQUESTS_CNT_FMT, ch->id);
    if (!rc)
	rc = nkn_mon_add(name, NULL, &ch->total_stat_requests_cnt,
	    sizeof(ch->total_stat_requests_cnt));
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_CLOSE_REQUESTS_CNT_FMT, ch->id);
    if (!rc)
	rc = nkn_mon_add(name, NULL, &ch->total_close_requests_cnt,
	    sizeof(ch->total_close_requests_cnt));
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_OPEN_FILES_CNT_FMT, ch->id);
    if (!rc)
	rc = nkn_mon_add(name, NULL, &ch->total_open_files_cnt,
	    sizeof(ch->total_open_files_cnt));
    snprintf(name, sizeof(name), VCS_CHAN_x_CURRENT_OPEN_FILES_CNT_FMT, ch->id);
    if (!rc)
	rc = nkn_mon_add(name, NULL, &ch->current_open_files_cnt,
	    sizeof(ch->current_open_files_cnt));
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_BYTES__READ_CNT_FMT, ch->id);
    if (!rc)
	rc = nkn_mon_add(name, NULL, &ch->total_bytes_read_cnt,
	    sizeof(ch->total_bytes_read_cnt));
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_OPEN_ERRORS_CNT_FMT, ch->id);
    if (!rc)
	rc = nkn_mon_add(name, NULL, &ch->total_open_errors_cnt,
	    sizeof(ch->total_open_errors_cnt));
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_STAT_ERRORS_CNT_FMT, ch->id);
    if (!rc)
	rc = nkn_mon_add(name, NULL, &ch->total_stat_errors_cnt,
	    sizeof(ch->total_stat_errors_cnt));
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_READ_ERRORS_CNT_FMT, ch->id);
    if (!rc)
	rc = nkn_mon_add(name, NULL, &ch->total_read_errors_cnt,
	    sizeof(ch->total_read_errors_cnt));
    snprintf(name, sizeof(name), VCS_CHAN_x_TOTAL_OPEN_VIRT_FILES_CNT_FMT, ch->id);
    if (!rc)
	rc = nkn_mon_add(name, NULL, &ch->total_open_virt_files_cnt,
	    sizeof(ch->total_open_virt_files_cnt));
    snprintf(name, sizeof(name), VCS_CHAN_x_CURRENT_OPEN_VIRT_FILES_CNT_FMT, ch->id);
    if (!rc)
	rc = nkn_mon_add(name, NULL, &ch->current_open_virt_files_cnt,
	    sizeof(ch->current_open_virt_files_cnt));

    return rc;
}

static void vcs_cleanup_server_counters(void)
{
    nkn_mon_delete(VCS_TOTAL_CLIENTS_CNT, NULL);
    nkn_mon_delete(VCS_CURRENT_CLIENTS_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_DISCONNECT_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_LOST_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_SOCK_ERROR_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_CONNECT_REQUESTS_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_OPEN_REQUESTS_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_STAT_REQUESTS_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_CLOSE_REQUESTS_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_OPEN_FILES_CNT, NULL);
    nkn_mon_delete(VCS_CURRENT_OPEN_FILES_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_BYTES__READ_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_OPEN_ERRORS_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_STAT_ERRORS_CNT, NULL);
    nkn_mon_delete(VCS_TOTAL_READ_ERRORS_CNT, NULL);
}

static int vcs_setup_server_counters(void)
{
    int rc = 0;

    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_CLIENTS_CNT, NULL,
	    &vc->total_clients_cnt, sizeof(vc->total_clients_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_CURRENT_CLIENTS_CNT, NULL,
	    &vc->current_clients_cnt, sizeof(vc->current_clients_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_DISCONNECT_CNT, NULL,
	    &vc->total_disconnect_cnt, sizeof(vc->total_disconnect_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_LOST_CNT, NULL,
	    &vc->total_lost_cnt, sizeof(vc->total_lost_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_SOCK_ERROR_CNT, NULL,
	    &vc->total_socket_error_cnt, sizeof(vc->total_socket_error_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_CONNECT_REQUESTS_CNT, NULL,
	    &vc->total_connect_requests_cnt, sizeof(vc->total_connect_requests_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_OPEN_REQUESTS_CNT, NULL,
	    &vc->total_open_requests_cnt, sizeof(vc->total_open_requests_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_STAT_REQUESTS_CNT, NULL,
	    &vc->total_stat_requests_cnt, sizeof(vc->total_stat_requests_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_CLOSE_REQUESTS_CNT, NULL,
	    &vc->total_close_requests_cnt, sizeof(vc->total_close_requests_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_OPEN_FILES_CNT, NULL,
	    &vc->total_open_files_cnt, sizeof(vc->total_open_files_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_CURRENT_OPEN_FILES_CNT, NULL,
	    &vc->current_open_files_cnt, sizeof(vc->current_open_files_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_BYTES__READ_CNT, NULL,
	    &vc->total_bytes_read_cnt, sizeof(vc->total_bytes_read_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_OPEN_ERRORS_CNT, NULL,
	    &vc->total_open_errors_cnt, sizeof(vc->total_open_errors_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_STAT_ERRORS_CNT, NULL,
	    &vc->total_stat_errors_cnt, sizeof(vc->total_stat_errors_cnt));
    if (!rc)
	rc = nkn_mon_add(VCS_TOTAL_READ_ERRORS_CNT, NULL,
	    &vc->total_read_errors_cnt, sizeof(vc->total_read_errors_cnt));

    return rc;
}

static void vcs_cleanup(void)
{
    int i;
    nkn_vc_channel_t *ch;

    for (i = 0; i < VCS_N_CHANNELS; i++) {
	ch = &vc->vcs_vc_channels[i];
	if (ch->usage_count != 0) {
	    DBG_VCW("channel %d has non-zero usage count : %d\n", 
		    i, ch->usage_count);
	}

	if (vcs_channel_is_attached(ch)) {
	    DBG_VCW("channel %d still attached to VM '%s'\n", i, ch->vm_name);
	    vcs_virt_detach_device(ch->vm_name, ch->path, ch->disk_name);
	}

	vcs_cleanup_channel_counters(ch);
	if (ch->path != NULL) {
	    free(ch->path);
	}
    }
    vcs_cleanup_server_counters();
    vcs_workq_destroy(&vc->wq);
    vcs_cleanup_mount_point();
    memset(vc, 0, sizeof(nkn_vcs_t));
}

static int vcs_vc_init(void)
{
    int rc, i;
    nkn_vc_channel_t *ch;

    memset(vc, 0, sizeof(nkn_vcs_t));
    pthread_mutex_init(&vc->acceptor.mutex, NULL);
    pthread_cond_init(&vc->acceptor.cond, NULL);

    rc = vcs_setup_mount_point();
    if (rc) {
        return -1;
    }

    pthread_mutex_init(&vc->lock, NULL);

    rc = vcs_workq_init(&vc->wq);
    if (rc) {
	DBG_VCS("vcs_workq_init() failed: %s\n", strerror(errno));
	return -1;
    }

    rc = vcs_setup_server_counters();
    if (rc) {
	DBG_VCS("vcs_setup_server_counters() failed: %s\n", strerror(errno));
	return -1;
    }

    for (i = 0; i < VCS_N_CHANNELS; i++) {
	ch = &vc->vcs_vc_channels[i];
	ch->id = i;
	ch->next_slot = (VCS_N_CHANNELS/2);
	pthread_mutex_init(&ch->lock, NULL);
	snprintf(ch->name, VCS_MAX_CHANNEL_NAME_STR-1, "channel_%d.dat", i);
	ch->path = vcs_path(VCS_MOUNT_POINT, ch->name);
	if (ch->path == NULL) {
	    DBG_OOM();
	    goto cleanup;
	}
	rc  = vcs_setup_channel_counters(ch);
	if (rc) {
	    DBG_VCS("vcs_setup_channel_counters(%d) failed: %s\n", i, strerror(errno));
	    goto cleanup;
	}
    }
    vcs_virt_init();

    return 0;

cleanup:
    vcs_cleanup();
    return -1;
}

static void *vcs_main_thread(void *data)
{
    int rc;
    UNUSED_ARGUMENT(data);
    prctl(PR_SET_NAME, "nvsd-vcsmain", 0, 0, 0);

    rc = vcs_net_listen();
    if (rc < 0) {
	return NULL;
    }

    rc = pthread_create(&vc->accept_thread, NULL, vcs_accept_thread, NULL);
    if (rc < 0) {
	DBG_VCS("pthread_create() failed: %s\n", strerror(errno));
	vcs_net_shutdown();
	vcs_cleanup();
	return NULL;
    }

    rc = vcs_fuse_main();
    if (rc < 0) {
	DBG_VCE("vcs_fuse_main() returned: %d\n", rc);
    }

    return NULL;
}

int virt_cache_server_init(void)
{
    int rc;

    rc = vcs_vc_init();
    if (rc < 0) {
	return -1;
    }

    rc = pthread_create(&vc->main_thread, NULL, vcs_main_thread, NULL);
    if (rc < 0) {
	vcs_cleanup();
	return -1;
    }
    
    return 0;
}

/*
 * NB: can only be called after the main thread has completed.
 */
void virt_cache_server_exit(void)
{
    if (vc && vc->accept_thread) {
    	pthread_cancel(vc->accept_thread);
    	pthread_join(vc->accept_thread, NULL);
    }

    vcs_net_shutdown();
    vcs_cleanup();
}

/*
 * NB: for debugging only
 */
void virt_cache_server_wait(void)
{
    pthread_join(vc->main_thread, NULL); 
}


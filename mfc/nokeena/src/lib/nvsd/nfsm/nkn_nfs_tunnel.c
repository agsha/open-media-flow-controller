/*
 * (C) Copyright 2008-2011 Juniper Networks, Inc
 *
 * This file contains NFS tunnel implementation
 *
 * Author: Jeya ganesh babu
 *
 */

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/uio.h>
#include <glib.h>
#include <dirent.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <atomic_ops.h>
#include "nkn_nfs_api.h"
#include "nkn_nfs.h"
#include "nkn_mediamgr_api.h"
#include "nkn_defs.h"
#include "nkn_trace.h"
#include "http_header.h"
#include "fqueue.h"
#include "nkn_debug.h"
#include "nkn_namespace.h"
#include "nkn_am_api.h"
#include "queue.h"
#include "aio.h"
#include "nkn_assert.h"
#include "nkn_nfs.h"
#include "nkn_nfs_tunnel.h"

AO_t glob_nfs_tunnel_request;
AO_t glob_nfs_tunnel_mount_failed;
AO_t glob_nfs_tunnel_stat_failed;
AO_t glob_nfs_tunnel_invalid_offset;
AO_t glob_nfs_tunnel_stat_success;
AO_t glob_nfs_tunnel_invalid_pointer;
AO_t glob_nfs_tunnel_invalid_ret_pointer;
AO_t glob_nfs_tunnel_async_read_failed;
AO_t glob_nfs_tunnel_aio_return_failed;
AO_t glob_nfs_tunnel_namespace_error;
AO_t glob_nfs_tunnel_set_attr_failed;
AO_t glob_nfs_tunnel_malloc_failed;
extern AO_t glob_tot_size_from_tunnel;

/*
 * Tunnel mgr input routine
 */
void nfs_tunnel_mgr_input(nkn_task_id_t task_id)
{
    nfst_response_t  *nfst_cr;
    cache_response_t *cr;
    int              trace = 0;
    char             *filename = NULL;
    int              filename_len;
    struct stat	     sb;
    nfst_priv_data_t *nfst_priv = NULL;
    int		      no_data = 0;

    nkn_task_set_state(task_id, TASK_STATE_EVENT_WAIT);

    AO_fetch_and_add1(&glob_nfs_tunnel_request);
    nfst_cr = (nfst_response_t *) nkn_task_get_data(task_id);
    if(!nfst_cr) {
	NKN_ASSERT(0);
	AO_fetch_and_add1(&glob_nfs_tunnel_invalid_pointer);
	return;
    }
    cr = &nfst_cr->cr;
    if(cr->in_rflags & CR_RFLAG_TRACE)
	trace = 1;
    if (cr->in_rflags & CR_RFLAG_NO_DATA)
	no_data = 1;

    filename_len = strlen(nfst_cr->uri) + MAX_URI_SIZE;
    filename = (char *)nkn_malloc_type(filename_len, mod_nfst_filename);
    if(!filename) {
	AO_fetch_and_add1(&glob_nfs_tunnel_malloc_failed);
	cr->errcode = NKN_NFS_GET_URI_ERR;
	goto nfs_tunnel_error;
    }

    nfst_priv = (nfst_priv_data_t *)nkn_malloc_type(
				    sizeof(nfst_priv_data_t),
				    mod_nfst_priv_data);
    if(!nfst_priv) {
	free(filename);
	AO_fetch_and_add1(&glob_nfs_tunnel_malloc_failed);
	cr->errcode = NKN_NFS_GET_URI_ERR;
	goto nfs_tunnel_error;
    }

    nfst_priv->filename = filename;
    nfst_priv->ret_buf  = NULL;

    nkn_task_set_private(TASK_TYPE_NFS_TUNNEL_MANAGER, task_id,
			(void *)nfst_priv);

    if ((NFS_mount(cr->uol, cr->ns_token, filename,
			    filename_len, nfst_cr->uri, 0))) {
	cr->errcode = NKN_NFS_STAT_URI_ERR;
	AO_fetch_and_add1(&glob_nfs_tunnel_mount_failed);
        goto nfs_tunnel_error;
    }

    if(nkn_nfs_do_file_stat(filename, 0, &sb) != 0) {
	AO_fetch_and_add1(&glob_nfs_tunnel_stat_failed);
	DBG_LOG(MSG, MOD_NFS, "Stat failed after remount "
		"file: %s errno:%d",
		filename, errno);
        nkn_nfs_set_stat_ended(cr->ns_token);
	if(trace == 1) {
	    DBG_TRACE("Could not mount file. Please check "
			"filename: %s. errno = %d", filename, errno);
	}
	cr->errcode = NKN_NFS_STAT_URI_ERR;
	goto nfs_tunnel_error;
    }

    nfst_priv->filesize = sb.st_size;

    if(cr->uol.offset >= sb.st_size) {
	AO_fetch_and_add1(&glob_nfs_tunnel_invalid_offset);
        nkn_nfs_set_stat_ended(cr->ns_token);
	cr->errcode = NKN_NFS_STAT_INV_OFFSET;
	if(trace == 1) {
	    DBG_TRACE("End TRACE URI: %s."
		    " Invalid offset %ld available size %ld",
		    nfst_cr->uri, cr->uol.offset, sb.st_size);
	}
	goto nfs_tunnel_error;
    }

    nkn_nfs_set_stat_ended(cr->ns_token);
    AO_fetch_and_add1(&glob_nfs_tunnel_stat_success);

    if(trace == 1) {
	DBG_TRACE("End TRACE URI: %s. End NFS_stat. Success.",
		  nfst_cr->uri);
    }
    
    if (!no_data) {
	if(nkn_nfs_do_async_read(filename, cr->uol.offset,
				    NKN_NFS_LARGE_BLK_LEN, (void *)task_id,
				    NFS_GET_TYPE_TUNNEL)) {
	    AO_fetch_and_add1(&glob_nfs_tunnel_async_read_failed);
	    cr->errcode = NKN_NFS_GET_URI_ERR;
	    goto nfs_tunnel_error;
	}
    } else {
	if (nkn_nfs_tunnel_handle_returned_content(task_id,
						NULL, 0)) {
	    goto nfs_tunnel_error;
	}
    }

    return;

nfs_tunnel_error:
    cr->magic    = CACHE_RESP_RESP_MAGIC;
    cr->provider = Tunnel_provider;

    nkn_task_set_action_and_state(task_id, TASK_ACTION_OUTPUT,
				    TASK_STATE_RUNNABLE);
    return;
}

void nfs_tunnel_mgr_output(nkn_task_id_t tid __attribute((unused)))
{
    return;
}

/*
 * Tunnel mgr cleanup routine
 */
void nfs_tunnel_mgr_cleanup(nkn_task_id_t task_id)
{
    nfst_priv_data_t *nfst_priv;

    nfst_priv = (nfst_priv_data_t *)nkn_task_get_private(
						TASK_TYPE_NFS_TUNNEL_MANAGER,
						task_id);
    if(nfst_priv) {
	if(nfst_priv->filename)
	    free(nfst_priv->filename);

	if(nfst_priv->ret_buf)
	    free(nfst_priv->ret_buf);

	free(nfst_priv);
    }
    return;
}

/*
 * Tunnel data return routine
 */
int
nkn_nfs_tunnel_handle_returned_content(nkn_task_id_t task_id, uint8_t *in_buf,
					int in_buflen)
{
    nfst_response_t          *nfst_cr;
    cache_response_t         *cr;
    char                     *buf = NULL;
    int                      roundup_len, size, ret;
    int	                     num_iovecs = 0;
    int                      remaining_size, io_len;
    nfst_priv_data_t         *nfst_priv;
    nkn_attr_t               *attr_ptr;
    mime_header_t            mh;
    int			     no_data = 0;
#if 0
    const namespace_config_t *cfg = NULL;
    int                      cache_age_default = 0;
#endif

    nfst_cr = (nfst_response_t *) nkn_task_get_data(task_id);
    if(!nfst_cr) {
	NKN_ASSERT(0);
	AO_fetch_and_add1(&glob_nfs_tunnel_invalid_ret_pointer);
	return -1;
    }

    cr = &nfst_cr->cr;
    if (cr->in_rflags & CR_RFLAG_NO_DATA)
	no_data = 1;

    nfst_priv = (nfst_priv_data_t *)nkn_task_get_private(
						TASK_TYPE_NFS_TUNNEL_MANAGER,
						task_id);
    if(!nfst_priv) {
	AO_fetch_and_add1(&glob_nfs_tunnel_invalid_ret_pointer);
	cr->errcode = NKN_NFS_GET_URI_ERR;
	goto nfs_tunnel_error;
    }

    if(!nfst_priv->filename) {
	AO_fetch_and_add1(&glob_nfs_tunnel_invalid_ret_pointer);
	cr->errcode = NKN_NFS_GET_URI_ERR;
	goto nfs_tunnel_error;
    }

    if(in_buflen < 0) {
	AO_fetch_and_add1(&glob_nfs_tunnel_async_read_failed);
        cr->errcode = -in_buflen;
	goto nfs_tunnel_error;
    }

    if (!no_data) {
	if(!in_buf || !in_buflen) {
	    AO_fetch_and_add1(&glob_nfs_tunnel_aio_return_failed);
	    cr->errcode = NKN_NFS_GET_URI_ERR;
	    goto nfs_tunnel_error;
	}

	roundup_len = ((in_buflen / CM_MEM_PAGE_SIZE) + 1) * CM_MEM_PAGE_SIZE;

	buf = (char *)nkn_malloc_type(roundup_len, mod_nfst_xfer_buf_t);
	if(!buf) {
	    AO_fetch_and_add1(&glob_nfs_tunnel_malloc_failed);
	    cr->errcode = NKN_NFS_GET_URI_ERR;
	    goto nfs_tunnel_error;
	}

	memcpy(buf, in_buf, in_buflen);
	remaining_size = in_buflen;

	for(size=0; size<in_buflen; size+=CM_MEM_PAGE_SIZE) {
	    num_iovecs++;
	    if(remaining_size < CM_MEM_PAGE_SIZE) {
		io_len          = remaining_size;
	    } else {
		remaining_size -= CM_MEM_PAGE_SIZE;
		io_len          = CM_MEM_PAGE_SIZE;
	    }
	    cr->content[num_iovecs-1].iov_base = (void *)((char *)buf + size);
	    cr->content[num_iovecs-1].iov_len  = io_len;
	}
    }

    /* If GETATTR is set, return attr */
    if(cr->in_rflags & CR_RFLAG_GETATTR) {

	mime_hdr_init(&mh, MIME_PROT_HTTP, 0, 0);
	attr_ptr = (nkn_attr_t *)&nfst_priv->attr;

	nkn_attr_init(attr_ptr, NFS_ATTR_SIZE);

	/* Set the content length attribute in the mime header. */
	nkn_nfs_set_attr_content_length(attr_ptr, &mh, nfst_priv->filesize);
	// cache corrected age is zero in this case.	
	/*Bug:8888 Set the cache create time.*/
	attr_ptr->origin_cache_create = nkn_cur_ts;

#if 0
	cfg = namespace_to_config(cr->ns_token);
	if(!cfg) {
	    DBG_LOG(WARNING, MOD_NFS,
		    "\nNo configuration for this token \n");
	    AO_fetch_and_add1(&glob_nfs_tunnel_namespace_error);
	    cr->errcode = NKN_NFS_GET_URI_ERR;
	    release_namespace_token_t(cr->ns_token);
	    mime_hdr_shutdown(&mh);
	    goto nfs_tunnel_error;
	}
	cache_age_default = cfg->http_config->policies.cache_age_default;
	/* cache expiry time can be set in the cache-control header
	   Per Davey, this is the correct way to do it. */
	nkn_nfs_set_attr_cache_control(attr_ptr, &mh, cache_age_default);
	release_namespace_token_t(cr->ns_token);
#endif

	if (nkn_nfs_set_attr_stat_params(nfst_priv->filename, 0,
					attr_ptr, &mh)) {
	    AO_fetch_and_add1(&glob_nfs_tunnel_set_attr_failed);
	    cr->errcode = NKN_NFS_GET_URI_ERR;
	    mime_hdr_shutdown(&mh);
	    goto nfs_tunnel_error;
	}

	ret = http_header2nkn_attr(&mh, 0, attr_ptr);
	if (ret) {
	    DBG_LOG(WARNING, MOD_NFS,
		    "http_header2nkn_attr() failed ret=%d", ret);
	    cr->errcode = NKN_NFS_GET_URI_ERR;
	    mime_hdr_shutdown(&mh);
	    goto nfs_tunnel_error;
	}

	mime_hdr_shutdown(&mh);

	cr->attr = attr_ptr;
    }

    cr->length_response = in_buflen;
    cr->errcode         = 0;
    cr->num_nkn_iovecs  = num_iovecs;
    cr->magic           = CACHE_RESP_RESP_MAGIC;
    cr->provider	= Tunnel_provider;
    nfst_priv->ret_buf  = buf;
    AO_fetch_and_add(&glob_tot_size_from_tunnel, in_buflen);

    nkn_task_set_action_and_state(task_id, TASK_ACTION_OUTPUT,
				    TASK_STATE_RUNNABLE);
    return 0;
nfs_tunnel_error:
    if(buf)
	free(buf);
    cr->magic    = CACHE_RESP_RESP_MAGIC;
    cr->provider = Tunnel_provider;
    nkn_task_set_action_and_state(task_id, TASK_ACTION_OUTPUT,
				    TASK_STATE_RUNNABLE);
    return -1;
}

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
#include <sys/prctl.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/uio.h>
#include <dirent.h>
#include <ctype.h>
#include <pthread.h>
#include <aio.h>
#include <atomic_ops.h>
#include "nkn_nfs.h"

AO_t glob_nfs_aio_requests_started = 0;
AO_t glob_nfs_aio_requests_ended = 0;

const struct aiocb *nfs_pending_aiocb_array[MAX_LIST];
const struct aiocb *nfs_err_aiocb_array[MAX_LIST];
const struct aiocb * nfs_glob_cblist[MAX_LIST];
int nfs_next_free_indx = 0;
uint64_t glob_nfs_aio_comp_push = 0;
uint64_t glob_nfs_aio_pend_push = 0;
uint64_t glob_nfs_aio_sessions_returned = 0;
uint64_t glob_nfs_aio_pend_pop_cnt = 0;
uint64_t glob_nfs_aio_comp_pop_cnt = 0;
uint64_t glob_nfs_aio_in_progress = 0;
uint64_t glob_nfs_aio_ret_err = 0;
uint64_t glob_nfs_aio_read_err = 0;
uint64_t glob_nfs_aio_read_err_retry1 = 0;
uint64_t glob_nfs_aio_read_err_retry2 = 0;
uint64_t glob_nfs_aio_did_not_find_aiocb = 0;
uint64_t glob_nfs_aio_suspend_eagain = 0;
uint64_t glob_nfs_aio_open_file_err = 0;
uint64_t glob_nfs_aio_read_ret_err = 0;

nfs_get_resp_t nfs_aio_hash_tbl[NFS_MAX_FD];

void *
nfs_async_io_completion_thread_hdl(void *arg __attribute__((unused)))
{
    struct aiocb *my_aiocb = NULL;

    prctl(PR_SET_NAME, "nvsd-nfs-comp", 0, 0, 0);

    /* See if there are any completions on the completion queue where
       data is available from the async read */
    while(1) {
	my_aiocb = (struct aiocb *) g_async_queue_pop(nfs_gasync_aiocb_comp_q);
	if(!my_aiocb) {
	    continue;
	}
	nfs_handle_aio_content(my_aiocb->aio_fildes,
			       (uint8_t *)my_aiocb->aio_buf,
			       my_aiocb->aio_nbytes);
        if(my_aiocb->aio_fildes >= 0) {
    	    close(my_aiocb->aio_fildes);
        }
	my_aiocb->aio_fildes = -1;
	free((void *)my_aiocb->aio_buf);
	my_aiocb->aio_buf = NULL;
	free(my_aiocb);
	my_aiocb = NULL;
	glob_nfs_aio_requests_ended ++;
	glob_nfs_aio_sessions_returned ++;
    }
    return 0;
}

void *
nfs_async_io_suspend_thread_hdl_2(void *arg __attribute__((unused)))
{
    int ret = -1;
    int i, k;
    struct aiocb *my_aiocb = NULL;
    struct timespec timeout;
    int j = 0;
    int q_len = 0;
    timeout.tv_sec = 1000;
    int err_iocb_cnt = 0;
    int reterr = -1;

    prctl(PR_SET_NAME, "nvsd-nfs-send", 0, 0, 0);

    memset(nfs_err_aiocb_array, 0,
	   MAX_LIST * sizeof(struct aiocb *));

    while(1) {
	memset(nfs_pending_aiocb_array, 0,
	       MAX_LIST * sizeof(struct aiocb *));
	/* Wait for something to be put on the cblist */
	my_aiocb = NULL;
	if(err_iocb_cnt == 0) {
	    my_aiocb = g_async_queue_pop(nfs_gasync_aiocb_pend_q);
	} else {
	    my_aiocb = g_async_queue_try_pop(nfs_gasync_aiocb_pend_q);
	}

	i = 0;
	for(; i < err_iocb_cnt; i++) {
	    nfs_pending_aiocb_array[i] = nfs_err_aiocb_array[i];
	    nfs_err_aiocb_array[i] = NULL;
	}

	if(err_iocb_cnt) {
	    err_iocb_cnt = 0;
	    memset(nfs_err_aiocb_array, 0,
	       MAX_LIST * sizeof(struct aiocb *));
	}

	if(my_aiocb) {
	    glob_nfs_aio_pend_pop_cnt ++;
	    nfs_pending_aiocb_array[i] = my_aiocb;
	    i++;
	}

	/* Process multiple requests at the same time. */
	q_len = g_async_queue_length(nfs_gasync_aiocb_pend_q);
	if(q_len > 0) {
	    for(k = 0; k < q_len; k++) {
		my_aiocb = g_async_queue_try_pop(nfs_gasync_aiocb_pend_q);
		glob_nfs_aio_pend_pop_cnt ++;
		assert(my_aiocb);
		nfs_pending_aiocb_array[i] = my_aiocb;
		i++;
		if(i >= MAX_LIST) {
		    break;
		}
	    }
	}
	/* Suspend this thread until any of the I/Os have
	   completed.*/
	aio_suspend(nfs_pending_aiocb_array,
		    MAX_LIST, &timeout);
	while(errno == EAGAIN) {
	    aio_suspend(nfs_pending_aiocb_array,
			MAX_LIST, &timeout);
	    printf("\n eagain ");
	    glob_nfs_aio_suspend_eagain ++;
	    continue;
	}

	for(i=0; i < MAX_LIST; i++) {
	    my_aiocb = (struct aiocb *)nfs_pending_aiocb_array[i];
	    if(!my_aiocb) {
		glob_nfs_aio_did_not_find_aiocb ++;
		continue;
	    }

	    if ((my_aiocb->aio_buf == NULL)
		|| (my_aiocb->aio_fildes <= 0) ||
		(reterr = aio_error( my_aiocb ) == EINPROGRESS )) {
		nfs_err_aiocb_array[err_iocb_cnt] = my_aiocb;
		nfs_pending_aiocb_array[i] = NULL;
		err_iocb_cnt ++;
		glob_nfs_aio_in_progress ++;
		continue;
	    }

	    if(reterr != 0) {
		if((errno == EAGAIN) || (errno == EINTR)) {
		    nfs_err_aiocb_array[err_iocb_cnt] = my_aiocb;
		    nfs_pending_aiocb_array[i] = NULL;
		    err_iocb_cnt ++;
		    glob_nfs_aio_read_err_retry2 ++;
		} else {
		    nfs_pending_aiocb_array[i] = NULL;
		    glob_nfs_aio_ret_err ++;
		    my_aiocb->aio_nbytes = -errno;
		    g_async_queue_push(nfs_gasync_aiocb_comp_q, my_aiocb);
		}
		continue;
	    }

            j = 0;
	    if ((ret = aio_return( my_aiocb )) > 0) {
		nfs_pending_aiocb_array[i] = NULL;
		my_aiocb->aio_nbytes = ret;
		/* Push into the completion queue */
		glob_nfs_aio_comp_push ++;
		g_async_queue_push(nfs_gasync_aiocb_comp_q, my_aiocb);
	    } else {
		if((errno == EAGAIN) || (errno == EINTR)) {
		    /* read failed, consult errno */
		    nfs_err_aiocb_array[err_iocb_cnt] = my_aiocb;
		    nfs_pending_aiocb_array[i] = NULL;
		    glob_nfs_aio_read_err_retry1 ++;
		    err_iocb_cnt ++;
		} else {
		    nfs_pending_aiocb_array[i] = NULL;
		    my_aiocb->aio_nbytes = -errno;
		    glob_nfs_aio_read_err ++;
		    g_async_queue_push(nfs_gasync_aiocb_comp_q, my_aiocb);
		}
	    }
        }
    }

    return 0;
}

int
nkn_nfs_do_async_read(char *filename, uint64_t in_offset,
		      uint64_t in_length, void *ptr, int type)
{
    int fd = -1, ret = -1;
    //    struct aiocb *ptr_aiocb = &glob_aiocb;
    struct aiocb *ptr_aiocb;

    ptr_aiocb = (struct aiocb *) nkn_calloc_type (1, sizeof(struct aiocb),
                                                  mod_nfs_aio_aiocb);
    if(!ptr_aiocb) {
	return -1;
    }

    /* Allocate a data buffer for the aiocb request */
    ptr_aiocb->aio_buf = (uint8_t *) nkn_malloc_type (NKN_NFS_LARGE_BLK_LEN,
                                                     mod_nfs_aio_buf);
    if (!ptr_aiocb->aio_buf) {
	free (ptr_aiocb);
	return -1;
    }

    //printf("\nstarting filename: %s\n", filename);
    fd = open(filename, O_RDONLY | O_NONBLOCK);
    if (fd < 0 || fd >= NFS_MAX_FD) {
	free((void *)ptr_aiocb->aio_buf);
	free (ptr_aiocb);
	if(fd >= NFS_MAX_FD)
	    close(fd);
	glob_nfs_aio_open_file_err ++;
	return -1;
    }

    /* Initialize the necessary fields in the aiocb */
    ptr_aiocb->aio_fildes = fd;
    ptr_aiocb->aio_nbytes = in_length;
    ptr_aiocb->aio_offset = in_offset;

    nfs_aio_hash_tbl[fd].ptr  = (void *)ptr;
    nfs_aio_hash_tbl[fd].type = type;

    /* Non blocking read */
    ret = aio_read( ptr_aiocb );
    if (ret < 0) {
	glob_nfs_aio_read_ret_err ++;
	close(fd);
	free((void *)ptr_aiocb->aio_buf);
	free (ptr_aiocb);
	return -1;
    }

    /* Push into pending queue for reaper thread to handle */
    glob_nfs_aio_pend_push ++;
    AO_fetch_and_add1(&glob_nfs_aio_requests_started);
    g_async_queue_push(nfs_gasync_aiocb_pend_q, ptr_aiocb);

    return 0;
}

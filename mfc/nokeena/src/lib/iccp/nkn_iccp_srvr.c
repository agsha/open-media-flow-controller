/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains implementation of ICCP server routines
 *
 * Author: Jeya ganesh babu
 *
 */
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <pthread.h>
#include <fcntl.h>
#include "queue.h"
#include "nkn_defs.h"
#include "nkn_assert.h"
#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_iccp.h"
#include "nkn_iccp_cli.h"
#include "nkn_iccp_srvr.h"

/* global epoll related variables
 * Currently only one client is supported
 */
int g_iccp_epoll_fd;
int g_iccp_srvr_sock;
int g_iccp_client_fd;

/* Debug counter */
uint64_t glob_iccp_num_clients;

/* Callback function */
void(*g_iccp_srvr_callback)(void *);

/* ICCP list */
static pthread_mutex_t iccp_list_mutex;
TAILQ_HEAD(nkn_iccp_list_t, iccp_info_s) iccp_entry_list;


/*
 * Server socket init - Called from init and when there is socket error
 */
static int
iccp_srv_sock_init(void)
{
    int32_t len;
    int val = 1;
    struct sockaddr_un remote;
    struct epoll_event ep_event;

    if((g_iccp_srvr_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
	DBG_LOG(ERROR, MOD_ICCP, "Failed to create socket: %s", strerror(errno));
	return -1;
    }

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, ICCP_SOCK_PATH);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);

    unlink(remote.sun_path);

    setsockopt(g_iccp_srvr_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if(bind(g_iccp_srvr_sock, (struct sockaddr *)&remote, len) < 0) {
	DBG_LOG(ERROR, MOD_ICCP, "Bind failed: %s", strerror(errno));
	return -1;
    }

    nkn_iccp_nonblock(g_iccp_srvr_sock);

    if(listen(g_iccp_srvr_sock, MAX_ICCP_EVENTS) < 0) {
	DBG_LOG(ERROR, MOD_ICCP, "Listen failed: %s", strerror(errno));
	return -1;
    }

    ep_event.events  = EPOLLIN | EPOLLERR | EPOLLHUP;
    ep_event.data.fd = g_iccp_srvr_sock;

    if(epoll_ctl(g_iccp_epoll_fd, EPOLL_CTL_ADD, g_iccp_srvr_sock,
		    &ep_event) < 0) {
        if(errno == EEXIST ) {
	    DBG_LOG(ERROR, MOD_ICCP, "Adding listen fd: %s", strerror(errno));
            goto handle_err;
        } else {
	    DBG_LOG(SEVERE, MOD_ICCP, "Adding listen fd: %s", strerror(errno));
            goto handle_err;
        }
    }
    return 0;
handle_err:
    return -1;
}

/* Send message to client
 * There is a retry limit of 3, beyond which
 * the funciton returns error.
 */
static int
iccp_srvr_snd_msg(char *msg, size_t msg_len)
{
    int64_t ret;
    int retry_count = 0;

retry:;
    if(!g_iccp_client_fd) {
	return NKN_MM_ICCP_MESSAGE_SND_FAIL;
    }
    ret = write(g_iccp_client_fd, msg, msg_len);
    if(ret <= 0) {
	retry_count++;
	if(retry_count > MAX_ICCP_RETRIES)
	    return NKN_MM_ICCP_MESSAGE_SND_FAIL;
	goto retry;
    }
    if(ret != (int64_t)msg_len) {
	retry_count = 0;
	msg += ret;
	msg_len -= ret;
	goto retry;
    }
    return 0;
}

/*
 * Callback function triggered by CIM via EPOLLOUT
 * to send response to the CUE for all the 
 * entries that have the status. Once the status
 * is sent back, the iccp_info entry is cleaned
 * up
 */
static void
iccp_srvr_send_response(void)
{
    iccp_info_t *iccp_info = NULL, *tmp_iccp_info = NULL;
    char *msg;
    size_t msg_len;
    int ret;
    char crawl_name[2048];

    pthread_mutex_lock(&iccp_list_mutex);

    iccp_info = TAILQ_FIRST(&iccp_entry_list);

    TAILQ_FOREACH_SAFE(iccp_info, &iccp_entry_list, entries, tmp_iccp_info) { //, tmp_iccp_info) {
	if(!iccp_info)
	    break;
	pthread_mutex_lock(&iccp_info->entry_mutex);
	if(iccp_info->status) {
	    msg = iccp_form_response_message(iccp_info);
	    msg_len = ICCP_MSG_LEN(msg);
	    if(iccp_info->crawl_name)
		strcpy(crawl_name, iccp_info->crawl_name);
	    else {
		crawl_name[0] = '-';
		crawl_name[1] = '\0';
	    }
	    ret = iccp_srvr_snd_msg(msg, msg_len);
	    if(ret) {
		DBG_LOG(ERROR, MOD_ICCP, "%s: Unable to send message for uri %s",
			crawl_name, iccp_info->uri);
	    } else {
		DBG_LOG(MSG, MOD_ICCP, "%s: Message sent for uri %s %d %ld",
			crawl_name, iccp_info->uri, iccp_info->status,
			iccp_info->expiry);
	    }
	    TAILQ_REMOVE(&iccp_entry_list, iccp_info, entries);
	    pthread_mutex_unlock(&iccp_info->entry_mutex);
	    free(msg);
	    free_iccp_info(iccp_info);
	} else {
	    pthread_mutex_unlock(&iccp_info->entry_mutex);
	}
    }
    pthread_mutex_unlock(&iccp_list_mutex);
    return;
}


/*
 * Main ICCP srvr 
 */
void *
iccp_srvr_thrd(void *arg __attribute((unused)))
{
    int32_t nfds, ret, retry_count = 0;
    struct epoll_event ev[MAX_ICCP_EVENTS];
    struct epoll_event ep_event;
    iccp_info_t *iccp_info;
    uint64_t msg_len, n;
    char *msg;
    static int num_cli_err = 0;
    static int num_srv_err = 0;

    memset(ev, 0, (sizeof(struct epoll_event) * MAX_ICCP_EVENTS));

    while(1) {
	nfds = epoll_wait(g_iccp_epoll_fd, ev, MAX_ICCP_EVENTS, -1);
	if(nfds > 0) {
	    while(nfds) {
		if(ev[nfds-1].events & (EPOLLERR|EPOLLHUP)) {
		    if(epoll_ctl(g_iccp_epoll_fd, EPOLL_CTL_DEL,
				    ev[nfds-1].data.fd,
				    NULL) < 0) {
			DBG_LOG(ERROR, MOD_ICCP, "Epoll delete failed: %s",
				    strerror(errno));
		    }
		    if(ev[nfds-1].data.fd == g_iccp_srvr_sock) {
			/* Server down 
			 * Cleanup the clients as well
			 */
			num_srv_err++;
			close(g_iccp_srvr_sock);
			epoll_ctl(g_iccp_epoll_fd, EPOLL_CTL_DEL,
				    g_iccp_client_fd, NULL);
			close(g_iccp_client_fd);
retry:;
			ret = iccp_srv_sock_init();
			if(ret) {
			    sleep(1);
			    goto retry;
			}
			retry_count = 0;
		    } else {
			num_cli_err++;
			close(g_iccp_client_fd);
			g_iccp_client_fd = 0;
			glob_iccp_num_clients--;
		    }
		    break;
		    /* TODO: Should cleanup the iccp_info list here */
		}
		if(ev[nfds-1].events & EPOLLIN) {
		    /* For server epollin, accept the socket
		     * Currently we support only one client and the fd is
		     * stored in g_iccp_client_fd
		     */
		    if(ev[nfds-1].data.fd == g_iccp_srvr_sock) {
			g_iccp_client_fd = accept(g_iccp_srvr_sock, NULL, NULL);
			if(g_iccp_client_fd) {
			    ep_event.events  = EPOLLIN | EPOLLERR | EPOLLHUP;
			    ep_event.data.fd = g_iccp_client_fd;

			    nkn_iccp_nonblock(g_iccp_srvr_sock);

			    if(epoll_ctl(g_iccp_epoll_fd, EPOLL_CTL_ADD,
					    g_iccp_client_fd, &ep_event) < 0) {
				if(errno == EEXIST ) {
				    DBG_LOG(ERROR, MOD_ICCP, "Adding listen fd: %s",
						strerror(errno));
				} else {
				    DBG_LOG(SEVERE, MOD_ICCP, "Adding listen fd: %s",
						strerror(errno));
				}
			    } else {
				glob_iccp_num_clients++;
			    }
			}
		    } else {
			/* This is a client message
			 * Recv, decode and send it to CIM
			 */
			n = recv(ev[nfds-1].data.fd, &msg_len, sizeof(uint64_t),
				MSG_PEEK);
			if(msg_len) {
			    msg = nkn_calloc_type(1, msg_len, mod_iccp_msg);
			    n = recv(ev[nfds-1].data.fd, msg, msg_len, 0);
			    if(n <= 0) {
				DBG_LOG(ERROR, MOD_ICCP,
					    "Error in reading data from CUE");
				free(msg);
				continue;
			    }
			    iccp_info = iccp_decode_frame(msg);
			    free(msg);

			    if(!iccp_info || !iccp_info->uri) {
				free_iccp_info(iccp_info);
				continue;
			    }
			    /* Hold the iccp_info entry mutex for all the cim calls
			     */
			    pthread_mutex_lock(&iccp_list_mutex);
			    pthread_mutex_lock(&iccp_info->entry_mutex);
			    TAILQ_INSERT_TAIL(&iccp_entry_list, iccp_info,
						entries);
			    if(g_iccp_srvr_callback) {
				if(iccp_info->crawl_name) {
				    DBG_LOG(MSG, MOD_ICCP, "%s: Message rcvd for uri %s %d %ld",
					    iccp_info->crawl_name, iccp_info->uri,
					    iccp_info->status, iccp_info->expiry);
				} else {
				    DBG_LOG(MSG, MOD_ICCP, "-: Message rcvd for uri %s %d %ld",
					    iccp_info->uri, iccp_info->status,
					    iccp_info->expiry);
				}
				g_iccp_srvr_callback((void *)iccp_info);
				pthread_mutex_unlock(&iccp_info->entry_mutex);
			    } else {
				pthread_mutex_unlock(&iccp_info->entry_mutex);
				DBG_LOG(ERROR, MOD_ICCP,
					    "ICCP client callback not set");
				NKN_ASSERT(0);
			    }
			    pthread_mutex_unlock(&iccp_list_mutex);
			}
		    }
		}
		if(ev[nfds-1].events & EPOLLOUT) {
		    if(ev[nfds-1].data.fd == g_iccp_srvr_sock) {
			/* This should not happen */
			assert(0);
		    }
		    ep_event.events  = EPOLLIN | EPOLLERR | EPOLLHUP;
		    ep_event.data.fd = ev[nfds-1].data.fd;
		    if(epoll_ctl(g_iccp_epoll_fd, EPOLL_CTL_MOD,
				    ev[nfds-1].data.fd, &ep_event) < 0) {
			DBG_LOG(ERROR, MOD_ICCP, "Epoll modify failed: %s",
				    strerror(errno));
		    }
		    iccp_srvr_send_response();
		}
		nfds--;
	    }
	} else {
	    if(errno != EINTR)
		assert(0);
	}
    }

    return 0;
}

/*
 * ICCP server init - Called from CIM
 */
void
iccp_srvr_init(void(*srvr_callback_func)(void *))
{
    pthread_t iccp_srvr_thrd_t;
    int ret;

    iccp_init();

    g_iccp_srvr_callback = srvr_callback_func;
    g_iccp_epoll_fd = epoll_create(MAX_ICCP_EVENTS);

    TAILQ_INIT(&iccp_entry_list);
    ret = pthread_mutex_init(&iccp_list_mutex, NULL);
    if(ret < 0) {
        DBG_LOG(SEVERE, MOD_CUE, "CIM list mutex not created. ");
        return;
    }
    ret = iccp_srv_sock_init();
    if(ret) {
	assert(0);
    }
    pthread_create(&iccp_srvr_thrd_t, NULL, iccp_srvr_thrd,
		    NULL);
}

/*
 * Find all entries based on COD
 * The list of entry is returned with entry_mutex locked
 * The caller has to release the mutex
 */
iccp_info_t *
iccp_srvr_find_all_entry_cod(uint64_t cod)
{
    iccp_info_t *iccp_info, *ret=NULL, *prev_entry = NULL;

    pthread_mutex_lock(&iccp_list_mutex);
    iccp_info = TAILQ_FIRST(&iccp_entry_list);
    TAILQ_FOREACH(iccp_info, &iccp_entry_list, entries) {
	if(!iccp_info)
	    break;
	pthread_mutex_lock(&iccp_info->entry_mutex);
	if(iccp_info->cod == cod) {
	    if(prev_entry) {
		    prev_entry->next = iccp_info;
	    } else {
		ret = iccp_info;
	    }
	    prev_entry = iccp_info;
	    iccp_info->next = NULL;
	    continue;
	}
	pthread_mutex_unlock(&iccp_info->entry_mutex);
    }

    pthread_mutex_unlock(&iccp_list_mutex);

    return ret;
}

/*
 * Find all entries based on COD
 * The list of entry is returned with entry_mutex locked
 * The caller has to release the mutex
 */
int
iccp_srvr_find_dup_entry_cod(uint64_t cod, iccp_info_t *orig_iccp_info)
{
    iccp_info_t *iccp_info;
    int ret = 0;
    iccp_info = TAILQ_FIRST(&iccp_entry_list);
    TAILQ_FOREACH(iccp_info, &iccp_entry_list, entries) {
	if(!iccp_info)
	    break;
        if(iccp_info == orig_iccp_info)
	    continue;
	if(iccp_info->cod == cod) {
	    ret = -1;
	    break;
	}
    }
    return ret;
}

/*
 * Find an entry based on COD
 * The entry is returned with entry_mutex locked
 * The caller has to release the mutex
 */
iccp_info_t *
iccp_srvr_find_entry_cod(uint64_t cod)
{
    iccp_info_t *iccp_info, *ret=NULL;

    pthread_mutex_lock(&iccp_list_mutex);
    iccp_info = TAILQ_FIRST(&iccp_entry_list);
    TAILQ_FOREACH(iccp_info, &iccp_entry_list, entries) {
	if(!iccp_info)
	    break;
	pthread_mutex_lock(&iccp_info->entry_mutex);
	if(iccp_info->cod == cod) {
	    if(ret) {
		if(iccp_info->expiry > ret->expiry) {
		    ret = iccp_info;
		}
	    } else {
		ret = iccp_info;
	    }
	}
	pthread_mutex_unlock(&iccp_info->entry_mutex);
    }

    /* Return locked */
    if(ret)
	pthread_mutex_lock(&ret->entry_mutex);

    pthread_mutex_unlock(&iccp_list_mutex);

    return ret;
}

/*
 * Callback called from the CIM
 * The function will just enable EPOLLOUT for
 * the client fd and the rest will be done in
 * the epoll server context in iccp_srvr_send_response()
 */
void
iccp_srvr_callback()
{
    struct epoll_event ep_event;

    if(g_iccp_client_fd) {
	ep_event.events  = EPOLLIN | EPOLLERR |EPOLLOUT | EPOLLHUP;
	ep_event.data.fd = g_iccp_client_fd;
	if(epoll_ctl(g_iccp_epoll_fd, EPOLL_CTL_MOD,
			g_iccp_client_fd, &ep_event) < 0) {
	    DBG_LOG(ERROR, MOD_ICCP, "Epoll modify failed: %s",
			strerror(errno));
	}
    }

}


/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains implementation of ICCP client related routines
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
#include "nkn_assert.h"
#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_iccp.h"
#include "nkn_iccp_cli.h"
#include "nkn_iccp_srvr.h"
#define MAX_PARALLEL_TASK 100

int g_iccp_cli_sock;
int glob_iccp_cli_cur_adds;
int glob_iccp_cli_cur_dels;

uint64_t glob_iccp_cli_send_task;
uint32_t glob_iccp_cim_conn_status;
static pthread_mutex_t iccp_cim_conn_status_mutex = PTHREAD_MUTEX_INITIALIZER;

void(*g_iccp_cli_callback)(void *);

/*
 * ICCP client init
 */
void
iccp_cli_init(void(*cli_callback_func)(void *))
{
    pthread_t iccp_cli_thrd_t;

    g_iccp_cli_callback = cli_callback_func;
    pthread_create(&iccp_cli_thrd_t, NULL, iccp_cli_thrd,
		    NULL);
    iccp_init();
}

/*
 * Send ICCP client message to server
 * A retry of MAX_ICCP_RETRIES will be done
 * before returning error to the caller
 */
static int
iccp_cli_snd_msg(char *msg, size_t msg_len)
{
    int64_t ret;
    int retry_count = 0;

retry:;
    if(!g_iccp_cli_sock) {
	return NKN_MM_ICCP_MESSAGE_SND_FAIL;
    }
    ret = write(g_iccp_cli_sock, msg, msg_len);
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
    glob_iccp_cli_send_task++;
    return 0;
}

/*
 * Return iccp-cim connection status
 */
int
iccp_return_and_reset_cim_status()
{
    int status;
    pthread_mutex_lock(&iccp_cim_conn_status_mutex);
    status = glob_iccp_cim_conn_status;
    glob_iccp_cim_conn_status = 0;
    pthread_mutex_unlock(&iccp_cim_conn_status_mutex);
    return status;
}

/*
 * Form a request message and send it to CIM
 */
int
iccp_send_task(iccp_info_t *iccp_info)
{
    char *msg;
    size_t msg_len;
    int ret;
    char crawl_name[2048];

    if(iccp_info->crawl_name)
	strcpy(crawl_name, iccp_info->crawl_name);
    else {
	crawl_name[0] = '-';
	crawl_name[1] = '\0';
    }

    msg = iccp_form_request_message(iccp_info);
    if(!msg) {
	DBG_LOG(ERROR, MOD_ICCP, "%s: Unable to for message for uri %s",
		    crawl_name, iccp_info->uri);
	return NKN_MM_ICCP_MESSAGE_CREAT_FAIL;
    }
    msg_len = ICCP_MSG_LEN(msg);

    ret = iccp_cli_snd_msg(msg, msg_len);
    if(ret) {
	DBG_LOG(ERROR, MOD_ICCP, "%s: Unable to send message for uri %s",
		    crawl_name, iccp_info->uri);
    } else {
	DBG_LOG(MSG, MOD_ICCP, "%s: Message sent for uri %s %d %ld",
		    crawl_name, iccp_info->uri, iccp_info->status,
		    iccp_info->expiry);
    }
    free(msg);
    return ret;

}

/*
 * ICCP client main epoll thread
 */
void *
iccp_cli_thrd(void *arg __attribute((unused)))
{
    int32_t len;
    uint64_t msg_len, n;
    int nfds;
    struct sockaddr_un remote;
    struct epoll_event ep_event;
    struct epoll_event *ev;
    int g_iccp_epoll_fd;
    char *msg;
    iccp_info_t *iccp_info;

    g_iccp_epoll_fd = epoll_create(MAX_ICCP_EVENTS);

retry:;

    if(g_iccp_cli_sock)
	close(g_iccp_cli_sock);

    if((g_iccp_cli_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
	DBG_LOG(ERROR, MOD_ICCP, "Failed to create socket: %s", strerror(errno));
	sleep(1);
	goto retry;
    }

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, ICCP_SOCK_PATH);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);

    nkn_iccp_nonblock(g_iccp_cli_sock);

    if(connect(g_iccp_cli_sock, (const struct sockaddr *)&remote,
		    len) != 0) {
	DBG_LOG(ERROR, MOD_ICCP, "Failed to connect to socket: %s", strerror(errno));
	sleep(1);
	goto retry;
    }

    ep_event.events  = EPOLLIN | EPOLLERR |EPOLLHUP;
    ep_event.data.fd = g_iccp_cli_sock;

    if(epoll_ctl(g_iccp_epoll_fd, EPOLL_CTL_ADD, g_iccp_cli_sock,
		    &ep_event) < 0) {
        if(errno == EEXIST ) {
	    DBG_LOG(ERROR, MOD_ICCP, "Adding listen fd: %s", strerror(errno));
        } else {
	    DBG_LOG(SEVERE, MOD_ICCP, "Adding listen fd: %s", strerror(errno));
            goto retry;
        }
    }

    ev = (struct epoll_event *)nkn_calloc_type(MAX_ICCP_EVENTS,
					sizeof (struct epoll_event),
					mod_iccp_epoll);
    pthread_mutex_lock(&iccp_cim_conn_status_mutex);
    glob_iccp_cim_conn_status = 1;
    pthread_mutex_unlock(&iccp_cim_conn_status_mutex);

    while(1) {
	nfds = epoll_wait(g_iccp_epoll_fd, ev, MAX_ICCP_EVENTS, -1);
	if(nfds > 0) {
	    while(nfds) {
		if(ev[nfds-1].events & (EPOLLERR|EPOLLHUP)) {
		    if(epoll_ctl(g_iccp_epoll_fd, EPOLL_CTL_DEL,
				    ev[nfds-1].data.fd, NULL) < 0) {
			DBG_LOG(ERROR, MOD_ICCP, "Epoll delete failed: %s",
				    strerror(errno));
		    }
		    if(ev[nfds-1].data.fd == g_iccp_cli_sock) {
			sleep(1);
			goto retry;
		    }
		}
		if(ev[nfds-1].events & EPOLLIN) {
		    /* Recv data, decode and do the CUE callback for the message
		     */
		    recv(g_iccp_cli_sock, &msg_len, sizeof(uint64_t), MSG_PEEK);
		    if(msg_len) {
			msg = nkn_calloc_type(1, msg_len, mod_iccp_msg);
			n = read(g_iccp_cli_sock, msg, msg_len);
			if(!n) {
			    free(msg);
			    continue;
			}
			iccp_info = iccp_decode_frame(msg);
			free(msg);
			if(iccp_info && g_iccp_cli_callback) {
			    g_iccp_cli_callback(iccp_info);
			    free_iccp_info(iccp_info);
			} else {
			    if(iccp_info) {
				DBG_LOG(ERROR, MOD_ICCP,
					"ICCP client callback not set");
			    } else {
				DBG_LOG(ERROR, MOD_ICCP,
					"Unable to parse message");
			    }
			}
		    }
		}
		if(ev[nfds-1].events & EPOLLOUT) {
		    /* This should not happen */
		    assert(0);
		}
		nfds--;
	    }
	} else {
	    if(errno != EINTR)
		assert(0);
	    else
		continue;
	}
    }

    return 0;
}



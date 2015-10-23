/*
 * @file jpsd_network.c
 * @brief
 * jpsd_network.c - definations for jpsd network functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <atomic_ops.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/queue.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include "nkn_memalloc.h"
#include "nkn_stat.h"

#include "jpsd_defs.h"
#include "jpsd_network.h"

int NM_tot_threads = 2;

struct nm_threads g_NM_thrs[MAX_EPOLL_THREADS];
struct network_mgr *gnm;

NKNCNT_DEF(socket_wrongstate, uint64_t, "", "Socket Wrong State")
NKNCNT_DEF(socket_epollhup, uint64_t, "", "Socket Epoll Hup")
NKNCNT_DEF(socket_epollerr, uint64_t, "", "Socket Epoll Error")

static int NM_add_fd_to_event(int fd, uint32_t epollev)
{
	struct epoll_event ev;
	int op;

	op = (NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLL)) ?
                EPOLL_CTL_MOD : EPOLL_CTL_ADD;

        memset((char *)&ev, 0, sizeof(struct epoll_event));

        ev.events = epollev;
        ev.data.fd = fd;
	if (epoll_ctl(gnm[fd].pthr->epfd, op, fd, &ev) < 0) {
                if (errno == EEXIST) {
                        return TRUE;
                }
		return FALSE;
	}

	if (!NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLL) ) {
		NM_SET_FLAG(&gnm[fd], NMF_IN_EPOLL);
        }

	return TRUE;
}

int NM_add_event_epollin(int fd)
{
	if (NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLLIN)) {
                return TRUE;
        }

        if (NM_add_fd_to_event(fd, EPOLLIN) == FALSE)  {
                return FALSE;
        }

        NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLOUT);
        NM_SET_FLAG(&gnm[fd], NMF_IN_EPOLLIN);

        return TRUE;
}

int NM_add_event_epollout(int fd)
{
	if (NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLLOUT)) {
                return TRUE;
        }

        if (NM_add_fd_to_event(fd, EPOLLOUT) < 0) {
                return FALSE;
        }

        NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLIN);
        NM_SET_FLAG(&gnm[fd], NMF_IN_EPOLLOUT);

        return TRUE;
}

int NM_del_event_epoll_inout(int fd)
{
	if (NM_add_fd_to_event(fd, EPOLLERR | EPOLLHUP) == FALSE) {
                return FALSE;
        }

	NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLIN|NMF_IN_EPOLLOUT);

	return TRUE;
}

int NM_del_event_epoll(int fd)
{
	if (!NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLL)) {
                return TRUE;
        }

	if (epoll_ctl(gnm[fd].pthr->epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
                return TRUE;
        }

        NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLIN|NMF_IN_EPOLLOUT);
        NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLL);

        return TRUE;
}

int register_NM_socket(int fd, void *private_data, NM_func f_epollin,
		NM_func f_epollout, NM_func f_epollerr, NM_func f_epollhup)
{
	int num;
	network_mgr_t *pnm;

	if (fd < 0 || fd > MAX_GNM)
		return FALSE;

	pnm = &gnm[fd];
	num = fd % NM_tot_threads;
	if ((AO_load(&g_NM_thrs[num].cur_fds) >= g_NM_thrs[num].max_fds))
		return FALSE;

	pnm->fd = fd;
	pnm->private_data = private_data;
	pnm->f_epollin = f_epollin;
	pnm->f_epollout = f_epollout;
	pnm->f_epollerr = f_epollerr;
	pnm->f_epollhup = f_epollhup;
	pnm->pthr = &g_NM_thrs[num];

	NM_SET_FLAG(pnm, NMF_IN_USE);

	return TRUE;
}

void NM_close_socket(int fd)
{
	int ret;
	network_mgr_t *pnm;

	assert(fd != -1);

        pnm = &gnm[fd];
        pnm->fd = -1;
        pnm->flag = 0;
        pnm->private_data = NULL;
        pnm->incarn++;
	pnm->f_epollin = NULL;
	pnm->f_epollout = NULL;
	pnm->f_epollerr = NULL;
	pnm->f_epollhup = NULL;

	ret = close(fd);
	if (ret < 0) {
		return;
	}

	return;
}

int NM_set_socket_nonblock(int fd)
{
	int opts = (O_RDWR | O_NONBLOCK);
	int val = 1;
	int ret;

	ret = fcntl(fd, F_SETFL, opts);
	if (ret < 0)
		return FALSE;

	ret = setsockopt(fd, IPPROTO_TCP,
		 TCP_NODELAY, (char *)&val, sizeof(val));
	if (ret < 0)
		return FALSE;

	return TRUE;
}

static void *NM_main(void *arg)
{
        char name[64];
        int i;
        int ret;
        int res;
        int fd;
        int epoll_wait_timeout = 1;
        network_mgr_t *pnm;
        struct nm_threads *pnmthr = (struct nm_threads *)arg;

        snprintf(name, 64, "jpsd-network-%lu", pnmthr->num);
        prctl(PR_SET_NAME, name, 0, 0, 0);

        while (1) {
                res = epoll_wait(pnmthr->epfd, pnmthr->events,
				pnmthr->max_fds, epoll_wait_timeout);
                if (res == -1) {
                        if (errno == EINTR)
                                continue;
                } else if (res == 0) {
                        continue;
                }

                for (i = 0; i < res; i++) {
                        fd = pnmthr->events[i].data.fd;
                        pnm = (network_mgr_t *)&gnm[fd];
                        if (!NM_CHECK_FLAG(pnm, NMF_IN_USE)) {
                                NM_del_event_epoll(fd);
                                glob_socket_wrongstate++;
                                continue;
                        }
                        ret = TRUE;
                        if (pnmthr->events[i].events & EPOLLERR) {
                                ret = pnm->f_epollerr(fd, pnm->private_data);
                                glob_socket_epollerr++;
                        } else if (pnmthr->events[i].events & EPOLLHUP) {
                                ret = pnm->f_epollhup(fd, pnm->private_data);
                                glob_socket_epollhup++;
                        } else if (pnmthr->events[i].events & EPOLLIN) {
                                ret = pnm->f_epollin(fd, pnm->private_data);
                        } else if (pnmthr->events[i].events & EPOLLOUT) {
                                ret = pnm->f_epollout(fd, pnm->private_data);
                        }

                        if (ret == FALSE)
                                NM_close_socket(fd);
                }
        }

        return NULL;
}

void NM_init(void)
{
	int i, j;
        int max_fds;
        struct nm_threads *pnmthr;

        gnm = (network_mgr_t *)nkn_calloc_type(MAX_GNM,
                                sizeof(network_mgr_t), mod_diameter_t);
        assert(gnm != NULL);

        for (i = 0; i < MAX_GNM; i++) {
                gnm[i].fd = -1;
                gnm[i].incarn = 1;
        }

        /* balancing max_fds across NM_tot_threads  */
        max_fds = MAX_GNM % NM_tot_threads;
        max_fds = (MAX_GNM + max_fds) / NM_tot_threads;
        for (i = 0; i < NM_tot_threads; i++) {
                memset((char *)&g_NM_thrs[i], 0, sizeof(struct nm_threads));

                pnmthr = &g_NM_thrs[i];
                pnmthr->num = i;
                pnmthr->max_fds = max_fds;

                pnmthr->events = nkn_calloc_type(MAX_GNM,
                                                sizeof(struct epoll_event),
                                                mod_diameter_t);
                assert(pnmthr->events != NULL);

                pnmthr->epfd = epoll_create(pnmthr->max_fds);
                assert(pnmthr->epfd != -1);
                pthread_mutex_init(&pnmthr->nm_mutex, NULL);
        }

        return;
}

void NM_start(void)
{
	int i;

	for (i = 0; i < NM_tot_threads; i++)
		pthread_create(&g_NM_thrs[i].pid, NULL, NM_main, &g_NM_thrs[i]);

	return;
}

void NM_wait(void)
{
	pthread_join(g_NM_thrs[0].pid, NULL);
	return;
}

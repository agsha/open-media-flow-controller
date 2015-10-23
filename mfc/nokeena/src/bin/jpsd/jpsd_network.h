/*
 * @file jpsd_network.h
 * @brief
 * jpsd_network.h - declarations for network functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef _JPSD_NETWORK_H
#define _JPSD_NETWORK_H

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>

#define MAX_EPOLL_THREADS (64)

#define MAX_GNM	(1000)

#define NMF_IN_USE	0x0000000000000001
#define NMF_IN_EPOLL	0x0000000000000002
#define NMF_IN_EPOLLIN	0x0000000000000004
#define NMF_IN_EPOLLOUT	0x0000000000000010

#define NM_SET_FLAG(x, f)	(x)->flag |= (f);
#define NM_UNSET_FLAG(x, f)	(x)->flag &= ~(f);
#define NM_CHECK_FLAG(x, f)	((x)->flag & (f))

typedef int (* NM_func)(int sockfd, void *data);

struct network_mgr;
struct nm_threads;

typedef struct network_mgr {
	int32_t  fd;
	uint64_t flag;
	uint32_t incarn;
	void *private_data;
	NM_func f_epollin;
	NM_func f_epollout;
	NM_func f_epollerr;
	NM_func f_epollhup;
	struct nm_threads *pthr;
} network_mgr_t;

typedef struct nm_threads {
	pthread_t pid;
	pthread_mutex_t nm_mutex;
	uint64_t num;
	AO_t cur_fds;
        unsigned int max_fds;
        int epfd;
	unsigned int epoll_event_cnt;
        struct epoll_event *events;
} nm_threads_t;

extern struct network_mgr *gnm;
extern struct nm_threads g_NM_threads[MAX_EPOLL_THREADS];

int NM_add_event_epollin(int fd);
int NM_add_event_epollout(int fd);
int NM_del_event_epoll_inout(int fd);
int NM_del_event_epoll(int fd);

int register_NM_socket(int fd, void *private_data, NM_func f_epollin,
		NM_func f_epollout, NM_func f_epollerr, NM_func f_epollhup);
void NM_close_socket(int fd);

int NM_set_socket_nonblock(int fd);

#endif // _JPSD_NETWORK_H


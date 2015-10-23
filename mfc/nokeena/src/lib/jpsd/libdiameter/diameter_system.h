/*
 * @file diameter_system.h
 * @brief
 * diameter_system.h - declarations for diameter system functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef _DIAMETER_SYSTEM_H
#define _DIAMETER_SYSTEM_H

#include "sys/jnx/types.h"
#include "jnx/diameter_avp_common_defs.h"
#include "jnx/diameter_base_api.h"

#define FICTITIOUS_AUTH_APP 16777238
#define FICTITIOUS_ACCT_APP 11

#define PEER_NAME_LENGTH 32
#define PEERGROUP_NAME_LENGTH 32

#define SET_FLAG(x, f)		(x)->flag |= (f)
#define UNSET_FLAG(x, f)	(x)->flag &= ~(f)
#define CHECK_FLAG(x, f)	((x)->flag & (f))

#define PEER_REF_IN_MO		0x00000001

#define PEERGROUP_STARTED	0x00000001

extern uint32_t aVendors[];
extern diameter_application_id_t aVendorSpecificAuthApp[];

enum messenger_type {
	DIAMETER_CLIENT = 1,
	DIAMETER_SERVER,
};

typedef struct diameter_buffer_s {
	uint8_t *p;
	uint32_t size;
	uint32_t len;
} diameter_buffer_t;

typedef struct diameter_messanger_s {
	enum messenger_type type;
	int fd;
	int active;
	int flag;
	uint32_t src_addr;
	uint32_t dst_addr;
	uint16_t src_port;
	uint16_t dst_port;
	void *po;
	struct diameter_buffer_s *data;
	diameter_transport_handle transport_handle;
	diameter_messenger_observer_handle observer_handle;
	diameter_incoming_queue_config_t incoming;
	diameter_incoming_queue_config_t outgoing;
} diameter_messenger_t;

typedef struct diameter_peer_observer_s {
	char name[PEER_NAME_LENGTH];
	diameter_peer_handle peer;
	int fd;
	int flag;
	void *messenger;
	void *pgo;
	pthread_mutex_t mutex;
        unsigned int stats[DIAMETER_PEER_STAT_NUM_COUNTERS];
} diameter_peer_observer_t;

typedef struct diameter_peergroup_observer_s {
	char name[PEERGROUP_NAME_LENGTH];
	diameter_peergroup_handle pg;
	int flag;
	unsigned int stats[DIAMETER_PEERGROUP_STAT_NUM_COUNTERS];
} diameter_peergroup_observer_t;

void *diameter_create_iov(uint32_t size);

void diameter_release_iov(void *p);

void diameter_addref_iov(void *p);

uint32_t diameter_buf_get_size(diameter_buffer_handle buf, uint32_t *size);

uint32_t diameter_buf_get_ptr(diameter_buffer_handle data, uint8_t **p);

uint32_t diameter_buf_release(diameter_buffer_handle buf);

void diameter_peer_observer_add_ref(diameter_peer_observer_t *po);

void diameter_peer_observer_release(diameter_peer_observer_t *po);

void diameter_start(diameter_system_cb_table *system_cb_table,
			diameter_peer_cb_table *peer_cb_table,
			diameter_peergroup_cb_table *peergroup_cb_table,
			diameter_request_cb_table *request_cb_table,
			diameter_response_cb_table *response_cb_table);

void diameter_init(diameter_system_cb_table *system_cb_table,
			diameter_peer_cb_table *peer_cb_table,
			diameter_peergroup_cb_table *peergroup_cb_table,
			diameter_request_cb_table *request_cb_table,
			diameter_response_cb_table *response_cb_table);

#endif /* !_DIAMETER_SYSTEM_H */

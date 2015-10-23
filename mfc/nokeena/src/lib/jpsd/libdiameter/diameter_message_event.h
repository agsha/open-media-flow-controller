/*
 * @file diameter_message_event.h
 * @brief
 * diameter_message_event.h - declrations for diameter message event functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef _DIAMETER_MESSAGE_EVENT_H
#define _DIAMETER_MESSAGE_EVENT_H

#include "sys/jnx/types.h"
#include "jnx/diameter_avp_common_defs.h"
#include "jnx/diameter_base_api.h"

void diameter_request_on_response(diameter_request_observer_handle h,
				uint16_t n, struct iovec *request,
				diameter_buffer_handle response_message,
				diameter_peer_handle peer,
				diameter_peergroup_handle peergroup,
				void *ctx);

diameter_message_action_t diameter_request_on_pause(
				diameter_request_observer_handle h,
				uint16_t n, struct iovec *request,
				diameter_peergroup_handle peergroup,
				diameter_request_state_t state,
				uint32_t time_left_ms);

diameter_message_action_t diameter_request_on_error(
				diameter_request_observer_handle h,
				uint16_t n, struct iovec *request,
				diameter_peer_handle peer,
				diameter_peergroup_handle peergroup,
				diameter_request_state_t state,
				const char *error_message,
				int errorcode, uint32_t time_left_ms);

diameter_message_action_t diameter_request_on_timeout(
				diameter_request_observer_handle h,
				uint16_t n, struct iovec *request,
				diameter_peergroup_handle peergroup,
				diameter_request_state_t state);

diameter_message_action_t diameter_request_on_peer_timeout(
				diameter_request_observer_handle h,
				uint16_t n, struct iovec *request,
				diameter_peer_handle peer,
				diameter_peergroup_handle peergroup,
				diameter_request_state_t state,
				uint32_t time_left_ms);

void diameter_request_on_release(diameter_request_observer_handle h,
				uint16_t diam_num_iovecs,
				struct iovec *diam_pkt_iovec,
				diameter_peergroup_handle peergroup,
				diameter_request_state_t state,
				diameter_message_action_t action);

void diameter_response_on_release(diameter_response_observer_handle h);

#endif /* !_DIAMETER_MESSAGE_EVENT_H */

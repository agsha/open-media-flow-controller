/*
 * @file diameter_event.h
 * @brief
 * diameter_event.h - declarations for diameter event functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2014, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef _DIAMETER_EVENT_H
#define _DIAMETER_EVENT_H

#include "sys/jnx/types.h"
#include "jnx/diameter_avp_common_defs.h"
#include "jnx/diameter_base_api.h"

void diameter_peer_on_connect(diameter_peer_observer_handle h,
				diameter_peer_handle peer);

int diameter_peer_on_connect_timeout(diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				uint32_t elapsed_time_ms);

int diameter_peer_on_connect_failure(diameter_peer_observer_handle h,
				diameter_peer_handle peer);

void diameter_peer_on_disconnect(diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				diameter_peer_disconnect_reason_t reason);

void diameter_peer_on_incoming_queue_limit_exceeded(diameter_peer_observer_handle h,
						diameter_peer_handle peer,
						uint32_t queue_size);

void diameter_peer_on_incoming_queue_limit_recovered(diameter_peer_observer_handle h,
							diameter_peer_handle peer,
							uint32_t queue_size);

void diameter_peer_on_outgoing_queue_limit_exceeded(diameter_peer_observer_handle h,
							diameter_peer_handle peer);

void diameter_peer_on_outgoing_queue_limit_recovered(diameter_peer_observer_handle h,
							diameter_peer_handle peer);

void diameter_peer_on_state_change (diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				diameter_peer_state_t old,
				diameter_peer_state_t n);

void diameter_peer_on_dpr(diameter_peer_observer_handle h,
			diameter_peer_handle peer,
                        int disconnect_cause);

int diameter_peer_on_ce_failure(diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				unsigned long result_code,
				const char* error_message);

void diameter_peer_on_remote_error(diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				unsigned long result_code,
				const char* error_message);

void diameter_peer_on_release(diameter_peer_observer_handle h,
				diameter_peer_user_handle user_handle);

void diameter_peergroup_on_connect_event(diameter_peergroup_observer_handle h,
				diameter_peergroup_handle peergroup,
				diameter_peer_handle peer);

void diameter_peergroup_on_disconnect_event(diameter_peergroup_observer_handle h,
				diameter_peergroup_handle peergroup,
				diameter_peer_handle peer,
				diameter_peer_disconnect_reason_t reason,
				uint32_t num_pending_requests,
				uint32_t num_restarted_requests);

void diameter_peergroup_on_failover_event(diameter_peergroup_observer_handle h,
				diameter_peergroup_handle peergroup,
				diameter_peer_handle hpeer,
				uint32_t num_retransmitted_requests);

void diameter_peergroup_on_release_event(diameter_peergroup_observer_handle h,
				diameter_peergroup_user_handle user_handle);

#endif /* !_DIAMETER_EVENT_H */

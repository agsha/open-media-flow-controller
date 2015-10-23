/*
 * @file diameter_peer_event.c
 * @brief
 * diameter_peer_event.c - definations for diameter peer event functions
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
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <atomic_ops.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include "nkn_stat.h"
#include "nkn_debug.h"

#include "diameter_system.h"
#include "diameter_peer_event.h"

int peer_event_debug = 0;

NKNCNT_DEF(dia_peer_connect, uint64_t, "", "Peer Connect Event")
NKNCNT_DEF(dia_peer_connect_timeout, uint64_t, "", "Peer Connect Timeout")
NKNCNT_DEF(dia_peer_connect_failure, uint64_t, "", "Peer Connect Failure")
NKNCNT_DEF(dia_peer_disconnect, uint64_t, "", "Peer Disconnect Event")
NKNCNT_DEF(dia_peer_in_queue_exceeded, uint64_t, "", "Peer Incoming Queue Exceeded")
NKNCNT_DEF(dia_peer_in_queue_recovered, uint64_t, "", "Peer Incoming Queue Recovered")
NKNCNT_DEF(dia_peer_out_queue_exceeded, uint64_t, "", "Peer Outgoing Queue Exceeded")
NKNCNT_DEF(dia_peer_out_queue_recovered, uint64_t, "", "Peer Outgoing Queue Recovered")
NKNCNT_DEF(dia_peer_dpr, uint64_t, "", "Peer DPR event")
NKNCNT_DEF(dia_peer_ce_failure, uint64_t, "", "Peer CE Failure")
NKNCNT_DEF(dia_peer_remote_err, uint64_t, "", "Peer Remote Error")
NKNCNT_DEF(dia_peer_release, uint64_t, "", "Peer Release Event")

NKNCNT_DEF(dia_peergroup_connect, uint64_t, "", "Peergroup Connect Event")
NKNCNT_DEF(dia_peergroup_disconnect, uint64_t, "", "Peergroup Disconnect Event")
NKNCNT_DEF(dia_peergroup_failover, uint64_t, "", "Peergroup Failover")
NKNCNT_DEF(dia_peergroup_release, uint64_t, "", "Peergroup Release")

void diameter_peer_on_state_change(diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				diameter_peer_state_t old,
				diameter_peer_state_t new)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	if (peer_event_debug) {
		DBG_LOG(MSG, MOD_JPSD, "peer %s state %s -> state %s", po->name,
			diameter_peer_state_string(old), diameter_peer_state_string(new));
	}

	return;
}

void diameter_peer_on_connect(diameter_peer_observer_handle h,
				diameter_peer_handle peer)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_connect++;
	if (peer_event_debug) {
		DBG_LOG(MSG, MOD_JPSD, "diameter_peer_on_connect"
			" peer - %s peer_observer_handle - %p", po->name, h);
	}

	return;
}

int diameter_peer_on_connect_timeout(diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				uint32_t elapsed_time_ms)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_connect_timeout++;
	DBG_LOG(MSG, MOD_JPSD, "diameter_peer_on_connect_timeout"
		" peer - %s elapsed time %d", po->name, elapsed_time_ms);

	return 0;
}

int diameter_peer_on_connect_failure(diameter_peer_observer_handle h,
					diameter_peer_handle peer)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_connect_failure++;
	DBG_LOG(MSG, MOD_JPSD, "diameter_peer_on_connect_failure peer - %s", po->name);

	return 0;
}

void diameter_peer_on_disconnect(diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				diameter_peer_disconnect_reason_t reason)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_disconnect++;
	if (peer_event_debug) {
		DBG_LOG(MSG, MOD_JPSD, "diameter_peer_on_disconnect"
			" peer - %s reason - %d", po->name, reason);
	}

	return;
}

void diameter_peer_on_incoming_queue_limit_exceeded(
				diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				uint32_t queue_size)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_in_queue_exceeded++;
	DBG_LOG(MSG, MOD_JPSD,
		"diameter_peer_on_incoming_queue_limit_exceeded"
		" peer - %s queue_size - %d", po->name, queue_size);

	return;
}

void diameter_peer_on_incoming_queue_limit_recovered(
				diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				uint32_t queue_size)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_in_queue_recovered++;
	DBG_LOG(MSG, MOD_JPSD,
		"diameter_peer_on_incoming_queue_limit_recovered"
		" peer - %s queue_size - %d", po->name, queue_size);

	return;
}

void diameter_peer_on_outgoing_queue_limit_exceeded(
				diameter_peer_observer_handle h,
				diameter_peer_handle peer)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_out_queue_exceeded++;
	DBG_LOG(MSG, MOD_JPSD,
		"diameter_peer_on_outgoing_queue_limit_exceeded peer - %s", po->name);

	return;
}

void diameter_peer_on_outgoing_queue_limit_recovered(
				diameter_peer_observer_handle h,
				diameter_peer_handle peer)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_out_queue_recovered++;
	DBG_LOG(MSG, MOD_JPSD,
		"diameter_peer_on_outgoing_queue_limit_recovered peer - %p", po->name);

	return;
}

void diameter_peer_on_dpr(diameter_peer_observer_handle h,
			diameter_peer_handle peer,
                        int disconnect_cause)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_dpr++;
	DBG_LOG(MSG, MOD_JPSD, "diameter_peer_on_dpr"
		" peer - %s disconnect_cause - %d", po->name, disconnect_cause);

	return;
}

int diameter_peer_on_ce_failure(diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				unsigned long result_code,
				const char* error_message)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_ce_failure++;
	DBG_LOG(MSG, MOD_JPSD, "diameter_peer_on_ce_failure"
		" peer - %s result_code - %lu error_message - %s",
		po->name, result_code, error_message);

	return 0;
}

void diameter_peer_on_remote_error(diameter_peer_observer_handle h,
				diameter_peer_handle peer,
				unsigned long result_code,
				const char* error_message)
{
	(void) peer;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_remote_err++;
	DBG_LOG(MSG, MOD_JPSD, "diameter_peer_on_remote_error"
		" peer - %s result_code - %lu error_message - %s",
		po->name, result_code, error_message);

	return;
}

void diameter_peer_on_release(diameter_peer_observer_handle h,
				diameter_peer_user_handle user_handle)
{
	(void) user_handle;
	diameter_peer_observer_t *po = (diameter_peer_observer_t *)h;

	glob_dia_peer_release++;
	if (peer_event_debug) {
		DBG_LOG(MSG, MOD_JPSD, "diameter_peer_on_release"
			" peer_observer_handle - %s", po->name);
	}

	return;
}

static void diameter_peergroup_info(diameter_peergroup_handle peergroup)
{
	uint32_t vid[32];
	uint32_t rc;
	uint32_t size;
	uint32_t i;

	rc = diameter_peergroup_get_supported_vendor_ids(peergroup, vid, 32, &size);

	if (rc == DIAMETER_OK) {
		for (i = 0; i < size; i++) {
			DBG_LOG(MSG, MOD_JPSD, "%d", vid[i]);
		}
	} else {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_peergroup_get_supported_vendor_ids error %d", rc);
	}

	return;
}

void diameter_peergroup_on_connect_event(diameter_peergroup_observer_handle h,
				diameter_peergroup_handle peergroup,
				diameter_peer_handle peer)
{
	(void) peer;
	diameter_peergroup_observer_t *pgo = (diameter_peergroup_observer_t *)h;

	glob_dia_peergroup_connect++;
	if (peer_event_debug) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_peergroup_on_connect peergroup - %s", pgo->name);
		diameter_peergroup_info(peergroup);
	}

	return;
}

void diameter_peergroup_on_disconnect_event(diameter_peergroup_observer_handle h,
				diameter_peergroup_handle peergroup,
				diameter_peer_handle peer,
				diameter_peer_disconnect_reason_t reason,
				uint32_t num_pending_requests,
				uint32_t num_restarted_requests)
{
	(void) peer;
	diameter_peergroup_observer_t *pgo = (diameter_peergroup_observer_t *)h;

	glob_dia_peergroup_disconnect++;
	if (peer_event_debug) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_peergroup_on_disconnect peergroup - %s"
			" reason %d, num_pending_requests %d, num_restarted_requests %d",
			pgo->name, reason, num_pending_requests, num_restarted_requests);
		diameter_peergroup_info(peergroup);
	}

	return;
}

void diameter_peergroup_on_failover_event(diameter_peergroup_observer_handle h,
				diameter_peergroup_handle peergroup,
				diameter_peer_handle peer,
				uint32_t num_retransmitted_requests)

{
	(void) peer;
	diameter_peergroup_observer_t *pgo = (diameter_peergroup_observer_t *)h;

	glob_dia_peergroup_failover++;
	DBG_LOG(MSG, MOD_JPSD, "diameter_peergroup_on_failover"
		" peergroup - %s num_retransmitted_requests - %d",
		pgo->name, num_retransmitted_requests);
	diameter_peergroup_info(peergroup);

	return;
}

void diameter_peergroup_on_release_event(diameter_peergroup_observer_handle h,
				diameter_peergroup_user_handle user_handle)
{
	(void) user_handle;
	diameter_peergroup_observer_t *pgo = (diameter_peergroup_observer_t *)h;

	glob_dia_peergroup_release++;
	DBG_LOG(MSG, MOD_JPSD, "diameter_peergroup_on_release peergorup - %s", pgo->name);

	return;
}

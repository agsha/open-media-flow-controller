/*
 * @file diameter_message_event.c
 * @brief
 * diameter_message_event.c - definations for diameter message event functions
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
#include "diameter_message_event.h"

NKNCNT_DEF(dia_req_on_resp, uint64_t, "", "Request on Response");
NKNCNT_DEF(dia_req_on_pause, uint64_t, "", "Request on Pause");
NKNCNT_DEF(dia_req_on_err, uint64_t, "", "Request on Error");
NKNCNT_DEF(dia_req_on_timeout, uint64_t, "", "Request on Timeout");
NKNCNT_DEF(dia_req_on_peer_timeout, uint64_t, "", "Request on Peer Timeout");
NKNCNT_DEF(dia_req_on_release, uint64_t, "", "Request on Release");

NKNCNT_DEF(dia_res_on_release, uint64_t, "", "Response on Release");

void diameter_request_on_response(diameter_request_observer_handle h,
				uint16_t n, struct iovec *request,
				diameter_buffer_handle response_message,
				diameter_peer_handle peer,
				diameter_peergroup_handle peergroup,
				void *ctx)
{
	(void) h;
	(void) peergroup;
	(void) ctx;

	glob_dia_req_on_resp++;
	diameter_buf_release(response_message);

	return;
}

diameter_message_action_t diameter_request_on_pause(
				diameter_request_observer_handle h,
				uint16_t n, struct iovec *request,
				diameter_peergroup_handle peergroup,
				diameter_request_state_t state,
				uint32_t time_left_ms)
{
	(void) h;
	(void) time_left_ms;

	glob_dia_req_on_pause++;

	DBG_LOG(MSG, MOD_JPSD, "diameter_request_on_pause peergroup - %p"
		" request - %p count - %d state - %d", peergroup, request, n, state);

	return DIAMETER_MESSAGE_CONTINUE;
}

diameter_message_action_t diameter_request_on_error(
				diameter_request_observer_handle h,
				uint16_t n, struct iovec *request,
				diameter_peer_handle peer,
				diameter_peergroup_handle peergroup,
				diameter_request_state_t state,
				const char *error_message,
				int errorcode, uint32_t time_left_ms)
{
	(void) h;
	(void) peergroup;
	(void) error_message;
	(void) time_left_ms;

	glob_dia_req_on_err++;

	DBG_LOG(MSG, MOD_JPSD, "diameter_request_on_error peer - %p"
		" request - %p count - %d state - %d errorcode - %d",
		peer, request, n, state, errorcode);

	return DIAMETER_MESSAGE_CANCEL;
}

diameter_message_action_t diameter_request_on_timeout(
				diameter_request_observer_handle h,
				uint16_t n, struct iovec *request,
				diameter_peergroup_handle peergroup,
				diameter_request_state_t state)
{
	(void) h;
	(void) peergroup;

	glob_dia_req_on_timeout++;

	DBG_LOG(MSG, MOD_JPSD, "diameter_request_on_timeout peergroup - %p"
		" request - %p count - %d state - %d", peergroup, request, n, state);

	return DIAMETER_MESSAGE_CANCEL;
}

diameter_message_action_t diameter_request_on_peer_timeout(
				diameter_request_observer_handle h,
				uint16_t n, struct iovec *request,
				diameter_peer_handle peer,
				diameter_peergroup_handle peergroup,
				diameter_request_state_t state,
				uint32_t time_left_ms)
{
	(void) h;
	(void) peergroup;
	(void) time_left_ms;

	glob_dia_req_on_peer_timeout++;

	DBG_LOG(MSG, MOD_JPSD, "diameter_request_on_peer_timeout peer - %p"
		" request - %p count - %d state - %d", peer, request, n, state);

	return DIAMETER_MESSAGE_CONTINUE;
}

void diameter_request_on_release(diameter_request_observer_handle h,
				uint16_t diam_num_iovecs,
				struct iovec *diam_iovec,
				diameter_peergroup_handle peergroup,
				diameter_request_state_t state,
				diameter_message_action_t action)
{
	uint32_t count;

	(void) h;
	(void) peergroup;
	(void) state;
	(void) action;

	glob_dia_req_on_release++;
	for (count = 0; count < diam_num_iovecs; count++) {
		diameter_release_iov(diam_iovec[count].iov_base);
	}

	return;
}

void diameter_response_on_release(diameter_response_observer_handle h)
{
	(void) h;

	glob_dia_res_on_release++;

	return;
}

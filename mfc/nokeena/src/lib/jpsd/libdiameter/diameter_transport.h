/*
 * @file diameter_transport.h
 * @brief
 * diameter_transport.h - declarations for diameter transport functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef _DIAMETER_TRANSPORT_H
#define _DIAMETER_TRANSPORT_H

#include "sys/jnx/types.h"
#include "jnx/diameter_avp_common_defs.h"
#include "jnx/diameter_base_api.h"

uint32_t diameter_create_messenger(diameter_transport_handle transport,
				diameter_incoming_queue_config_t *incoming,
				diameter_outgoing_queue_config_t *outgoing,
				diameter_messenger_observer_handle observer_handle,
				diameter_messenger_handle *messenger);

uint32_t diameter_messenger_connect(diameter_messenger_handle messenger_handle,
				const diameter_address_t *address);

uint32_t diameter_messenger_disconnect(diameter_messenger_handle messenger_handle);

uint32_t diameter_messenger_send_data(diameter_messenger_handle messenger_handle,
				uint32_t n, struct iovec *request, void *uap);

uint32_t diameter_messenger_flow_off(diameter_messenger_handle messenger_handle);

uint32_t diameter_messenger_flow_on(diameter_messenger_handle messenger_handle);

uint32_t diameter_messenger_close(diameter_messenger_handle messenger_handle);

uint32_t diameter_messenger_configure_incoming_queue(
				diameter_messenger_handle messenger_handle,
				diameter_incoming_queue_config_t *incoming);

uint32_t diameter_messenger_configure_outgoing_queue(
				diameter_messenger_handle messenger_handle,
				diameter_outgoing_queue_config_t *outgoing);

void diameter_server_init(void);

void diameter_peer_observer_init(void);

#endif /* !_DIAMETER_TRANSPORT_H */

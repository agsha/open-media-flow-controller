/*
 * @file diameter_transport.c
 * @brief
 * diameter_transport.c - definations for diameter transport functions
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
#include <errno.h>
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

#include "nkn_memalloc.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

#include "jpsd_network.h"

#include "sys/jnx/types.h"
#include "jnx/getput.h"
#include "jnx/diameter_avp_common_defs.h"
#include "jnx/diameter_base_api.h"
#include "jnx/diameter_defs.h"

#include "diameter_system.h"
#include "diameter_transport.h"

static AO_t peer_id = 1;
static AO_t peergroup_id = 1;

#define MES_FD_REF_CLEAR	0x00000001

NKNCNT_DEF(dia_svr, uint16_t, "", "Total Diameter Servers")
NKNCNT_DEF(dia_svr_epollerr, int, "", "Total Diameter Server Epoll Error")
NKNCNT_DEF(dia_svr_epollhup, int, "", "Total Diameter Server Epoll Hangup")
NKNCNT_DEF(dia_svr_epollout, int, "", "Total Diameter Server Epoll out")

NKNCNT_DEF(dia_peer_create_err, int, "", "Diameter Peer Create Error")
NKNCNT_DEF(dia_peer_create_err1, int, "", "Diameter Peer Create Error")
NKNCNT_DEF(dia_peer_create_err2, int, "", "Diameter Peer Create Error")
NKNCNT_DEF(dia_peer_create_err3, int, "", "Diameter Peer Create Error")
NKNCNT_DEF(dia_peer_create_err4, int, "", "Diameter Peer Create Error")
NKNCNT_DEF(dia_peer_create_err5, int, "", "Diameter Peer Create Error")
NKNCNT_DEF(dia_peer_create_err6, int, "", "Diameter Peer Create Error")
NKNCNT_DEF(dia_peer_create_err7, int, "", "Diameter Peer Create Error")
NKNCNT_DEF(dia_peer_create_err8, int, "", "Diameter Peer Create Error")
NKNCNT_DEF(dia_peer_create_err9, int, "", "Diameter Peer Create Error")

NKNCNT_DEF(dia_peergroup_create_err, int, "", "Diameter Peergroup Create Error")
NKNCNT_DEF(dia_peergroup_create_err1, int, "", "Diameter Peergroup Create Error")
NKNCNT_DEF(dia_peergroup_start_err, int, "", "Diameter Peergroup Start Error")

NKNCNT_DEF(dia_peer_add_err, int, "", "Diameter Peer Add to Peergroup Error")
NKNCNT_DEF(dia_messenger_accept_err, int, "", "Diameter Messenger Accept Error")

NKNCNT_DEF(dia_peer_epollerr, int, "", "Total Diameter Peer Epoll Error")
NKNCNT_DEF(dia_peer_epollhup, int, "", "Total Diameter Peer Epoll Hangup")
NKNCNT_DEF(dia_peer_epollout, int, "", "Total Diameter Peer Epoll out")
NKNCNT_DEF(dia_peer_epollin_err, int, "", "Total Diameter Peer Epollin Error")

NKNCNT_DEF(dia_peer_tot, AO_t, "", "Total Diameter Peers")
NKNCNT_DEF(dia_peer_cur, AO_t, "", "Opened Diameter Peers")
NKNCNT_DEF(dia_peer_active, AO_t, "", "Active Diameter Peers")

NKNCNT_DEF(dia_peergroup_tot, AO_t, "", "Total Diameter Peergroups")
NKNCNT_DEF(dia_peergroup_cur, AO_t, "", "Opened Diameter Peergroupss")
NKNCNT_DEF(dia_peergroup_active, AO_t, "", "Active Diameter Peergroups")

extern int NM_tot_threads;

uint32_t aVendors[] = {10415};

diameter_application_id_t aVendorSpecificAuthApp[] =
        {{10415, FICTITIOUS_AUTH_APP}, {1411, FICTITIOUS_ACCT_APP}};

diameter_peer_observer_t *gpo;

void diameter_peer_observer_init(void)
{
	int i;

	gpo = nkn_calloc_type(1,
			sizeof(diameter_peer_observer_t) * MAX_GNM,
			mod_diameter_t);
	if (gpo == NULL)
		assert(0);

	for (i = 0; i < MAX_GNM; i++) {
		gpo[i].fd = i;
		pthread_mutex_init(&gpo[i].mutex, NULL);
	}

	return;
}

#if 0
static void diameter_messenger_release(diameter_messenger_t *messenger)
{
	int fd;

	if (messenger->data) {
		if (messenger->data->p)
			free(messenger->data->p);
		free(messenger->data);
		messenger->data = NULL;
	}

	fd = messenger->fd;
	free(messenger);
	NM_close_socket(fd);

	return;
}

void diameter_peer_observer_release(diameter_peer_observer_t *po)
{
	int ret;

	diameter_messenger_t *messenger = po->messenger;
	diameter_peergroup_observer_t *pgo = po->pgo;


	if (CHECK_FLAG(po, PEER_REF_IN_MO)) {
		ret = diameter_messenger_close_indication((uint64_t)messenger,
						messenger->observer_handle);
		assert(ret == DIAMETER_OK);
		UNSET_FLAG(po, PEER_REF_IN_MO);
	}

	if (pgo) {
		ret = diameter_peergroup_release(pgo->pg);
		assert (ret == DIAMETER_OK);
		free(pgo);
		AO_fetch_and_sub1(&glob_dia_peergroup_cur);
		if (CHECK_FLAG(pgo, PEERGROUP_STARTED)) {
			AO_fetch_and_sub1(&glob_dia_peergroup_active);
			UNSET_FLAG(pgo, PEERGROUP_STARTED);
		}
	}

	if (messenger) {
		diameter_messenger_release(messenger);
	}

	ret = diameter_peer_release(po->peer);
	assert (ret == DIAMETER_OK);

	AO_fetch_and_sub1(&glob_dia_peer_cur);
	AO_fetch_and_sub1(&glob_dia_peer_active);

	return;
}
#endif

static int diameter_peergroup_create(diameter_peer_observer_t *po)
{
	char name[] = "peergroup";
	int len = sizeof(name);
	int ret = 0;
	int id;

	diameter_peergroup_observer_t *pgo;

	/* create peergroup observer */
	pgo = nkn_calloc_type(1, sizeof(diameter_peergroup_observer_t),
				mod_diameter_t);
	if (pgo == NULL) {
		return FALSE;
	}

	id = AO_fetch_and_add1(&peergroup_id);
	snprintf(pgo->name, 32, "%s-%d", name, id);

	/* create peergroup */
	ret = diameter_system_create_peergroup(pgo->name,
				pgo, pgo, id, &(pgo->pg));
	if (ret != DIAMETER_OK) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_system_create_peergroup error"
			" peergroup - %s", pgo->name);
		glob_dia_peergroup_create_err1++;
		free(pgo);
		return FALSE;
	}
#if 0
        ret = diameter_peergroup_start(pgo->pg);
        if (ret != DIAMETER_CB_OK) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_peergroup_start error"
			" peergroup - %s", pgo->name);
                glob_dia_peergroup_start_err++;
		free(pgo);
		return FALSE;
        }
#endif

	po->pgo = pgo;

	AO_fetch_and_add1(&glob_dia_peergroup_tot);
	AO_fetch_and_add1(&glob_dia_peergroup_cur);

	return TRUE;
}

static int diameter_peer_create(diameter_messenger_t *messenger,
					diameter_peer_observer_t **ppo)
{
	char name[] = "peer";
	int len = sizeof(name);
	int id;
	int ret = 0;

	diameter_address_t address =
			{DIAMETER_IPV4, {0x7f000001}, 3868, DIAMETER_TCP, 0};
	diameter_peer_observer_t *po;

	address.u.ipv4 = htonl(messenger->src_addr);
	address.port = htons(messenger->src_port);

	/* get peer observer */
	po = &gpo[messenger->fd];
	if (po == NULL) {
		return FALSE;
	}

	id = AO_fetch_and_add1(&peer_id);
	snprintf(po->name, 32, "%s-%d", name, id);
	po->fd = messenger->fd;

	/* create peer */
	ret = diameter_system_create_peer(po->name, po, po, &po->peer);
	if (ret != DIAMETER_OK) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_system_create_peer error peer - %s", po->name);
		glob_dia_peer_create_err1++;
		free(po);
		return FALSE;
	}

	/* set transport */
	ret = diameter_peer_set_transport(po->peer, po);
	if (ret != DIAMETER_OK) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_system_create_peer error peer  - %s", po->name);
		glob_dia_peer_create_err2++;
		free(po);
		return FALSE;
	}

	/* set supported vendor ids */
	ret = diameter_peer_set_supported_vendor_ids(po->peer,
			aVendors, sizeof(aVendors)/sizeof(aVendors[0]));
	if (ret != DIAMETER_OK) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_system_create_peer error peer - %s", po->name);
		glob_dia_peer_create_err3++;
		free(po);
		return FALSE;
	}

	/* set auth application ids */
	ret = diameter_peer_set_auth_application_ids(po->peer, aVendorSpecificAuthApp,
			sizeof(aVendorSpecificAuthApp)/sizeof(aVendorSpecificAuthApp[0]));
	if (ret != DIAMETER_OK) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_system_create_peer error peer - %s", po->name);
		glob_dia_peer_create_err4++;
		free(po);
		return FALSE;
	}

	/* set remote address */
	ret = diameter_peer_set_remote_addresses(po->peer, &address, 1);
	if (ret != DIAMETER_OK) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_system_create_peer error peer - %s", po->name);
		glob_dia_peer_create_err5++;
		free(po);
		return FALSE;
	}

	/* set reconnect timeeout */
	ret = diameter_peer_set_reconnect_timeout(po->peer, 600000);
	if (ret != DIAMETER_OK) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_system_create_peer error peer - %s", po->name);
		glob_dia_peer_create_err6++;
		free(po);
		return FALSE;
	}

	/* set watchdog timeout */
	ret = diameter_peer_set_watchdog_timeout(po->peer, 100000);
	if (ret != DIAMETER_OK) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_system_create_peer error peer - %s", po->name);
		glob_dia_peer_create_err7++;
		free(po);
		return FALSE;
	}

	/* set repeat timeout */
	ret = diameter_peer_set_repeat_timeout(po->peer, 60000);
	if (ret != DIAMETER_OK) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_system_create_peer error peer - %s", po->name);
		glob_dia_peer_create_err8++;
		free(po);
		return FALSE;
	}

	/* set active */
	ret = diameter_peer_set_active(po->peer, true);
	if (ret != DIAMETER_OK) {
		DBG_LOG(MSG, MOD_JPSD,
			"diameter_system_create_peer error peer - %s", po->name);
		glob_dia_peer_create_err9++;
		free(po);
		return FALSE;
	}

	po->messenger = messenger;
	messenger->po = po;
	*ppo = po;

	AO_fetch_and_add1(&glob_dia_peer_tot);
	AO_fetch_and_add1(&glob_dia_peer_cur);
	AO_fetch_and_add1(&glob_dia_peer_active);

	return TRUE;
}

static void close_messenger(diameter_messenger_t *messenger)
{
        int fd;
        int ret;
        int state;
        diameter_peer_observer_t *po = messenger->po;
        diameter_peergroup_observer_t *pgo = po->pgo;

        fd = messenger->fd;
        if (messenger->data) {
                if (messenger->data->p)
                        free(messenger->data->p);
                free(messenger->data);
        }

        if (CHECK_FLAG(po, PEER_REF_IN_MO)) {
                ret = diameter_messenger_close_indication((uint64_t)messenger,
                                                messenger->observer_handle);
                assert(ret == DIAMETER_OK);
                UNSET_FLAG(po, PEER_REF_IN_MO);
        }

        if (pgo) {
                ret = diameter_peergroup_release(pgo->pg);
                assert(ret == DIAMETER_OK);
                free(pgo);
                AO_fetch_and_sub1(&glob_dia_peergroup_cur);
        }

        if (po) {
                ret = diameter_peer_release(po->peer);
                assert(ret == DIAMETER_OK);
                free(po);
                AO_fetch_and_sub1(&glob_dia_peer_cur);
                AO_fetch_and_sub1(&glob_dia_peer_active);
        }

        free(messenger);
        NM_close_socket(fd);

        return;
}

static int diameter_epollerr(int fd, void *private_data)
{
        diameter_messenger_t *messenger = (diameter_messenger_t *)private_data;
	diameter_peer_observer_t *po = messenger->po;

	glob_dia_peer_epollerr++;
	DBG_LOG(MSG, MOD_JPSD, "diameter_epoll err peer - %s", po->name);

        diameter_messenger_disconnect_indication((uint64_t)messenger,
                                        messenger->observer_handle);
        close_messenger(messenger);

	return TRUE;
}

static int diameter_epollhup(int fd, void *private_data)
{
        diameter_messenger_t *messenger = (diameter_messenger_t *)private_data;
	diameter_peer_observer_t *po = messenger->po;

	glob_dia_peer_epollhup++;
	DBG_LOG(MSG, MOD_JPSD, "diameter_epoll hup peer - %s", po->name);

	diameter_messenger_disconnect_indication((uint64_t)messenger,
                                        messenger->observer_handle);
        close_messenger(messenger);

	return TRUE;
}

static int diameter_epollout(int fd, void *private_data)
{
	(void) fd;
	(void) private_data;

	glob_dia_peer_epollout++;

	return TRUE;
}

static int diameter_epollin(int fd, void *private_data)
{
	uint8_t *p;
        uint32_t len;
        uint32_t msg_len;
        int ret;

	uint32_t buf_len = sizeof(struct diameter_buffer_s);
	uint32_t hdr_len = sizeof(diameter_message_header_t);

        diameter_messenger_t *messenger = (diameter_messenger_t *)private_data;
	diameter_peer_observer_t *po = messenger->po;
	diameter_peergroup_observer_t *pgo = po->pgo;

	/* allocate memory to recv packets if it is not */
	if (messenger->data == NULL) {
		messenger->data = nkn_malloc_type(buf_len, mod_diameter_t);
		if (messenger->data == NULL) {
			goto err;
		}
                messenger->data->p = nkn_malloc_type(1024, mod_diameter_t);
		if (messenger->data->p == NULL) {
			goto err;
		}
		messenger->data->size = 0;
		messenger->data->len = 1024;
        }

	/* recv header */
        if (messenger->data->size < hdr_len) {
                len = hdr_len;
                msg_len = 0;
        } else {
		/* find payload len */
		p = messenger->data->p;
                msg_len = diameter_message_length((diameter_message_header_t *)p);
                len = msg_len - messenger->data->size;
		if (msg_len > messenger->data->len) {
			p = nkn_realloc_type(messenger->data->p,
						msg_len, mod_diameter_t);
			if (p == NULL) {
				goto err;
			}
			messenger->data->p = p;
			messenger->data->len = msg_len;
		}
        }

        ret = recv(fd, (messenger->data->p + messenger->data->size), len, 0);
        if (ret <= 0) {
		goto err;
        } else {
                messenger->data->size += ret;
		/* call diameter apis to process data */
                if (messenger->data->size == msg_len) {
                        diameter_messenger_data_indication((uint64_t)messenger,
				messenger->observer_handle, messenger->data, NULL);
                        messenger->data  = NULL;
		}
        }

	if (!CHECK_FLAG(pgo, PEERGROUP_STARTED)) {
		if (is_ropen_state(po->peer) == 0) return TRUE;
		ret = diameter_peergroup_start(pgo->pg);
		if (ret != DIAMETER_CB_OK) {
			DBG_LOG(MSG, MOD_JPSD,
				"diameter_peergroup_start error"
					" peergroup - %s", pgo->name);
			glob_dia_peergroup_start_err++;
			goto err;
		}
		SET_FLAG(pgo, PEERGROUP_STARTED);
        }

	return TRUE;

err:
	glob_dia_peer_epollin_err++;
        diameter_messenger_disconnect_indication((uint64_t)messenger,
                                        messenger->observer_handle);
        close_messenger(messenger);

        return TRUE;
}

uint32_t diameter_create_messenger(diameter_transport_handle transport,
				diameter_incoming_queue_config_t* incoming,
				diameter_outgoing_queue_config_t* outgoing,
				diameter_messenger_observer_handle observer_handle,
				diameter_messenger_handle *messenger_handle)
{
        int fd;
	int ret;
	int size;
        diameter_messenger_t *messenger;
	network_mgr_t *pnm;

        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
                return DIAMETER_CB_ERROR;

	/* create the messenger */
	size = sizeof(diameter_messenger_t);
	messenger = nkn_calloc_type(1, size, mod_diameter_t);
	if (messenger == NULL) {
		NM_close_socket(fd);
		return DIAMETER_CB_ERROR;
	}

	/* initialize the messenger */
	pnm = &gnm[fd];
	messenger->fd = fd;
	messenger->transport_handle = transport;
	messenger->observer_handle = observer_handle;
	memcpy(&messenger->incoming, incoming,
			sizeof(diameter_incoming_queue_config_t));
	memcpy(&messenger->outgoing, outgoing,
			sizeof(diameter_outgoing_queue_config_t));

	ret = register_NM_socket(fd, messenger, diameter_epollin,
			diameter_epollout, diameter_epollerr, diameter_epollhup);
	if (ret == FALSE) {
		close_messenger(messenger);
		return DIAMETER_CB_ERROR;
	}

	*messenger_handle = (diameter_messenger_handle)messenger;

        return DIAMETER_CB_OK;
}


uint32_t diameter_messenger_connect(diameter_messenger_handle messenger_handle,
				const diameter_address_t* address)
{
        int fd;
	int ret;
	socklen_t dlen;
        struct sockaddr_in addr;
	struct sockaddr_in dip;
        diameter_messenger_t *messenger;

        switch(address->family) {
        case DIAMETER_IPV4:
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = htonl(address->u.ipv4);
                addr.sin_port =  htons(address->port);
                break;
        case DIAMETER_IPV6:
                return DIAMETER_CB_ERROR;
        }

        messenger = (diameter_messenger_t *)messenger_handle;
        fd = messenger->fd;

	/* connect with the registerd address */
        ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        if (ret < 0) {
                diameter_messenger_connect_failure(messenger_handle,
					messenger->observer_handle);
                return DIAMETER_CB_ERROR;
        }

	/* src address and port should be us */
	if (getsockname(fd, &dip, &dlen) != -1) {
		messenger->src_addr = dip.sin_addr.s_addr;
		messenger->src_port = dip.sin_port;
        } else {
                return DIAMETER_CB_ERROR;
        }
	messenger->dst_addr = addr.sin_addr.s_addr;
	messenger->dst_port = addr.sin_port;

        diameter_messenger_connect_confirmation(messenger_handle,
					messenger->observer_handle);

	return DIAMETER_CB_OK;
}

uint32_t diameter_messenger_disconnect(diameter_messenger_handle messenger_handle)
{
	int fd;
        diameter_messenger_t *messenger = (diameter_messenger_t *)messenger_handle;

	fd = messenger->fd;
	diameter_messenger_disconnect_confirmation(messenger_handle,
					messenger->observer_handle);

        return DIAMETER_CB_OK;
}

uint32_t diameter_messenger_send_data(diameter_messenger_handle messenger_handle,
				uint32_t n, struct iovec *request, void *uap)
{
        int fd, ret;
        diameter_messenger_t *messenger;

        (void) uap;
	messenger = (diameter_messenger_t *)messenger_handle;
	fd = messenger->fd;

        ret = writev(fd, request, n);
	if (ret == -1) {
		if (errno == EAGAIN) {
			NM_del_event_epoll(fd);
			NM_add_event_epollout(fd);
			return DIAMETER_CB_OK;
		}
                return DIAMETER_CB_ERROR;
        }

        return DIAMETER_CB_OK;
}

uint32_t diameter_messenger_flow_off(diameter_messenger_handle messenger_handle)
{
        (void) messenger_handle;
        return DIAMETER_CB_OK;
}

uint32_t diameter_messenger_flow_on(diameter_messenger_handle messenger_handle)
{
        (void) messenger_handle;
        return DIAMETER_CB_OK;
}

uint32_t diameter_messenger_configure_incoming_queue(
				diameter_messenger_handle messenger_handle,
				diameter_incoming_queue_config_t *incoming)
{
	(void) messenger_handle;
	(void) incoming;

	return DIAMETER_CB_OK;
}

uint32_t diameter_messenger_configure_outgoing_queue(
				diameter_messenger_handle messenger_handle,
				diameter_outgoing_queue_config_t *outgoing)
{
	(void) messenger_handle;
	(void) outgoing;

	return DIAMETER_CB_OK;
}

uint32_t diameter_messenger_close(diameter_messenger_handle messenger_handle)
{
	int fd;
	int ret;
        diameter_messenger_t *messenger;
	diameter_peer_observer_t *po;

	messenger = (diameter_messenger_t *)messenger_handle;
	if (messenger->type != DIAMETER_SERVER) {
		return DIAMETER_CB_OK;
	}

	if (messenger->observer_handle) {
		po = messenger->po;
		ret = diameter_messenger_close_indication((uint64_t)messenger,
						messenger->observer_handle);
		assert(ret == DIAMETER_OK);
		UNSET_FLAG(po, PEER_REF_IN_MO);
	}

        return DIAMETER_CB_OK;
}

static void diameter_messenger_init(int fd, struct sockaddr_storage *addr)
{
	int ret;
	int timeout = 10000;
	int priority = 1;
	socklen_t dlen;
	struct sockaddr_in *addrptr;
	struct sockaddr_in dip;
	network_mgr_t *pnm;
	diameter_messenger_t *messenger;
	diameter_peer_observer_t *po;
	diameter_peergroup_observer_t *pgo;

	/* create messenger */
	messenger = nkn_calloc_type(1, sizeof(diameter_messenger_t), mod_diameter_t);
	if (messenger == NULL) {
		NM_close_socket(fd);
		return;
	}

	/* initialize messenger */
	pnm = &gnm[fd];
	messenger->fd = fd;
	messenger->type = DIAMETER_SERVER;
	addrptr = (struct sockaddr_in *)addr;
	/* src address and port are incoming connection's address and port */
	messenger->src_addr = addrptr->sin_addr.s_addr;
	messenger->src_port = addrptr->sin_port;
	dlen = sizeof(struct sockaddr_in);
	memset(&dip, 0, dlen);
	if (getsockname(fd, &dip, &dlen) != -1) {
		messenger->dst_addr = dip.sin_addr.s_addr;
		messenger->dst_port = dip.sin_port;
        } else {
		close_messenger(messenger);
		return;
	}
	ret = register_NM_socket(fd, (void *)messenger,
				diameter_epollin,
				diameter_epollout,
				diameter_epollerr,
				diameter_epollhup);
	if (ret == FALSE) {
		close_messenger(messenger);
		return;
	}

	/* create the peer and peer observer */
	ret = diameter_peer_create(messenger, &po);
	if (ret == FALSE) {
		glob_dia_peer_create_err++;
		close_messenger(messenger);
		return;
	}

	/* create the peergroup and peergroup observer */
	ret = diameter_peergroup_create(po);
	if (ret == FALSE) {
		glob_dia_peergroup_create_err++;
		close_messenger(messenger);
		return;
	}
	pgo = po->pgo;

	/* add the peer to the peergroup */
	ret = diameter_peergroup_add_peer(pgo->pg, po->peer, priority, timeout);
	if (ret != DIAMETER_OK) {
		glob_dia_peer_add_err++;
		close_messenger(messenger);
		return;
	}

	/* create the messenger observer */
	ret = diameter_messenger_accept_indication((uint64_t)messenger,
				po->peer, &(messenger->observer_handle));
	if (ret !=  DIAMETER_OK) {
		glob_dia_messenger_accept_err++;
		close_messenger(messenger);
		return;
	}

	SET_FLAG(po, PEER_REF_IN_MO);

	/* finally add fd into the epoll */
	if (NM_add_event_epollin(fd) == FALSE) {
		close_messenger(messenger);
		return;
	}

	messenger->active = 1;

	DBG_LOG(MSG, MOD_JPSD,
		"peer - %s is registered with peergroup - %s", po->name, pgo->name);

	return;
}

static int diameter_svr_epollerr(int fd, void *private_data)
{
	(void) fd;
	(void) private_data;

	glob_dia_svr_epollerr++;

	return TRUE;
}

static int diameter_svr_epollhup(int fd, void *private_data)
{
	(void) fd;
	(void) private_data;

	glob_dia_svr_epollhup++;

	return TRUE;
}

static int diameter_svr_epollout(int fd, void *private_data)
{
	(void) fd;
	(void) private_data;

	glob_dia_svr_epollout++;

	return TRUE;
}

static int diameter_svr_epollin(int fd, void *private_data)
{
	int clifd;
	int cnt;
	int ret;
	struct sockaddr_in addr;
	socklen_t addrlen;
	(void)private_data;

	addrlen = sizeof(struct sockaddr_in);
	for (cnt = 0; cnt < 500; cnt++) {
		clifd = accept(fd, (struct sockaddr *)&addr, &addrlen);
		if (clifd == -1) {
			return TRUE;
		}
		ret = NM_set_socket_nonblock(clifd);
		if (ret == FALSE) {
			NM_close_socket(clifd);
			continue;
		}
		diameter_messenger_init(clifd, (struct sockaddr_storage *)&addr);
	}

	return TRUE;
}

void diameter_server_init(void)
{
	int i;
	int ret;
	int val;
	int listenfd;
	struct sockaddr_in srv;

	for (i = 0; i < NM_tot_threads; i++) {
		listenfd = socket(AF_INET, SOCK_STREAM, 0);
		if (listenfd == -1) {
			continue;
		}

		val = 1;
		ret = setsockopt(listenfd, SOL_SOCKET,
				SO_REUSEADDR, &val, sizeof(val));
		if (ret == -1) {
			close(listenfd);
			continue;
		}

		ret = setsockopt(listenfd, SOL_SOCKET,
				SO_REUSEPORT, &val, sizeof(val));
		if (ret == -1) {
			close(listenfd);
			continue;
		}

		memset(&srv, 0, sizeof(srv));
		srv.sin_family = AF_INET;
		srv.sin_addr.s_addr = INADDR_ANY;
		srv.sin_port = htons(3868);

		ret = bind(listenfd, (struct sockaddr *)&srv, sizeof(srv));
		if (ret == -1) {
			close(listenfd);
			continue;
		}

		ret = NM_set_socket_nonblock(listenfd);
		if (ret == FALSE) {
			close(listenfd);
			continue;
		}

		listen(listenfd, 1000);
		ret = register_NM_socket(listenfd, NULL,
				diameter_svr_epollin,
				diameter_svr_epollout,
				diameter_svr_epollerr,
				diameter_svr_epollhup);
		if (ret == FALSE) {
			close(listenfd);
			continue;
		}
		if (NM_add_event_epollin(listenfd) == FALSE) {
			close(listenfd);
			continue;
		}
		glob_dia_svr++;
	}

	assert(glob_dia_svr != 0);

	return;
}

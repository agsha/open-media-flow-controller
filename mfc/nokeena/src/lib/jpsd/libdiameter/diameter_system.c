/*
 * @file diameter_system.c
 * @brief
 * diameter_system.c - definations for diameter system functions.
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

#include "nkn_memalloc.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

#include "diameter_system.h"
#include "diameter_timer.h"
#include "diameter_transport.h"
#include "diameter_peer_event.h"
#include "diameter_message_event.h"

#define DIAMETER_DATA_STORE_ITEMS_MAX 2000000

struct diameter_data_store_s;

typedef uint32_t data_store_hash(struct diameter_data_store_s *tree, void *key);

typedef struct diameter_data_store_s {
	diameter_data_store_parameters_t  parameters;
	void *array[DIAMETER_DATA_STORE_ITEMS_MAX];
	pthread_mutex_t cs;
	data_store_hash *cb_hash;
} diameter_data_store_t;

typedef struct diameter_data_store_iterator_s {
	diameter_data_store_t *ds;
	uint32_t index;
} diameter_data_store_iterator_t;

NKNCNT_DEF(dia_obj_req_ctx, AO_t, "", "Object Type - Incoming Request Context")
NKNCNT_DEF(dia_obj_peer_event, AO_t, "", "Object Type - Peer Event")
NKNCNT_DEF(dia_obj_peer, AO_t, "", "Object Type - Peer")
NKNCNT_DEF(dia_obj_peer_connection, AO_t, "", "Object Type - Peer Event")
NKNCNT_DEF(dia_obj_peergroup, AO_t, "", "Object Type - Peergroup")
NKNCNT_DEF(dia_obj_server, AO_t, "", "Object Type - Server")
NKNCNT_DEF(dia_obj_unknown, AO_t, "", "Object Type - Unknown")

NKNCNT_DEF(dia_obj_req_ctx_cnt, AO_t, "",
			"Object Type - Incoming Request Context Refcount")
NKNCNT_DEF(dia_obj_peer_event_cnt, AO_t, "",
			"Object Type - Peer Event Refcount")
NKNCNT_DEF(dia_obj_peer_cnt, AO_t, "", "Object Type - Peer Refcount")
NKNCNT_DEF(dia_obj_peer_connection_cnt, AO_t, "",
			"Object Type - Peer Event Refcount")
NKNCNT_DEF(dia_obj_peergroup_cnt, AO_t, "", "Object Type - Peergroup Refcount")
NKNCNT_DEF(dia_obj_server_cnt, AO_t, "", "Object Type - Server Refcount")
NKNCNT_DEF(dia_obj_unknown_cnt, AO_t, "", "Object Type - Unknown Refcount")

static void *diameter_alloc(uint32_t size, bool critical __attribute__((unused)))
{
	void *p;

	p = nkn_malloc_type(size, mod_diameter_t);
	return p;
}

static void diameter_free(void *p, bool critical __attribute__((unused)))
{
	free(p);
	return;
}

static void *diameter_object_alloc(diameter_object_type_t type, uint32_t size)
{
	void *p;
	unsigned int *refcount;

	p = nkn_malloc_type((sizeof(unsigned int)+size), mod_diameter_t);
	if (p == NULL)
		return NULL;
	refcount = (unsigned int *)p;
	AO_int_store(refcount, 0);

	switch(type) {
	case DIAMETER_TYPE_INCOMING_REQ_CTX :
		AO_fetch_and_add1(&glob_dia_obj_req_ctx);
		break;
	case DIAMETER_TYPE_PEER_EVENT:
		AO_fetch_and_add1(&glob_dia_obj_peer_event);
		break;
	case DIAMETER_TYPE_PEER:
		AO_fetch_and_add1(&glob_dia_obj_peer);
		break;
	case DIAMETER_TYPE_PEER_CONNECTION:
		AO_fetch_and_add1(&glob_dia_obj_peer_connection);
		break;
	case DIAMETER_TYPE_PEERGROUP:
		AO_fetch_and_add1(&glob_dia_obj_peergroup);
		break;
	case DIAMETER_TYPE_SERVER:
		AO_fetch_and_add1(&glob_dia_obj_server);
		break;
	default:
		AO_fetch_and_add1(&glob_dia_obj_unknown);
		break;
	}

	return (uint8_t *)p + sizeof(unsigned int);
}

static uint32_t diameter_object_free(diameter_object_type_t type, void *p)
{
	unsigned int *refcount;

	if (p == NULL)
		return DIAMETER_CB_ERROR;
	refcount = (unsigned int *)p - 1;
	free(refcount);

	switch(type) {
	case DIAMETER_TYPE_INCOMING_REQ_CTX:
		AO_fetch_and_sub1(&glob_dia_obj_req_ctx);
		break;
	case DIAMETER_TYPE_PEER_EVENT:
		AO_fetch_and_sub1(&glob_dia_obj_peer_event);
		break;
	case DIAMETER_TYPE_PEER:
		AO_fetch_and_sub1(&glob_dia_obj_peer);
		break;
	case DIAMETER_TYPE_PEER_CONNECTION:
		AO_fetch_and_sub1(&glob_dia_obj_peer_connection);
		break;
	case DIAMETER_TYPE_PEERGROUP:
		AO_fetch_and_sub1(&glob_dia_obj_peergroup);
		break;
	case DIAMETER_TYPE_SERVER:
		AO_fetch_and_sub1(&glob_dia_obj_server);
		break;
	default:
		AO_fetch_and_sub1(&glob_dia_obj_unknown);
		break;
	}

	return DIAMETER_CB_OK;
}

static int diameter_object_addref(diameter_object_type_t type, void *p)
{
	unsigned int *refcount;

	switch(type) {
	case DIAMETER_TYPE_INCOMING_REQ_CTX :
		AO_fetch_and_add1(&glob_dia_obj_req_ctx_cnt);
		break;
	case DIAMETER_TYPE_PEER_EVENT:
		AO_fetch_and_add1(&glob_dia_obj_peer_event_cnt);
		break;
	case DIAMETER_TYPE_PEER:
		AO_fetch_and_add1(&glob_dia_obj_peer_cnt);
		break;
	case DIAMETER_TYPE_PEER_CONNECTION:
		AO_fetch_and_add1(&glob_dia_obj_peer_connection_cnt);
		break;
	case DIAMETER_TYPE_PEERGROUP:
		AO_fetch_and_add1(&glob_dia_obj_peergroup_cnt);
		break;
	case DIAMETER_TYPE_SERVER:
		AO_fetch_and_add1(&glob_dia_obj_server_cnt);
		break;
	default:
		AO_fetch_and_add1(&glob_dia_obj_unknown_cnt);
		break;
	}

	refcount = (unsigned int *)p - 1;
	return (AO_int_fetch_and_add1(refcount) + 1);
}

static int diameter_object_release(diameter_object_type_t type, void *p)
{
	unsigned int *refcount;

	switch(type) {
	case DIAMETER_TYPE_INCOMING_REQ_CTX:
		AO_fetch_and_sub1(&glob_dia_obj_req_ctx_cnt);
		break;
	case DIAMETER_TYPE_PEER_EVENT:
		AO_fetch_and_sub1(&glob_dia_obj_peer_event_cnt);
		break;
	case DIAMETER_TYPE_PEER:
		AO_fetch_and_sub1(&glob_dia_obj_peer_cnt);
		break;
	case DIAMETER_TYPE_PEER_CONNECTION:
		AO_fetch_and_sub1(&glob_dia_obj_peer_connection_cnt);
		break;
	case DIAMETER_TYPE_PEERGROUP:
		AO_fetch_and_sub1(&glob_dia_obj_peergroup_cnt);
		break;
	case DIAMETER_TYPE_SERVER:
		AO_fetch_and_sub1(&glob_dia_obj_server_cnt);
		break;
	default:
		AO_fetch_and_sub1(&glob_dia_obj_unknown_cnt);
		break;
	}

	refcount = (unsigned int *)p - 1;
	return (AO_int_fetch_and_sub1(refcount) - 1);
}

static uint32_t diameter_create_lock(diameter_lock_handle *lock)
{
	pthread_rwlockattr_t rwlock_attr;
	pthread_rwlock_t *rwlock;
	int ret;


	ret = pthread_rwlockattr_init(&rwlock_attr);
	if (ret)
		return DIAMETER_CB_ERROR;

	ret = pthread_rwlockattr_setkind_np(&rwlock_attr,
			PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
	if (ret)
		return DIAMETER_CB_ERROR;

	rwlock = nkn_malloc_type(sizeof(pthread_rwlock_t), mod_diameter_t);
	if (rwlock == NULL)
		return DIAMETER_CB_ERROR;

	ret = pthread_rwlock_init(rwlock, &rwlock_attr);
	if (ret) {
		free(rwlock);
		return DIAMETER_CB_ERROR;
	}

	*lock = rwlock;

	return DIAMETER_CB_OK;
}

static uint32_t diameter_destroy_lock(diameter_lock_handle lock)
{
	pthread_rwlock_t *rwlock = (pthread_rwlock_t *)lock;

	pthread_rwlock_destroy(rwlock);
	free(rwlock);

	return DIAMETER_CB_OK;
}

static uint32_t diameter_read_lock(diameter_lock_handle lock)
{
	pthread_rwlock_t *rwlock = (pthread_rwlock_t *)lock;

	pthread_rwlock_rdlock(rwlock);

	return DIAMETER_CB_OK;
}

static uint32_t diameter_read_unlock(diameter_lock_handle lock)
{
	pthread_rwlock_t *rwlock = (pthread_rwlock_t *)lock;

	pthread_rwlock_unlock(rwlock);

	return DIAMETER_CB_OK;
}

static uint32_t diameter_write_lock(diameter_lock_handle lock)
{
	pthread_rwlock_t *rwlock = (pthread_rwlock_t *)lock;

	pthread_rwlock_wrlock(rwlock);

	return DIAMETER_CB_OK;
}

static uint32_t diameter_write_unlock(diameter_lock_handle lock)
{
	pthread_rwlock_t *rwlock = (pthread_rwlock_t *)lock;

	pthread_rwlock_unlock(rwlock);

	return DIAMETER_CB_OK;
}

void *diameter_create_iov(uint32_t size)
{
	void *p;
	p = nkn_malloc_type((sizeof(unsigned int)+size), mod_diameter_t);
	if (p == NULL)
		return NULL;
	AO_int_store((unsigned int *)p, 1);
	return (unsigned int *)p + 1;
}

void diameter_release_iov(void *p)
{
	unsigned int *refcount = (unsigned int *)p - 1;
	if (*refcount == 1)
		free(refcount);
	else
		AO_int_fetch_and_sub1(refcount);
	return;
}

void diameter_addref_iov(void *p)
{
	unsigned int *refcount = (unsigned int *)p - 1;
	AO_int_fetch_and_add1(refcount);

	return;
}

static int diameter_edit_iov(struct iovec *iov __attribute__((unused)))
{
    return DIAMETER_CB_OK;
}

static void diameter_peer_stat_set(diameter_peer_handle peer,
				diameter_peer_stat_t stat, int value)
{
	diameter_peer_observer_t *po;

	diameter_peer_get_user_data_handle(peer, (void *)&po);
	AO_int_store(&po->stats[stat], value);

	return;
}

static void diameter_peer_stat_increment(diameter_peer_handle peer,
				diameter_peer_stat_t stat)
{
	diameter_peer_observer_t *po;

	diameter_peer_get_user_data_handle(peer, (void *)&po);
	AO_int_fetch_and_add1(&po->stats[stat]);

	return;
}

static void diameter_peer_stat_decrement(diameter_peer_handle peer,
				diameter_peer_stat_t stat)
{
	diameter_peer_observer_t *po;

	diameter_peer_get_user_data_handle(peer, (void *)&po);
	AO_int_fetch_and_sub1(&po->stats[stat]);

	return;
}

static void diameter_peergroup_stat_increment(
				diameter_peergroup_handle peergroup,
				diameter_peergroup_stat_t stat)
{
	diameter_peergroup_observer_t *pgo;

	diameter_peergroup_get_user_data_handle(peergroup, (void *)&pgo);
	AO_int_fetch_and_add1(&pgo->stats[stat]);

	return;
}

static void diameter_peergroup_stat_decrement(
				diameter_peergroup_handle peergroup,
				diameter_peergroup_stat_t stat)
{
	diameter_peergroup_observer_t *pgo;

	diameter_peergroup_get_user_data_handle(peergroup, (void *)&pgo);
	AO_int_fetch_and_sub1(&pgo->stats[stat]);

	return;
}

static uint32_t diameter_data_store_hash_int(
				diameter_data_store_t *tree, void *key)
{
    (void)tree;
    uint32_t *idx = (uint32_t *)key;

    return *idx;
}

#define mix(a,b,c) \
{ \
      a -= b; a -= c; a ^= (c>>13); \
      b -= c; b -= a; b ^= (a<<8); \
      c -= a; c -= b; c ^= (b>>13); \
      a -= b; a -= c; a ^= (c>>12);  \
      b -= c; b -= a; b ^= (a<<16); \
      c -= a; c -= b; c ^= (b>>5); \
      a -= b; a -= c; a ^= (c>>3);  \
      b -= c; b -= a; b ^= (a<<10); \
      c -= a; c -= b; c ^= (b>>15); \
}

static int32_t hash(unsigned char *k, uint32_t length, uint32_t initval)
{
	uint32_t a, b, c, len;

	/* Set up the internal state */
	len = length;
	a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
	c = initval;         /* the previous hash value */

	/* handle most of the key */
	while (len >= 12) {
		a += (k[0] + ((uint32_t)k[1]<<8) +
			((uint32_t)k[2]<<16) + ((uint32_t)k[3]<<24));
		b += (k[4] + ((uint32_t)k[5]<<8) +
			((uint32_t)k[6]<<16) + ((uint32_t)k[7]<<24));
		c += (k[8] + ((uint32_t)k[9]<<8) +
			((uint32_t)k[10]<<16) + ((uint32_t)k[11]<<24));
		mix(a,b,c);
		k += 12; len -= 12;
	}
	/* handle the last 11 bytes */
	c += length;
	switch (len) { /* all the case statements fall through */
	case 11:
		c += ((uint32_t)k[10]<<24);
        case 10:
		c += ((uint32_t)k[9]<<16);
        case 9:
		c += ((uint32_t)k[8]<<8);
	/* the first byte of c is reserved for the length */
        case 8:
		b += ((uint32_t)k[7]<<24);
        case 7:
		b += ((uint32_t)k[6]<<16);
        case 6:
		b += ((uint32_t)k[5]<<8);
        case 5:
		b += k[4];
        case 4:
		a += ((uint32_t)k[3]<<24);
        case 3:
		a += ((uint32_t)k[2]<<16);
        case 2:
		a += ((uint32_t)k[1]<<8);
        case 1:
		a += k[0];
	}
	mix(a,b,c);

	return c % DIAMETER_DATA_STORE_ITEMS_MAX;
}

static uint32_t diameter_data_store_hash_string(
				diameter_data_store_t *tree, void *key)
{
	uint32_t length = tree->parameters.key_len[0];
	uint32_t idx = hash((unsigned char*)key, length, 0);

	return idx;
}

static uint32_t diameter_data_store_create(diameter_data_store_type_t type,
				const char *name,
				diameter_data_store_parameters_t *parameters,
				diameter_data_store_handle *handle)
{
	(void) type;
	(void) name;
	diameter_data_store_t *tree = NULL;

	if (parameters->total_keys == 1) {
		switch (parameters->key_type[0]) {
		case DIAMETER_DATA_STORE_KEY_INT32:
			tree = nkn_malloc_type(
				sizeof(diameter_data_store_t), mod_diameter_t);
			memset(tree, 0, sizeof(diameter_data_store_t));
			tree->cb_hash = diameter_data_store_hash_int;
			break;
		case DIAMETER_DATA_STORE_KEY_INT64:
			tree = nkn_malloc_type(
				sizeof(diameter_data_store_t), mod_diameter_t);
			memset(tree, 0, sizeof(diameter_data_store_t));
			tree->cb_hash = diameter_data_store_hash_string;
			break;
		case DIAMETER_DATA_STORE_KEY_STRING:
			tree = nkn_malloc_type(
				sizeof(diameter_data_store_t), mod_diameter_t);
			memset(tree, 0, sizeof(diameter_data_store_t));
			tree->cb_hash = diameter_data_store_hash_string;
			break;
		default:
			return DIAMETER_CB_ERROR;
		}

		tree->parameters = *parameters;
		pthread_mutex_init(&tree->cs, NULL);
		*handle = (diameter_data_store_handle) tree;

		return DIAMETER_CB_OK;
	}

        return DIAMETER_CB_ERROR;
}

static uint32_t diameter_data_store_destroy(diameter_data_store_handle handle)
{
	diameter_data_store_t *tree = (diameter_data_store_t *)handle;
	pthread_mutex_destroy(&tree->cs);
	free(tree);

        return DIAMETER_CB_OK;
}

static uint32_t diameter_data_store_insert(diameter_data_store_handle handle,
				void *obj, void *key[DIAMETER_DATA_STORE_MAX_KEYS])
{
	(void)key;
	diameter_data_store_t *tree = (diameter_data_store_t *)handle;
	void *ori_key = ((uint8_t*) obj + tree->parameters.key_offset[0]);
	uint32_t idx = tree->cb_hash(tree, ori_key);

	pthread_mutex_lock(&tree->cs);
	tree->array[idx] = obj;
	pthread_mutex_unlock(&tree->cs);

        return DIAMETER_CB_OK;
}

static uint32_t diameter_data_store_remove(diameter_data_store_handle handle,
				void *key[DIAMETER_DATA_STORE_MAX_KEYS], void **obj)
{
	diameter_data_store_t *tree = (diameter_data_store_t*)handle;
	uint32_t idx = tree->cb_hash(tree, key[0]);

	pthread_mutex_lock (&tree->cs);
	*obj = tree->array [idx];
	tree->array [idx] = NULL;
	pthread_mutex_unlock (&tree->cs);

	return DIAMETER_CB_OK;
}

static uint32_t diameter_data_store_find(diameter_data_store_handle handle,
				void *key[DIAMETER_DATA_STORE_MAX_KEYS], void **obj)
{
	diameter_data_store_t *tree = (diameter_data_store_t *)handle;
	uint32_t idx = tree->cb_hash(tree, key);

	pthread_mutex_lock (&tree->cs);
	*obj = tree->array [idx];
	pthread_mutex_unlock (&tree->cs);

	return DIAMETER_CB_OK;
}

static uint32_t diameter_data_store_callback_find(
				diameter_data_store_handle handle,
				void *key[DIAMETER_DATA_STORE_MAX_KEYS],
				void **obj, void *args)
{
	(void) handle;
	(void) key;
	(void) obj;
	(void) args;

	return DIAMETER_CB_OK;
}

static uint32_t diameter_data_store_create_iterator(
				diameter_data_store_handle handle,
				uint32_t flags, void *params,
				diameter_data_store_iterator_handle *p)
{
	(void) flags;
	(void) params;

	diameter_data_store_t *tree = (diameter_data_store_t *)handle;
	diameter_data_store_iterator_t *it;

	it  = nkn_malloc_type(
			sizeof(diameter_data_store_iterator_t), mod_diameter_t);

	it->index = 0;
	it->ds = tree;

	*p = (diameter_data_store_iterator_handle)it;

	return DIAMETER_CB_OK;

}

static uint32_t diameter_data_store_destroy_iterator(
				diameter_data_store_handle handle,
				diameter_data_store_iterator_handle p)
{
	(void) handle;
	diameter_data_store_iterator_t *it = (diameter_data_store_iterator_t *)p;

	free(it);

	return DIAMETER_CB_OK;
}

static uint32_t diameter_data_store_iterator_get_next(
				diameter_data_store_handle handle,
				diameter_data_store_iterator_handle iter_handle,
				void **obj)
{
	(void) handle;
	uint32_t rc;
	diameter_data_store_iterator_t *p;

	p = (diameter_data_store_iterator_t *)iter_handle;
	pthread_mutex_lock(&p->ds->cs);

	while (p->index < DIAMETER_DATA_STORE_ITEMS_MAX &&
				p->ds->array[p->index] == NULL)
		p->index++;

	if (p->index < DIAMETER_DATA_STORE_ITEMS_MAX) {
		*obj = p->ds->array[p->index++];
		rc = DIAMETER_CB_OK;
	} else {
		rc = DIAMETER_CB_ERROR;
	}
	pthread_mutex_unlock (&p->ds->cs);

	return rc;
}

static uint32_t diameter_schedule(diameter_peer_handle peer,
				diameter_peer_event_handle event)
{
	int ret = 0;
	diameter_peer_observer_t *po;

	diameter_peer_get_user_data_handle(peer, (void *)&po);
	pthread_mutex_lock(&po->mutex);
        ret = diameter_peer_event(peer, event);
	pthread_mutex_unlock(&po->mutex);

        return ret;
}

uint32_t diameter_buf_get_size(diameter_buffer_handle buf, uint32_t *size)
{
	*size = ((struct diameter_buffer_s *)buf)->size;

	return DIAMETER_CB_OK;
}

uint32_t diameter_buf_get_ptr(diameter_buffer_handle buf, uint8_t **p)
{
	*p = ((struct diameter_buffer_s *)buf)->p;

	return DIAMETER_CB_OK;
}

uint32_t diameter_buf_release(diameter_buffer_handle buf)
{
	free(((struct diameter_buffer_s *)buf)->p);
	free(buf);

	return DIAMETER_CB_OK;
}

static void diameter_system_table_init(diameter_system_cb_table *system_cb_table)
{
	/* register memory callback functions */
	system_cb_table->memory_cb_table.cb_allocate = diameter_alloc;
	system_cb_table->memory_cb_table.cb_free = diameter_free;
	system_cb_table->memory_cb_table.cb_object_allocate = diameter_object_alloc;
	system_cb_table->memory_cb_table.cb_object_free = diameter_object_free;
	system_cb_table->memory_cb_table.cb_object_addref = diameter_object_addref;
	system_cb_table->memory_cb_table.cb_object_release = diameter_object_release;

	/* register lock callaback functions */
	system_cb_table->lock_cb_table.cb_create = diameter_create_lock;
	system_cb_table->lock_cb_table.cb_destroy = diameter_destroy_lock;
	system_cb_table->lock_cb_table.cb_read_lock = diameter_read_lock;
	system_cb_table->lock_cb_table.cb_read_unlock = diameter_read_unlock;
	system_cb_table->lock_cb_table.cb_write_lock = diameter_write_lock;
	system_cb_table->lock_cb_table.cb_write_unlock = diameter_write_unlock;

	/* register iov callback functions */
	system_cb_table->memory_cb_table.cb_iov_create = diameter_create_iov;
	system_cb_table->memory_cb_table.cb_iov_release = diameter_release_iov;
	system_cb_table->memory_cb_table.cb_iov_edit = diameter_edit_iov;

	/* register buffer callabck fucntions */
	system_cb_table->buffer_cb_table.cb_get_size = diameter_buf_get_size;
	system_cb_table->buffer_cb_table.cb_get_ptr = diameter_buf_get_ptr;
	system_cb_table->buffer_cb_table.cb_release = diameter_buf_release;

	/* register timer callaback functions */
	system_cb_table->timer_cb_table.cb_create_timer = diameter_create_timer;
	system_cb_table->timer_cb_table.cb_destroy_timer = diameter_destroy_timer;
	system_cb_table->timer_cb_table.cb_start_timer = diameter_start_timer;
	system_cb_table->timer_cb_table.cb_restart_timer = diameter_restart_timer;
	system_cb_table->timer_cb_table.cb_stop_timer = diameter_stop_timer;
	system_cb_table->timer_cb_table.cb_get_remaining_time =
						diameter_get_remaining_time;

	/* register datastore callback functions */
	system_cb_table->data_store_cb_table.cb_create = diameter_data_store_create;
	system_cb_table->data_store_cb_table.cb_destroy = diameter_data_store_destroy;
	system_cb_table->data_store_cb_table.cb_insert = diameter_data_store_insert;
	system_cb_table->data_store_cb_table.cb_remove = diameter_data_store_remove;
	system_cb_table->data_store_cb_table.cb_find = diameter_data_store_find;
	system_cb_table->data_store_cb_table.cb_create_iterator =
						diameter_data_store_create_iterator;
	system_cb_table->data_store_cb_table.cb_destroy_iterator =
						diameter_data_store_destroy_iterator;
	system_cb_table->data_store_cb_table.cb_get_next =
						diameter_data_store_iterator_get_next;

	/* register scheduler callback function */
	system_cb_table->cb_schedule = diameter_schedule;

	/* register transport callback function */
	system_cb_table->transport_cb_table.cb_create_messenger =
						diameter_create_messenger;

	/* register messenger callback functions */
	system_cb_table->messenger_cb_table.cb_connect =
						diameter_messenger_connect;
	system_cb_table->messenger_cb_table.cb_send_data =
						diameter_messenger_send_data;
	system_cb_table->messenger_cb_table.cb_flow_off =
						diameter_messenger_flow_off;
	system_cb_table->messenger_cb_table.cb_flow_on =
						diameter_messenger_flow_on;
	system_cb_table->messenger_cb_table.cb_disconnect =
						diameter_messenger_disconnect;
	system_cb_table->messenger_cb_table.cb_close =
						diameter_messenger_close;

	/* register peer stats callback functions */
	system_cb_table->statistics_cb_table.cb_peer_stat_set =
						diameter_peer_stat_set;
	system_cb_table->statistics_cb_table.cb_peer_stat_increment =
						diameter_peer_stat_increment;
	system_cb_table->statistics_cb_table.cb_peer_stat_decrement =
						diameter_peer_stat_decrement;

	/* register peergroup stats callback functions */
	system_cb_table->statistics_cb_table.cb_peergroup_stat_increment =
						diameter_peergroup_stat_increment;
	system_cb_table->statistics_cb_table.cb_peergroup_stat_decrement =
						diameter_peergroup_stat_decrement;

	return;
}

static void diameter_peer_table_init(diameter_peer_cb_table *peer_cb_table)
{
	/* register peer events callback functions */
	peer_cb_table->cb_on_state_change = diameter_peer_on_state_change;
	peer_cb_table->cb_on_connect = diameter_peer_on_connect;
	peer_cb_table->cb_on_connect_failure = diameter_peer_on_connect_failure;
	peer_cb_table->cb_on_connect_timeout = diameter_peer_on_connect_timeout;
	peer_cb_table->cb_on_disconnect = diameter_peer_on_disconnect;
	peer_cb_table->cb_on_incoming_queue_limit_exceeded =
				diameter_peer_on_incoming_queue_limit_exceeded;
	peer_cb_table->cb_on_incoming_queue_limit_recovered =
				diameter_peer_on_incoming_queue_limit_recovered;
	peer_cb_table->cb_on_outgoing_queue_limit_exceeded =
				diameter_peer_on_outgoing_queue_limit_exceeded;
	peer_cb_table->cb_on_outgoing_queue_limit_recovered =
				diameter_peer_on_outgoing_queue_limit_recovered;
	peer_cb_table->cb_on_dpr = diameter_peer_on_dpr;
	peer_cb_table->cb_on_ce_failure = diameter_peer_on_ce_failure;
	peer_cb_table->cb_on_remote_error = diameter_peer_on_remote_error;
	peer_cb_table->cb_on_release = diameter_peer_on_release;

	return;
}

static void diameter_peergroup_table_init(
				diameter_peergroup_cb_table *peergroup_cb_table)
{
	/* register peergroup events callback functions */
	peergroup_cb_table->cb_on_connect =
				diameter_peergroup_on_connect_event;
	peergroup_cb_table->cb_on_disconnect =
				diameter_peergroup_on_disconnect_event;
	peergroup_cb_table->cb_on_failover =
				diameter_peergroup_on_failover_event;
	peergroup_cb_table->cb_on_release =
				diameter_peergroup_on_release_event;

	return;
}

static void diameter_request_table_init(diameter_request_cb_table *request_cb_table)
{
	/* register message events callback functions */
	request_cb_table->cb_on_response = diameter_request_on_response;
	request_cb_table->cb_on_pause = diameter_request_on_pause;
	request_cb_table->cb_on_error = diameter_request_on_error;
	request_cb_table->cb_on_timeout = diameter_request_on_timeout;
	request_cb_table->cb_on_peer_timeout = diameter_request_on_peer_timeout;
	request_cb_table->cb_on_release = diameter_request_on_release;

	return;
}

static void diameter_response_table_init(
				diameter_response_cb_table *response_cb_table)
{
	/* register message events callback functions */
	response_cb_table->cb_on_release = diameter_response_on_release;

	return;
}

void diameter_init(diameter_system_cb_table *system_cb_table,
			diameter_peer_cb_table *peer_cb_table,
			diameter_peergroup_cb_table *peergroup_cb_table,
			diameter_request_cb_table *request_cb_table,
			diameter_response_cb_table *response_cb_table)
{
	diameter_system_table_init(system_cb_table);
	diameter_peer_table_init(peer_cb_table);
	diameter_peergroup_table_init(peergroup_cb_table);
	diameter_request_table_init(request_cb_table);
	diameter_response_table_init(response_cb_table);

	return;
}

void diameter_start(diameter_system_cb_table *system_cb_table,
			diameter_peer_cb_table *peer_cb_table,
			diameter_peergroup_cb_table *peergroup_cb_table,
			diameter_request_cb_table *request_cb_table,
			diameter_response_cb_table *response_cb_table)
{
	int ret;

	ret = diameter_system_start(system_cb_table, peer_cb_table,
			peergroup_cb_table, request_cb_table, response_cb_table);
	assert(ret == DIAMETER_OK);

	diameter_peer_observer_init();

	diameter_server_init();

	return;
}

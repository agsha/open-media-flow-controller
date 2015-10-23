/*
 * @file jpsd_tdf.h
 * @brief
 * jpsd_tdf.h - declarations for tdf functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef _JPSD_TDF_H
#define _JPSD_TDF_H

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>

#include "queue.h"
#include "nkn_memalloc.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

#include "jnx/getput.h"
#include "jnx/jsf/jsf_ip.h"
#include "jnx/diameter_avp_common_defs.h"
#include "jnx/dia_avp_dict_api.h"
#include "jnx/diameter_defs.h"
#include "jnx/diameter_avp_api.h"

#include "diameter_system.h"
#include "diameter_avp.h"

#define MAX_DOMAIN		16000
#define MAX_DOMAIN_RULE		16000
#define MAX_TDF_THREADS		32
#define MAX_TDF_SESSION		16000
#define MAX_DOMAIN_HASH		100
#define MAX_SESSION_HASH	1000
#define MAX_SESSION_IP		32000
#define MAX_IP_HASH		409600

typedef struct domain_rule_s {
        char name[64];
	uint32_t len;
	int active;
        uint32_t precedence;
        int action_type;
        char uplink_vrf[128];
	int uplink_vrf_len;
        char downlink_vrf[128];
	int downlink_vrf_len;
} domain_rule_t;

typedef struct domain_s {
        char name[64];
        uint32_t len;
        int active;
	int incarn;
        domain_rule_t rule;
	pthread_rwlock_t rwlock;
        TAILQ_ENTRY(domain_s) entry;
} domain_t;

typedef struct ip_info_s {
	int first;
	int latest;
	uint32_t ip;
        TAILQ_ENTRY(ip_info_s) entry;
} ip_info_t;

enum TDF_DOMAIN_STATE {
	READY = 1,
        RAA_WAIT,
        REPLAY,
	REPLAY_RAA_WAIT,
};

typedef struct rar_ctx_s {
	char session_id[128];
	uint32_t session_len;
        uint32_t session_hash;
} rar_ctx_t;

typedef struct tdf_session_s {
        char origin_host[128];
        uint32_t origin_len;
	char origin_realm[128];
	uint32_t origin_realm_len;
        char session_id[128];
        uint32_t session_len;
        uint32_t session_hash;
        char domain_name[128];
        uint32_t domain_len;
	uint32_t auth_app_id;
        int state;
        int slot;
	int incarn;
	int thread_id;
        void *po;
	int version;
	int tot_ip;
	int tot_ref_ip;
	int tot_add_ip;
	int tot_del_ip;
	uint32_t *add_list;
	uint32_t *del_list;
        domain_t *tdf_domain;
	uint32_t ip_list[MAX_SESSION_IP];
	uint32_t ref_ip_list[MAX_SESSION_IP];
        TAILQ_ENTRY(tdf_session_s) session_entry;
	TAILQ_ENTRY(tdf_session_s) queue_entry;
} tdf_session_t;

typedef struct session_mgr_s {
        int total;
        int head;
        int tail;
        int slot[MAX_TDF_SESSION];
} session_mgr_t;

enum recv_type {
        CCR = 1,
	RAA = 2,
};

typedef struct ccr_s {
        int type;
	int thread_id;
        uint32_t session_hash;
        uint32_t application;
        uint32_t command;
        uint32_t end_to_end;
        uint32_t hop_by_hop;
	int auth_app_id;
        void *connection;
	void *origin_host;
	int origin_len;
	void *origin_realm;
	int origin_realm_len;
        diameter_peer_handle peer;
        diameter_avp_variant_t *session_avp;
        diameter_avp_payload_handle payload_in;
} ccr_t;

typedef struct raa_s {
	int thread_id;
	void *data;
	diameter_peer_handle peer;
	diameter_avp_payload_handle payload_in;
} raa_t;

typedef struct recv_ctx_s {
        int type;
        int thread_id;
        void *data;
        void *po;
        TAILQ_ENTRY(recv_ctx_s) entry;
} recv_ctx_t;

typedef struct tdf_threads {
        pthread_t pid;
        pthread_mutex_t mutex;
        int num;
	int ready;
	int version;
	int tot_ip;
	uint32_t ip_list[MAX_SESSION_IP];
        TAILQ_HEAD(recv_queue, recv_ctx_s) rq;
	TAILQ_HEAD(session_queue, tdf_session_s) sq;
} tdf_recv_thread_t;

domain_t *g_domain;
domain_rule_t *g_domain_rule;

pthread_mutex_t domain_mutex[MAX_DOMAIN_HASH];
TAILQ_HEAD(tdf_domain_head_s, domain_s) g_tdf_domain[MAX_DOMAIN_HASH];

pthread_rwlock_t ip_info_rwlock[MAX_IP_HASH];
TAILQ_HEAD(ip_info_head_s, ip_info_s) g_ip_info_head[MAX_IP_HASH];

static inline uint32_t HASH(const char *name, unsigned int len, int max_hash)
{
        unsigned int hash = 0;
        unsigned int i = 0;

        while( i < len) {
                hash += name[i] | 0x20 ;
                hash += (hash << 10);
                hash ^= (hash >> 6);
                i++ ;
        }

        hash += (hash << 3);
        hash ^= (hash >> 11);
        hash += (hash << 15);

        return hash % max_hash;
}

static int domain_lookup(const char *name, uint32_t len, domain_t **hndl)
{
        struct tdf_domain_head_s *tdf_domain_head;
        struct domain_s *domain, *tmp_domain;
        int found = 0;
        int hash;

        hash = HASH(name, len, MAX_DOMAIN_HASH);
        tdf_domain_head = &g_tdf_domain[hash];
        pthread_mutex_lock(&domain_mutex[hash]);
        TAILQ_FOREACH_SAFE(domain, tdf_domain_head, entry, tmp_domain) {
                pthread_mutex_unlock(&domain_mutex[hash]);
                pthread_rwlock_rdlock(&domain->rwlock);
                if (domain->len == len){
                        if (memcmp(domain->name, name, len) == 0) {
                                found = 1;
                                if (hndl) *hndl = domain;
                                pthread_rwlock_unlock(&domain->rwlock);
                                break;
                        }
                }
                pthread_rwlock_unlock(&domain->rwlock);
                pthread_mutex_lock(&domain_mutex[hash]);
        }
        pthread_mutex_unlock(&domain_mutex[hash]);

        return found;
}

static int insert_domain(domain_t *domain)
{
	int hash;
        struct tdf_domain_head_s *tdf_domain_head;

	hash = HASH(domain->name, domain->len, MAX_DOMAIN_HASH);
	tdf_domain_head = &g_tdf_domain[hash];
	pthread_mutex_lock(&domain_mutex[hash]);
	TAILQ_INSERT_TAIL(tdf_domain_head, domain, entry);
	pthread_mutex_unlock(&domain_mutex[hash]);

	return 0;
}

static int delete_domain(domain_t *domain)
{
	int hash;
        struct tdf_domain_head_s *tdf_domain_head;

	hash = HASH(domain->name, domain->len, MAX_DOMAIN_HASH);
	tdf_domain_head = &g_tdf_domain[hash];
	pthread_mutex_lock(&domain_mutex[hash]);
	TAILQ_REMOVE(tdf_domain_head, domain, entry);
	pthread_mutex_unlock(&domain_mutex[hash]);

	return 0;
}

#endif // _JPSD_TDF_H


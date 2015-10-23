/*
 * @file jpsd_tdf.c
 * @brief
 * jpsd_tdf.c - definations for jpsd-tdf functions.
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
#include <sys/uio.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "jpsd_defs.h"
#include "jpsd_network.h"
#include "jpsd_tdf.h"

char local_host[] = "jpsd";
char local_realm[] = "juniper.net";
char local_session_id[128];
uint32_t local_session_len;

int diameter_version = 1;
int tdf_recv_threads = 4;

int tdf_session_init_done;

AO_t endtoend;

tdf_recv_thread_t g_tdf_thrs[MAX_TDF_THREADS];

pthread_mutex_t session_mutex[MAX_SESSION_HASH];
TAILQ_HEAD(tdf_session_head_s, tdf_session_s) g_tdf_session[MAX_SESSION_HASH];

tdf_session_t *gtdf_session;
session_mgr_t session_mgr;
pthread_mutex_t session_mgr_mutex;

diameter_avp_dict_handle dictionary;

diameter_system_cb_table system_cb_table;
diameter_peer_cb_table peer_cb_table;
diameter_peergroup_cb_table peergroup_cb_table;
diameter_request_cb_table request_cb_table;
diameter_response_cb_table response_cb_table;
diameter_request_handler_cb_table request_handler_cb_table;

NKNCNT_DEF(dia_req_msg_parse_err, uint64_t, "", "Request Message Parse Error")
NKNCNT_DEF(dia_req_invalid_payload_handle, uint64_t, "", "Request Invalid Payload Handle")
NKNCNT_DEF(dia_req_session_id_err1, uint64_t, "", "Request Session Id Err")
NKNCNT_DEF(dia_req_session_id_err2, uint64_t, "", "Request Session Id Err")
NKNCNT_DEF(dia_req_origin_host_err, uint64_t, "", "Request Origin Host Error")
NKNCNT_DEF(dia_req_origin_realm_err, uint64_t, "", "Request Origin Host Realm Error")
NKNCNT_DEF(dia_req_auth_app_id_err, uint64_t, "", "Request Origin Host Error")
NKNCNT_DEF(dia_req_origin_state_id_err, uint64_t, "", "Request Origin State Id Error")
NKNCNT_DEF(dia_req_dest_realm_err, uint64_t, "", "Request Origin Dest Realm Error")

NKNCNT_DEF(dia_res_msg_parse_err, uint64_t, "", "Response Message Parse Error")
NKNCNT_DEF(dia_res_invalid_payload_handle, uint64_t, "", "Response Invalid Payload Handle")
NKNCNT_DEF(dia_res_session_id_err1, uint64_t, "", "Response Session Id Err")
NKNCNT_DEF(dia_res_session_id_err2, uint64_t, "", "Response Session Id Err")
NKNCNT_DEF(dia_res_session_id_err3, uint64_t, "", "Response Session Id Err")
NKNCNT_DEF(dia_res_origin_host_err, uint64_t, "", "Response Origin Host Error")
NKNCNT_DEF(dia_res_origin_realm_err, uint64_t, "", "Response Origin Host Realm Error")

NKNCNT_DEF(dia_req_ccr_err1, uint64_t, "", "Request CCR Err")
NKNCNT_DEF(dia_req_ccr_err2, uint64_t, "", "Request CCR Err")
NKNCNT_DEF(dia_req_ccr_err3, uint64_t, "", "Request CCR Err")
NKNCNT_DEF(dia_req_ccr_err4, uint64_t, "", "Request CCR Err")

NKNCNT_DEF(dia_req_rar_err1, uint64_t, "", "Request RAR Err")
NKNCNT_DEF(dia_req_rar_err2, uint64_t, "", "Request RAR Err")

NKNCNT_DEF(dia_res_raa_err1, uint64_t, "", "Response RAA Err")
NKNCNT_DEF(dia_res_raa_err2, uint64_t, "", "Response RAA Err")
NKNCNT_DEF(dia_res_raa_result_code_err, uint64_t, "", "Response RAA Result Code Err")
NKNCNT_DEF(dia_res_raa_fail, uint64_t, "", "Response RAA Failure")

NKNCNT_DEF(dia_req_invalid_po_err, uint64_t, "", "Request Invalid PO Error")
NKNCNT_DEF(dia_res_invalid_po_err, uint64_t, "", "Response Invalid PO Error")

NKNCNT_DEF(dia_req_session_err, uint64_t, "", "Request Session Error")
NKNCNT_DEF(dia_req_domain_err1, uint64_t, "", "Request Domain Error")
NKNCNT_DEF(dia_req_domain_err2, uint64_t, "", "Request Domain Error")
NKNCNT_DEF(dia_req_domain_err3, uint64_t, "", "Request Domain Error")
NKNCNT_DEF(dia_req_domain_err4, uint64_t, "", "Request Domain Error")

NKNCNT_DEF(dia_domain_incarn_err, uint64_t, "", "Request Domain Removed")

NKNCNT_DEF(dia_ip_mem_err, uint64_t, "", "Mem failure for adding IP")

static void *tdf_get_session(void)
{
	tdf_session_t *tdf_session;
	int slot;

	pthread_mutex_lock(&session_mgr_mutex);
	if (session_mgr.total == 0) {
		tdf_session = NULL;
	} else {
		slot = session_mgr.slot[session_mgr.head];
		tdf_session = &gtdf_session[slot];
		session_mgr.total--;
		session_mgr.head++;
		if (session_mgr.head == MAX_TDF_SESSION) {
			session_mgr.head = 0;
		}
	}
	pthread_mutex_unlock(&session_mgr_mutex);

	return tdf_session;
}

static void tdf_put_session(tdf_session_t *tdf_session)
{
	int slot;

	if (tdf_session->del_list)
		free(tdf_session->del_list);
	if (tdf_session->add_list)
		free(tdf_session->add_list);
	slot = tdf_session->slot;
	pthread_mutex_lock(&session_mgr_mutex);
	session_mgr.slot[session_mgr.tail] = slot;
	session_mgr.total++;
	session_mgr.tail++;
	if (session_mgr.tail == MAX_TDF_SESSION) {
		session_mgr.tail = 0;
	}
	pthread_mutex_unlock(&session_mgr_mutex);

	return;
}

static void tdf_session_init(void)
{
	int i;

	gtdf_session = nkn_calloc_type(1,
			sizeof(tdf_session_t) * MAX_TDF_SESSION,
			mod_diameter_t);
	if (gtdf_session == NULL)
		assert(0);

	for (i = 0; i < MAX_TDF_SESSION; i++) {
		gtdf_session[i].slot = i;
		tdf_put_session(&gtdf_session[i]);
	}

	for (i = 0; i < MAX_SESSION_HASH; i++) {
		TAILQ_INIT(&g_tdf_session[i]);
	}

	for (i = 0; i < MAX_IP_HASH; i++) {
		TAILQ_INIT(&g_ip_info_head[i]);
		pthread_rwlock_init(&ip_info_rwlock[i], NULL);
	}

	tdf_session_init_done = 1;

	return;
}

static int tdf_session_lookup(char *session_id, uint32_t session_len,
			int session_hash, tdf_session_t **tdf_session_hndl)
{
	struct tdf_session_head_s *tdf_session_head;
	struct tdf_session_s *tdf_session, *tmp_tdf_session;
	int found = 0;
	uint32_t hash = session_hash;

	if (hash == 0)
		hash = HASH(session_id, session_len, MAX_SESSION_HASH);

	tdf_session_head = &g_tdf_session[hash];
	pthread_mutex_lock(&session_mutex[hash]);
	TAILQ_FOREACH_SAFE(tdf_session, tdf_session_head, session_entry, tmp_tdf_session) {
		pthread_mutex_unlock(&session_mutex[hash]);
		if (tdf_session->session_len == session_len) {
			if (memcmp(tdf_session->session_id,
					session_id, session_len) == 0) {
				found = 1;
				if (tdf_session_hndl)
					*tdf_session_hndl = tdf_session;
				break;
			}
		}
		pthread_mutex_lock(&session_mutex[hash]);
	}
	pthread_mutex_unlock(&session_mutex[hash]);

	return found;
}

static void tdf_session_remove_entry(tdf_session_t *tdf_session, uint32_t hash)
{
	struct tdf_session_head_s *tdf_session_head;

	if (hash == 0)
		hash =  HASH(tdf_session->session_id,
					tdf_session->session_len, MAX_SESSION_HASH);

	tdf_session_head = &g_tdf_session[hash];
	pthread_mutex_lock(&session_mutex[hash]);
	TAILQ_REMOVE(tdf_session_head, tdf_session, session_entry);
	pthread_mutex_unlock(&session_mutex[hash]);

	return;
}

static void tdf_session_add_entry(tdf_session_t *tdf_session, uint32_t hash)
{
	struct tdf_session_head_s *tdf_session_head;

	if (hash == 0)
		hash =  HASH(tdf_session->session_id,
					tdf_session->session_len, MAX_SESSION_HASH);

	tdf_session_head = &g_tdf_session[hash];
	pthread_mutex_lock(&session_mutex[hash]);
	TAILQ_INSERT_TAIL(tdf_session_head, tdf_session, session_entry);
	pthread_mutex_unlock(&session_mutex[hash]);

	return;
}

static int diameter_process_ccr_initial(diameter_peer_observer_t *po, ccr_t *ccr)
{
        int ret;
	int id;
	int auth_app_id;
	int result_code;
        uint16_t count;
        uint8_t *domain_name;
        uint32_t domain_size;
	uint8_t *session_id;
        uint32_t session_id_size;
        uint64_t messenger_handle;
        struct iovec *iov;

	int type = ccr->type;
	uint32_t session_hash = ccr->session_hash;
	uint32_t application = ccr->application;
	uint32_t command = ccr->command;
	uint32_t end_to_end = ccr->end_to_end;
	uint32_t hop_by_hop = ccr->hop_by_hop;
	void *connection = ccr->connection;
	diameter_peer_handle peer = ccr->peer;
	diameter_avp_variant_t *session_avp = ccr->session_avp;
	diameter_avp_payload_handle payload_in = ccr->payload_in;

	diameter_base_avp_t avp[2];
        diameter_message_info_t info_out;
        diameter_avp_message_handle message_out;
        diameter_avp_payload_handle grp_payload;
        diameter_avp_payload_handle payload_out;

	domain_t *domain;
	tdf_session_t *tdf_session;

	id = ccr->thread_id;
	session_id = session_avp->data.a_i8.p;
	session_id_size = session_avp->avp_payload_len;

        ret = diameter_get_group(payload_in,
			AAA_DIAM_AVP_SUBSCRIPTION_ID, 0, NULL, &grp_payload);
        if (ret != DIAMETER_AVP_SUCCESS) {
                glob_dia_req_domain_err1++;
		result_code = DIAMETER_RC_MISSING_AVP;
		goto err;
        }
        ret = diameter_avp_payload_get_number_of_avps_by_ordinal(
			grp_payload, AAA_DIAM_AVP_SUBSCRIPTION_ID_DATA, &count);
        if (ret != DIAMETER_AVP_SUCCESS) {
                glob_dia_req_domain_err2++;
		result_code = DIAMETER_RC_MISSING_AVP;
		goto err;
        }

        ret = diameter_get_diam_identity(grp_payload,
			AAA_DIAM_AVP_SUBSCRIPTION_ID_DATA, 0,
			NULL, &domain_name, &domain_size);
        if (ret != DIAMETER_AVP_SUCCESS) {
                glob_dia_req_domain_err3++;
		result_code = DIAMETER_RC_MISSING_AVP;
		goto err;
        }

	if (domain_lookup((char *)domain_name, domain_size, &domain) == 0) {
		glob_dia_req_domain_err4++;
		result_code = DIAMETER_RC_MISSING_AVP;
		goto err;
	}

	pthread_rwlock_rdlock(&domain->rwlock);
	if (domain->active == 0) {
		glob_dia_req_domain_err4++;
		result_code = DIAMETER_RC_MISSING_AVP;
		pthread_rwlock_unlock(&domain->rwlock);
		goto err;
	}

	iov = diameter_create_iov(sizeof(*iov));
	if (iov == NULL) {
                result_code = DIAMETER_RC_OUT_OF_SPACE;
		pthread_rwlock_unlock(&domain->rwlock);
		goto err;
	}
	iov->iov_len = 0;
        iov->iov_base = diameter_create_iov(1024);
        if (iov->iov_base == NULL) {
                result_code = DIAMETER_RC_OUT_OF_SPACE;
		pthread_rwlock_unlock(&domain->rwlock);
		goto err;
        }

	tdf_session = tdf_get_session();
	if (tdf_session == NULL) {
		glob_dia_req_session_err++;
		result_code = DIAMETER_RC_OUT_OF_SPACE;
		pthread_rwlock_unlock(&domain->rwlock);
		goto err;
	}

	tdf_session->add_list = nkn_calloc_type(1, sizeof(uint32_t), mod_diameter_t);
	tdf_session->del_list = nkn_calloc_type(1, sizeof(uint32_t), mod_diameter_t);
	if (tdf_session->add_list == NULL || tdf_session->del_list == NULL) {
		glob_dia_req_session_err++;
		result_code = DIAMETER_RC_OUT_OF_SPACE;
		pthread_rwlock_unlock(&domain->rwlock);
		goto err;
	}

	memcpy(tdf_session->session_id, session_id, session_id_size);
	tdf_session->session_len = session_id_size;

        info_out.command_flags = 0;
        info_out.command_code = command;
        info_out.application_id = application;
        info_out.end_to_end = end_to_end;

        info_out.message_length = 0;
        info_out.message_version  = diameter_version;
        info_out.hop_by_hop = hop_by_hop;

        ret = diameter_avp_create_message(&info_out,
			&message_out, dictionary, iov);
        ret = diameter_avp_message_get_payload(message_out, &payload_out);

        ret = diameter_add_octet_string(AAA_DIAM_AVP_SESSION_ID,
			payload_out, (uint8_t *)tdf_session->session_id,
			tdf_session->session_len, NULL);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_ORIGIN_HOST, payload_out,
                        (uint8_t *)local_host, sizeof(local_host), NULL);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_ORIGIN_REALM, payload_out,
                        (uint8_t *)local_realm, sizeof(local_realm), NULL);
        ret = diameter_add_uint32(AAA_DIAM_AVP_AUTH_APP_ID,
			payload_out, ccr->auth_app_id, NULL);
        ret = diameter_add_uint32(AAA_DIAM_AVP_CC_REQ_TYPE,
			payload_out, INITIAL_REQUEST, NULL);

        ret = diameter_add_uint32(AAA_DIAM_AVP_RESULT_CODE,
			payload_out, DIAMETER_RC_SUCCESS, NULL);
        ret = diameter_avp_finalize_message(message_out);

        ret = diameter_send_response(connection, 1, iov, NULL);

        if (ret != DIAMETER_OK) {
		if (iov) {
			diameter_release_iov(iov->iov_base);
			diameter_release_iov(iov);
		}
		pthread_rwlock_unlock(&domain->rwlock);
		return ret;
        }

	memcpy(tdf_session->origin_host, ccr->origin_host, ccr->origin_len);
	tdf_session->origin_len = ccr->origin_len;
	memcpy(tdf_session->origin_realm, ccr->origin_realm, ccr->origin_realm_len);
	tdf_session->origin_realm_len = ccr->origin_realm_len;
        memcpy(tdf_session->session_id,
			session_avp->data.a_i8.p, session_avp->avp_payload_len);
        tdf_session->session_len = session_avp->avp_payload_len;
	tdf_session->session_hash = session_hash;
	tdf_session->auth_app_id = ccr->auth_app_id;
	tdf_session->incarn = domain->incarn;
	tdf_session->po = po;
	tdf_session->thread_id = id;
	tdf_session->tdf_domain = domain;
	tdf_session->version = 0;
	tdf_session->state = READY;
	tdf_session->tot_ip = 0;
	tdf_session->tot_ref_ip = 0;
	tdf_session->tot_add_ip = 0;
	tdf_session->tot_del_ip = 0;
	pthread_rwlock_unlock(&domain->rwlock);

	tdf_session_add_entry(tdf_session, tdf_session->session_hash);
	TAILQ_INSERT_TAIL(&g_tdf_thrs[id].sq, tdf_session, queue_entry);

	diameter_release_iov(iov);

	return ret;

err:
	if (iov) {
		if (iov->iov_base)
			diameter_release_iov(iov->iov_base);
		diameter_release_iov(iov);
	}

	memset(avp, 0, sizeof(avp));
	avp[0].avp_code = DIAMETER_CODE_SESSION_ID;
	avp[0].avp_len = session_id_size;
	avp[0].avp_u.p = session_id;
	avp[1].avp_code = DIAMETER_CODE_AUTH_APPLICATION_ID;
	avp[1].avp_len = sizeof(uint32_t);
	avp[1].avp_u.p = &ccr->auth_app_id;

        ret = diameter_incoming_request_error_response(peer, connection,
					application, command, end_to_end,
					hop_by_hop, result_code, 2, avp, NULL);

	return DIAMETER_CB_ERROR;
}

static int diameter_process_ccr_update(diameter_peer_observer_t *po, ccr_t *ccr)
{
        int ret;
	int id;
	int result_code;
        uint16_t count;
	uint8_t *session_id;
        uint32_t session_id_size;
        struct iovec *iov;

	int type = ccr->type;
	uint32_t session_hash = ccr->session_hash;
	uint32_t application = ccr->application;
	uint32_t command = ccr->command;
	uint32_t end_to_end = ccr->end_to_end;
	uint32_t hop_by_hop = ccr->hop_by_hop;
	void *connection = ccr->connection;
	diameter_peer_handle peer = ccr->peer;
	diameter_avp_variant_t *session_avp = ccr->session_avp;
	diameter_avp_payload_handle payload_in = ccr->payload_in;

	diameter_base_avp_t avp[2];
	diameter_avp_variant_t var;
        diameter_message_info_t info_out;
        diameter_avp_message_handle message_out;
        diameter_avp_payload_handle grp_payload;
        diameter_avp_payload_handle payload_out;

	tdf_session_t *tdf_session;

	session_id = session_avp->data.a_i8.p;
	session_id_size = session_avp->avp_payload_len;

	ret = tdf_session_lookup((char *)session_id,
		session_id_size, session_hash, &tdf_session);
	if (ret == 0) {
		DBG_LOG(MSG, MOD_JPSD,"Request CCR-U session id "
				"- %*s - No Match", session_id_size, session_id);
		result_code = DIAMETER_RC_UNKNOWN_SESSION_ID;
		goto err;
	}

	if (tdf_session->incarn != tdf_session->tdf_domain->incarn) {
		result_code = DIAMETER_RC_UNKNOWN_SESSION_ID;
		goto err;
	}

	iov = diameter_create_iov(sizeof(*iov));
	if (iov == NULL) {
                result_code = DIAMETER_RC_OUT_OF_SPACE;
		goto err;
	}
	iov->iov_len = 0;
        iov->iov_base = diameter_create_iov(1024);
        if (iov->iov_base == NULL) {
                result_code = DIAMETER_RC_OUT_OF_SPACE;
		goto err;
	}

        info_out.command_flags = 0;
        info_out.command_code = command;
        info_out.application_id = application;
        info_out.end_to_end = end_to_end;

        info_out.message_length = 0;
        info_out.message_version  = diameter_version;
        info_out.hop_by_hop = hop_by_hop;

        ret = diameter_avp_create_message(&info_out,
			&message_out, dictionary, iov);
        ret = diameter_avp_message_get_payload(message_out, &payload_out);

        ret = diameter_add_octet_string(AAA_DIAM_AVP_SESSION_ID,
			payload_out, (uint8_t *)tdf_session->session_id,
			tdf_session->session_len, NULL);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_ORIGIN_HOST, payload_out,
                        (uint8_t *)local_host, sizeof(local_host), NULL);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_ORIGIN_REALM, payload_out,
                        (uint8_t *)local_realm, sizeof(local_realm), NULL);
        ret = diameter_add_uint32(AAA_DIAM_AVP_AUTH_APP_ID,
			payload_out, ccr->auth_app_id, NULL);
        ret = diameter_add_uint32(AAA_DIAM_AVP_CC_REQ_TYPE,
			payload_out, UPDATE_REQUEST, NULL);

        ret = diameter_add_uint32(AAA_DIAM_AVP_RESULT_CODE,
			payload_out, DIAMETER_RC_SUCCESS, NULL);
        ret = diameter_avp_finalize_message(message_out);

        ret = diameter_send_response(connection, 1, iov, NULL);

        if (ret != DIAMETER_OK) {
		diameter_release_iov(iov->iov_base);
        }
	diameter_release_iov(iov);

	return ret;

err:
	if (iov) {
		if (iov->iov_base)
			diameter_release_iov(iov->iov_base);
		diameter_release_iov(iov);
	}

	memset(avp, 0, sizeof(avp));
	avp[0].avp_code = DIAMETER_CODE_SESSION_ID;
	avp[0].avp_len = session_id_size;
	avp[0].avp_u.p = session_id;
	avp[1].avp_code = DIAMETER_CODE_AUTH_APPLICATION_ID;
	avp[1].avp_len = sizeof(uint32_t);
	avp[1].avp_u.p = &ccr->auth_app_id;

        ret = diameter_incoming_request_error_response(peer, connection,
					application, command, end_to_end,
					hop_by_hop, result_code, 2, avp, NULL);

	return DIAMETER_CB_ERROR;
}

static int diameter_process_ccr_termination(
			diameter_peer_observer_t *po, ccr_t *ccr)
{
        int ret;
	int id;
	int result_code;
        uint16_t count;
	uint8_t *session_id;
        uint32_t session_id_size;
        struct iovec *iov;

	int type = ccr->type;
	uint32_t session_hash = ccr->session_hash;
	uint32_t application = ccr->application;
	uint32_t command = ccr->command;
	uint32_t end_to_end = ccr->end_to_end;
	uint32_t hop_by_hop = ccr->hop_by_hop;
	void *connection = ccr->connection;
	diameter_peer_handle peer = ccr->peer;
	diameter_avp_variant_t *session_avp = ccr->session_avp;
	diameter_avp_payload_handle payload_in = ccr->payload_in;

	diameter_base_avp_t avp[2];
        diameter_message_info_t info_out;
        diameter_avp_message_handle message_out;
        diameter_avp_payload_handle grp_payload;
        diameter_avp_payload_handle payload_out;

	tdf_session_t *tdf_session;

	session_id = session_avp->data.a_i8.p;
	session_id_size = session_avp->avp_payload_len;

	ret = tdf_session_lookup((char *)session_id,
		session_id_size, session_hash, &tdf_session);
	if (ret == 0) {
		result_code = DIAMETER_RC_UNKNOWN_SESSION_ID;
		goto err;
	}

        iov = diameter_create_iov(sizeof(*iov));
        if (iov == NULL) {
                result_code = DIAMETER_RC_OUT_OF_SPACE;
		goto err;
	}
	iov->iov_len = 0;
        iov->iov_base = diameter_create_iov(1024);
        if (iov->iov_base == NULL) {
                result_code = DIAMETER_RC_OUT_OF_SPACE;
		goto err;
	}

        info_out.command_flags = 0;

        info_out.command_flags = 0;
        info_out.command_code = command;
        info_out.application_id = application;
        info_out.end_to_end = end_to_end;

        info_out.message_length = 0;
        info_out.message_version  = diameter_version;
        info_out.hop_by_hop = hop_by_hop;

        ret = diameter_avp_create_message(&info_out,
			&message_out, dictionary, iov);
        ret = diameter_avp_message_get_payload(message_out, &payload_out);

        ret = diameter_add_octet_string(AAA_DIAM_AVP_SESSION_ID,
			payload_out, (uint8_t *)tdf_session->session_id,
			tdf_session->session_len, NULL);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_ORIGIN_HOST, payload_out,
                        (uint8_t *)local_host, sizeof(local_host), NULL);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_ORIGIN_REALM, payload_out,
                        (uint8_t *)local_realm, sizeof(local_realm), NULL);
        ret = diameter_add_uint32(AAA_DIAM_AVP_AUTH_APP_ID,
			payload_out, ccr->auth_app_id, NULL);
        ret = diameter_add_uint32(AAA_DIAM_AVP_CC_REQ_TYPE,
			payload_out, TERMINATION_REQUEST, NULL);

        ret = diameter_add_uint32(AAA_DIAM_AVP_RESULT_CODE,
			payload_out, DIAMETER_RC_SUCCESS, NULL);
        ret = diameter_avp_finalize_message(message_out);

        ret = diameter_send_response(connection, 1, iov, NULL);

        if (ret != DIAMETER_OK) {
		diameter_release_iov(iov->iov_base);
        }
	diameter_release_iov(iov);

	tdf_session_remove_entry(tdf_session, tdf_session->session_hash);
	TAILQ_REMOVE(&g_tdf_thrs[tdf_session->thread_id].sq, tdf_session, queue_entry);
	tdf_put_session(tdf_session);


	return ret;

err:
	if (iov) {
		if (iov->iov_base)
			diameter_release_iov(iov->iov_base);
		diameter_release_iov(iov);
	}

	memset(avp, 0, sizeof(avp));
	avp[0].avp_code = DIAMETER_CODE_SESSION_ID;
	avp[0].avp_len = session_id_size;
	avp[0].avp_u.p = session_id;
	avp[1].avp_code = DIAMETER_CODE_AUTH_APPLICATION_ID;
	avp[1].avp_len = sizeof(uint32_t);
	avp[1].avp_u.p = &ccr->auth_app_id;

        ret = diameter_incoming_request_error_response(peer, connection,
					application, command, end_to_end,
					hop_by_hop, result_code, 2, avp, NULL);

	return DIAMETER_CB_ERROR;
}

static void diameter_response_handler(
			diameter_request_observer_handle handle,
			uint16_t n, struct iovec *request,
			diameter_buffer_handle response_message,
			diameter_peer_handle peer,
			diameter_peergroup_handle peergroup,
			diameter_messenger_data_handle messenger_data)
{
	uint8_t *data;
	uint32_t size;
	uint16_t count;
	uint8_t *session_id;
        uint32_t session_id_size;
	uint8_t *origin;
        uint32_t origin_len;
	uint8_t *origin_realm;
        uint32_t origin_realm_len;
	int ret;
	int id;
	uint32_t hash;

	raa_t *raa = NULL;
	recv_ctx_t *recv_ctx = NULL;

	diameter_avp_variant_t *session_avp = NULL;
	diameter_avp_message_handle message_in;
	diameter_avp_payload_handle payload_in;
	diameter_avp_payload_handle error_handle;

        diameter_peer_observer_t *po;

	rar_ctx_t *rar_ctx = (rar_ctx_t *)handle;

	diameter_buf_get_ptr(response_message, &data);
	diameter_buf_get_size(response_message, &size);

        po = NULL;
        diameter_peer_get_user_data_handle(peer, (void *)&po);
        if (po == NULL) {
		glob_dia_res_invalid_po_err;
		return;
        }

	ret = diameter_avp_parse_message(dictionary, data,
				size, 0, &message_in, &error_handle);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_res_msg_parse_err++;
		return;
	}

	ret = diameter_avp_message_get_payload(message_in, &payload_in);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_res_invalid_payload_handle++;
		return;
	}

	ret = diameter_avp_payload_get_number_of_avps_by_ordinal(
				payload_in, AAA_DIAM_AVP_SESSION_ID, &count);
	if (ret != DIAMETER_AVP_SUCCESS || count > 1) {
		glob_dia_res_session_id_err1++;
		return;
	}

	ret = diameter_get_diam_identity(payload_in, AAA_DIAM_AVP_SESSION_ID,
				0, &session_avp, &session_id, &session_id_size);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_res_session_id_err2++;
		return;
	}

	if (session_id_size != local_session_len ||
		strncmp((char *)session_id, local_session_id, session_id_size)) {
		glob_dia_res_session_id_err3++;
		return;
	}

	ret = diameter_get_diam_identity(payload_in, AAA_DIAM_AVP_ORIGIN_HOST,
				0, NULL, &origin, &origin_len);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_res_origin_host_err++;
		return;
	}

	ret = diameter_get_diam_identity(payload_in, AAA_DIAM_AVP_ORIGIN_REALM,
				0, NULL, &origin_realm, &origin_realm_len);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_res_origin_realm_err++;
		return;
	}

	recv_ctx = nkn_calloc_type(1, sizeof(struct recv_ctx_s), mod_diameter_t);
	if (recv_ctx == NULL) {
		glob_dia_res_raa_err1++;
		goto err;
	}

	raa = nkn_calloc_type(1, sizeof(struct raa_s), mod_diameter_t);
        if (raa == NULL){
                glob_dia_res_raa_err2++;
                goto err;
        }


	hash = rar_ctx->session_hash;
	id = hash % tdf_recv_threads;

	raa->thread_id = id;
	raa->data = rar_ctx;
	raa->peer = peer;
	raa->payload_in = payload_in;

	recv_ctx->type = RAA;
	recv_ctx->data = raa;
	recv_ctx->po = po;
	recv_ctx->thread_id = id;

	pthread_mutex_lock(&g_tdf_thrs[id].mutex);
	TAILQ_INSERT_TAIL(&g_tdf_thrs[id].rq, recv_ctx, entry);
	pthread_mutex_unlock(&g_tdf_thrs[id].mutex);

	return;

err:
	if (raa)
		free(raa);

	if (recv_ctx)
		free(recv_ctx);

	return;
}


static int diameter_request_handler_receive_request(
			diameter_request_handler_handle handle,
			uint32_t application, uint32_t command,
			uint32_t end_to_end, uint32_t hop_by_hop,
			diameter_buffer_handle packet,
			diameter_incoming_request_context connection,
			diameter_peer_handle peer)
{
	uint8_t *data;
	uint32_t size;
	uint16_t count;
	int value;
	int auth_app_id;
	uint8_t *session_id;
        uint32_t session_id_size;
	uint8_t *origin;
        uint32_t origin_len;
	uint8_t *origin_realm;
        uint32_t origin_realm_len;
	uint8_t *name;
	uint32_t name_len;
	int ret;
	int total_avps;
	int result_code;
	int id;
	uint32_t hash;

	ccr_t *ccr = NULL;
	recv_ctx_t *recv_ctx = NULL;
	auth_app_id = 0;
	total_avps = 1;

	diameter_avp_variant_t *session_avp = NULL;
	diameter_base_avp_t avp[2];
	diameter_avp_message_handle message_in;
	diameter_avp_payload_handle payload_in;
	diameter_avp_payload_handle error_handle;

        diameter_peer_observer_t *po;

	diameter_buf_get_ptr(packet, &data);
	diameter_buf_get_size(packet, &size);

        po = NULL;
        diameter_peer_get_user_data_handle(peer, (void *)&po);
        if (po == NULL) {
		glob_dia_req_invalid_po_err;
		return DIAMETER_CB_ERROR;
        }

	ret = diameter_avp_parse_message(dictionary, data,
				size, 0, &message_in, &error_handle);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_req_msg_parse_err++;
		return DIAMETER_CB_ERROR;
	}

	ret = diameter_avp_message_get_payload(message_in, &payload_in);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_req_invalid_payload_handle++;
		return DIAMETER_CB_ERROR;
	}

	ret = diameter_avp_payload_get_number_of_avps_by_ordinal(
				payload_in, AAA_DIAM_AVP_SESSION_ID, &count);
	if (ret != DIAMETER_AVP_SUCCESS || count > 1) {
		glob_dia_req_session_id_err1++;
		return DIAMETER_CB_ERROR;
	}

	ret = diameter_get_diam_identity(payload_in, AAA_DIAM_AVP_SESSION_ID,
				0, &session_avp, &session_id, &session_id_size);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_req_session_id_err2++;
		return DIAMETER_CB_ERROR;
	}

	ret = diameter_get_enum(payload_in, AAA_DIAM_AVP_AUTH_APP_ID,
				NULL, 0, &auth_app_id);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_req_auth_app_id_err++;
		result_code = DIAMETER_RC_MISSING_AVP;
		goto err;
	}

	ret = diameter_get_diam_identity(payload_in, AAA_DIAM_AVP_ORIGIN_HOST,
				0, NULL, &origin, &origin_len);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_req_origin_host_err++;
		result_code = DIAMETER_RC_MISSING_AVP;
		goto err;
	}

	ret = diameter_get_diam_identity(payload_in, AAA_DIAM_AVP_ORIGIN_REALM,
				0, NULL, &origin_realm, &origin_realm_len);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_req_origin_realm_err++;
		result_code = DIAMETER_RC_MISSING_AVP;
		goto err;
	}

	ret = diameter_get_enum(payload_in, AAA_DIAM_AVP_ORIGIN_STATE_ID,
				NULL, 0, &value);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_req_origin_state_id_err++;
		result_code = DIAMETER_RC_MISSING_AVP;
		goto err;
	}

	ret = diameter_get_diam_identity(payload_in, AAA_DIAM_AVP_DEST_REALM,
				0, NULL, &name, &name_len);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_req_dest_realm_err++;
		result_code = DIAMETER_RC_MISSING_AVP;
		goto err;
	}

	ret = diameter_avp_payload_get_number_of_avps_by_ordinal(
				payload_in, AAA_DIAM_AVP_CC_REQ_TYPE, &count);
	if (ret != DIAMETER_AVP_SUCCESS || count > 1) {
		glob_dia_req_ccr_err1++;
		result_code = DIAMETER_RC_MISSING_AVP;
		goto err;
	}

	ret = diameter_get_enum(payload_in, AAA_DIAM_AVP_CC_REQ_TYPE, NULL, 0, &value);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_req_ccr_err2++;
		result_code = DIAMETER_RC_MISSING_AVP;
		goto err;
	}

	switch(value) {
	case INITIAL_REQUEST:
	case UPDATE_REQUEST:
	case TERMINATION_REQUEST:
		break;
	default:
                result_code = DIAMETER_RC_INVALID_AVP_VALUE;
                goto err;
	}

	recv_ctx = nkn_calloc_type(1, sizeof(struct recv_ctx_s), mod_diameter_t);
	if (recv_ctx == NULL) {
		glob_dia_req_ccr_err3++;
		result_code = DIAMETER_RC_OUT_OF_SPACE;
		goto err;
	}

	ccr = nkn_calloc_type(1, sizeof(struct ccr_s), mod_diameter_t);
	if (ccr == NULL){
		glob_dia_req_ccr_err4++;
		result_code = DIAMETER_RC_OUT_OF_SPACE;
		goto err;
	}

	hash = HASH((char *)session_id, session_id_size, MAX_SESSION_HASH);
	id = hash % tdf_recv_threads;

	ccr->type = value;
	ccr->application = application;
	ccr->command = command;
	ccr->end_to_end = end_to_end;
	ccr->hop_by_hop = hop_by_hop;
	ccr->auth_app_id = auth_app_id;
	ccr->origin_host = origin;
	ccr->origin_len = origin_len;
	ccr->origin_realm = origin_realm;
	ccr->origin_realm_len = origin_realm_len;
	ccr->session_avp = session_avp;
	ccr->peer = peer;
	ccr->payload_in = payload_in;
	ccr->session_hash = hash;
	ccr->connection = connection;
	ccr->thread_id = id;

	recv_ctx->type = CCR;
	recv_ctx->data = ccr;
	recv_ctx->po = po;
	recv_ctx->thread_id = id;

	pthread_mutex_lock(&g_tdf_thrs[id].mutex);
	TAILQ_INSERT_TAIL(&g_tdf_thrs[id].rq, recv_ctx, entry);
	pthread_mutex_unlock(&g_tdf_thrs[id].mutex);

	return DIAMETER_CB_OK;

err:
	if (ccr)
		free(ccr);

	if (recv_ctx)
		free(recv_ctx);

	memset(avp, 0, sizeof(avp));
	avp[0].avp_code = DIAMETER_CODE_SESSION_ID;
	avp[0].avp_len = session_id_size;
	avp[0].avp_u.p = session_id;
	if (auth_app_id) {
		avp[1].avp_code = DIAMETER_CODE_AUTH_APPLICATION_ID;
		avp[1].avp_len = sizeof(uint32_t);
		avp[1].avp_u.p = &auth_app_id;
		total_avps = 2;
	}

        ret = diameter_incoming_request_error_response(peer, connection,
					application, command, end_to_end,
					hop_by_hop, result_code, total_avps, avp, NULL);

	return DIAMETER_CB_ERROR;
}

static int process_ccr_request(void *po, struct ccr_s *ccr)
{
	int ret;
	int result_code;
	uint8_t *session_id;
        uint32_t session_id_size;
	uint32_t session_hash;
	diameter_base_avp_t avp;

	session_id = ccr->session_avp->data.a_i8.p;
	session_id_size = ccr->session_avp->avp_payload_len;
	session_hash = ccr->session_hash;

        switch(ccr->type) {
        case INITIAL_REQUEST:
		DBG_LOG(MSG, MOD_JPSD,"Request CCR-I session id - %*s session hash "
				"- %u", session_id_size, session_id, session_hash);
                ret = diameter_process_ccr_initial(po, ccr);
                break;
        case UPDATE_REQUEST:
		DBG_LOG(MSG, MOD_JPSD,"Request CCR-U session id - %*s session_hash "
				"- %u", session_id_size, session_id, session_hash);
                ret = diameter_process_ccr_update(po, ccr);
                break;
        case TERMINATION_REQUEST:
		DBG_LOG(MSG, MOD_JPSD,"Request CCR-T session id - %*s session_hash "
				"- %u", session_id_size, session_id, session_hash);
                ret = diameter_process_ccr_termination(po, ccr);
                break;
	default:
		return 0;
        }

	return ret;
}

static int process_raa_request(void *po, struct raa_s *raa)
{
	int ret;
	int result_code;
	uint16_t count;
        uint8_t *session_id;
        uint32_t session_id_size;
        uint32_t session_hash;

	rar_ctx_t *rar_ctx;
	tdf_session_t *tdf_session;

	rar_ctx = (rar_ctx_t *)raa->data;
	session_id = (uint8_t *)rar_ctx->session_id;
	session_id_size = rar_ctx->session_len;
	session_hash = rar_ctx->session_hash;

	ret = tdf_session_lookup((char *)session_id,
		session_id_size, session_hash, &tdf_session);
	if (ret == 0) {
		DBG_LOG(MSG, MOD_JPSD,"Response RAA session id "
				"- %*s - No Match", session_id_size, session_id);
		return DIAMETER_CB_ERROR;
	} else {
		DBG_LOG(MSG, MOD_JPSD,"Response RAA session id "
				"- %*s ", session_id_size, session_id);
	}

	ret = diameter_avp_payload_get_number_of_avps_by_ordinal(
			raa->payload_in, AAA_DIAM_AVP_RESULT_CODE, &count);
	if (ret != DIAMETER_AVP_SUCCESS || count > 1) {
		glob_dia_res_raa_result_code_err++;
		return DIAMETER_CB_ERROR;
        }

#if 0
	ret = diameter_get_enum(raa->payload_in,
			AAA_DIAM_AVP_RESULT_CODE, NULL, 0, &result_code);
	if (ret != DIAMETER_AVP_SUCCESS) {
		glob_dia_res_raa_fail++;
		return DIAMETER_CB_ERROR;
	}
#endif
	memcpy(tdf_session->ip_list, tdf_session->ref_ip_list,
			sizeof(uint32_t) * tdf_session->tot_ref_ip);
	tdf_session->tot_ip = tdf_session->tot_ref_ip;

	tdf_session->state = READY;
	free(rar_ctx);

	return DIAMETER_CB_OK;
}

static int process_recv_data(struct recv_ctx_s *recv_ctx)
{
	int ret;

	switch(recv_ctx->type) {
	case CCR:
		ret = process_ccr_request(recv_ctx->po, recv_ctx->data);
		free(recv_ctx->data);
		free(recv_ctx);
		return ret;
	case RAA:
		ret = process_raa_request(recv_ctx->po, recv_ctx->data);
		free(recv_ctx);
		return ret;
	default:
		break;
	}

	return DIAMETER_CB_OK;
}

static int ip_info_lookup(uint32_t ip, uint32_t ip_hash, ip_info_t **ip_info_hndl)
{
	struct ip_info_head_s *ip_info_head;
	struct ip_info_s *ip_info, *tmp_ip_info;
	int found = 0;
	uint32_t hash = ip_hash;

	if (hash == 0)
		hash = ip % MAX_IP_HASH;

	ip_info_head = &g_ip_info_head[hash];
	pthread_rwlock_rdlock(&ip_info_rwlock[hash]);
	TAILQ_FOREACH_SAFE(ip_info, ip_info_head, entry, tmp_ip_info) {
		if (ip_info->ip == ip) {
			found = 1;
			if (ip_info_hndl) {
				*ip_info_hndl = ip_info;
			}
			break;
		}
	}
	pthread_rwlock_unlock(&ip_info_rwlock[hash]);

	return found;
}

static void ip_info_remove_entry(ip_info_t *ip_info, uint32_t hash)
{
	struct ip_info_head_s *ip_info_head;

	if (hash == 0)
		hash =  ip_info->ip % MAX_IP_HASH;

	ip_info_head = &g_ip_info_head[hash];
	pthread_rwlock_wrlock(&ip_info_rwlock[hash]);
	TAILQ_REMOVE(ip_info_head, ip_info, entry);
	pthread_rwlock_unlock(&ip_info_rwlock[hash]);

	return;
}

static void ip_info_add_entry(ip_info_t *ip_info, uint32_t hash)
{
	struct ip_info_head_s *ip_info_head;

	if (hash == 0)
		hash =  ip_info->ip % MAX_IP_HASH;

	ip_info_head = &g_ip_info_head[hash];
	pthread_rwlock_wrlock(&ip_info_rwlock[hash]);
	TAILQ_INSERT_TAIL(ip_info_head, ip_info, entry);
	pthread_rwlock_unlock(&ip_info_rwlock[hash]);

	return;
}

static int push_ip_list(int version, uint32_t *ip, int len)
{
	int i;
	int ip_len;
	uint32_t hash;
	ip_info_t *ip_info;

	for (i = 0; i < len; i++) {
		hash = ip[i] % MAX_IP_HASH;
		if (ip_info_lookup(ip[i], hash, &ip_info)) {
			ip_info->latest = version;
		} else {
			ip_info = nkn_calloc_type(1, sizeof(ip_info_t), mod_diameter_t);
			if (ip_info == NULL) {
				glob_dia_ip_mem_err++;
				continue;
			}
			ip_info->first = version;
			ip_info->latest = version;
			ip_info->ip = ip[i];
			ip_info_add_entry(ip_info, hash);
		}
	}

	return 0;
}

static int send_update(int version, tdf_session_t *tdf_session)
{
	int i;
	int j;
	int ret = 0;
	int count = 0;
	int id = tdf_session->thread_id;
	int add_list = 0;
	int del_list = 0;
	int flow_desc_len = 0;
	char *flow_desc;
	char *ip;
	struct iovec *iov;
	struct in_addr out;

	ip_info_t *ip_info;
	rar_ctx_t *rar_ctx;

        diameter_message_info_t info_out;
        diameter_avp_message_handle message_out;
        diameter_avp_payload_handle grp_payload;
        diameter_avp_payload_handle payload_out;

	diameter_peer_observer_t *po;
	diameter_peergroup_observer_t *pgo;
	diameter_peergroup_handle pg;

	domain_rule_t *domain_rule = &(tdf_session->tdf_domain->rule);

	if (tdf_session->incarn != tdf_session->tdf_domain->incarn) {
		glob_dia_domain_incarn_err++;
		return 0;
	}

	if (tdf_session->version == g_tdf_thrs[id].version) {
		return 0;
	}

	if (tdf_session->state == RAA_WAIT ||
		tdf_session->state == REPLAY_RAA_WAIT) {
		return 0;
	}

	tdf_session->tot_ref_ip = g_tdf_thrs[id].tot_ip;

	if (g_tdf_thrs[id].tot_ip) {
		memcpy(tdf_session->ref_ip_list, g_tdf_thrs[id].ip_list,
					sizeof(uint32_t) * g_tdf_thrs[id].tot_ip);
	}

	if (tdf_session->tot_ref_ip) {
		tdf_session->add_list = nkn_realloc_type(tdf_session->add_list,
				(tdf_session->tot_ref_ip * sizeof(uint32_t)), mod_diameter_t);
	}

	if (tdf_session->tot_ip) {
		tdf_session->del_list = nkn_realloc_type(tdf_session->del_list,
				(tdf_session->tot_ip * sizeof(uint32_t)), mod_diameter_t);
	}

	if (tdf_session->tot_ip == 0) {
		memcpy(tdf_session->add_list, tdf_session->ref_ip_list,
				sizeof(uint32_t) * tdf_session->tot_ref_ip);
		tdf_session->tot_add_ip = tdf_session->tot_ref_ip;
		add_list = 1;
	}

	if (tdf_session->tot_ref_ip == 0) {
		memcpy(tdf_session->del_list, tdf_session->ip_list,
				sizeof(uint32_t) * tdf_session->tot_ip);
		tdf_session->tot_del_ip = tdf_session->tot_ip;
		del_list = 1;
	}

	if (add_list == 0) {
		j = 0;
		for (i = 0; i < tdf_session->tot_ref_ip; i++) {
			if (ip_info_lookup(tdf_session->ref_ip_list[i], 0 , &ip_info)) {
				if (ip_info->first >= tdf_session->version &&
					ip_info->latest <=  tdf_session->version) {
					continue;
				}
				tdf_session->add_list[j] = tdf_session->ref_ip_list[i];
				j++;
			} else {
				assert(0);
			}
		}
		tdf_session->tot_add_ip = j;
	}

	if (del_list == 0) {
		j = 0;
		for (i = 0; i < tdf_session->tot_ip; i++) {
			if (ip_info_lookup(tdf_session->ip_list[i], 0 , &ip_info)) {
				if (ip_info->latest <  g_tdf_thrs[id].version) {
					tdf_session->del_list[j] = tdf_session->ip_list[i];
					j++;
				}
			} else {
				tdf_session->del_list[j] = tdf_session->ip_list[i];
				j++;
			}
		}
		tdf_session->tot_del_ip = j;
	}

	tdf_session->version = g_tdf_thrs[id].version;

	DBG_LOG(MSG, MOD_JPSD, "add ip list - ");
	for (i = 0; i < tdf_session->tot_add_ip; i++) {
		DBG_LOG(MSG, MOD_JPSD, "%x", tdf_session->add_list[i]);
	}

	DBG_LOG(MSG, MOD_JPSD, "del ip list - ");
	for (i = 0; i < tdf_session->tot_del_ip; i++) {
		DBG_LOG(MSG, MOD_JPSD, "%x", tdf_session->del_list[i]);
	}

	iov = diameter_create_iov(sizeof(*iov));
#if 0
        if (iov == NULL) {
		glob_dia_req_rar_err1++;
                goto err;
        }
#endif
        iov->iov_len = 0;
        iov->iov_base = diameter_create_iov(64*1024);
#if 0
        if (iov->iov_base == NULL) {
		glob_dia_req_rar_err2++;
                goto err;
        }
#endif

        info_out.command_flags = DIAMETER_COMMAND_FLAG_R;
        info_out.command_code = DIAMETER_COMMAND_RE_AUTH ;
        info_out.application_id = FICTITIOUS_AUTH_APP;
        info_out.end_to_end = AO_fetch_and_add1(&endtoend);

        info_out.message_length = 0;
        info_out.message_version  = diameter_version;
        info_out.hop_by_hop = 0;

	ret = diameter_avp_create_message(&info_out,
			&message_out, dictionary, iov);
	ret = diameter_avp_message_get_payload(message_out, &payload_out);

#if 0
	ret = diameter_add_octet_string(AAA_DIAM_AVP_SESSION_ID,
                        payload_out, (uint8_t *)tdf_session->session_id,
                        tdf_session->session_len, NULL);
#endif
	ret = diameter_add_octet_string(AAA_DIAM_AVP_SESSION_ID,
                        payload_out, (uint8_t *)local_session_id,
                        local_session_len, NULL);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_ORIGIN_HOST, payload_out,
                        (uint8_t *)local_host, sizeof(local_host), NULL);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_ORIGIN_REALM, payload_out,
                        (uint8_t *)local_realm, sizeof(local_realm), NULL);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_DEST_HOST,
			payload_out, (uint8_t *)tdf_session->origin_host,
			tdf_session->origin_len, NULL);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_DEST_REALM,
			payload_out, (uint8_t *)tdf_session->origin_realm,
			tdf_session->origin_realm_len, NULL);
        ret = diameter_add_uint32(AAA_DIAM_AVP_AUTH_APP_ID,
                        payload_out, FICTITIOUS_AUTH_APP, NULL);
        ret = diameter_add_uint32(AAA_DIAM_AVP_REAUTH_REQ_TYPE,
                        payload_out, 0, NULL);

	/* create the add-list */
	/* start the charging rule install grouped avp */
	ret = diameter_avp_payload_start_grouped_avp(payload_out,
			AAA_DIAM_AVP_CHARGING_RULE_INSTALL);
	/* start the charging rule defination grouped avp */
	ret = diameter_avp_payload_start_grouped_avp(payload_out,
			AAA_DIAM_AVP_CHARGING_RULE_DEFINITION);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_CHARGING_RULE_NAME,
			payload_out, (uint8_t *)domain_rule->name,
			domain_rule->len, NULL);
	/* start the flow information grouped avp */
	ret = diameter_avp_payload_start_grouped_avp(payload_out,
			AAA_DIAM_AVP_FLOW_INFORMATION);
	flow_desc = nkn_calloc_type(1, 1024, mod_diameter_t);
	flow_desc_len = 0;
	for (i = 0; i < tdf_session->tot_add_ip; i++) {
		strcat(flow_desc, "permit ");
		flow_desc_len += 7;
		strcat(flow_desc, "out ");
		flow_desc_len += 4;
		strcat(flow_desc, "ip ");
		flow_desc_len += 3;
		strcat(flow_desc, "from ");
		flow_desc_len += 5;
		strcat(flow_desc, "any ");
		flow_desc_len += 4;
		strcat(flow_desc, "to ");
		flow_desc_len += 3;
		out.s_addr = tdf_session->add_list[i];
		ip = inet_ntoa(out);
		strcat(flow_desc, ip);
		flow_desc_len += strlen(ip);
		ret = diameter_add_octet_string(
				AAA_DIAM_AVP_FLOW_DESCRIPTION,
				payload_out, (uint8_t *)flow_desc,
				flow_desc_len, NULL);
		flow_desc[0] = '\0';
		flow_desc_len = 0;
	}
        ret = diameter_add_octet_string(
			AAA_DIAM_AVP_PACKET_FILTER_IDENTIFIER,
			payload_out, (uint8_t *)"jpsd_packet_filter",
			18, NULL);
        ret = diameter_add_octet_string(
			AAA_DIAM_AVP_FLOW_LABEL,
			payload_out, (uint8_t *)"jpsd_flow_label",
			15, NULL);
        ret = diameter_add_uint32(AAA_DIAM_AVP_FLOW_DIRECTION,
                        payload_out, 3, NULL);
	ret = diameter_avp_payload_end_grouped_avp (payload_out, 0);
	/* end of flow information grouped avp */
        ret = diameter_add_uint32(AAA_DIAM_AVP_PRECEDENCE,
                        payload_out, domain_rule->precedence, NULL);
	ret = diameter_avp_payload_end_grouped_avp (payload_out, 0);
	/* end of charging rule defination grouped avp */
	if (domain_rule->uplink_vrf_len) {
		ret = diameter_add_octet_string(
				AAA_DIAM_AVP_STEERING_VRF_UL,
				payload_out, (uint8_t *)domain_rule->uplink_vrf,
				domain_rule->uplink_vrf_len, NULL);
	}
	if (domain_rule->downlink_vrf_len) {
		ret = diameter_add_octet_string(
				AAA_DIAM_AVP_STEERING_VRF_DL,
				payload_out, (uint8_t *)domain_rule->downlink_vrf,
				domain_rule->downlink_vrf_len, NULL);
	}
	ret = diameter_avp_payload_end_grouped_avp (payload_out, 0);
	/* end of charging rule install grouped avp */

	if (tdf_session->tot_del_ip == 0) {
		free(flow_desc);
		goto data_end;
	}

	/* create the del-list */
	/* start the charging rule remove grouped avp */
	ret = diameter_avp_payload_start_grouped_avp(payload_out,
			AAA_DIAM_AVP_CHARGING_RULE_REMOVE);
	/* start the charging rule defination grouped avp */
	ret = diameter_avp_payload_start_grouped_avp(payload_out,
			AAA_DIAM_AVP_CHARGING_RULE_DEFINITION);
        ret = diameter_add_octet_string(AAA_DIAM_AVP_CHARGING_RULE_NAME,
			payload_out, (uint8_t *)domain_rule->name,
			domain_rule->len, NULL);
	/* start the flow information grouped avp */
	ret = diameter_avp_payload_start_grouped_avp(payload_out,
			AAA_DIAM_AVP_FLOW_INFORMATION);
	flow_desc_len = 0;
	for (i = 0; i < tdf_session->tot_del_ip; i++) {
		strcat(flow_desc, "permit ");
		flow_desc_len += 7;
		strcat(flow_desc, "out ");
		flow_desc_len += 4;
		strcat(flow_desc, "ip ");
		flow_desc_len += 3;
		strcat(flow_desc, "from ");
		flow_desc_len += 5;
		strcat(flow_desc, "any ");
		flow_desc_len += 4;
		strcat(flow_desc, "to ");
		flow_desc_len += 3;
		out.s_addr = tdf_session->del_list[i];
		ip = inet_ntoa(out);
		strcat(flow_desc, ip);
		flow_desc_len += strlen(ip);
		ret = diameter_add_octet_string(
				AAA_DIAM_AVP_FLOW_DESCRIPTION,
				payload_out, (uint8_t *)flow_desc,
				flow_desc_len, NULL);
		flow_desc[0] = '\0';
		flow_desc_len = 0;
	}
	free(flow_desc);
        ret = diameter_add_octet_string(
			AAA_DIAM_AVP_PACKET_FILTER_IDENTIFIER,
			payload_out, (uint8_t *)"jpsd_packet_filter",
			18, NULL);
        ret = diameter_add_octet_string(
			AAA_DIAM_AVP_FLOW_LABEL,
			payload_out, (uint8_t *)"jpsd_flow_label",
			15, NULL);
        ret = diameter_add_uint32(AAA_DIAM_AVP_FLOW_DIRECTION,
                        payload_out, 3, NULL);
	ret = diameter_avp_payload_end_grouped_avp (payload_out, 0);
	/* end of flow information grouped avp */
        ret = diameter_add_uint32(AAA_DIAM_AVP_PRECEDENCE,
                        payload_out, domain_rule->precedence, NULL);
	ret = diameter_avp_payload_end_grouped_avp (payload_out, 0);
	/* end of charging rule defination grouped avp */
	if (domain_rule->uplink_vrf_len) {
		ret = diameter_add_octet_string(
				AAA_DIAM_AVP_STEERING_VRF_UL, payload_out,
				(uint8_t *)domain_rule->uplink_vrf,
				domain_rule->uplink_vrf_len, NULL);
	}
	if (domain_rule->downlink_vrf_len) {
		ret = diameter_add_octet_string(
				AAA_DIAM_AVP_STEERING_VRF_DL, payload_out,
				(uint8_t *)domain_rule->downlink_vrf,
				domain_rule->downlink_vrf_len, NULL);
	}
	ret = diameter_avp_payload_end_grouped_avp (payload_out, 0);
	/* end of charging rule remove grouped avp */

data_end:
        ret = diameter_avp_finalize_message(message_out);

	rar_ctx = nkn_calloc_type(1, sizeof(rar_ctx_t), mod_diameter_t);
	memcpy(rar_ctx->session_id,
			tdf_session->session_id, tdf_session->session_len);
	rar_ctx->session_len = tdf_session->session_len;
	rar_ctx->session_hash = tdf_session->session_hash;

	po = tdf_session->po;
	pgo = po->pgo;
	pg = pgo->pg;

	ret = diameter_peergroup_send_request(pg, 1, iov,
			60000, 0, 0, rar_ctx, po->peer);

        if (ret != DIAMETER_OK) {
		if (iov) {
			free(rar_ctx);
			diameter_release_iov(iov->iov_base);
			diameter_release_iov(iov);
			return 0;
		}
	}
	tdf_session->state = RAA_WAIT;


err:
	return 0;
}


void get_cacheable_ip_list(void)
{
	uint32_t ip0[] = {0x10000010, 0x10000011, 0x10000012};
	uint32_t ip1[] = {0x10101010, 0x10101011, 0x10101012};
	uint32_t ip2[] = {0x20202020, 0x20202021, 0x20202022};
	uint32_t ip3[] = {0x30303030, 0x30303031, 0x30303032};
	uint32_t ip4[] = {0x40404040, 0x40404041, 0x40404042};
	uint32_t ip5[] = {0x50505050, 0x50505051, 0x50505052};
	uint32_t ip6[] = {0x60606060, 0x60606061, 0x60606062};
	uint32_t ip7[] = {0x70707070, 0x70707071, 0x70707072};
	uint32_t ip8[] = {0x80808080, 0x80808081, 0x80808082};
	uint32_t ip9[] = {0x90909090, 0x90909091, 0x90909092};
	uint32_t *ip;
	int len = 3;
	int update = 1;
	int i;
	int ret;
	static int version = 0;
	int value;

	if (update) {
		version++;
		value = version % 10;
		switch (value) {
		case 0:
			ip = ip0;
			break;
		case 1:
			ip = ip1;
			break;
		case 2:
			ip = ip2;
			break;
		case 3:
			ip = ip3;
			break;
		case 4:
			ip = ip4;
			break;
		case 5:
			ip = ip5;
			break;
		case 6:
			ip = ip6;
			break;
		case 7:
			ip = ip7;
			break;
		case 8:
			ip = ip8;
			break;
		case 9:
			ip = ip9;
			break;
		default:
			version--;
			return;
		}
		push_ip_list(version, ip, len);
	}

	for (i = 0; i < tdf_recv_threads; i++) {
		if (g_tdf_thrs[i].ready) {
			memcpy(g_tdf_thrs[i].ip_list, ip,
					sizeof(uint32_t) * len);
			g_tdf_thrs[i].tot_ip = len;
			g_tdf_thrs[i].ready = 0;
			g_tdf_thrs[i].version = version;
		}
	}

	return;
}

static void *tdf_recv(void *arg)
{
	char name[64];
	int ret;
	struct tdf_threads *ptdf_thr = (struct tdf_threads *)arg;
	struct recv_ctx_s *recv_ctx, *tmp_recv_ctx;
	struct tdf_session_s *tdf_session, *tmp_tdf_session;

	snprintf(name, 64, "tdf-recv-%lu", (unsigned long)ptdf_thr->num);
	prctl(PR_SET_NAME, name, 0, 0, 0);

	while (1) {
		pthread_mutex_lock(&ptdf_thr->mutex);
		TAILQ_FOREACH_SAFE(recv_ctx, &ptdf_thr->rq, entry, tmp_recv_ctx) {
			TAILQ_REMOVE(&ptdf_thr->rq, recv_ctx, entry);
			pthread_mutex_unlock(&ptdf_thr->mutex);
			ret = process_recv_data(recv_ctx);
			pthread_mutex_lock(&ptdf_thr->mutex);
		}
		pthread_mutex_unlock(&ptdf_thr->mutex);
		if (ptdf_thr->ready == 1) {
			sleep(2);
		} else {
			TAILQ_FOREACH_SAFE(tdf_session, &ptdf_thr->sq,
						queue_entry, tmp_tdf_session) {
				ret = send_update(ptdf_thr->version, tdf_session);
			}
			ptdf_thr->ready = 1;
		}
	}

	return NULL;
}

static void tdf_thread_start(void)
{
	int i;
	int j;

	for (i = 0; i < tdf_recv_threads; i++) {
		pthread_mutex_init(&g_tdf_thrs[i].mutex, NULL);
		g_tdf_thrs[i].num = i;
		g_tdf_thrs[i].ready = 1;
		TAILQ_INIT(&g_tdf_thrs[i].rq);
		TAILQ_INIT(&g_tdf_thrs[i].sq);
		pthread_create(&g_tdf_thrs[i].pid, NULL, tdf_recv, &g_tdf_thrs[i]);
	}

	return;
}

void jpsd_tdf_start(void)
{
	int ret;

	snprintf(local_session_id, 128, "%s-%lu", local_host, random());
	local_session_len = strlen(local_session_id);

	tdf_session_init();

	tdf_thread_start();

	diameter_dict_init(AAA_DIAM_AVP_MAX);

	diameter_dict_get_handle((char *)"jpsd", &dictionary);

        diameter_init(&system_cb_table, &peer_cb_table,
			&peergroup_cb_table, &request_cb_table, &response_cb_table);

	request_cb_table.cb_on_response = diameter_response_handler;

        request_handler_cb_table.cb_receive_request =
			diameter_request_handler_receive_request;

        ret = diameter_system_set_default_request_handler(
			&request_handler_cb_table, NULL);
        assert(ret == DIAMETER_OK);

        ret = diameter_system_set_request_cache_size(MAX_TDF_SESSION);
        assert(ret == DIAMETER_OK);

        diameter_start(&system_cb_table, &peer_cb_table,
		&peergroup_cb_table, &request_cb_table, &response_cb_table);

        ret = diameter_system_set_local_origin(local_host, local_realm);
        assert(ret == DIAMETER_OK);


	return;
}

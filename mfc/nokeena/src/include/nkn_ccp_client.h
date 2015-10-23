/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains CCP client related defines.
 *
 * Author: Srujan
 *
 */
#ifndef _NKN_CCP_CLIENT_H
#define _NKN_CCP_CLIENT_H

#include <sys/types.h>
#include <inttypes.h>
#include <sys/queue.h>
#include "nkn_ccp.h"
#include <atomic_ops.h>

/*
 * This is the max. time the client should wait before querying the
 * status of the previously posted task when the server sends
 * INPROGRESS/ NOT_STARTED status for the task.
 */
#define DEFAULT_WAITING_TIME 15

typedef struct urlnode {
    char *url;
    struct urlnode *next;
    int status;
    int errcode;
    ccp_action_type_t action_type;
} url_t;

typedef struct ccp_client_session_ctxt {
    void *ptr;
    ccp_xml_t xmlnode;
    uint32_t src_post_id;
    uint32_t dest_task_id;
    int32_t sock;
    char *recv_buf;
    int32_t offset;
    int32_t status;
    int32_t wait_time;
    int8_t retries;
    int in_waitq;
    query_status_t query_status;
    url_t *urlnode;
    char *crawl_name;
    TAILQ_ENTRY (ccp_client_session_ctxt) waitq_entries;
    int32_t (*status_callback_func) (void *);
    void (*cleanup_callback_func) (void *);
} ccp_client_session_ctxt_t;

int64_t g_src_post_id;

typedef enum {
    ADD = 1,
    DELETE,
    ADD_ASX,
    DELETE_ASX,
    NONE
}change_list_type_t;


typedef enum {
    SESSION_INITIATED = 1,
    POST_TASK,
    RECV_POST_ACK,
    SEND_QUERY,
    RECV_QUERY_RESPONSE,
    ABORT_TASK,
    RECV_ABRT_ACK,
    TASK_COMPLETED,
    ERROR_CCP,
    ERROR_CCP_POST,
} ccp_client_states_t;

extern struct ccp_epoll_struct *g_ccp_epoll_node;

void nkn_ccp_client_process_epoll_events (void *data);
int32_t nkn_ccp_client_add_epoll_event(int event_flags,
					ccp_client_session_ctxt_t *obj);
int32_t nkn_ccp_client_change_epoll_event (int event_flags,
					ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_del_epoll_event (ccp_client_session_ctxt_t *conn_obj);
void nkn_ccp_client_wait_routine (void);
int32_t nkn_ccp_open_client_conn (int *sock);
int32_t nkn_ccp_client_sm (ccp_client_session_ctxt_t *conn_obj);
void	nkn_ccp_cleanup_session (ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_add_to_waitq (ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_form_xml_message (ccp_client_session_ctxt_t *conn_obj,
					ccp_command_types_t command);
int32_t nkn_ccp_client_insert_url_entry (ccp_xml_t *xmlnode, char *url,
					int64_t expiry, int8_t cachepin,
					int generate_asx_flag, char *etag,
					int64_t objsize,
					char *domain);
void nkn_ccp_client_cleanup_send_buffers (ccp_xml_t *xmlnode);
int32_t nkn_ccp_client_set_cachepin (ccp_xml_t *xmlnode);
int32_t nkn_ccp_client_insert_expiry (ccp_xml_t *xmlnode, int64_t expiry);
int32_t nkn_ccp_client_insert_uri_count (ccp_xml_t *xmlnode, int32_t num_urls);

extern pthread_mutex_t g_ctxt_mutex;

ccp_client_session_ctxt_t * nkn_ccp_client_start_new_session (
					int32_t (*status_func)(void *ptr),
					void (*cleanup_func) (void *ptr),
					void *ptr, char *crawl_name);
int32_t nkn_ccp_client_start_new_payload (ccp_client_session_ctxt_t *conn_obj,
					change_list_type_t payload_file_type);
int32_t nkn_ccp_client_parse_message (ccp_client_session_ctxt_t *conn_obj,
					char *search_cmd);
int32_t nkn_ccp_client_open_conn (int *sock);

/*
 * state machine functions
 */

int32_t nkn_ccp_client_post_task (ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_task_completed (ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_recv_abrt_ack (ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_abort_task (ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_recv_query_resp (ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_send_query (ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_recv_post_ack (ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_end_payload(ccp_client_session_ctxt_t *conn_obj);
extern char *ccp_source_name;
extern char *ccp_dest_name;
void nkn_ccp_client_init(char *source_name, char *dest_name, int num_crawlers);
void nkn_ccp_client_remove_from_waitq(ccp_client_session_ctxt_t *conn_obj);
void nkn_ccp_client_set_status_abort(ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_check_if_task_completed(
					ccp_client_session_ctxt_t *conn_obj);
int32_t nkn_ccp_client_delete_epoll_event(int32_t sock);
int32_t nkn_ccp_client_set_genasx_flag (ccp_xml_t *xmlnode);
int32_t nkn_ccp_client_add_client_domain(ccp_xml_t *xmlnode,
					char *client_domain);

uint64_t glob_ccp_client_socket_creation_fails;
uint64_t glob_ccp_client_invalid_tid_cnt;
uint64_t glob_ccp_client_invalid_searchcmd_cnt;
uint64_t glob_ccp_client_start_payload_err_cnt;
uint64_t glob_ccp_client_end_payload_err_cnt;
uint64_t glob_ccp_client_add_epoll_event_err;
uint64_t glob_ccp_client_change_epoll_event_err;
uint64_t glob_ccp_client_conn_reset_cnt;
uint64_t glob_ccp_client_num_postack_fail_cnt;
uint64_t glob_ccp_client_query_ack_fail_cnt;
uint64_t glob_ccp_client_abrt_ack_fail_cnt;
#endif /* _NKN_CCP_CLIENT_H */

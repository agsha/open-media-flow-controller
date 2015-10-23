/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains CCP server related defines.
 *
 * Author: Srujan
 *
 */
#ifndef _NKN_CCP_SERVER_H
#define _NKN_CCP_SERVER_H

#include <pthread.h>
#include <atomic_ops.h>
#include <sys/queue.h>
#include "nkn_cod.h"
#include "nkn_ccp.h"

#define MAX_STATUS_LEN 32

typedef enum {
    SRV_FREE = 1,
    SRV_NOT_FREE,
} ccp_server_session_state_t;

typedef struct ccp_urlnode {
    nkn_cod_t cod;
    char *url;
    char *domain;
    char *etag;
    int64_t size;
    time_t expiry;
    int cache_pin;
    int flag;
    ccp_action_type_t action;
#define CCP_ENTRY_IN_USE 1
    int errcode;
    query_status_t status;
    //to store asx data
    char *data;
    int64_t data_len;
    int asx_gen_flag;
    void **buffer_ptr;
    int buffer_count;
    void *stat_ptr;
    struct ccp_urlnode *next;
    pthread_mutex_t entry_mutex;
} ccp_urlnode_t;


typedef struct ccp_client_data {
    ccp_command_types_t command;
    ccp_action_type_t action;
    query_status_t query_status;
    time_t expiry;
    int cache_pin;
    int8_t command_ack;
    int32_t wait_time;
    int32_t count;
    char *client_domain;
    char *crawl_name;
    void *stat_ptr;
    ccp_urlnode_t *url_entry;
    TAILQ_ENTRY (ccp_client_data) entries;
} ccp_client_data_t;


typedef struct ccp_inout_buf {
    char *recv_buf;
    int32_t offset;
    int32_t sock;
    ccp_xml_t *server_response;
    void *ptr;
    int conn_close;
} ccp_inout_buf_t;

typedef struct ccp_server_session_ctxt
{
    ccp_client_data_t client_ctxt;
    ccp_server_session_state_t state;
    ccp_inout_buf_t *inout_data;
    void (*dest_callback_func)(ccp_client_data_t *);
    int tid;
    int pid;
    int end_session;
    TAILQ_ENTRY (ccp_server_session_ctxt) entries;
} ccp_server_session_ctxt_t;

extern AO_t g_ccp_tid;

extern struct ccp_epoll_struct *ccp_server_epoll_node;
extern int32_t ccp_server_sock;
extern ccp_server_session_ctxt_t *ccp_server_ctxt_head;

int32_t nkn_ccp_server_open_tcp_conn (int *sock);
ccp_server_session_ctxt_t *get_new_server_session_ctxt (void);
ccp_server_session_ctxt_t *find_server_ctxt(int32_t tid);
void nkn_ccp_server_read_data(struct ccp_inout_buf *inout_data);
int32_t nkn_ccp_server_send_data(ccp_inout_buf_t *inout_data);
void nkn_ccp_server_cleanup_send_buf(ccp_xml_t **server_response);
int32_t nkn_ccp_server_handle_response(ccp_client_data_t *client_ctxt);
int32_t nkn_ccp_server_form_postack_message(ccp_xml_t *server_response, ccp_server_session_ctxt_t *conn_obj);
int32_t nkn_ccp_server_insert_wait_period(ccp_xml_t *server_response, char *wait_time);
int32_t nkn_ccp_server_form_queryack_message(ccp_xml_t *server_response, ccp_server_session_ctxt_t *conn_obj);
int32_t nkn_ccp_server_form_abrtack_message(ccp_xml_t *server_response, ccp_server_session_ctxt_t *conn_obj);
void nkn_ccp_server_ctxt_cleanup(void *ptr);
void nkn_ccp_server_cleanup_client_ctxt(ccp_client_data_t *ptr);
void nkn_ccp_server_cleanup_inout_buffer(ccp_inout_buf_t *ptr);
void nkn_ccp_server_init(void(*dest_callback_func)(ccp_client_data_t *), char *source_name, char *dest_name);

uint64_t glob_ccp_server_conn_fail_cnt;
#endif /* _NKN_CCP_SERVER_H */


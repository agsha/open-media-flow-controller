/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains CCP common defines.
 *
 * Author: Srujan
 *
 */
#ifndef _NKN_CCP_H_
#define _NKN_CCP_H_

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <pthread.h>
#include "nkn_defs.h"
#define MAX_RETRIES 5
#define EPOLL_OUT_EVENTS   (EPOLLOUT | EPOLLERR | EPOLLHUP)
#define EPOLL_IN_EVENTS   (EPOLLIN | EPOLLERR | EPOLLHUP)
#define MY_ENCODING "ISO-8859-1"
#define SOCK_PATH "/vtmp/CSE_CUE_SOCKET"

/* Note: If this enum is modified
 * corresponding change needs to 
 * be done in nkn_iccp.h
 */
typedef enum {
    CCP_ACTION_ADD = 1,
    CCP_ACTION_DELETE,
    CCP_ACTION_LIST,
    CCP_MAX_ACTION,
} ccp_action_type_t;

typedef enum {
    CCP_OK = 1,
    CCP_NOT_STARTED,
    CCP_IN_PROGRESS,
    CCP_INVALID,
    CCP_NOT_OK,
    CCP_MAX_STATUS,
}query_status_t;


typedef struct ccp_xml {
    xmlNodePtr cmdNode;
    xmlNodePtr rootNode;
    xmlDocPtr doc;
    xmlChar *xmlbuf;
} ccp_xml_t;



typedef struct ccp_epoll_struct {
    struct epoll_event *events;
    int32_t epoll_fd;
} ccp_epoll_struct_t;

typedef enum {
    CCP_POST =1,
    CCP_POSTACK,
    CCP_QUERY,
    CCP_QUERYACK,
    CCP_ABRT,
    CCP_ABRTACK,
} ccp_command_types_t;

int32_t nkn_ccp_send_message (int32_t sock, char *buf);
int32_t nkn_ccp_recv_message (int32_t sock, char *buf);
void nkn_ccp_nonblock(int sockfd);
struct ccp_epoll_struct * nkn_ccp_create (int32_t num_fds);
int32_t nkn_ccp_end_xml_payload (ccp_xml_t *xmlnode);
int32_t nkn_ccp_start_xml_payload (ccp_xml_t *xmlnode, ccp_command_types_t command, int pid, int tid);
extern const char *g_source_name, *g_dest_name;
int32_t nkn_ccp_check_if_proper_xml (char *buf, int64_t size);
int32_t nkn_ccp_get_pid_tid (xmlChar *command) ;

uint64_t glob_ccp_send_failures;
uint64_t glob_ccp_recv_failures;
uint64_t glob_ccp_start_payload_err_cnt;
uint64_t glob_ccp_pid_tid_errors;
#endif /* _NKN_CCP_H */


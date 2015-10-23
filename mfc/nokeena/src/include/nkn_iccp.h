/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains ICCP common defines
 *
 * Author: Jeya ganesh babu
 *
 */
#ifndef _NKN_ICCP_H
#define _NKN_ICCP_H

#include <sys/time.h>
#include <pthread.h>
#include "queue.h"
#include "nkn_defs.h"

typedef enum {
    ICCP_ACTION_ADD = 1,
    ICCP_ACTION_DELETE,
    ICCP_ACTION_LIST,
    ICCP_MAX_ACTION,
} iccp_action_type_t;

typedef enum {
    ICCP_OK = 1,
    ICCP_NOT_STARTED,
    ICCP_IN_PROGRESS,
    ICCP_INVALID,
    ICCP_NOT_OK,
    ICCP_CLEANUP,
    ICCP_MAX_STATUS,
}iccp_query_status_t;

/* ICCP structure used by both 
 * client and server
 */
typedef struct iccp_info_s {
    int	    command;
    char    *uri;
    int	    cache_pin;
    int	    status;
    char    *etag;
    time_t  expiry;
    int64_t content_len;
    void    *data;
    size_t  data_len;
    char    *crawl_name;
    char    *client_domain;
    int	    errcode;
    uint64_t cod;
    void    *stat_ptr;
    void    **buffer_ptr;
    int     buffer_count;
    int     retry_count;
#define HEX_ENCODED_URI 0x1
#define VERSION_EXPIRED 0x2
#define ICCP_RETRY      0x4
    int     flag;
    TAILQ_ENTRY(iccp_info_s) entries;
    pthread_mutex_t entry_mutex;
    struct iccp_info_s *next;
}iccp_info_t;

typedef struct iccp_tlv_int_t {
    int	    type;
    int	    len;
    uint64_t value;
}iccp_tlv_int_t;

typedef struct iccp_tlv_char_t {
    int	    type;
    int	    len;
    char    value[1];
}iccp_tlv_char_t;

#define ICCP_SOCK_PATH "/vtmp/CIM_SOCKET"
#define MAX_ICCP_EVENTS 11
/* Maximum parallel adds and deletes to nvsd */
#define MAX_ICCP_PARALLEL_TASKS 100
#define MAX_ICCP_RETRY 3

#define ICCP_VERSION	    1
#define ICCP_MSG_LEN(msg)   (*((uint64_t *)msg))
#define MAX_FIELDS	    10
#define TYPE_FIELD_SIZE	    (sizeof(int))
#define LENGTH_FIELD_SIZE   (sizeof(int))
#define MSG_LEN_SIZE	    (sizeof(uint64_t))
#define VERSION_SIZE	    (sizeof(uint64_t))
#define COMMAND_SIZE	    (sizeof(uint64_t))
#define EXPIRY_SIZE	    (sizeof(uint64_t))
#define PIN_SIZE	    (sizeof(uint64_t))
#define CONTENT_LEN_SIZE    (sizeof(uint64_t))

typedef enum {
    ICCP_TYPE_VERSION = 1,
    ICCP_TYPE_COMMAND,
    ICCP_TYPE_URI,
    ICCP_TYPE_EXPIRY,
    ICCP_TYPE_ETAG,
    ICCP_TYPE_PIN,
    ICCP_TYPE_CONTENT_LEN,
    ICCP_TYPE_DATA,
    ICCP_TYPE_CLIENT_DOMAIN,
    ICCP_TYPE_CRAWL_NAME,
    ICCP_TYPE_STATUS,
    ICCP_TYPE_ERRCODE,
}iccp_msg_type_t;

void iccp_init(void);
iccp_info_t *iccp_decode_frame(char *msg);
char * iccp_form_request_message(iccp_info_t *iccp_info);
char * iccp_form_response_message(iccp_info_t *iccp_info);
void free_iccp_info(iccp_info_t *iccp_info);
void nkn_iccp_nonblock(int sockfd);

#endif /* _NKN_ICCP_H */

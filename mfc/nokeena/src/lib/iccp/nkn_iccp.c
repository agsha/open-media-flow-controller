/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains implementation of ICCP common routines
 *
 * Author: Jeya ganesh babu
 *
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include "nkn_assert.h"
#include "nkn_errno.h"
#include "nkn_debug.h"
#include "nkn_iccp.h"
#include "nkn_iccp_cli.h"
#include "nkn_iccp_srvr.h"


/*
 * Common ICCP init - Currently nothing needed
 */
void
iccp_init()
{
}

void
nkn_iccp_nonblock(int sockfd)
{
    int opts;
    opts = fcntl(sockfd, F_GETFL);
    if(opts < 0) {
        DBG_LOG (ERROR, MOD_ICCP, "%s", strerror(errno));
	return;
    }
    opts = (opts | O_NONBLOCK);
    if(fcntl(sockfd, F_SETFL, opts) < 0) {
        DBG_LOG (ERROR, MOD_ICCP, "%s", strerror(errno));
    }
}

/*
 * Add an integer tlv to the message
 */
static void
iccp_add_tlv_int(char *msg, int type, uint64_t value)
{
    iccp_tlv_int_t *tlv = (iccp_tlv_int_t *)(&msg[ICCP_MSG_LEN(msg)]);
    tlv->type	= type;
    tlv->len	= sizeof(uint64_t);
    tlv->value	= value;
    ICCP_MSG_LEN(msg) += sizeof(iccp_tlv_int_t);
}

/*
 * Add an char/binary data tlv to the message
 */
static void
iccp_add_tlv_char(char *msg, int type, char *value, uint64_t data_len)
{
    iccp_tlv_char_t *tlv = (iccp_tlv_char_t *)(&msg[ICCP_MSG_LEN(msg)]);

    tlv->type	= type;
    tlv->len	= data_len;
    memcpy((char *)&tlv->value[0], value, data_len);
    ICCP_MSG_LEN(msg) += (sizeof(iccp_tlv_char_t) - sizeof(char) + data_len);
}

/*
 * Form a request message - called by CUE
 */
char *
iccp_form_request_message(iccp_info_t *iccp_info)
{
    char *msg;
    uint64_t msg_len = (MAX_FIELDS * TYPE_FIELD_SIZE) +
		    (MAX_FIELDS * LENGTH_FIELD_SIZE) +
		    VERSION_SIZE + COMMAND_SIZE +
		    EXPIRY_SIZE + PIN_SIZE +
		    CONTENT_LEN_SIZE + iccp_info->data_len +
		    MSG_LEN_SIZE;

    if(!iccp_info->uri)
	return NULL;
    else
	msg_len += strlen(iccp_info->uri);
    if(iccp_info->etag)
	msg_len += strlen(iccp_info->etag);
    if(iccp_info->crawl_name)
	msg_len += strlen(iccp_info->crawl_name);
    if(iccp_info->client_domain)
	msg_len += strlen(iccp_info->client_domain);
    msg_len = msg_len * 2;

    msg = nkn_calloc_type(1, msg_len, mod_iccp_msg);
    if(!msg) {
	return NULL;
    }

    /* ICCP_MSG_LEN is the lenth header in the message */
    ICCP_MSG_LEN(msg) = MSG_LEN_SIZE;
    iccp_add_tlv_int(msg, ICCP_TYPE_VERSION, ICCP_VERSION);
    iccp_add_tlv_int(msg, ICCP_TYPE_COMMAND, iccp_info->command);
    iccp_add_tlv_char(msg, ICCP_TYPE_URI, iccp_info->uri,
			strlen(iccp_info->uri));
    iccp_add_tlv_int(msg, ICCP_TYPE_EXPIRY, iccp_info->expiry);
    iccp_add_tlv_int(msg, ICCP_TYPE_PIN, iccp_info->cache_pin);
    if(iccp_info->etag)
	iccp_add_tlv_char(msg, ICCP_TYPE_ETAG, iccp_info->etag,
			strlen(iccp_info->etag));
    iccp_add_tlv_int(msg, ICCP_TYPE_CONTENT_LEN, iccp_info->content_len);
    if(iccp_info->data)
	iccp_add_tlv_char(msg, ICCP_TYPE_DATA, iccp_info->data,
			    iccp_info->data_len);
    if(iccp_info->client_domain)
	iccp_add_tlv_char(msg, ICCP_TYPE_CLIENT_DOMAIN, iccp_info->client_domain,
			strlen(iccp_info->client_domain));
    if(iccp_info->crawl_name)
	iccp_add_tlv_char(msg, ICCP_TYPE_CRAWL_NAME, iccp_info->crawl_name,
			strlen(iccp_info->crawl_name));
    if(ICCP_MSG_LEN(msg) > msg_len)
	assert(0);

    return msg;
}

/*
 * Form a response message - called by CIM
 */
char *
iccp_form_response_message(iccp_info_t *iccp_info)
{
    char *msg;
    uint64_t msg_len = (MAX_FIELDS * TYPE_FIELD_SIZE) +
		    (MAX_FIELDS * LENGTH_FIELD_SIZE) +
		    VERSION_SIZE + COMMAND_SIZE +
		    EXPIRY_SIZE + PIN_SIZE +
		    CONTENT_LEN_SIZE + iccp_info->data_len +
		    MSG_LEN_SIZE;

    if(!iccp_info->uri)
	return NULL;
    else
	msg_len += strlen(iccp_info->uri);
    if(iccp_info->etag)
	msg_len += strlen(iccp_info->etag);
    if(iccp_info->crawl_name)
	msg_len += strlen(iccp_info->crawl_name);
    if(iccp_info->client_domain)
	msg_len += strlen(iccp_info->client_domain);
    msg_len = msg_len * 2;

    msg = nkn_calloc_type(1, msg_len, mod_iccp_msg);
    if(!msg) {
	return NULL;
    }

    ICCP_MSG_LEN(msg) = MSG_LEN_SIZE;
    iccp_add_tlv_int(msg, ICCP_TYPE_VERSION, ICCP_VERSION);
    iccp_add_tlv_int(msg, ICCP_TYPE_COMMAND, iccp_info->command);
    iccp_add_tlv_char(msg, ICCP_TYPE_URI, iccp_info->uri,
			strlen(iccp_info->uri));
    iccp_add_tlv_int(msg, ICCP_TYPE_STATUS, iccp_info->status);
    iccp_add_tlv_int(msg, ICCP_TYPE_ERRCODE, iccp_info->errcode);
    if(iccp_info->crawl_name)
	iccp_add_tlv_char(msg, ICCP_TYPE_CRAWL_NAME, iccp_info->crawl_name,
			strlen(iccp_info->crawl_name));
    if(iccp_info->client_domain)
	iccp_add_tlv_char(msg, ICCP_TYPE_CLIENT_DOMAIN, iccp_info->client_domain,
			strlen(iccp_info->client_domain));
    if(ICCP_MSG_LEN(msg) > msg_len)
	assert(0);

    return msg;
}

/*
 * Decode the recieved frame - Called from both CUE and CIM
 * This will create the iccp_info structure. The structure should
 * be freed up by the caller after usage
 */
iccp_info_t *iccp_decode_frame(char *msg)
{

    iccp_info_t *iccp_info;
    uint64_t msg_len = ICCP_MSG_LEN(msg);
    iccp_tlv_int_t *int_tlv;
    iccp_tlv_char_t *char_tlv;
    char *ptr;
    uint64_t size;
    int ret;

    iccp_info = nkn_calloc_type(1, sizeof(iccp_info_t), mod_iccp_info);
    ret = pthread_mutex_init(&iccp_info->entry_mutex, NULL);
    if(ret < 0) {
	free_iccp_info(iccp_info);
	return NULL;
    }

    ptr = (char *)&msg[MSG_LEN_SIZE];

    /* Parse each tlv and form the iccp_info
     */
    while(ptr<(msg+msg_len)) {
	int_tlv = (iccp_tlv_int_t *)ptr;
	switch(int_tlv->type) {
	    case ICCP_TYPE_VERSION:
		if(int_tlv->value != ICCP_VERSION) {
		    DBG_LOG(SEVERE, MOD_ICCP, "Invalid ICCP version %ld",
			    int_tlv->value);
		    return NULL;
		}
		size = sizeof(iccp_tlv_int_t);
		break;
	    case ICCP_TYPE_COMMAND:
		iccp_info->command = int_tlv->value;
		size = sizeof(iccp_tlv_int_t);
		break;
	    case ICCP_TYPE_URI:
		iccp_info->uri = nkn_calloc_type(1, int_tlv->len + 1,
						    mod_iccp_tlv);
		char_tlv = (iccp_tlv_char_t *)ptr;
		memcpy(iccp_info->uri, &char_tlv->value[0],
			char_tlv->len);
		iccp_info->uri[char_tlv->len] = '\0';
		size = sizeof(iccp_tlv_char_t) - sizeof(char) + char_tlv->len;
		break;
	    case ICCP_TYPE_EXPIRY:
		iccp_info->expiry = int_tlv->value;
		size = sizeof(iccp_tlv_int_t);
		break;
	    case ICCP_TYPE_ETAG:
		iccp_info->etag = nkn_calloc_type(1, int_tlv->len + 1,
						    mod_iccp_tlv);
		char_tlv = (iccp_tlv_char_t *)ptr;
		memcpy(iccp_info->etag, &char_tlv->value[0],
			char_tlv->len);
		iccp_info->etag[char_tlv->len] = '\0';
		size = sizeof(iccp_tlv_char_t) - sizeof(char) + char_tlv->len;
		break;
	    case ICCP_TYPE_PIN:
		iccp_info->cache_pin = int_tlv->value;
		size = sizeof(iccp_tlv_int_t);
		break;
	    case ICCP_TYPE_CONTENT_LEN:
		iccp_info->content_len = int_tlv->value;
		size = sizeof(iccp_tlv_int_t);
		break;
	    case ICCP_TYPE_DATA:
		iccp_info->data = nkn_calloc_type(1, int_tlv->len,
						    mod_iccp_tlv);
		char_tlv = (iccp_tlv_char_t *)ptr;
		memcpy(iccp_info->data, &char_tlv->value[0],
			char_tlv->len);
		iccp_info->data_len = char_tlv->len;
		size = sizeof(iccp_tlv_char_t) - sizeof(char) + char_tlv->len;
		break;
	    case ICCP_TYPE_CLIENT_DOMAIN:
		iccp_info->client_domain = nkn_calloc_type(1, int_tlv->len + 1,
							    mod_iccp_tlv);
		char_tlv = (iccp_tlv_char_t *)ptr;
		memcpy(iccp_info->client_domain, &char_tlv->value[0],
			char_tlv->len);
		iccp_info->client_domain[char_tlv->len] = '\0';
		size = sizeof(iccp_tlv_char_t) - sizeof(char) + char_tlv->len;
		break;
	    case ICCP_TYPE_CRAWL_NAME:
		iccp_info->crawl_name = nkn_calloc_type(1, int_tlv->len + 1,
							mod_iccp_tlv);
		char_tlv = (iccp_tlv_char_t *)ptr;
		memcpy(iccp_info->crawl_name, &char_tlv->value[0],
			char_tlv->len);
		iccp_info->crawl_name[char_tlv->len] = '\0';
		size = sizeof(iccp_tlv_char_t) - sizeof(char) + char_tlv->len;
		break;
	    case ICCP_TYPE_STATUS:
		iccp_info->status = int_tlv->value;
		size = sizeof(iccp_tlv_int_t);
		break;
	    case ICCP_TYPE_ERRCODE:
		iccp_info->errcode = int_tlv->value;
		size = sizeof(iccp_tlv_int_t);
		break;
	    default:
		assert(0);
	}
	ptr += size;
    }

    return iccp_info;
}

/* 
 * Free the iccp_info structure
 */
void free_iccp_info(iccp_info_t *iccp_info)
{
    if(!iccp_info)
	return;

    if(iccp_info->uri)
	free(iccp_info->uri);
    if(iccp_info->etag)
	free(iccp_info->etag);
    if(iccp_info->data)
	free(iccp_info->data);
    if(iccp_info->crawl_name)
	free(iccp_info->crawl_name);
    if(iccp_info->client_domain)
	free(iccp_info->client_domain);

    free(iccp_info);
}

#ifdef ICCP_DEBUG
int main(void)
{
    iccp_srvr_init();
    while(1) {
	sleep(1);
    }
    return 0;
}
#endif

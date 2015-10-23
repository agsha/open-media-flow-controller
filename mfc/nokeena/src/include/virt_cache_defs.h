//=============================================================================
//
//
// Purpose: This file defines the various data structures for the use between
//          the VirtCache Client and Server on Control Message Pipe.
//
// Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
//
//
//=============================================================================

#ifndef __virt_cache_defs_h__
#define __virt_cache_defs_h__

#pragma once

//-----------------------------------------------------------------------------
// Forward Declarations & Include Files
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <string.h>

//#define TEST_WITH_LOCAL_DISK

//*****************************************************************************
// NB: This value needs to change if any message structure size or 
//     layout changes!
#define VCCMP_MAJOR_VER                 1
// NB: This value should change for any other changes to the header file.
#define VCCMP_MINOR_VER                 1
//*****************************************************************************


#define VCCMP_SERVER_ADDR               "10.84.77.1"
#define VCCMP_SERVER_PORT               11266
#define VCCMP_INVALID_MSG_ID            0
#define VCCMP_INVALID_CLIENT_ID         0
#define VCCMP_CONNECTION_MSG_ID         1
#define VCCMP_EVENT_MSG_ID              2
#define VCCMP_MAX_CONTENT_TYPE_STR      32
#define VCCMP_MAX_ETAG_STR              128
#define VCCMP_MAX_VM_NAME_STR           64
#define VCCMP_MAX_MFC_VER_STR           64

#define VCCMP_INVALID_CHANNEL_ID        0xFF
#define VCCMP_CHANNEL_SIZE              0x4000000000000000ull
#define VCCMP_SLOTS_PER_CHANNEL         (256 * 1024)

#define VCVD_SECTOR_SIZE                512
#define VCVD_SIGNATURE                  "VIRT$$$$"
#define VCVD_SIGNATURE_LEN              8


//-----------------------------------------------------------------------------
// vccmp_msg_type definition
//-----------------------------------------------------------------------------
typedef enum vccmp_msg_type
{
    VCCMPMT_UNKNOWN = 0,
    VCCMPMT_CONNECT_REQ,
    VCCMPMT_CONNECT_RSP,
    VCCMPMT_STAT_REQ,
    VCCMPMT_STAT_RSP,
    VCCMPMT_OPEN_REQ,
    VCCMPMT_OPEN_RSP,

    // NB: No responses intentionally!!!
    VCCMPMT_CLOSE_REQ,
    VCCMPMT_DISCONNECT_REQ,

    // NB: Debugging Messages
    VCCMPMT_QUERY_CHANNEL_REQ,
    VCCMPMT_QUERY_CHANNEL_RSP,
    VCCMPMT_QUERY_SLOT_REQ,
    VCCMPMT_QUERY_SLOT_RSP,

} vccmp_msg_type_t;


// NB: Pack the structure on a byte-aligned as they are going over the wire.
#pragma pack(push, 1)

//-----------------------------------------------------------------------------
// vccmp_virtual_drive_boot_sector definition
//-----------------------------------------------------------------------------
typedef struct vccmp_virtual_drive_boot_sector
{
    char            sig[VCVD_SIGNATURE_LEN];
    uint8_t         channel_id;
    char            vm_name[VCCMP_MAX_VM_NAME_STR];

    // NB: Subtract any new fields sizes from the unused array 
    //     so that it stays 512 bytes in length!
    uint8_t         ununsed[512 - (VCVD_SIGNATURE_LEN + 
                                   1 + 
                                   VCCMP_MAX_VM_NAME_STR)];
} vccmp_virtual_drive_boot_sector_t;


//-----------------------------------------------------------------------------
// vccmp_slot_read_address definition
//-----------------------------------------------------------------------------
typedef union vccmp_slot_read_address
{
    uint64_t        value;
    struct {
        uint32_t    low_part;
        uint32_t    high_part;
    } parts;
#ifdef TEST_WITH_LOCAL_DISK
    struct {
        uint64_t        offset : 32;
        uint64_t        index : 2;
        uint64_t        nonce : 1;
        uint64_t        slot_read : 1;
        uint64_t        unused : 28;
    } fields;
#else
    struct {
        uint64_t        offset : 36;
        uint64_t        index : 18;
        uint64_t        nonce : 7;
        uint64_t        slot_read : 1;
        uint64_t        unused : 2;
    } fields;
#endif
} vccmp_slot_read_address_t;


//-----------------------------------------------------------------------------
// vccmp_msg_header definition
//-----------------------------------------------------------------------------
typedef struct vccmp_msg_header
{
    uint32_t          msg_length;
    vccmp_msg_type_t  msg_type;
    uint64_t          msg_id;
} vccmp_msg_header_t;

//-----------------------------------------------------------------------------
// vccmp_stat_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_stat_data
{
    uint64_t    file_length;
    int64_t     last_modified_time;     // UTC in usecs.
    int64_t     expiration_time;        // UTC in usecs.
    uint64_t    bit_rate;
    char        content_type[VCCMP_MAX_CONTENT_TYPE_STR];
    char        etag[VCCMP_MAX_ETAG_STR];
} vccmp_stat_data_t;

//-----------------------------------------------------------------------------
// vccmp_slot_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_slot_data
{
    uint32_t        index;
    uint8_t         nonce;
} vccmp_slot_data_t;

//-----------------------------------------------------------------------------
// vccmp_connect_req_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_connect_req_data
{
    uint8_t     vccmp_major;
    uint8_t     vccmp_minor;
    char        mfc_ver[VCCMP_MAX_MFC_VER_STR];
    char        vm_name[VCCMP_MAX_VM_NAME_STR];
    uint64_t    client_id;
} vccmp_connect_req_data_t;

//-----------------------------------------------------------------------------
// vccmp_connect_req_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_connect_req_msg
{
    vccmp_msg_header_t        msg_hdr;
    vccmp_connect_req_data_t  msg_data;
} vccmp_connect_req_msg_t;

//-----------------------------------------------------------------------------
// vccmp_connect_rsp_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_connect_rsp_data
{
    int32_t     result_code;
    uint8_t     channel_id;     // 0 based
} vccmp_connect_rsp_data_t;

//-----------------------------------------------------------------------------
// vccmp_connect_rsp_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_connect_rsp_msg
{
    vccmp_msg_header_t        msg_hdr;
    vccmp_connect_rsp_data_t  msg_data;
} vccmp_connect_rsp_msg_t;

//-----------------------------------------------------------------------------
// vccmp_stat_req_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_stat_req_data
{
    uint32_t    uri_len;        // string is appended to end of the message.
} vccmp_stat_req_data_t;

//-----------------------------------------------------------------------------
// vccmp_stat_req_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_stat_req_msg
{
    vccmp_msg_header_t    msg_hdr;
    vccmp_stat_req_data_t msg_data;
} vccmp_stat_req_msg_t;

//-----------------------------------------------------------------------------
// vccmp_stat_rsp_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_stat_rsp_data
{
    int32_t           result_code;
    vccmp_stat_data_t stat_data;
} vccmp_stat_rsp_data_t;

//-----------------------------------------------------------------------------
// vccmp_stat_rsp_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_stat_rsp_msg
{
    vccmp_msg_header_t    msg_hdr;
    vccmp_stat_rsp_data_t msg_data;
} vccmp_stat_rsp_msg_t;

//-----------------------------------------------------------------------------
// vccmp_open_req_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_open_req_data
{
    uint32_t    streaming_read_size;
    uint32_t    uri_len;        // string is appended to end of the message.
} vccmp_open_req_data_t;

//-----------------------------------------------------------------------------
// vccmp_open_req_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_open_req_msg
{
    vccmp_msg_header_t    msg_hdr;
    vccmp_open_req_data_t msg_data;
} vccmp_open_req_msg_t;

//-----------------------------------------------------------------------------
// vccmp_open_rsp_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_open_rsp_data
{
    int32_t           result_code;
    vccmp_slot_data_t slot_data;
    vccmp_stat_data_t stat_data;
} vccmp_open_rsp_data_t;

//-----------------------------------------------------------------------------
// vccmp_open_rsp_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_open_rsp_msg
{
    vccmp_msg_header_t    msg_hdr;
    vccmp_open_rsp_data_t msg_data;
} vccmp_open_rsp_msg_t;

//-----------------------------------------------------------------------------
// vccmp_close_req_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_close_req_data
{
    vccmp_slot_data_t     slot_data;
} vccmp_close_req_data_t;

//-----------------------------------------------------------------------------
// vccmp_close_req_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_close_req_msg
{
    vccmp_msg_header_t      msg_hdr;
    vccmp_close_req_data_t  msg_data;
} vccmp_close_req_msg_t;

//-----------------------------------------------------------------------------
// vccmp_disconnect_req_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_disconnect_req_data
{
    uint8_t     channel_id;
} vccmp_disconnect_req_data_t;

//-----------------------------------------------------------------------------
// vccmp_disconnect_req_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_disconnect_req_msg
{
    vccmp_msg_header_t          msg_hdr;
    vccmp_disconnect_req_data_t msg_data;
} vccmp_disconnect_req_msg_t;

//-----------------------------------------------------------------------------
// vccmp_query_channel_req_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_query_channel_req_data
{
    uint8_t           channel_id;
} vccmp_query_channel_req_data_t;

//-----------------------------------------------------------------------------
// vccmp_query_channel_req_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_query_channel_req_msg
{
    vccmp_msg_header_t              msg_hdr;
    vccmp_query_channel_req_data_t  msg_data;
} vccmp_query_channel_req_msg_t;

//-----------------------------------------------------------------------------
// vccmp_query_channel_rsp_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_query_channel_rsp_data
{
    int32_t            result_code;
    int32_t            usage_count;
    uint64_t           bitmask[VCCMP_SLOTS_PER_CHANNEL / 64];
    char               vm_name[VCCMP_MAX_VM_NAME_STR];
} vccmp_query_channel_rsp_data_t;

//-----------------------------------------------------------------------------
// vccmp_query_channel_rsp_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_query_channel_rsp_msg
{
    vccmp_msg_header_t              msg_hdr;
    vccmp_query_channel_rsp_data_t  msg_data;
} vccmp_query_channel_rsp_msg_t;

//-----------------------------------------------------------------------------
// vccmp_query_slot_req_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_query_slot_req_data
{
    uint8_t            channel_id;
    uint32_t           slot_index;
} vccmp_query_slot_req_data_t;

//-----------------------------------------------------------------------------
// vccmp_query_slot_req_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_query_slot_req_msg
{
    vccmp_msg_header_t              msg_hdr;
    vccmp_query_slot_req_data_t     msg_data;
} vccmp_query_slot_req_msg_t;

//-----------------------------------------------------------------------------
// vccmp_query_slot_rsp_data definition
//-----------------------------------------------------------------------------
typedef struct vccmp_query_slot_rsp_data
{
    int32_t            result_code;
    int8_t             nonce;
    uint64_t           file_length;
    uint64_t           client_id;
    uint32_t           uri_len; // string is appended to end of the message.
} vccmp_query_slot_rsp_data_t;

//-----------------------------------------------------------------------------
// vccmp_query_slot_rsp_msg definition
//-----------------------------------------------------------------------------
typedef struct vccmp_query_slot_rsp_msg
{
    vccmp_msg_header_t              msg_hdr;
    vccmp_query_slot_rsp_data_t     msg_data;
} vccmp_query_slot_rsp_msg_t;


//-----------------------------------------------------------------------------
// vcc_stats_data_t definition
//-----------------------------------------------------------------------------
typedef struct vcc_stats_data
{
    uint64_t     total_connect_cnt;
    uint64_t     total_lost_cnt;
    uint64_t     total_error_cnt;
    uint64_t     total_connect_requests_cnt;
    uint64_t     total_open_requests_cnt;
    uint64_t     total_stat_requests_cnt;
    uint64_t     total_read_requests_cnt;
    uint64_t     total_close_requests_cnt;
    uint64_t     total_disconnect_requests_cnt;
    uint64_t     total_open_files_cnt;
    uint64_t     current_open_files_cnt;
    uint64_t     total_bytes_read_cnt;
    uint64_t     total_open_errors_cnt;
    uint64_t     total_stat_errors_cnt;
    uint64_t     total_read_errors_cnt;
} vcc_stats_data_t;

#pragma pack(pop)

// Return a displayable string for the vccmp_msg_type value.
static inline const char* get_msg_type_string(
    vccmp_msg_type_t request_type)
{
    switch (request_type) 
    {
    case VCCMPMT_CONNECT_REQ:
	return "Connect-Request";
    case VCCMPMT_CONNECT_RSP:
	return "Connect-Response";
    case VCCMPMT_STAT_REQ:
	return "Stat-Request";
    case VCCMPMT_STAT_RSP:
	return "Stat-Response";
    case VCCMPMT_OPEN_REQ:
	return "Open-Request";
    case VCCMPMT_OPEN_RSP:
	return "Open-Response";

    case VCCMPMT_CLOSE_REQ:
	return "Close-Request";
    case VCCMPMT_DISCONNECT_REQ:
	return "Disconnect-Request";

    case VCCMPMT_QUERY_CHANNEL_REQ:
	return "Query-Channel-Request";
    case VCCMPMT_QUERY_CHANNEL_RSP:
	return "Query-Channel-Response";
    case VCCMPMT_QUERY_SLOT_REQ:
	return "Query-Slot-Request";
    case VCCMPMT_QUERY_SLOT_RSP:
	return "Query-Slot-Response";
    case VCCMPMT_UNKNOWN:
    default:
	return "<Unknown>";
    }
}

// Functions to initialize message to default values.
static inline void vccmp_reset_msg_header(
    vccmp_msg_header_t* hdr) 
{
    hdr->msg_length = 0;
    hdr->msg_type = VCCMPMT_UNKNOWN;
    hdr->msg_id = VCCMP_INVALID_MSG_ID;
}

static inline void vccmp_reset_stat_data(
    vccmp_stat_data_t* data) 
{
    data->file_length = 0;
    data->last_modified_time = 0;
    data->expiration_time = 0;
    data->bit_rate = 0;
    memset(data->content_type, 0, VCCMP_MAX_CONTENT_TYPE_STR);
    memset(data->etag, 0, VCCMP_MAX_ETAG_STR);
}

static inline void vccmp_setup_connect_req_msg(
    vccmp_connect_req_msg_t* msg,
    const char *vm_name,
    const char *mfc_ver,
    uint64_t client_id)
{
    msg->msg_hdr.msg_type = VCCMPMT_CONNECT_REQ;
    msg->msg_hdr.msg_id = VCCMP_CONNECTION_MSG_ID;
    msg->msg_hdr.msg_length = sizeof(vccmp_connect_req_msg_t);
    msg->msg_data.vccmp_major = VCCMP_MAJOR_VER;
    msg->msg_data.vccmp_minor = VCCMP_MINOR_VER;
    strncpy(msg->msg_data.mfc_ver, mfc_ver, sizeof(msg->msg_data.mfc_ver)-1);
    strncpy(msg->msg_data.vm_name, vm_name, sizeof(msg->msg_data.vm_name)-1);
    msg->msg_data.client_id = client_id;
}

static inline void vccmp_setup_connect_rsp_msg(
    vccmp_connect_rsp_msg_t* msg,
    int32_t result_code,
    uint8_t channel_id)
{
    msg->msg_hdr.msg_type = VCCMPMT_CONNECT_RSP;
    msg->msg_hdr.msg_id = VCCMP_CONNECTION_MSG_ID;
    msg->msg_hdr.msg_length = sizeof(vccmp_connect_rsp_msg_t);
    msg->msg_data.result_code = result_code;
    msg->msg_data.channel_id = channel_id;
}

static inline void vccmp_setup_stat_req_msg(
    vccmp_stat_req_msg_t* msg,
    uint64_t msg_id,
    uint32_t uri_len)
{
    msg->msg_hdr.msg_type = VCCMPMT_STAT_REQ;
    msg->msg_hdr.msg_id = msg_id;
    msg->msg_hdr.msg_length = sizeof(vccmp_stat_req_msg_t) + uri_len;
    msg->msg_data.uri_len = uri_len;
}

static inline void vccmp_setup_stat_rsp_msg(
    vccmp_stat_rsp_msg_t* msg,
    uint64_t msg_id,
    int32_t result_code,
    vccmp_stat_data_t* stat_data) 
{
    msg->msg_hdr.msg_type = VCCMPMT_STAT_RSP;
    msg->msg_hdr.msg_id = msg_id;
    msg->msg_hdr.msg_length = sizeof(vccmp_stat_rsp_msg_t);
    msg->msg_data.result_code = result_code;
    if (stat_data != NULL) {
	msg->msg_data.stat_data = *stat_data;
    } else {
	memset(&msg->msg_data.stat_data, 0, sizeof(vccmp_stat_data_t));
    }
}

static inline void vccmp_setup_open_req_msg(
    vccmp_open_req_msg_t* msg,
    uint64_t msg_id,
    uint32_t streaming_read_size,
    uint32_t uri_len)
{
    msg->msg_hdr.msg_type = VCCMPMT_OPEN_REQ;
    msg->msg_hdr.msg_id = msg_id;
    msg->msg_hdr.msg_length = sizeof(vccmp_open_req_msg_t) + uri_len;
    msg->msg_data.streaming_read_size = streaming_read_size;
    msg->msg_data.uri_len = uri_len;
}

static inline void vccmp_setup_open_rsp_msg(
    vccmp_open_rsp_msg_t* msg,
    uint64_t msg_id,
    int32_t result_code,
    vccmp_slot_data_t* slot_data,
    vccmp_stat_data_t* stat_data) 
{
    msg->msg_hdr.msg_type = VCCMPMT_OPEN_RSP;
    msg->msg_hdr.msg_id = msg_id;
    msg->msg_hdr.msg_length = sizeof(vccmp_open_rsp_msg_t);
    msg->msg_data.result_code = result_code;
    if (slot_data) {
	msg->msg_data.slot_data = *slot_data;
    } else {
	memset(&msg->msg_data.slot_data, 0, sizeof(vccmp_slot_data_t));
    }
    if (stat_data) {
	msg->msg_data.stat_data = *stat_data;
    } else {
	memset(&msg->msg_data.stat_data, 0, sizeof(vccmp_stat_data_t));
    }
}


static inline void vccmp_setup_close_req_msg(
    vccmp_close_req_msg_t* msg,
    uint64_t msg_id,
    vccmp_slot_data_t* slot_data)
{
    msg->msg_hdr.msg_type = VCCMPMT_CLOSE_REQ;
    msg->msg_hdr.msg_id = msg_id;
    msg->msg_hdr.msg_length = sizeof(vccmp_close_req_msg_t);
    if (slot_data) {
	msg->msg_data.slot_data = *slot_data;
    } else {
	memset(&msg->msg_data.slot_data, 0, sizeof(vccmp_slot_data_t));
    }
}

static inline void vccmp_setup_disconnect_req_msg(
    vccmp_disconnect_req_msg_t* msg,
    uint8_t channel_id)
{
    msg->msg_hdr.msg_type = VCCMPMT_DISCONNECT_REQ;
    msg->msg_hdr.msg_id = VCCMP_CONNECTION_MSG_ID;
    msg->msg_hdr.msg_length = sizeof(vccmp_disconnect_req_msg_t);
    msg->msg_data.channel_id = channel_id;
}


static inline void vccmp_setup_query_channel_req_msg(
    vccmp_query_channel_req_msg_t* msg,
    uint64_t msg_id,
    uint8_t channel_id)
{
    msg->msg_hdr.msg_type = VCCMPMT_QUERY_CHANNEL_REQ;
    msg->msg_hdr.msg_id = msg_id;
    msg->msg_hdr.msg_length = sizeof(vccmp_query_channel_req_msg_t);
    msg->msg_data.channel_id = channel_id;
}

static inline void vccmp_setup_query_channel_rsp_msg(
    vccmp_query_channel_rsp_msg_t* msg,
    uint64_t msg_id,
    uint32_t result_code,
    int32_t  usage_count,
    const char *vm)
{
    msg->msg_hdr.msg_type = VCCMPMT_QUERY_CHANNEL_RSP;
    msg->msg_hdr.msg_id = msg_id;
    msg->msg_hdr.msg_length = sizeof(vccmp_query_channel_rsp_msg_t);
    memset(&msg->msg_data, 0, sizeof(vccmp_query_channel_rsp_data_t));
    msg->msg_data.result_code = result_code;
    msg->msg_data.usage_count = usage_count;
    if (vm != NULL) {
	strncpy(msg->msg_data.vm_name, vm, sizeof((msg->msg_data.vm_name)-1));
    }
}

static inline void vccmp_setup_query_slot_req_msg(
    vccmp_query_slot_req_msg_t* msg,
    uint64_t msg_id,
    uint8_t channel_id,
    uint32_t slot_index)
{
    msg->msg_hdr.msg_type = VCCMPMT_QUERY_SLOT_REQ;
    msg->msg_hdr.msg_id = msg_id;
    msg->msg_hdr.msg_length = sizeof(vccmp_query_slot_req_msg_t);
    msg->msg_data.channel_id = channel_id;
    msg->msg_data.slot_index = slot_index;
}

static inline void vccmp_setup_query_slot_rsp_msg(
    vccmp_query_slot_rsp_msg_t* msg,
    uint64_t msg_id,
    uint32_t result_code,
    uint8_t nonce,
    uint64_t client_id,
    uint64_t file_length,
    uint32_t uri_len)
{
    msg->msg_hdr.msg_type = VCCMPMT_QUERY_SLOT_RSP;
    msg->msg_hdr.msg_id = msg_id;
    msg->msg_hdr.msg_length = (sizeof(vccmp_query_slot_rsp_msg_t) + uri_len);
    msg->msg_data.result_code = result_code;
    msg->msg_data.nonce = nonce;
    msg->msg_data.client_id = client_id;
    msg->msg_data.file_length = file_length;
    msg->msg_data.uri_len = uri_len;
}

static inline void vccmp_setup_virtual_drive_boot_sector(
    vccmp_virtual_drive_boot_sector_t* boot_sector,
    uint8_t channel_id,
    const char* vm_name)
{
    memset(boot_sector, 0, sizeof(vccmp_virtual_drive_boot_sector_t));
    strcpy(boot_sector->sig, VCVD_SIGNATURE);
    boot_sector->channel_id = channel_id;
    if (vm_name != NULL) {
	strncpy(boot_sector->vm_name, vm_name, sizeof((boot_sector->vm_name)-1));
    } else {
	memset(boot_sector->vm_name, 0, sizeof(boot_sector->vm_name));
    }
}

static inline void vcc_reset_stats_data(
    vcc_stats_data_t* stats_data)
{
    memset(stats_data, 0, sizeof(vcc_stats_data_t));
}

#endif // __virt_cache_defs_h__

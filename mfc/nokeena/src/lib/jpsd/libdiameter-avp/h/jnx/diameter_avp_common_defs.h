/*
 * $Id: diameter_avp_common_defs.h 674749 2014-10-03 18:47:53Z vitaly $
 *
 * diameter_avp_common_defs.h - Diameter AVP data structures
 *
 * Copyright (c) 2011-2013, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef __DIAMETER_AVP_COMMON_DEFS_H__
#define __DIAMETER_AVP_COMMON_DEFS_H__

/* command flags */
enum diameter_command_flags_enum
{
    DIAMETER_COMMAND_FLAG_T  = 0x10000000,
    DIAMETER_COMMAND_FLAG_E  = 0x20000000,
    DIAMETER_COMMAND_FLAG_P  = 0x40000000,
    DIAMETER_COMMAND_FLAG_R  = 0x80000000
};
#define DIAMETER_COMMAND_FLAG_RESERVED (~(DIAMETER_COMMAND_FLAG_T \
                | DIAMETER_COMMAND_FLAG_E \
                | DIAMETER_COMMAND_FLAG_P \
                | DIAMETER_COMMAND_FLAG_R))

/* command codes */
enum diameter_command_code_enum
{
    DIAMETER_COMMAND_ABORT_SESSION              = 274,
    DIAMETER_COMMAND_ACCOUNTING                 = 271,
    DIAMETER_COMMAND_CAPABILITIES_EXCHANGE      = 257,
    DIAMETER_COMMAND_DEVICE_WATCHDOG            = 280,
    DIAMETER_COMMAND_DISCONNECT_PEER            = 282,

    DIAMETER_COMMAND_SESSION_TERMINATION        = 275,

    /* NASREQ */
    DIAMETER_COMMAND_AA                         = 265,
    DIAMETER_COMMAND_RE_AUTH                    = 258,
    /* DCCA */
    DIAMETER_COMMAND_CC                         = 272
 };

/* message header in network byte order */
typedef struct diameter_message_header_struct
{
    uint32_t  message_version_length;
    uint32_t  command_flags_code;
    uint32_t  application_id;
    uint32_t  hop_by_hop_identifier;
    uint32_t  end_to_end_identifier;

} diameter_message_header_t;

/* decoded message header in host byte order */
typedef struct diameter_message_info_struct {
    uint32_t            message_version;
    uint32_t            message_length;
    uint32_t            command_flags;
    uint32_t            command_code;
    uint32_t            application_id ;
    uint32_t            hop_by_hop;
    uint32_t            end_to_end;
} diameter_message_info_t;

/* message version/length masks */
enum diameter_message_version_enum
{
    DIAMETER_MASK_MESSAGE_VERSION   = 0xFF000000,
    DIAMETER_MASK_MESSAGE_LENGTH    = 0x00FFFFFF
};

/* versions */
enum diameter_version_enum
{
    DIAMETER_VERSION_1              = 0x01000000
};

/* Vendor Ids */
typedef enum {
    VENDOR_ID_NONE     = 0,
    VENDOR_ID_3GPP     = 10415,
    VENDOR_ID_JUNIPER  = 2636,
    VENDOR_ID_VODAFONE = 12645,
    VENDOR_ID_ERICSSON = 193,
    VENDOR_ID_MICROSOFT = 311,
    VENDOR_ID_JUNIPER_UNISPHERE = 4874
} aaa_diam_vendor_id_t;

/* command flags/command code masks */
enum diameter_command_mask_enum
{
    DIAMETER_MASK_COMMAND_FLAGS     = 0xFF000000,
    DIAMETER_MASK_COMMAND_CODE      = 0x00FFFFFF
};
/* AVP header */
typedef struct diameter_avp_header_struct
{
    uint32_t avp_code;
    uint32_t avp_flags_length;
} diameter_avp_header_t;

/* AVP vendor-specific header */
typedef struct diameter_avp_vs_header_struct
{
    uint32_t    avp_code;
    uint32_t    avp_flags_length;
    uint32_t    avp_vendor_id;
} diameter_avp_vs_header_t;

static inline uint32_t diameter_pad (uint32_t len)
{
    return (len + 3) & ~3;
}
/* avp flags/length masks */
enum diameter_avp_mask_enum
{
    DIAMETER_MASK_AVP_FLAGS      = 0xFF000000,
    DIAMETER_MASK_AVP_LENGTH     = 0x00FFFFFF
};

/* avp flags */
enum diameter_avp_flag_enum
{
    DIAMETER_AVP_FLAG_P          = 0x20000000,
    DIAMETER_AVP_FLAG_M          = 0x40000000,
    DIAMETER_AVP_FLAG_V          = 0x80000000
};

#define AAA_RADIUS_AVP_FLAG_TAG  0x1

/* base AVP codes */
enum diameter_avp_code_enum
{
    DIAMETER_CODE_ACCT_APPLICATION_ID            = 259,
    DIAMETER_CODE_AUTH_APPLICATION_ID            = 258,
    DIAMETER_CODE_DESTINATION_HOST               = 293,
    DIAMETER_CODE_DESTINATION_REALM              = 283,
    DIAMETER_CODE_DISCONNECT_CAUSE               = 273,
    DIAMETER_CODE_ERROR_MESSAGE                  = 281,
    DIAMETER_CODE_ERROR_REPORTING_HOST           = 294,
    DIAMETER_CODE_EXPERIMENTAL_RESULT            = 297,
    DIAMETER_CODE_EXPERIMENTAL_RESULT_CODE       = 298,
    DIAMETER_CODE_FAILED_AVP                     = 279,
    DIAMETER_CODE_FIRMWARE_REVISION              = 267,
    DIAMETER_CODE_HOST_IP_ADDRESS                = 257,
    DIAMETER_CODE_INBAND_SECURITY_ID             = 299,
    DIAMETER_CODE_ORIGIN_HOST                    = 264,
    DIAMETER_CODE_ORIGIN_REALM                   = 296,
    DIAMETER_CODE_ORIGIN_STATE_ID                = 278,
    DIAMETER_CODE_PRODUCT_NAME                   = 269,
    DIAMETER_CODE_PROXY_HOST                     = 280,
    DIAMETER_CODE_RESULT_CODE                    = 268,
    DIAMETER_CODE_SESSION_ID                     = 263,
    DIAMETER_CODE_SUPPORTED_VENDOR_ID            = 265,
    DIAMETER_CODE_VENDOR_ID                      = 266,
    DIAMETER_CODE_VENDOR_SPECIFIC_APPLICATION_ID = 260,
    DIAMETER_CODE_REDIRECT_HOST_USAGE            = 261,
    DIAMETER_CODE_REDIRECT_HOST                  = 292,
    DIAMETER_CODE_RE_AUTH_REQUEST_TYPE           = 285
};

/* applications */
enum diameter_application_enum
{
    DIAMETER_APPLICATION_COMMON_MESSAGES        = 0,
    DIAMETER_APPLICATION_NASREQ                 = 1,
    DIAMETER_APPLICATION_MOBILE_IP              = 2,
    DIAMETER_APPLICATION_BASE_ACCOUNTING        = 3,
    DIAMETER_APPLICATION_DCCA                   = 4,
    DIAMETER_APPLICATION_GX                     = 16777238,
    DIAMETER_APPLICATION_RELAY                  = 0xFFFFFFFF
};

/* result code classes */
enum diameter_result_class_enum
{
    DIAMETER_RCC_INFORMATIONAL       = 1000,
    DIAMETER_RCC_SUCCESS             = 2000,
    DIAMETER_RCC_PROTOCOL_ERROR      = 3000,
    DIAMETER_RCC_TRANSIENT_FAILURE   = 4000,
    DIAMETER_RCC_PERMANENT_FAILURE   = 5000
};

enum diameter_result_code_enum
{
    /* informational */
    DIAMETER_RC_MULTI_ROUND_AUTH                = 1001,

    /* success */
    DIAMETER_RC_SUCCESS                         = 2001,
    DIAMETER_RC_LIMITED_SUCCESS                 = 2002,

    /* protocol errors */
    DIAMETER_RC_COMMAND_UNSUPPORTED             = 3001,
    DIAMETER_RC_UNABLE_TO_DELIVER               = 3002,
    DIAMETER_RC_REALM_NOT_SERVED                = 3003,
    DIAMETER_RC_TOO_BUSY                        = 3004,
    DIAMETER_RC_LOOP_DETECTED                   = 3005,
    DIAMETER_RC_REDIRECT_INDICATION             = 3006,
    DIAMETER_RC_APPLICATION_UNSUPPORTED         = 3007,
    DIAMETER_RC_INVALID_HDR_BITS                = 3008,
    DIAMETER_RC_INVALID_AVP_BITS                = 3009,
    DIAMETER_RC_UNKNOWN_PEER                    = 3010,

    /* transient failures */
    DIAMETER_RC_AUTHENTICATION_REJECTED         = 4001,
    DIAMETER_RC_OUT_OF_SPACE                    = 4002,
    DIAMETER_RC_ELECTION_LOST                   = 4003,

    /* permanent failures */
    DIAMETER_RC_AVP_UNSUPPORTED                 = 5001,
    DIAMETER_RC_UNKNOWN_SESSION_ID              = 5002,
    DIAMETER_RC_AUTHORIZATION_REJECTED          = 5003,
    DIAMETER_RC_INVALID_AVP_VALUE               = 5004,
    DIAMETER_RC_MISSING_AVP                     = 5005,
    DIAMETER_RC_RESOURCES_EXCEEDED              = 5006,
    DIAMETER_RC_CONTRADICTING_AVPS              = 5007,
    DIAMETER_RC_AVP_NOT_ALLOWED                 = 5008,
    DIAMETER_RC_AVP_OCCURS_TOO_MANY_TIMES       = 5009,
    DIAMETER_RC_NO_COMMON_APPLICATION           = 5010,
    DIAMETER_RC_UNSUPPORTED_VERSION             = 5011,
    DIAMETER_RC_UNABLE_TO_COMPLY                = 5012,
    DIAMETER_RC_INVALID_BIT_IN_HEADER           = 5013,
    DIAMETER_RC_INVALID_AVP_LENGTH              = 5014,
    DIAMETER_RC_INVALID_MESSAGE_LENGTH          = 5015,
    DIAMETER_RC_INVALID_AVP_BIT_COMBO           = 5016,
    DIAMETER_RC_NO_COMMON_SECURITY              = 5017
};
/* Disconnect-Cause */
enum diameter_disconnect_cause_enum
{
    DIAMETER_DC_REBOOTING                    = 0,
    DIAMETER_DC_BUSY                         = 1,
    DIAMETER_DC_DO_NOT_WANT_TO_TALK_TO_YOU   = 2
};

/* Re-Auth-Request-Type Value */
enum diameter_re_auth_request_type_enum
{
    DIAMETER_RAR_TYPE_AUTHORIZE_ONLY          = 0,
    DIAMETER_RAR_TYPE_AUTHORIZE_AUTHENTICATE  = 1
};

/* IANA Address Family Numbers */
enum diameter_iana_address_family_enum
{
    IANA_AF_IP   = 1,
    IANA_AF_IP6  = 2
};

enum diameter_port_enum
{
    DIAMETER_DEFAULT_PORT = 3868
};
/* Inband-Security-Id AVP values */
enum diameter_inband_security_enum
{
    DIAMETER_NO_INBAND_SECURITY     = 0,
    DIAMETER_INBAND_SECURITY_TLS    = 1
};

typedef enum diameter_peer_state_e {
    /*
     * Diameter SNMP mib depends on the 
     * ordering and value of this enum
     * please notify kgirish before editing this 
     */
    DIAMETER_PEER_STATE_CLOSED = 0,
    DIAMETER_PEER_STATE_WAIT_CONN_ACK,
    DIAMETER_PEER_STATE_WAIT_I_CEA,
    DIAMETER_PEER_STATE_WAIT_CONN_ACK_ELECT,
    DIAMETER_PEER_STATE_WAIT_RETURNS,
    DIAMETER_PEER_STATE_R_OPEN,
    DIAMETER_PEER_STATE_I_OPEN,
    DIAMETER_PEER_STATE_CLOSING,
    /* this must be the last */
    DIAMETER_PEER_NUM_STATES
} diameter_peer_state_t;

static inline const char* diameter_peer_state_string(diameter_peer_state_t state)
{ 
    /*
      Diameter SNMP mib depends on the 
     * ordering and value of this enum
     * please notify kgirish before editing this 
     */
    switch (state) {
    case DIAMETER_PEER_STATE_CLOSED:
        return "Closed";
    case DIAMETER_PEER_STATE_WAIT_CONN_ACK:
        return "Wait-Conn-Ack";
    case DIAMETER_PEER_STATE_WAIT_I_CEA:
        return "Wait-I-CEA";
    case DIAMETER_PEER_STATE_WAIT_CONN_ACK_ELECT:
        return "Wait-Conn-Ack/Elect";
    case DIAMETER_PEER_STATE_WAIT_RETURNS:
        return "Wait-Returns";
    case DIAMETER_PEER_STATE_R_OPEN:
        return "R-Open";
    case DIAMETER_PEER_STATE_I_OPEN:
        return "I-Open";
    case DIAMETER_PEER_STATE_CLOSING:
        return "Closing";
    case DIAMETER_PEER_NUM_STATES:
        break;
    }
    return "Unknown";
}
typedef enum diameter_watchdog_state_e {
    DIAMETER_WATCHDOG_INITIAL = 0,   // The initial state when it is first created.
    DIAMETER_WATCHDOG_OKAY,          // The connection is up
    DIAMETER_WATCHDOG_SUSPECT,       // Failover has been initiated on the connection.
    DIAMETER_WATCHDOG_DOWN,          // Connection has been closed.
    DIAMETER_WATCHDOG_REOPEN,        // Attempting to reopen a closed connection
} diameter_watchdog_state_t;

static inline
const char* diameter_watchdog_state_string(diameter_watchdog_state_t state)
{
    switch (state)
    {
    case DIAMETER_WATCHDOG_INITIAL:
        return "initial";
    case DIAMETER_WATCHDOG_OKAY:
        return "okay";
    case DIAMETER_WATCHDOG_SUSPECT:
        return "suspect";
    case DIAMETER_WATCHDOG_DOWN:
        return "down";
    case DIAMETER_WATCHDOG_REOPEN:
        return "reopen";
    }
    return "unknown watchdog state";
}

/* IANA Address family numbers */
typedef enum diameter_iana_addr_family_s {
    DIAMETER_AF_IP_VERSION_4 = 1,
    DIAMETER_AF_IP_VERSION_6 = 2
} diameter_iana_addr_family_t;

#endif // __DIAMETER_AVP_COMMON_DEFS_H__

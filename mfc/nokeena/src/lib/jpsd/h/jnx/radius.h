/*
 * radius.h
 *
 * Gazal Sahai, July 2010
 *
 * Public header file for the Remote Authentication Dial In User
 * Service (RADIUS) client library.  This library implements the
 * RADIUS protocol client operations as described by the RFC 2865 and
 * the RFC 2866.  The RADIUS protocol can be used for carrying
 * authentication, authorization, and configuration information
 * between Network Access Server and a shared Authentication Server.
 *
 * The RADIUS accounting protocol extends the RADIUS protocol to cover
 * delivery of accounting information from the Network Access Server
 * (NAS) to a RADIUS accounting server.
 *
 * References:
 *
 *   RFC 2548   Microsoft Vendor-specific RADIUS Attributes
 *   RFC 2865   Remote Authentication Dial In User Service (RADIUS)
 *   RFC 2866   RADIUS Accounting
 *   RFC 2867   RADIUS Accounting Modifications for Tunnel Protocol Support
 *   RFC 2868   RADIUS Attributes for Tunnel Protocol Support
 *   RFC 2869   RADIUS Extensions
 *   RFC 3162   RADIUS and IPv6

 * Copyright (c) 2010-2013, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef __RADIUS_H__
#define __RADIUS_H__

/************************** Types and definitions ***************************/

#define AAA_UTILS_TF_DEBUG (1 << 5)

/* The RFC 2865 specifies minimum and maximum packet sizes to be 20
   and 4096.  The RFC 2866 specifies the maximum packet size to 4095
   so we use here the biggest value. */
#define RADIUS_MIN_PACKET_SIZE 20
#define RADIUS_MAX_PACKET_SIZE 4096

/* The default UDP ports for radius access and accounting servers. */
#define RADIUS_ACCESS_DEFAULT_PORT          1812
#define RADIUS_ACCOUNTING_DEFAULT_PORT      1813
#define RADIUS_DYN_AUTH_DEFAULT_PORT        3799
#define RADIUS_ACCESS_DEFAULT_TIMEOUT       3
#define RADIUS_ACCESS_DEFAULT_RETRY         3
#define RADIUS_AUTHENTICATOR_SIZE           16

/* VSA specific processing info */
#define RADIUS_VENDOR_ID_LENGTH                   4
#define RADIUS_GENERIC_VSA_HDR_LENGTH             2
#define RADIUS_GENERIC_AVP_HDR_LENGTH             2
#define RADIUS_GENERIC_TOTAL_VSA_HDR_LEN          8 
#define RADIUS_GENERIC_VSA_TYPE_FIELD_OFFSET      4
#define RADIUS_GENERIC_VSA_LENGTH_FIELD_OFFSET    5

/* 
 * Port 0 is used to indicate a radius server not being used for 
 * authentication or accounting.
 * if radius_server->port is 0 
 *       this server will not be used for authentication
 * if radius_server->acct_port is 0 
 *       this server will not be used for accounting
 *
 */
#define RADIUS_PORT_ZERO                    "0"
/* MD5 digest length and HMAC block size. */
#define RADIUS_MD5_LENGTH 16
#define RADIUS_HMAC_BLOCK 64

#define TRUE 1
#define FALSE 0
#define PUTLONG NS_PUT32
#define NS_PUT32 __ns_put32

#define AAA_UTILS_TRACE_BUF_LEN 256
#define AAA_UTILS_TRACE_ALL       5

#define RADIUS_IS_RESPONSE_MSG(_op_)                \
    (((_op_) == RADIUS_ACCESS_ACCEPT)           ||  \
     ((_op_) == RADIUS_DISCONNECT_RESPONSE_ACK) ||  \
     ((_op_) == RADIUS_COA_RESPONSE_ACK))


#define RADIUS_IS_DYNAMIC_AUTH_RSP(_op_)            \
    (((_op_) == RADIUS_DISCONNECT_RESPONSE_ACK) ||  \
     ((_op_) == RADIUS_DISCONNECT_RESPONSE_NAK) ||  \
     ((_op_) == RADIUS_COA_RESPONSE_ACK)        ||  \
     ((_op_) == RADIUS_COA_RESPONSE_NAK))

/*
 * RADIUS TUNNEL TYPES (RFC 2868)
 */
#define RADIUS_TUNNEL_TYPE_PPTP            1
#define RADIUS_TUNNEL_TYPE_L2F             2
#define RADIUS_TUNNEL_TYPE_L2TP            3
#define RADIUS_TUNNEL_TYPE_ATMP            4
#define RADIUS_TUNNEL_TYPE_VTP             5
#define RADIUS_TUNNEL_TYPE_AH              6
#define RADIUS_TUNNEL_TYPE_IPIP_ENCAP      7
#define RADIUS_TUNNEL_TYPE_MIN_IPIP        8
#define RADIUS_TUNNEL_TYPE_ESP             9
#define RADIUS_TUNNEL_TYPE_GRE            10
#define RADIUS_TUNNEL_TYPE_DVS            11
#define RADIUS_TUNNEL_TYPE_IPIP_TUNNEL    12
#define RADIUS_TUNNEL_TYPE_VLAN           13

/* the following used for error checking the attribute length of
 * the following attributes:
 * RADIUS_AVP_FRAMED_IPV6_PREFIX,
 * RADIUS_AVP_DELEGATED_IPV6_PREFIX
 */

#define AAA_RADIUS_MIN_FRAMED_IPV6_PREFIX_ATTRIB_LEN  2
#define AAA_RADIUS_MAX_FRAMED_IPV6_PREFIX_ATTRIB_LEN  18
/* the following used for error checking the actual length in bits
 * of an ipv6 prefix contained in the following:
 * RADIUS_AVP_FRAMED_IPV6_PREFIX,
 * RADIUS_AVP_DELEGATED_IPV6_PREFIX
 */
#define AAA_RADIUS_MAX_IPV6_PREFIX_BITLEN          128

/* also used for error checking attribute
 * length for RADIUS_AVP_LOGIN_IPV6_HOST
 */
#define AAA_RADIUS_IPV6_ATTRIB_LEN                16


/* Radius operation codes. */
typedef enum
{
    RADIUS_ACCESS_REQUEST             = 1,
    RADIUS_ACCESS_ACCEPT              = 2,
    RADIUS_ACCESS_REJECT              = 3,
    RADIUS_ACCOUNTING_REQUEST         = 4,  /* RFC 2866 */
    RADIUS_ACCOUNTING_RESPONSE        = 5,  /* RFC 2866 */
    RADIUS_ACCESS_CHALLENGE           = 11,
    RADIUS_DISCONNECT_REQUEST         = 40,
    RADIUS_DISCONNECT_RESPONSE_ACK    = 41,
    RADIUS_DISCONNECT_RESPONSE_NAK    = 42,
    RADIUS_COA_REQUEST                = 43,
    RADIUS_COA_RESPONSE_ACK           = 44,
    RADIUS_COA_RESPONSE_NAK           = 45,
} radius_operation_code_t;

/* Radius attribute types. */
typedef enum
{
  /* Attribute types from the RFC 2865. */
    RADIUS_AVP_USER_NAME                      = 1,
    RADIUS_AVP_USER_PASSWORD                  = 2,
    RADIUS_AVP_CHAP_PASSWORD                  = 3,
    RADIUS_AVP_NAS_IP_ADDRESS                 = 4,
    RADIUS_AVP_NAS_PORT                       = 5,
    RADIUS_AVP_SERVICE_TYPE                   = 6,
    RADIUS_AVP_FRAMED_PROTOCOL                = 7,
    RADIUS_AVP_FRAMED_IP_ADDRESS              = 8,
    RADIUS_AVP_FRAMED_IP_NETMASK              = 9,
    RADIUS_AVP_FRAMED_ROUTING                 = 10,
    RADIUS_AVP_FILTER_ID                      = 11,
    RADIUS_AVP_FRAMED_MTU                     = 12,
    RADIUS_AVP_FRAMED_COMPRESSION             = 13,
    RADIUS_AVP_LOGIN_IP_HOST                  = 14,
    RADIUS_AVP_LOGIN_SERVICE                  = 15,
    RADIUS_AVP_LOGIN_TCP_PORT                 = 16,
    RADIUS_AVP_REPLY_MESSAGE                  = 18,
    RADIUS_AVP_CALLBACK_NUMBER                = 19,
    RADIUS_AVP_CALLBACK_ID                    = 20,
    RADIUS_AVP_FRAMED_ROUTE                   = 22,
    RADIUS_AVP_FRAMED_IPX_NETWORK             = 23,
    RADIUS_AVP_STATE                          = 24,
    RADIUS_AVP_CLASS                          = 25,
    RADIUS_AVP_VENDOR_SPECIFIC                = 26,
    RADIUS_AVP_SESSION_TIMEOUT                = 27,
    RADIUS_AVP_IDLE_TIMEOUT                   = 28,
    RADIUS_AVP_TERMINATION_ACTION             = 29,
    RADIUS_AVP_CALLED_STATION_ID              = 30,
    RADIUS_AVP_CALLING_STATION_ID             = 31,
    RADIUS_AVP_NAS_IDENTIFIER                 = 32,
    RADIUS_AVP_PROXY_STATE                    = 33,
    RADIUS_AVP_LOGIN_LAT_SERVICE              = 34,
    RADIUS_AVP_LOGIN_LAT_NODE                 = 35,
    RADIUS_AVP_LOGIN_LAT_GROUP                = 36,
    RADIUS_AVP_FRAMED_APPLETALK_LINK          = 37,
    RADIUS_AVP_FRAMED_APPLETALK_NETWORK       = 38,
    RADIUS_AVP_FRAMED_APPLETALK_ZONE          = 39,
    RADIUS_AVP_CHAP_CHALLENGE                 = 60,
    RADIUS_AVP_NAS_PORT_TYPE                  = 61,
    RADIUS_AVP_PORT_LIMIT                     = 62,
    RADIUS_AVP_LOGIN_LAT_PORT                 = 63,

    /* Attribute types from the RFC 2866. */
    RADIUS_AVP_ACCT_STATUS_TYPE               = 40,
    RADIUS_AVP_ACCT_DELAY_TIME                = 41,
    RADIUS_AVP_ACCT_INPUT_OCTETS              = 42,
    RADIUS_AVP_ACCT_OUTPUT_OCTETS             = 43,
    RADIUS_AVP_ACCT_SESSION_ID                = 44,
    RADIUS_AVP_ACCT_AUTHENTIC                 = 45,
    RADIUS_AVP_ACCT_SESSION_TIME              = 46,
    RADIUS_AVP_ACCT_INPUT_PACKETS             = 47,
    RADIUS_AVP_ACCT_OUTPUT_PACKETS            = 48,
    RADIUS_AVP_ACCT_TERMINATE_CAUSE           = 49,
    RADIUS_AVP_ACCT_MULTI_SESSION_ID          = 50,
    RADIUS_AVP_ACCT_LINK_COUNT                = 51,

    /* Attribute types form the RFC 2867.  */
    RADIUS_AVP_ACCT_TUNNEL_CONNECTION         = 68,
    RADIUS_AVP_ACCT_TUNNEL_PACKETS_LOST       = 86,

    /* Attribute types from the RFC 2868. */
    RADIUS_AVP_TUNNEL_TYPE                    = 64,
    RADIUS_AVP_TUNNEL_MEDIUM_TYPE             = 65,
    RADIUS_AVP_TUNNEL_CLIENT_ENDPOINT         = 66,
    RADIUS_AVP_TUNNEL_SERVER_ENDPOINT         = 67,
    RADIUS_AVP_TUNNEL_PASSWORD                = 69,
    RADIUS_AVP_TUNNEL_PRIVATE_GROUP_ID        = 81,
    RADIUS_AVP_TUNNEL_ASSIGNMENT_ID           = 82,
    RADIUS_AVP_TUNNEL_PREFERENCE              = 83,
    RADIUS_AVP_TUNNEL_CLIENT_AUTH_ID          = 90,
    RADIUS_AVP_TUNNEL_SERVER_AUTH_ID          = 91,

    /* Attribute types from the RFC 2869. */
    RADIUS_AVP_ACCT_INPUT_GIGAWORDS           = 52,
    RADIUS_AVP_ACCT_OUTPUT_GIGAWORDS          = 53,
    RADIUS_AVP_EVENT_TIMESTAMP                = 55,
    RADIUS_AVP_ARAP_PASSWORD                  = 70,
    RADIUS_AVP_ARAP_FEATURES                  = 71,
    RADIUS_AVP_ARAP_ZONE_ACCESS               = 72,
    RADIUS_AVP_ARAP_SECURITY                  = 73,
    RADIUS_AVP_ARAP_SECURITY_DATA             = 74,
    RADIUS_AVP_PASSWORD_RETRY                 = 75,
    RADIUS_AVP_PROMPT                         = 76,
    RADIUS_AVP_CONNECT_INFO                   = 77,
    RADIUS_AVP_CONFIGURATION_TOKEN            = 78,
    RADIUS_AVP_EAP_MESSAGE                    = 79,
    RADIUS_AVP_MESSAGE_AUTHENTICATOR          = 80,
    RADIUS_AVP_ARAP_CHALLENGE_RESPONSE        = 84,
    RADIUS_AVP_ACCT_INTERIM_INTERVAL          = 85,
    RADIUS_AVP_NAS_PORT_ID                    = 87,
    RADIUS_AVP_FRAMED_POOL                    = 88,

    /* Attribute types from the RFC 3162. */
    RADIUS_AVP_NAS_IPV6_ADDRESS               = 95,
    RADIUS_AVP_FRAMED_INTERFACE_ID            = 96,
    RADIUS_AVP_FRAMED_IPV6_PREFIX             = 97,
    RADIUS_AVP_LOGIN_IPV6_HOST                = 98,
    RADIUS_AVP_FRAMED_IPV6_ROUTE              = 99,
    RADIUS_AVP_FRAMED_IPV6_POOL               = 100,
    /* Attribute type from RFC4818 */
    RADIUS_AVP_DELEGATED_IPV6_PREFIX          = 123,

    /* Non-RFC attribute types, defined by IANA/radius. */
    RADIUS_AVP_ORIGINATING_LINE_INFO          = 94, /* [Trifunovic] */

    /* Attributes types from the RFC 2865. */
    RADIUS_AVP_EXPERIMENTAL_START             = 192,
    RADIUS_AVP_IMPLEMENTATION_SPECIFIC_START  = 224,
    RADIUS_AVP_RESERVED_START                 = 241
  ,
    /* Attribute types from the RFC 4372 */
    RADIUS_AVP_CHARGEABLE_USER_IDENTITY       = 89,

    /* Attribute type from the RFC 5176 */
    RADIUS_AVP_ERROR_CAUSE       = 101,
} radius_avp_type_t;

/* Values for Vendor-Type field of Vendor-Specific AVP */

typedef enum
{
    RADIUS_VENDOR_ID_NONE                    = 0,
    RADIUS_VENDOR_ID_MS                      = 311,
    RADIUS_VENDOR_ID_JUNIPER                 = 4874,
    RADIUS_VENDOR_ID_CISCO                   = 9,
    RADIUS_VENDOR_ID_3GPP                    = 10415,
    RADIUS_VENDOR_ID_WIMAXFORUM              = 24757, 
} radius_vendor_id_t;

/* Juniper Vendor-Specific RADIUS types. */
typedef enum
{
    RADIUS_VENDOR_JUNIPER_VR_NAME                 = 1,
    RADIUS_VENDOR_JUNIPER_IP_POOL_NAME            = 2,
    RADIUS_VENDOR_JUNIPER_PRIMARY_DNS             = 4,
    RADIUS_VENDOR_JUNIPER_SECONDARY_DNS           = 5,
    RADIUS_VENDOR_JUNIPER_PRIMARY_WINS            = 6,
    RADIUS_VENDOR_JUNIPER_SECONDARY_WINS          = 7,
    RADIUS_VENDOR_JUNIPER_TUNNEL_VIRTUAL_ROUTER   = 8,
    RADIUS_VENDOR_JUNIPER_TUNNEL_PASSWORD         = 9,
    RADIUS_VENDOR_JUNIPER_INGRESS_POLICY_NAME     = 10,
    RADIUS_VENDOR_JUNIPER_EGRESS_POLICY_NAME      = 11,
    RADIUS_VENDOR_JUNIPER_INGRESS_STATISTICS      = 12,
    RADIUS_VENDOR_JUNIPER_EGRESS_STATISTICS       = 13,
    RADIUS_VENDOR_JUNIPER_IGMP_ENABLE             = 23,
    RADIUS_VENDOR_JUNIPER_PPPOE_DESCRIPTION       = 24,
    RADIUS_VENDOR_JUNIPER_REDIRECT_VR_NAME        = 25,
    RADIUS_VENDOR_JUNIPER_PPPOE_URL               = 28,
    RADIUS_VENDOR_JUNIPER_TUNNEL_NAS_PORT_METHOD  = 30,
    RADIUS_VENDOR_JUNIPER_SERVICE_BUNDLE          = 31,
    RADIUS_VENDOR_JUNIPER_TUNNEL_MAX_SESSIONS     = 33,
    RADIUS_VENDOR_JUNIPER_FRAMED_IP_ROUTE_TAG     = 34,
    RADIUS_VENDOR_JUNIPER_INPUT_GIGAPACKETS       = 42,
    RADIUS_VENDOR_JUNIPER_OUTPUT_GIGAPACKETS      = 43,
    RADIUS_VENDOR_JUNIPER_TUNNEL_INTERFACE_ID     = 44,
    RADIUS_VENDOR_JUNIPER_IPV6_LOCAL_INTERFACE    = 46,
    RADIUS_VENDOR_JUNIPER_IPV6_PRIMARY_DNS        = 47,
    RADIUS_VENDOR_JUNIPER_IPV6_SECONDARY_DNS      = 48,
    RADIUS_VENDOR_JUNIPER_DISCONNECT_CAUSE        = 51,
    RADIUS_VENDOR_JUNIPER_RADIUS_CLIENT_ADDRESS   = 52,
    RADIUS_VENDOR_JUNIPER_L2TP_RCV_WINDOW_SIZE    = 54,
    RADIUS_VENDOR_JUNIPER_DHCP_OPTIONS            = 55,
    RADIUS_VENDOR_JUNIPER_DHCP_MAC_ADDRESS        = 56,
    RADIUS_VENDOR_JUNIPER_DHCP_GI_ADDRESS         = 57,
    RADIUS_VENDOR_JUNIPER_LI_ACTION               = 58,
    RADIUS_VENDOR_JUNIPER_INTERCEPTION_IDENTIFIER = 59,
    RADIUS_VENDOR_JUNIPER_MD_IP_ADDRESS           = 60,
    RADIUS_VENDOR_JUNIPER_MD_IP_PORT              = 61,
    RADIUS_VENDOR_JUNIPER_MLPPP_BUNDLE_NAME       = 62,
    RADIUS_VENDOR_JUNIPER_INTERFACE_DESCRIPTION   = 63,
    RADIUS_VENDOR_JUNIPER_TUNNEL_GROUP            = 64,
    RADIUS_VENDOR_JUNIPER_ACTIVATE_SERVICE        = 65,
    RADIUS_VENDOR_JUNIPER_DEACTIVATE_SERVICE      = 66,
    RADIUS_VENDOR_JUNIPER_SERVICE_VOLUME          = 67,
    RADIUS_VENDOR_JUNIPER_SERVICE_TIMEOUT         = 68,
    RADIUS_VENDOR_JUNIPER_SERVICE_STATISTICS      = 69,
    RADIUS_VENDOR_JUNIPER_IGNORE_DF_BIT           = 70,
    RADIUS_VENDOR_JUNIPER_IGMP_ACCESS_GROUP_NAME  = 71,
    RADIUS_VENDOR_JUNIPER_IGMP_ACCESS_SRC_GROUP_NAME  = 72,
    RADIUS_VENDOR_JUNIPER_MLD_ACCESS_GROUP_NAME   = 74,
    RADIUS_VENDOR_JUNIPER_MLD_ACCESS_SRC_GROUP_NAME  = 75,
    RADIUS_VENDOR_JUNIPER_MLD_VERSION             = 77,
    RADIUS_VENDOR_JUNIPER_IGMP_VERSION            = 78,
    RADIUS_VENDOR_JUNIPER_IP_MCAST_ADM_BW_LIMIT   = 79,
    RADIUS_VENDOR_JUNIPER_IPV6_MCAST_ADM_BW_LIMIT = 80,
    RADIUS_VENDOR_JUNIPER_MIP_ALGORITHM           = 84,
    RADIUS_VENDOR_JUNIPER_MIP_SPI                 = 85,
    RADIUS_VENDOR_JUNIPER_MIP_KEY                 = 86,
    RADIUS_VENDOR_JUNIPER_MIP_REPLAY              = 87,
    RADIUS_VENDOR_JUNIPER_MIP_LIFETIME            = 89,
    RADIUS_VENDOR_JUNIPER_L2TP_RESYNC_METHOD      = 90,
    RADIUS_VENDOR_JUNIPER_TUNNEL_SWITCH_PROFILE   = 91,
    RADIUS_VENDOR_JUNIPER_TUNNEL_TX_SPEED_METHOD  = 94,
    RADIUS_VENDOR_JUNIPER_IGMP_QUERY_INTERVAL     = 95,
    RADIUS_VENDOR_JUNIPER_IGMP_MAX_RESPONSE_TIME  = 96,
    RADIUS_VENDOR_JUNIPER_IGMP_IMMEDIATE_LEAVE    = 97,
    RADIUS_VENDOR_JUNIPER_MLD_QUERY_INTERVAL      = 98,
    RADIUS_VENDOR_JUNIPER_MLD_MAX_RESPONSE_TIME   = 99,
    RADIUS_VENDOR_JUNIPER_MLD_IMMEDIATE_LEAVE     = 100,
    RADIUS_VENDOR_JUNIPER_IPV6_INGRESS_POLICY_NAME= 106,
    RADIUS_VENDOR_JUNIPER_IPV6_EGRESS_POLICY_NAME = 107,
    RADIUS_VENDOR_JUNIPER_COS_PARAMETER_TYPE      = 108,
    RADIUS_VENDOR_JUNIPER_DHCP_GUIDED_RELAY_SERVER = 109,
    RADIUS_VENDOR_JUNIPER_IPV6_NDRA_PREFIX        = 129,
    RADIUS_VENDOR_JUNIPER_MAX_CLIENTS_PER_INTERFACE = 143,
    RADIUS_VENDOR_JUNIPER_COS_SCHEDULER_PARAMETER_TYPE      = 146,
    RADIUS_VENDOR_JUNIPER_FUF_PARAMETER_TYPE      = 148,
    RADIUS_VENDOR_JUNIPER_IPV6_INPUT_OCTETS       = 151,
    RADIUS_VENDOR_JUNIPER_IPV6_OUTPUT_OCTETS      = 152,
    RADIUS_VENDOR_JUNIPER_IPV6_INPUT_GIGAPACKETS  = 153,
    RADIUS_VENDOR_JUNIPER_IPV6_OUTPUT_GIGAPACKETS = 154,
    RADIUS_VENDOR_JUNIPER_IPV6_INPUT_GIGAWORDS    = 155,
    RADIUS_VENDOR_JUNIPER_IPV6_OUTPUT_GIGAWORDS   = 156,
    RADIUS_VENDOR_JUNIPER_IPV6_NDRA_POOL          = 157,
    RADIUS_VENDOR_JUNIPER_REDIR_GW_ADDR           = 175,
    RADIUS_VENDOR_JUNIPER_APN_NAME                = 176,
} radius_vendor_juniper_type_t;

typedef enum
{
    RADIUS_VENDOR_3GPP_IMSI                        = 1,
    RADIUS_VENDOR_3GPP_CHARGING_ID                 = 2,
    RADIUS_VENDOR_3GPP_PDP_TYPE                    = 3,
    RADIUS_VENDOR_3GPP_CG_ADDRESS                  = 4,
    RADIUS_VENDOR_3GPP_GPRS_NEG_QOS                = 5,
    RADIUS_VENDOR_3GPP_SGSN_ADDRESS                = 6,
    RADIUS_VENDOR_3GPP_GGSN_ADDRESS                = 7,
    RADIUS_VENDOR_3GPP_IMSI_MCC_MNC                = 8,
    RADIUS_VENDOR_3GPP_GGSN_MCC_MNC                = 9,
    RADIUS_VENDOR_3GPP_NSAPI                       = 10,
    RADIUS_VENDOR_3GPP_SESS_STOP_IND               = 11,
    RADIUS_VENDOR_3GPP_SELECTION_MODE              = 12,
    RADIUS_VENDOR_3GPP_CHARGING_CHAR               = 13,
    RADIUS_VENDOR_3GPP_IPV6_DNS_SERVERS            = 17,
    RADIUS_VENDOR_3GPP_SGSN_MCC_MNC                = 18,
    RADIUS_VENDOR_3GPP_IMEISV                      = 20,
    RADIUS_VENDOR_3GPP_RAT_TYPE                    = 21,
    RADIUS_VENDOR_3GPP_USER_LOC_INFO               = 22,
    RADIUS_VENDOR_3GPP_MS_TIMEZONE                 = 23,
    RADIUS_VENDOR_3GPP_PACKET_FILTER               = 25,
    RADIUS_VENDOR_3GPP_NEG_DSCP                    = 26,
    RADIUS_VENDOR_3GPP_ALLOCATE_IP_TYPE            = 27,
} radius_vendor_3gpp_type_t;

typedef enum
{
    RADIUS_VENDOR_MS_PRIMARY_DNS_SERVER            = 28,
    RADIUS_VENDOR_MS_SECONDARY_DNS_SERVER          = 29,
    RADIUS_VENDOR_MS_PRIMARY_NBNS_SERVER           = 30,
    RADIUS_VENDOR_MS_SECONDARY_NBNS_SERVER         = 31,
} radius_vendor_ms_type_t;

/* cisco Vendor-Specific RADIUS types. */
typedef enum
{
    RADIUS_VENDOR_CISCO_AVPAIR                  = 1,
} radius_vendor_cisco_type_t;

typedef enum {
    RADIUS_AVP_ACCT_STATUS_TYPE_START            = 1,
    RADIUS_AVP_ACCT_STATUS_TYPE_STOP             = 2,
    RADIUS_AVP_ACCT_STATUS_TYPE_INTERIM_UPDATE   = 3,
    RADIUS_AVP_ACCT_STATUS_TYPE_ACCOUNTING_ON    = 7,
    RADIUS_AVP_ACCT_STATUS_TYPE_ACCOUNTING_OFF   = 8
} radius_acct_status_type_t;

/* Values for RADIUS NAS-Port-Type attribute. */
typedef enum
{
  /* Attribute values from RFC 2865. */
    RADIUS_NAS_PORT_TYPE_ASYNC                 = 0,
    RADIUS_NAS_PORT_TYPE_SYNC                  = 1,
    RADIUS_NAS_PORT_TYPE_ISDN_SYNC             = 2,
    RADIUS_NAS_PORT_TYPE_ISDN_ASYNC_V120       = 3,
    RADIUS_NAS_PORT_TYPE_ISDN_ASYNC_V110       = 4,
    RADIUS_NAS_PORT_TYPE_VIRTUAL               = 5,
    RADIUS_NAS_PORT_TYPE_PIAFS                 = 6,
    RADIUS_NAS_PORT_TYPE_HDLC_CLEAR            = 7,
    RADIUS_NAS_PORT_TYPE_X25                   = 8,
    RADIUS_NAS_PORT_TYPE_X75                   = 9,
    RADIUS_NAS_PORT_TYPE_G3FAX                 = 10,
    RADIUS_NAS_PORT_TYPE_SDSL                  = 11,
    RADIUS_NAS_PORT_TYPE_ADSL_CAP              = 12,
    RADIUS_NAS_PORT_TYPE_ADSL_DMT              = 13,
    RADIUS_NAS_PORT_TYPE_ADSL_IDSL             = 14,
    RADIUS_NAS_PORT_TYPE_ETHERNET              = 15,
    RADIUS_NAS_PORT_TYPE_XDSL                  = 16,
    RADIUS_NAS_PORT_TYPE_CABLE                 = 17,
    RADIUS_NAS_PORT_TYPE_WIRELESS_OTHER        = 18,
    RADIUS_NAS_PORT_TYPE_WIRELESS_IEEE_802_11  = 19
} radius_nas_port_type_t;

/* Values for RADIUS Service-Type attribute */
typedef enum {
    /* Magic value yet not used in any spec */
    RADIUS_SERVICE_TYPE_NONE                    = 0,
    /* RFC 2865 */
    RADIUS_SERVICE_TYPE_LOGIN                   = 1,
    RADIUS_SERVICE_TYPE_FRAMED                  = 2,
    RADIUS_SERVICE_TYPE_CALLBACK_LOGIN          = 3,
    RADIUS_SERVICE_TYPE_CALLBACK_FRAMED         = 4,
    RADIUS_SERVICE_TYPE_OUTBOUND                = 5,
    RADIUS_SERVICE_TYPE_ADMINISTRATIVE          = 6,
    RADIUS_SERVICE_TYPE_NAS_PROMPT              = 7,
    RADIUS_SERVICE_TYPE_AUTHENTICATE_ONLY       = 8,
    RADIUS_SERVICE_TYPE_CALLBACK_NAS_PROMPT     = 9,
    RADIUS_SERVICE_TYPE_CALL_CHECK              = 10,
    RADIUS_SERVICE_TYPE_CALLBACK_ADMINISTRATIVE = 11,
} radius_service_type_t;

/* Values for RADIUS Framed-Protocol attribute */
typedef enum {
    /* Magic value not yet used in any specification */
    RADIUS_FRAMED_PROTOCOL_NONE                 = 0,
    /* RFC 2865 */
    RADIUS_FRAMED_PROTOCOL_PPP                  = 1,
    RADIUS_FRAMED_PROTOCOL_SLIP                 = 2,
    RADIUS_FRAMED_PROTOCOL_ARAP                 = 3,
    RADIUS_FRAMED_PROTOCOL_GANDALF              = 4,
    RADIUS_FRAMED_PROTOCOL_XYLOGICS_IPX_SLIP    = 5,
    RADIUS_FRAMED_PROTOCOL_X75_SYNC             = 6,
} radius_framed_protocol_type_t;

/* Success codes for packet attribute adding and retrieving. */
typedef enum
{
    /* The operation was successful. */
    RADIUS_AVP_STATUS_SUCCESS,

    /* The length of the attribute is too long.  This is returned when
       adding attributes to a packet. */
    RADIUS_AVP_STATUS_VALUE_TOO_LONG,

    /* Too many attributes for a radius packet.  There is a fixed amount
     of attributes (in bytes) that can be added for a single radius
     packet. */
    RADIUS_AVP_STATUS_TOO_MANY,

    /* No memory to add new attribute. */
    RADIUS_AVP_STATUS_OUT_OF_MEMORY,

    /* The AVP already exists in the requests and it may not be included
        more than once. */
    RADIUS_AVP_STATUS_ALREADY_EXISTS,

    /* The attribute was not found from the packet.  This is returned
        when fetching attributes from a reply packet. */
    RADIUS_AVP_STATUS_NOT_FOUND,

    /* Parse error in the packet.  This is returned when fetching 
         attributes from a reply packet. */
    RADIUS_AVP_STATUS_PARSE_ERROR,

    /* Last AVP while parsing */
    RADIUS_AVP_STATUS_LAST,
} radius_avp_status_t;

/* Values for RADIUS NAK Error Cause attribute */
typedef enum {
    RADIUS_ERROR_CAUSE_NONE                                   = 0,
    RADIUS_ERROR_CAUSE_MIN_ACK                                = 200, 
    RADIUS_ERROR_CAUSE_MAX_ACK                                = 299,
    RADIUS_ERROR_CAUSE_MIN_NAK                                = 400,
    RADIUS_ERROR_CAUSE_UNSUPPORTED_ATTRIBUTE                  = 401,
    RADIUS_ERROR_CAUSE_MISSING_ATTRIBUTE                      = 402,
    RADIUS_ERROR_CAUSE_NAS_IDENTIFICATION_MISMATCH            = 403,
    RADIUS_ERROR_CAUSE_INVALID_REQUEST                        = 404,
    RADIUS_ERROR_CAUSE_UNSUPPORTED_SERVICE                    = 405,
    RADIUS_ERROR_CAUSE_UNSUPPORTED_EXTENSION                  = 406,
    RADIUS_ERROR_CAUSE_INVALID_ATTRIBUTE_VALUE                = 407,
    RADIUS_ERROR_CAUSE_ADMINISTRATIVELY_PROHIBITED            = 501,
    RADIUS_ERROR_CAUSE_REQUEST_NOT_ROUTABLE                   = 502,
    RADIUS_ERROR_CAUSE_SESSION_CONTEXT_NOT_FOUND              = 503,
    RADIUS_ERROR_CAUSE_SESSION_CONTEXT_NOT_REMOVABLE          = 504,
    RADIUS_ERROR_CAUSE_OTHER_PROXY_PROCESSING_ERROR           = 505,
    RADIUS_ERROR_CAUSE_RESOURCES_UNAVAILABLE                  = 506,
    RADIUS_ERROR_CAUSE_REQUEST_INITIATED                      = 507,
    RADIUS_ERROR_CAUSE_MULTIPLE_SESSION_SELECTION_UNSUPPORTED = 508,
    RADIUS_ERROR_CAUSE_MAX_NAK                                = 599
} radius_nak_error_cause_t;

typedef enum {
    RADIUS_TERM_CAUSE_USER_REQUEST = 1,
    RADIUS_TERM_CAUSE_LOST_CARRIER,
    RADIUS_TERM_CAUSE_LOST_SERVICE,
    RADIUS_TERM_CAUSE_IDLE_TIMEOUT,
    RADIUS_TERM_CAUSE_SESSION_TIMEOUT,
    RADIUS_TERM_CAUSE_ADMIN_RESET,
    RADIUS_TERM_CAUSE_ADMIN_REBOOT,
    RADIUS_TERM_CAUSE_PORT_ERROR,
    RADIUS_TERM_CAUSE_NAS_ERROR,
    RADIUS_TERM_CAUSE_NAS_REQUEST,
    RADIUS_TERM_CAUSE_NAS_REBOOT,
    RADIUS_TERM_CAUSE_PORT_UNNEEDED,
    RADIUS_TERM_CAUSE_PORT_PREEMPTED,
    RADIUS_TERM_CAUSE_PORT_SUSPENDED,
    RADIUS_TERM_CAUSE_SERVICE_UNAVAILABLE,
    RADIUS_TERM_CAUSE_CALLBACK,
    RADIUS_TERM_CAUSE_USER_ERROR,
    RADIUS_TERM_CAUSE_HOST_REQUEST,
} radius_term_cause_t;

typedef struct radius_keyword_s
{
    const char *name;
    long code;
} radius_keyword_t;

/* Mapping between radius_avp_status_t and their names. */
extern const radius_keyword_t radius_avp_status_codes[];
typedef struct radius_client_request_s radius_client_request_t;

/* Status codes for radius client requests. */
typedef enum
{
    /* The request was successful. */
    RADIUS_CLIENT_REQ_SUCCESS,

    /* The request was malformed. */
    RADIUS_CLIENT_REQ_MALFORMED_REQUEST,

    /* Insufficient resources to perform the request. */
    RADIUS_CLIENT_REQ_INSUFFICIENT_RESOURCES,

    /* The request timed out. */
    RADIUS_CLIENT_REQ_TIMEOUT,

    /* The server reply was malformed. */
    RADIUS_CLIENT_REQ_MALFORMED_REPLY
} radius_client_request_status_t;

/* A callback function of this type is called to notify the status of
   the ssh_radius_client_request() function.  The argument `status'
   specifies the status of the request.  If the request was
   successful, the argument `reply_code' is valid and it specifies the
   type of the response packet.  The attributes of the response packet
   remain valid as long as the control remains in the callback
   function.  You can use the reply processing functions to fetch the
   attribute values from the response packet. */
typedef void (*radius_client_request_cb)(radius_client_request_status_t status,
                                         radius_client_request_t *request,
                                         radius_operation_code_t reply_code,
                                         void *context);

/*********************** Client Request Structure ***********************/

/* A radius client request object. */
struct radius_client_request_s
{
    /* The request packet. */
    uint8_t *request;
    uint32_t request_allocated;

    /* The number of bytes used from the request. */
    uint32_t request_used;
} __attribute__((packed));

typedef struct radius_hdr_s
{
    uint8_t opcode;
    uint8_t identifier;
    uint16_t length;
    uint8_t authenticator[16];
}  __attribute__((packed)) radius_hdr_t;

typedef struct radius_avp_header_s
{
    uint8_t   type;
    uint8_t   length;
    uint8_t   value[0];
}  __attribute__((packed)) radius_avp_header_t;

typedef struct radius_avp_vs_header_s
{
    uint8_t   type;
    uint8_t   length;
    uint32_t  vendor_id;
    uint8_t   vs_type;
    uint8_t   vs_length;
}  __attribute__((packed)) radius_avp_vs_header_t;

/***************************** Client requests ******************************/

/* Add a new attribute to the client request `request'.  The argument
   `type' specifies the type of the attribute and its value is given
   in the arguments `value', `value_len'.  The function returns an
   radius_avp_status_t which describes the success of the operation. */
radius_avp_status_t 
radius_client_request_add_attribute(radius_client_request_t *request,
                                    radius_avp_type_t type,
                                    const uint8_t *value,
                                    uint32_t value_len);

/* Add a new vendor specific attribute to the client request `request'.
   The 'vendor_id' parameter specifies the vendor-id'. The argument
   `vs_type' specifies the vendor-type of the attribute and
   its value  is given in the arguments `value', `value_len'. If 'vendor_id'
   is RADIUS_VENDOR_ID_NONE, then the call is equivalent to
   radius_client_request_add_attribute(request,vs_type,value,value_len).
   The function returns an radius_avp_status_t which describes the success
   of the operation. */
radius_avp_status_t
radius_client_request_add_vs_attribute(radius_client_request_t *request,
                                       radius_vendor_id_t vendor_id,
                                       uint8_t vs_type,
                                       const uint8_t *value,
                                       uint32_t value_len);

/* Mapping between radius_client_request_status_t and their names. */
extern const radius_keyword_t radius_client_request_status_codes[];

/************************ Reply processing functions ************************/

/* A structure for traversing the AVP's in RADIUS responses. Please do not
   access the fields in this structure directly. The definition of this
   structure is given explicitly so it can be placed on the stack for
   convenience. */
typedef struct radius_client_reply_enumerator_s
{
    /* The request we are associated with */
    uint8_t *req;

    /* The current type to enumerate. */
    radius_vendor_id_t vendor_selector;
    unsigned int type_selector;

    /* Current Vendor id */
    radius_vendor_id_t vendor_id;

    /* The current attribute enumerate position */
    uint32_t current_offset;

    /* The next attribute enumerate position. */
    uint32_t avp_offset;

    /* The endpoint of this list of AVP's in the
     request (offset of first byte not part of the list). */
    uint32_t current_length;

    /* Total length */
    uint32_t prev_length;
} radius_client_reply_enumerator_t;

void 
radius_client_reply_enumerate_init(radius_client_reply_enumerator_t *e,
                                   uint8_t *request, uint32_t len,
                                   unsigned int type);

radius_vendor_id_t
radius_client_reply_enumerate_get_vendor(radius_client_reply_enumerator_t *e);

bool
radius_client_reply_enumerate_subtypes(radius_client_reply_enumerator_t *e);

radius_avp_status_t 
radius_client_reply_enumerate_next(radius_client_reply_enumerator_t *e,
                                   radius_vendor_id_t *vendor_id_return,
                                   radius_avp_type_t *type_return,
                                   uint8_t **value_return,
                                   size_t *value_len_return);

/*********************** Information about attributes ***********************/

/* Attribute value types. */
/* If there is change in the below definitions, do update ipc_sm_af.h */
typedef enum
{
    /* UTF-8 encoded ISO 10646 characters. */
    RADIUS_AVP_VALUE_TEXT,

    /* Binary data. */
    RADIUS_AVP_VALUE_STRING,

    /* 32 bit address value, most significant octet first. */
    RADIUS_AVP_VALUE_ADDRESS,

    /* 128 bit address value, most significant octet first. */
    RADIUS_AVP_VALUE_IPV6_ADDRESS,

    /* 32 bit unsigned value, most significant octet first. */
    RADIUS_AVP_VALUE_INTEGER,

    /* 32 bit unsigned value, most significant octet first.  The value
     is seconds since 00:00:00 UTC, January 1, 1970. */
    RADIUS_AVP_VALUE_TIME,

    /* 8 bit tag followed by binary data. */
    RADIUS_AVP_VALUE_TAG_STRING,

    /* 8 bit tag followed by 32 bit unsigned value, most significant
     octet first. */
    RADIUS_AVP_VALUE_TAG_INTEGER,

    /* IPv6 Prefix */
    RADIUS_AVP_VALUE_V6_PREFIX
} radius_avp_value_type_t;

/* Information about attributes. */
typedef struct radius_avp_info_s
{
    /* The attribute type. */
    radius_avp_type_t type;

    /* The name of the attribute. */
    const char *name;

    /* The type of the attribute value. */
    radius_avp_value_type_t value_type;
} radius_avp_info_t;

typedef struct radius_vsa_info_rec_s
{
    /* The attribute type. */
    radius_vendor_id_t vendor_id;

    /* The name of the attribute. */
    const char *name;

    /* The type of the attribute value. */
    const radius_avp_info_t *value_table;
} radius_vsa_info_rec_t;

/* Trace function */
typedef void (*aaa_utils_trace_func_t)(void *trace_handle, u_int64_t flag,
                                       u_int32_t debug_level,
                                       const char *fmt, ...);
void
radius_hex_to_ascii (char *buff, int size, uint64_t num);

uint64_t
radius_ascii_to_hex (char *buff, int size);

uint32_t
radius_ascii_to_int(char *buff, int size);

/* Information about known attributes. */
extern const radius_avp_info_t radius_avp_info_table[];

/* Return information about attribute type `type'.  The function
   returns an information structure or NULL if the attribute type
   `type' is unknown. */
const radius_avp_info_t *radius_avp_info(radius_avp_type_t type);

/* Return information about attribute `name'.  The function returns an
   information structure or NULL if the attribute `name' is
   unknown. */
const radius_avp_info_t *radius_avp_info_name(const char *name);

/* Keyword table containing mapping between RADIUS codes and
   their names. */
extern const radius_keyword_t radius_operation_codes[];

/* Keyword table containing mapping from `Acct-Status-Type' names
   to their values. */
extern const radius_keyword_t radius_acct_status_types[];

/* Keyword table containing mapping from NAS-Port-Type AVP
   (RADIUS_NAS_PORT_TYPE_*) values to names. */
extern const radius_keyword_t radius_nas_port_types[];

/* Keyword table for mapping between Framed-Protocol AVP values
   and names */

extern const radius_keyword_t radius_framed_protocols[];

/* Keyword table for mapping between Service-Type AVP values
   and names */
extern const radius_keyword_t radius_service_types[];

extern int
radius_acct_reply_isok(radius_client_request_status_t status,
                       radius_client_request_t *request,
                       radius_operation_code_t reply_code);

extern void
radius_print_avp_info(void *trace_handle, radius_avp_type_t attr_type,
                      uint8_t *attr_value,
                      size_t attr_len,
                      radius_vendor_id_t vendor_id,
                      aaa_utils_trace_func_t trace_func);

const radius_avp_info_t*
radius_vsa_info(radius_vendor_id_t vendor_id,
                radius_avp_type_t type);

const radius_keyword_t* 
radius_ms_info (radius_avp_type_t type);

const char*
radius_find_keyword_name(const radius_keyword_t *keywords,
                         long code);

void
radius_free_request(radius_client_request_t *req);

void
radius_hexdump_detail(size_t offset, const char *avp_name,
                      radius_avp_type_t avp_type, const unsigned char *data,
                      size_t buf_siz);

void
radius_encode_utf8_char(char *dest, const char *src, uint16_t len);

void
radius_decode_utf8_char(uint8_t *dest, char *src, uint8_t len);

int
radius_asciiToUTF8(unsigned char* out, uint16_t *outlen,
                   const unsigned char* in, uint16_t *inlen);

int
radius_UTF8Toascii(unsigned char* out, uint16_t *outlen,
                   const unsigned char* in, uint16_t *inlen); 

#define radius_get_operation_code_str(code) \
    radius_find_keyword_name(radius_operation_codes, code)

#endif /* __RADIUS_H */

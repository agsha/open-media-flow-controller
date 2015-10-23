/*
 * $Id: diameter_avp_api.h 669489 2014-09-09 04:37:05Z pnallur $
 *
 * diameter_avp_api.h - Diameter AVP data structures
 *
 * Copyright (c) 2011-2013, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef __DIAMETER_AVP_API_H__
#define __DIAMETER_AVP_API_H__


#define DIAMETER_AVP_IS_HANDLE_VALID(__handle)	(__handle)

#define DIAMETER_MAX_ADDR_LEN       256
#define DIAMTER_PORTS_LIST_STR_LEN  512
#define DIAMETER_AVP_LEN_SIZE       3
#define DIAMETER_MAX_UTF8_SIZE      1024

/* diameter message handle used by application */
typedef struct diameter_avp_msg_s*  diameter_avp_message_handle;

/* payload handle used by application for reference purpose */
typedef struct diameter_avp_payload_s* diameter_avp_payload_handle;


/* generic avp value definition */
typedef struct diameter_avp_variant_s {
    uint8_t  *avp_header;       /* points to avp header */
    uint32_t avp_payload_len;   /* length of payload */

    diameter_avp_format_t   vt;
    union {
        struct _a_char {
            char *p;
        } a_char;

        struct _a_i8 {
            uint8_t *p;
        } a_i8;

        uint16_t v_i16;

        int32_t v_si32;

        int64_t v_si64;

        uint32_t v_i32;

        uint64_t v_i64;

        /* grouped avp handle */
        diameter_avp_payload_handle grouped_avp;
    } data;
} diameter_avp_variant_t;

/* derived data types format */

typedef enum diameter_str_encoding_type_s {
    DIAMETER_ENC_ASCII = 1,
    DIAMETER_ENC_UCS2,
    DIAMETER_ENC_UCS4
} diameter_str_encoding_type_t;

typedef struct diameter_str_s {
    diameter_str_encoding_type_t enc_type;
    uint8_t     *str;
    uint32_t    len;
    uint8_t     buffer[DIAMETER_MAX_UTF8_SIZE];
} diameter_str_t;

typedef enum diameter_transport_s {
    DIAMETER_SCTP_TRANS_PROTO = 0, /* default */
    DIAMETER_TCP_TRANS_PROTO,
    DIAMETER_UDP_TRANS_PROTO,
    DIAMETER_TRANSPORT_MAX
} diameter_transport_t;

typedef enum diameter_protocol_s {
    DIAMETER_DIAMETER_PROTO = 0, /* default */
    DIAMETER_RADIUS_PROTO,
    DIAMETER_TACACS_PLUS,
    DIAMETER_PROTO_MAX
} diameter_protocol_t;

#define DIAMETER_FQDN_MAX_LEN   256

typedef struct diameter_uri_s {
    char        fqdn[DIAMETER_FQDN_MAX_LEN];
    uint32_t    fqdn_len;
    uint8_t     field_presence_mask;
#define DIAMETER_AVP_PORT_MASK          0x01
#define DIAMETER_AVP_TRANSPORT_MASK     0x02
#define DIAMETER_AVP_PROTOCOL_MASK      0x04
    uint32_t    port;
    diameter_transport_t tranport;
    diameter_protocol_t  proto;
} diameter_uri_t;

typedef struct diameter_filter_options_s {
    uint8_t     presence_mask;
/*
 * Match if the packet is a fragment and this is notthe first fragment
 * of the datagram.  frag may notbe used in conjunction with either
 * tcpflags or TCP/UDP port specifications
 */
#define DIAMETER_FILTER_OPTION_FRAG         0x01
#define DIAMETER_FILTER_OPTION_IP           0x02
#define DIAMETER_FILTER_OPTION_TCP          0x04
/* TCP packets only, Match packets that have the RST or ACK bits set.*/
#define DIAMETER_FILTER_OPTION_ESTABLISHED  0x10
/* TCP packets only, Match packets that have the SYN bit set but no ACK bit */
#define DIAMETER_FILTER_OPTION_SETUP        0x20
#define DIAMETER_FILTER_OPTION_TCP_FLAGS    0x40
#define DIAMETER_FILTER_OPTION_ICMP_TYPES   0x80


    uint8_t     ip_options;
#define DIAMETER_IP_OPTION_STRICT_SOURCE_ROUTE  0x01
#define DIAMETER_IP_OPTION_LOOSE_SOURCE_ROUTE   0x02
#define DIAMETER_IP_OPTION_RECORD_PACKET_ROUTE  0x04
#define DIAMETER_IP_OPTION_TIMESTAMP            0x08
    uint8_t     tcp_options;
#define DIAMETER_TCP_OPTION_MAX_SEGMENT_SIZE    0x01
#define DIAMETER_TCP_OPTION_TCP_WINDOW_ADV      0x02
#define DIAMETER_TCP_OPTION_SELECTIVE_ACK       0x04
#define DIAMETER_TCP_OPTION_TIMESTAMP           0x08
#define DIAMETER_TCP_OPTION_CONNECTION_COUNT    0x10
    uint8_t     tcp_flags;
#define DIAMETER_TCP_FLAGS_FIN                  0x01
#define DIAMETER_TCP_FLAGS_SYN                  0x02
#define DIAMETER_TCP_FLAGS_PSH                  0x04
#define DIAMETER_TCP_FLAGS_ACK                  0x08
#define DIAMETER_TCP_FLAGS_URG                  0x10
/* ICMP packets only */
    uint32_t    icmp_types;
#define DIAMETER_ICMP_TYPE_ECHO_REPLY           0x0001
#define DIAMETER_ICMP_TYPE_DEST_UNREACHABLE     0x0002
#define DIAMETER_ICMP_TYPE_SOURCE_QUENCH        0x0004
#define DIAMETER_ICMP_TYPE_REDIRECT             0x0008
#define DIAMETER_ICMP_TYPE_ECHO_REQUEST         0x0010
#define DIAMETER_ICMP_TYPE_ROUTER_ADV           0x0020
#define DIAMETER_ICMP_TYPE_ROUTER_SOLICITATION  0x0040
#define DIAMETER_ICMP_TYPE_TTL_EXCEEDED         0x0080
#define DIAMETER_ICMP_TYPE_IP_HEADER_BAD        0x0100
#define DIAMETER_ICMP_TYPE_TIMESTAMP_REQUEST    0x0200
#define DIAMETER_ICMP_TYPE_TIMESTAMP_REPLY      0x0400
#define DIAMETER_ICMP_TYPE_INFO_REQUEST         0x0800
#define DIAMETER_ICMP_TYPE_INFO_REPLY           0x1000
#define DIAMETER_ICMP_TYPE_ADDRESS_MASK_REQUEST 0x2000
#define DIAMETER_ICMP_TYPE_ADDRESS_MASK_REPLY   0x4000

} diameter_ip_filter_options_t;


typedef enum diameter_filter_action_s {
    DIAMETER_PERMIT = 0,
    DIAMETER_DENY
} diameter_ip_filter_action_t;

typedef enum diameter_filter_dir_s {
    DIAMETER_DIR_IN = 0,
    DIAMETER_DIR_OUT
} diameter_dir_t;

typedef struct diameter_ip_filter_s {
    /**
     * An IPv4 or IPv6 number in dotted- quad or
     * canonical IPv6 form.  Only this exact IP
     * number will match the rule. ipno/bits  An
     * IP number as above with a mask width of
     * the form 1.2.3.4/24.
    **/
    char        address[DIAMETER_MAX_ADDR_LEN];
    /*{port/port-port}[,ports[,...]]*/
    char        ports[DIAMTER_PORTS_LIST_STR_LEN];
} diameter_ip_filter_t;

typedef struct diameter_ip_filter_rule_s {
    diameter_ip_filter_action_t     action;
    diameter_dir_t                  dir;
    /** IP protocol number as defined in RFC 5237,
     * '-1' for 'any' protocol
     **/
    int16_t                         proto;
    diameter_ip_filter_t            src_ip_filter;
    diameter_ip_filter_t            dst_ip_filter;
    diameter_ip_filter_options_t    options; /* ignored currently */

} diameter_ip_filter_rule_t;

typedef enum diameter_qos_filter_action_s {
    DIAMETER_QOS_FILTER_TAG = 0,
    DIAMETER_QOS_FILTER_METER
} diameter_qos_filter_action_t;


typedef struct diameter_qos_filter_rule_s {
    diameter_qos_filter_action_t    action;
    diameter_dir_t          filter_dir;
    uint8_t                 proto; /* IP protocol number */
    /**
        ipno    An IPv4 or IPv6 number in dotted-
                quad or canonical IPv6 form.  Only
                this exact IP number will match the
                rule.
                ipno/bits  An IP number as above with a mask
                width of the form 1.2.3.4/24.
    **/
    diameter_ip_filter_t    src_ip_filter;
    diameter_ip_filter_t    dst_ip_filter;
} diameter_qos_filter_rule_t;

typedef enum diameter_avp_iterate_action_e {
    DIAMETER_AVP_ACTION_CONTINUE = 0,
    DIAMETER_AVP_ACTION_CANCEL
} diameter_avp_iterate_action_t;

typedef diameter_avp_iterate_action_t
        (*diameter_avp_cb_next_avp_t)(void                          *context,
                                      uint32_t                       ordinal,
                                      diameter_avp_variant_t        *value,
                                      diameter_avp_attribute_info_t *info,
                                      uint32_t                       flags);


/*
 * @brief
 * Initialize the Diameter AVP dictionary function table.
 * @param [in] function_table
 *             Dictionary function table of memory callbacks.
 * @param [in] ordinal_max
 *             Dictionary ordinal's maximum value.
 *
 * @return int
 *          valid values:   DIAMETER_AVP_SUCCESS, DIAMETER_AVP_
 */
int diameter_avp_lib_init(diameter_avp_cb_table_t *function_table, uint32_t ordinal_max);

/**
 * @brief
 * Parse the given diameter message buffer message header and payload based on
 * given dictionary
 *
 * @param [in] hDictionary
 * 		dictionary handle defining the schema of the given message
 * @param [in] data
 * 		data buffer pointer pointing to diameter message buffer
 * @param [in] length
 *		data buffer length
 * @param [in] flags
 *		flags controlling the parsing
 * @param [out] pMessage
 *		handle to the message
 * @param [in/out] pError_handle
 *		contains the failed avps list based on error according to the RFC
 *
 * @return int
 *      result code based on 'diameter_result_code_enum'
 */
int diameter_avp_parse_message (diameter_avp_dict_handle hDictionary,
                                uint8_t* data, uint32_t length, uint32_t nFlags,
                                diameter_avp_message_handle* pMessage,
                                diameter_avp_payload_handle *pError_handle);


/**
 * @brief
 * Formats previously parsed AVPs into data buffer
 *
 * @param[in]  h
 *      Diameter AVP payload handle
 *
 * @return int
 *      result code based on 'diameter_result_code_enum'
 */
int
diameter_avp_reformat_message(diameter_avp_message_handle h);

/**
 * @brief
 * Parse the given radius message buffer message header and payload based on
 * given dictionary
 *
 * @param [in] hDictionary
 *      dictionary handle defining the schema of the given message
 * @param [in] data
 *      data buffer pointer pointing to radius message buffer
 * @param [in] length
 *      data buffer length
 * @param [in] flags
 *      flags controlling the parsing
 * @param [out] pMessage
 *      handle to the message
 *
 * @return int
 *      result code based on 'diameter_result_code_enum'
 */
int
diameter_avp_parse_radius_message (diameter_avp_dict_handle parse_dictionary,
                                   uint8_t* data, uint32_t length,
                                   uint32_t nFlags,
                                   diameter_avp_data_handle buffer_handle,
                                   diameter_avp_dict_handle format_dictionary,
                                   diameter_avp_message_handle *p_msg_handle);
/**
 * @brief
 * Returns the messsage header info of a given parsed message
 * @param [in] hMessage
 *		handle to the message
 * @param [out] p_msg_hdr
 *		pointer to the message header
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE
 */

int diameter_avp_get_message_info (diameter_avp_message_handle h_msg,
		diameter_message_info_t **p_msg_hdr);

/**
 * @brief
 * Returns the messsage header info of a given parsed message
 * @param [in] data
 *      pointer to the encoded data buffer
 * @param [in] length
 *      length of the buffer
 * @param [in/out] p_msg_hdr
 *      pointer to the message header
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:   DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_VAL
 */

int diameter_avp_parse_message_header (const uint8_t *data, uint32_t length,
                                   diameter_message_info_t *p_msg_hdr);

/**
 * @brief
 * Returns the payload handle of a given parsed message
 * @param [in] hMessage
 *		handle to the message
 * @param [out] p_payload
 *		pointer to payload handle
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE
 */

int diameter_avp_message_get_payload (diameter_avp_message_handle h_msg,
	diameter_avp_payload_handle *h_payload);

/**
 * @brief
 * Returns the number of avp instances in a given parsed payload every grouped
 * avp within a payload is considered a single avp instance
 *
 * @param [in] h_payload
 *		handle to the payload
 * @param [out] p_size
 *		number of avps
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE
 */

int diameter_avp_payload_get_number_of_avps (diameter_avp_payload_handle h_payload,
		uint16_t *p_size);

/**
 * @brief
 * Returns the number of avp instances for a given avp in a given 
 * parsed payload every grouped avp within a payload is considered 
 * a single avp instance
 *
 * @param [in] h_payload
 *      handle to the payload
 * @param [out] p_size
 *      number of avps
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:   DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 *                      DIAMETER_AVP_FAIL
 */

int
diameter_avp_payload_get_number_of_avps_by_ordinal (diameter_avp_payload_handle h_payload,
                                uint32_t ordinal, uint16_t* p_size);

/**
 * @brief
 * Returns the avp information at the given index in the parsed payload
 *
 * @param [in] h_payload
 *		handle to the payload
 * @param [in] n_index
 *		avp index
 * @param [out] p_size
 *		number of avps
 * @param [out] p_ordinal
 *		ordinal number of the avp at the given index
 * @param [out] p_attrib_info
 *		meta data information of the attribute retrieved from the dictionary if present
 * @param [out] p_attrib_val
 *		attribute value
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 *						DIAMETER_AVP_INVALID_INDEX
 */

int diameter_avp_payload_get_avp (diameter_avp_payload_handle hPacket,
                                  uint16_t n_index,
                                  uint32_t *p_ordinal,
                                  diameter_avp_variant_t **p_attrib_val);

/**
 * @brief
 * Returns the avp information at the given index for a given ordinal in the
 * parsed payload
 *
 * @param [in] h_payload
 *		handle to the payload
 * @param [in] ordinal
 *		ordinal number of the required attribute
 * @param [in] n_index
 *		avp index within the list of avp instances of given ordinal
 * @param [out] p_attrib_val
 *		attribute value
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 *						DIAMETER_AVP_INVALID_INDEX
 */

int diameter_avp_payload_get_avp_by_ordinal (diameter_avp_payload_handle hPacket,
		                                uint32_t ordinal, uint16_t n_index,
                                        diameter_avp_variant_t **p_attrib_val);

/**
 * @brief
 * This function facilitates diameter payload (either for Diameter message or
 * Grouped AVP) formatting by creating a virtual packet.
 *
 * @param [out] h_payload
 *		handle to the payload
 * @param [in] h_dictionary
 * 		dictionary handle defining the schema of the given message
 * @param [in] h_data
 * 		data handle where message is formed, transparent to the avp lib
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_MEM_FAILURE,
 */

int diameter_avp_create_payload (diameter_avp_payload_handle* h_payload,
                                 diameter_avp_message_handle h_message);

/**
 * @brief
 * This function facilitates diameter message formatting by creating a virtual
 * message.
 *
 * @param [in] p_msg_hdr
 *		pointer to the message header
 * @param [out] h_message
 *		handle to the message
 * @param [in] h_dictionary
 * 		dictionary handle defining the schema of the given message
 * @param [in] h_data
 * 		data handle where message is formed, transparent to the avp lib
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 */


int diameter_avp_create_message (diameter_message_info_t* p_msg_hdr,
		                        diameter_avp_message_handle* h_message, 
                                diameter_avp_dict_handle h_dictionary, 
                                diameter_avp_data_handle h_data);

/**
 * @brief
 * This function must be called at the end of  diameter message formatting
 * to set proper length of the message.
 *
 * @param [out] h_message
 *      handle to the message
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:   DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 */


int diameter_avp_finalize_message (diameter_avp_message_handle h_message);

/**
 * @brief
 * This function releases the given handle. The handle will become invalid and
 * all memory associated with that handle will be deallocated.
 *
 * @param [in] h_message
 *      handle to the message
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 */

int diameter_avp_message_release (diameter_avp_message_handle h_message);

/**
 * @brief
 * This function releases the given handle. If the payload is not referenced
 * by a message (after diameter_avp_message_set_payload  call) then the handle
 * will become invalid and all memory associated with that handle is deallocated.
 *
 * @param [in] h_payload
 *      handle to the payload
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 */

int diameter_avp_payload_release (diameter_avp_payload_handle h_payload);

/**
 * @brief
 * This function inline formats the attribute data to the packet.
 *
 * @param [in] h_payload
 *      handle to the payload
 * @param [in] ordinal
 *      ordinal number of the required attribute
 * @param [in] p_attrib_val
 *      attribute value
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 *                      DIAMETER_AVP_UNKNOWN_AVP
 */

int diameter_avp_payload_add_avp (diameter_avp_payload_handle hPacket,
                                  uint32_t ordinal, diameter_avp_variant_t*
                                  p_attrib_val);

/**
 * @brief
 * This function deletes previously parsed attribute instance
 *
 * @param [in] h_payload
 *      handle to the payload
 * @param [in] ordinal
 *      ordinal number of the required attribute
 * @param [in] n_index
 *      avp index within the list of avp instances of given ordinal
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 *                      DIAMETER_AVP_UNKNOWN_AVP, DIAMETER_AVP_INVALID_INDEX
 */

int diameter_avp_payload_delete_avp (diameter_avp_payload_handle h,
                                     uint32_t ordinal, uint16_t nIndex);

/**
 * @brief
 * This function assumes that given avp data is in encoded form including
 * both header and the payload and copies the same directly into the given
 * payload. Encoded data given for attribute is assumed to be available
 * in flat memory
 *
 * @param [in] h_payload
 *      handle to the payload
 * @param [in] ordinal
 *      ordinal number of the attribute
 * @param [in] p_data
 *      encoded data for attribute having both header and payload in
 *      flat memory
 * @param [in] length
 *      length of the attribute data
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:   DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 *                      DIAMETER_AVP_UNKNOWN_AVP, DIAMETER_AVP_INVALID_VAL
 */

int diameter_avp_payload_copy_avp (diameter_avp_payload_handle h_payload, 
                                   uint32_t ordinal, const uint8_t *p_data, 
                                   const uint32_t length);
/**
 * @brief
 * This function appends Grouped attribute data represented as a packet handle
 * to the packet
 *
 * @param [in] h_parent_payload
 *      handle to the parent payload to which grouped avp be appended
 * @param [in] ordinal
 *      ordinal number of the required attribute
 * @param [in] h_new_payload
 *      handle to the payload of grouped avp
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 *                      DIAMETER_AVP_UNKNOWN_AVP
 */

int diameter_avp_payload_add_grouped_avp (diameter_avp_payload_handle 
                            h_parent_payload, uint32_t ordinal, 
                            diameter_avp_payload_handle h_new_payload);

/**
 * @brief
 * This function overrides Grouped attribute data
 *
 * @param [in] h_parent_payload
 *      handle to the parent payload
 * @param [in] ordinal
 *      ordinal number of the required attribute
 * @param [in] n_index
 *      avp index within the list of avp instances of given ordinal
 * @param [in] h_new_payload
 *      handle to the payload of grouped avp
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 *                      DIAMETER_AVP_UNKNOWN_AVP
 */

int diameter_avp_payload_set_grouped_avp (diameter_avp_payload_handle h_parent_payload,
		                    uint32_t ordinal, uint16_t n_index, 
                            diameter_avp_payload_handle h_new_payload);

/**
 * @brief
 * This function starts in-place formatting Grouped attribute data to avoid
 * memory copy from one payload to another.
 *
 * @param [in] h_payload
 *      handle to the parent payload
 * @param [in] ordinal
 *      ordinal number of the required attribute
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 *                      DIAMETER_AVP_UNKNOWN_AVP
 */

int diameter_avp_payload_start_grouped_avp (diameter_avp_payload_handle h_payload,
                                            uint32_t ordinal);
/**
 * @brief
 * This function ends in-place formatting Grouped attribute data.
 *
 * @param [in] h_payload
 *      handle to the parent payload
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 */

int diameter_avp_payload_end_grouped_avp (diameter_avp_payload_handle   hPacket,
                                          uint32_t                      flags);

/**
 * @brief
 * This function returns parent for the given payload to assist recursive
 * algorithms of parsing formatting Grouped AVPs.
 *
 * @param [in] h_payload
 *      handle to the parent payload
 * @param [in] h_parent_payload
 *      handle to the parent payload
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_INVALID_HANDLE,
 */

int diameter_avp_payload_get_parent (diameter_avp_payload_handle h_payload,
                              diameter_avp_payload_handle* h_parent_payload);


/**
 * @brief
 * This function iterates through the diameter encoded message stored in iovec
 * and returns the avp instance in order 
 *
 * @param [in] num_iovecs
 *      number of iovec objects chained together
 * @param [in] data
 *      encoded diameter message stored in iovec vector
 * @param [in] dictionary
 *      attribute schema
 * @param [in] fp
 *      application callback function for printing the parsed attribute in order       
 * 
 */   
int diameter_avp_iterate_msg (uint32_t num_iovecs,
                               struct iovec* data,
                               diameter_avp_dict_handle dictionary,
                               void* context,
                               diameter_avp_cb_next_avp_t fp);

/**
 * @brief
 * This function iterates through RADIUS encoded message
 * and returns the avp instance in order
 *
 * @param [in] data
 *      RADIUS message
 * @param [in] length
 *      length of RADIUS message
 * @param [in] dictionary
 *      attribute schema
 * @param [in] fp
 *      application callback function for printing the parsed attribute in order
 *
 */

int
diameter_avp_iterate_radius_msg (uint8_t                    *data,
                                 uint32_t                    length,
                                 diameter_avp_dict_handle    dictionary,
                                 void                       *context,
                                 diameter_avp_cb_next_avp_t  fp);

/* derived data encoding/decoding APIs */

/**
 * @brief
 * This function encodes the ip address compliant to RFC 3588
 *
 * @param [in] addr
 *	IPv4/v6 address in user data format
 * @param [out] dest
 *	where formatted text is stored, could be the field in
 *	'diameter_avp_variant_t'
 * @param [out] size
 *	size of the formatted value
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */

int diameter_avp_encode_ip_address(const ipvx_addr_t *addr, uint8_t *dest, 
                                   uint32_t	*size);

/**
 * @brief
 * This function decodes the ip address compliant to RFC 3588
 *
 * @param [in] src
 *	where formatted text is stored, field in 'diameter_avp_variant_t'
 * @param [in] size
 *	size of the formatted value, field in 'diameter_avp_variant_t'
 * @param [out] addr
 *	IPv4/v6 address in user data format
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */

int diameter_avp_decode_ip_address(const uint8_t *src, uint8_t size, 
                                   ipvx_addr_t *addr);

/**
 * @brief
 * This function encodes the time compliant to RFC 3588
 *
 * @param [in] time
 *	time in user data format
 * @param [out] f_time
 *	where formatted time is stored, could be the field in 'diameter_avp_variant_t'
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */

int diameter_avp_encode_time(time_t ntime, uint32_t *f_time);

/**
 * @brief
 * This function decodes the time compliant to RFC 3588
 *
 * @param [in]  f_time
 *	where formatted time is stored, field in 'diameter_avp_variant_t'
 * @param [out] time
 *	time in user data format
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */
int diameter_avp_decode_time(const uint32_t *f_time, time_t* ntime);


/**
 * @brief
 * This function encodes the ASCII/UCS2/UCS4 to utf-8 format
 *
 * @param [in] str
 *	string in user data format
 * @param [out] utf8_str
 *	where formatted utf8 str is stored, could be the field in
 *	'diameter_avp_variant_t'
 * @param [out] len
 *	size of the utf8 string
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */

int diameter_avp_encode_utf8 (uint8_t *input_str, uint32_t len,
                              diameter_str_t *str);

/**
 * @brief
 * This function decodes the utf-8 format to ASCII/UCS2/UCS4
 *
 * @param [in] utf8_str
 *	where formatted text is stored, field in 'diameter_avp_variant_t'
 * @param [in] len
 *	size of the utf8 string, field in 'diameter_avp_variant_t'
 * @param [in/out] str
 *	Idata user specified character format
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */

int diameter_avp_decode_utf8 (uint8_t *utf8_str, uint32_t len,
                              diameter_str_t *str);

/**
 * @brief
 * This function encodes diameter Uri user data format to the one defined in
 *  RFC 3588
 *
 * @param [in] uri_info
 *	string in user data format
 * @param [out] dest
 *	where formatted data is stored, could be the field in
 *	'diameter_avp_variant_t'
 * @param [out] len
 *	size of the formatted uri
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */

int diameter_avp_encode_diameter_uri (const diameter_uri_t *uri_info, 
                                      uint8_t *dest, uint32_t *len);


/**
 * @brief
 * This function decodes string to diameter Uri user data format according 
 * to the one defined in RFC 3588
 *
 * @param [in] src
 *	formatted diameter uri pointing to field in 'diameter_avp_variant_t'
 * @param [out] len
 *	where formatted data is stored, could be the field in
 *	'diameter_avp_variant_t'
 * @param [out] uri_info
 *	URI information in user data format
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */

int diameter_avp_decode_diameter_uri (const uint8_t *src, uint32_t len, 
                                      diameter_uri_t *uri_info);

/**
 * @brief
 * This function encodes diameter ip filter rule user data format to the 
 * one defined in RFC 3588
 *
 * @param [in] filter_rule
 *	string in user data format
 * @param [out] dest
 *	where formatted data is stored, could be the field in
 *	'diameter_avp_variant_t'
 * @param [out] len
 *	size of the formatted filter rule
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */


int diameter_avp_encode_diameter_ip_filter_rule (const diameter_ip_filter_rule_t 
                                    *filter_rule, uint8_t *dest, uint32_t *len);

/**
 * @brief
 * This function decodes string to diameter ip filter rule user data format
 * defined in RFC 3588
 *
 * @param [in] dest
 *	formatted diameter ip filter rule pointing to field in 'diameter_avp_variant_t'
 *  to octet string
 * @param [in] len
 *	where formatted data is stored, could be the field in
 *	'diameter_avp_variant_t'
 * @param [out] filter_rule
 *	ip filter rule in user data format
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */

int diameter_avp_decode_diameter_ip_filter_rule (uint8_t *dest, uint32_t len,
                                   diameter_ip_filter_rule_t *filter_rule);

/**
 * @brief
 * This function encodes diameter qos filter rule user data format to the one defined in
 *  RFC 3588
 *
 * @param [in] filter_rule
 *	string in user data format
 * @param [out] dest
 *	where formatted data is stored, could be the field in
 *	'diameter_avp_variant_t' an octet string
 * @param [out] len
 *	size of the formatted filter rule
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */


int diameter_avp_encode_diameter_qos_filter_rule (const diameter_qos_filter_rule_t 
                           *filter_rule, uint8_t *dest, uint32_t *len);

/**
 * @brief
 * This function decodes string to diameter qos filter rule user data format
 * defined in RFC 3588
 *
 * @param [in] dest
 *	formatted diameter qos filter rule pointing to field in 'diameter_avp_variant_t'
 * @param [in] len
 *	where formatted data is stored, could be the field in
 *	'diameter_avp_variant_t'
 * @param [out] filter_rule
 *	qos filter rule in user data format
 *
 * @return int
 *      returns the result of the operation based on 'diameter_avp_rc_t'
 *      valid values:	DIAMETER_AVP_SUCCESS, DIAMETER_AVP_FAIL
 */

int diameter_avp_decode_diameter_qos_filter_rule (const uint8_t *dest, uint32_t len,
                                  diameter_qos_filter_rule_t *filter_rule);

/**
 * @brief
 * This function checks if the attribute is present in the payload. It only keeps track
 * of the attribute presence at the first level
 *
 * @param [in] h_payload
 *      handle to the parent payload
 * @param [in] ordinal
 *      ordinal number of the attribute
 *
 * @return int
 *      returns the result if the attribute is present
 */
bool diameter_avp_attribute_present(diameter_avp_payload_handle payload, uint32_t ordinal);

void diameter_avp_vty_unit_test_handler (int action);


/**
 * @brief
 * This function sets the packet identifier field
 * in diameter message info. Used by radius proxy
 *
 * @param [in] h_payload
 *      handle to the parent payload
 * @param [in] pkt_id
 *      radius packet identifier
 *
 * @return: None
 */
void
diameter_avp_set_radius_pkt_id(diameter_avp_message_handle h,
                               u_int32_t  e_to_e_id);

int diameter_avp_get_data_offset (diameter_avp_message_handle h,
                                  uint32_t *data_offset);


#endif /* __DIAMETER_AVP_API_H__ */

/* File : mime_header.h
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

#ifndef _MIME_HEADER_H_
#define _MIME_HEADER_H_
/** 
 * @file mime_header.h
 * @brief Common Data structure definitons for mime headers [Derived from http header implementation]
 * @author 
 * @version 1.00
 * @date 2009-09-03
 */

#include <sys/param.h>
#include <sys/types.h>
#ifndef  PROTO_HTTP_LIB
#include "nkn_attr.h"
#endif /* !PROTO_HTTP_LIB */
#include <uuid/uuid.h>
#include <atomic_ops.h>
#include "header_datatype.h"

#define MIME_MAX_HDR_DEFS 128

/** 
 * @brief MIME Header type definition
 * Common type (int) for HTTP/RTSP or any other MIME extention
 */
//typedef mime_header_id_t int;
typedef int mime_header_id_t;


/** 
 * @brief MIME Header descriptor
 * Used to describe HTTP/RTSP Header tables
 */
typedef struct mime_header_descriptor_t {
    const char *name;
    int namelen;
    mime_header_id_t id;
    mime_hdr_datatype_t type;
    int comma_hdr;
} mime_header_descriptor_t;

/** 
 * @brief Name Value Descriptor 
 *
 */
typedef struct name_value_descriptor {
    const char *name;
    int id;
    int has_value;
    mime_hdr_datatype_t type;
} name_value_descriptor_t;

/**
 * @brief Mime version descriptor
 * used for object versioning
 */
typedef struct version_counter {
    union {
       struct {
           uuid_t incarnation;
           AO_t counter;
       } t_ver;
       char fin_version[sizeof(uuid_t) + sizeof(AO_t)];
    } ver;
} version_counter_t;

/**
 * @brief Define supported protocol types
 */
typedef enum mime_protocol_t {
    MIME_PROT_HTTP = 0x00,
    MIME_PROT_RTSP,

    MIME_PROT_MAX
} mime_protocol_t;


/* TODO - Version
 */

#define MIME_HEADER_VERSION 0x2
#define MIME_HEADER_HEAPSIZE (sizeof(heap_data_t) * 64)


/**
 * @brief MIME Header
 */
typedef struct mime_header {
    /*
     ***************************************************************************
     * Note: Changes to this structure will require corresponding changes
     *       to serial_mime_header_t along with the associated 
     *       serialize/deserialize interfaces.
     ***************************************************************************
     */
    /* type - specifies the type of protocol dealing with */
    dataddr_t known_header_map[MIME_MAX_HDR_DEFS];
    mime_protocol_t protocol;
    int32_t version;

    dataddr_t pad[6];
    uint16_t cnt_known_headers;
    uint16_t cnt_unknown_headers;
    uint16_t cnt_namevalue_headers;
    uint16_t cnt_querystring_headers;
    dataddr_t unknown_header_head;
    dataddr_t query_string_head;
    dataddr_t name_value_head;

    char heap[MIME_HEADER_HEAPSIZE];
    int32_t heap_free_index;
    int32_t heap_frees;

    char *heap_ext;		/* free() on destruction */
    int32_t heap_ext_size;

    const char *ext_data;
    int32_t ext_data_size;
} mime_header_t;

#define SERIAL_MIME_HEADER_MAGIC 0x20095678

typedef struct serial_mime_header {
    int32_t magic;
    int32_t total_size;		/* size(serial_mime_header)+
    				 *   sparse known header data +
				 *   int_heap + ext_heap + ext_data
				 */
    mime_protocol_t protocol;
    int32_t version;

    uint16_t cnt_known_headers;
    uint16_t cnt_unknown_headers;
    uint16_t cnt_namevalue_headers;
    uint16_t cnt_querystring_headers;
    dataddr_t unknown_header_head;
    dataddr_t query_string_head;
    dataddr_t name_value_head;

    int32_t heap_free_index;
    int32_t heap_frees;

    char known_header_bitmap[roundup(MIME_MAX_HDR_DEFS, NBBY)];
    int32_t heap_size;
    int32_t heap_ext_size;
    int32_t ext_data_size;

    /* 0) Sparse known header map data */
    /* 1) Internal heap data */
    /* 2) External heap data */
    /* 3) External data */
} serial_mime_header_t;


/* mime module init */
void MIME_init(void);

/*
 * P U B L I C  F U N C T I O N S
 */

int mime_hdr_startup(void);
/**
 * @brief Initialize mime object
 * 	call this function after allocation
 *
 * @param header - Allocated header to be initalized
 * @param protocol - Underlying Supported Protocol spec
 * @param ext_data - External data buffer to be referenced
 * @param ext_data_size - Length of the external data buffer
 *
 * @return  -
 */
int mime_hdr_init(mime_header_t * header, mime_protocol_t protocol,
		  const char *ext_data, int32_t ext_data_size);

/**
 * @brief Re-initialize previously allocated/used mime structure
 * 	TODO - Check if protocol is needed.
 *
 * @param header - Header struct to be reset
 * @param ext_data - External data buffer to be referenced
 * @param ext_data_size - Length of the external data buffer
 *
 * @return  -
 */
int mime_hdr_reset(mime_header_t * header, const char *ext_data,
		   int32_t ext_data_size);

/**
 * @brief Cleanup function for mime header
 * 	Should be called before freeing any header object
 *
 * @param header - Header to be cleaned up
 *
 * @return -
 */
int mime_hdr_shutdown(mime_header_t * header);

/**
 * @brief String to Header Id definition conversion
 *
 *
 * @param protocol -  mime protocol id
 * @param str - header name
 * @param strlen - length of header name
 * @param enum_header - Enumeration of header definition
 *
 * @return -
 */
int mime_hdr_strtohdr(mime_protocol_t protocol, const char *str,
		      int strlen, int *enum_header);

/**
 * @brief - Add header based on defined header id
 *          Reference data if in [ext_data, len] else copy to heap
 *
 * @param header - Header Context structure
 * @param token - Header enumeration identifier
 * @param val - Header Data
 * @param len - Header Data Length
 *
 * @return -
 */
int mime_hdr_add_known(mime_header_t * header, int token,
		       const char *val, int len);

/**
 * @brief - Add header based on header name
 * 	Reference data if in [ext_data, len] else copy to heap
 *
 * @param header - Header Context struct
 * @param name - Name of the header
 * @param namelen - Length of the header
 * @param val -
 * @param vallen
 *
 * @return -
 */
int mime_hdr_add_unknown(mime_header_t * header, const char *name,
			 int namelen, const char *val, int vallen);

/**
 * @brief - Add header based on header name
 *	 Always allocates and copies data to heap
 *
 * @param header - Header Context
 * @param name - Name of the header
 * @param namelen - Length of the header
 * @param val - Value of the header
 * @param vallen - Value Length
 * @param user_attr -
 *
 * @return -
 */
int mime_hdr_add_namevalue(mime_header_t * header, const char *name,
			   int namelen, const char *val, int vallen,
			   u_int8_t user_attr);

/**
 * @brief Updates a known header value
 *
 * @param header - Header Context
 * @param token - Header Id
 * @param val - Header Value
 * @param len - Header Value length
 * @param replace_value -  1=Replace; 0=Update/Append
 *
 * @return -
 */
int mime_hdr_update_known(mime_header_t * header, int token,
			  const char *val, int len,
			  int replace_value
			  /* 1=Replace; 0=Update/Append */ );


/**
 * @brief Delete known Header
 *
 * @param header - Header Context
 * @param token - Header Id
 *
 * @return -
 */
int mime_hdr_del_known(mime_header_t * header, int token);


/**
 * @brief Delete unknown Header
 *
 * @param hd - Header context
 * @param name - Header Name
 * @param namelen - Length of the given header name
 *
 * @return -
 */
int mime_hdr_del_unknown(mime_header_t * hd, const char *name,
			 int namelen);

/**
 * @brief Delete Name-Value
 *
 * @param hd - Header Context
 * @param name - Name of the header
 * @param namelen - Length of header name
 *
 * @return -
 */
int mime_hdr_del_namevalue(mime_header_t * hd, const char *name,
			   int namelen);

/**
 * @brief Delete All known headers refered by the context
 *
 * @param header - Header context
 *
 * @return -
 */
int mime_hdr_del_all_known(mime_header_t * header);

/**
 * @brief Delete All unknown headers 
 *
 * @param hd - Header context
 *
 * @return -
 */
int mime_hdr_del_all_unknown(mime_header_t * hd);

/**
 * @brief Delete all name value pairs
 *
 * @param hd - Header Context
 *
 * @return -
 */
int mime_hdr_del_all_namevalue(mime_header_t * hd);

/**
 * @brief Check if known header is present
 *
 * @param hd - Header context
 * @param token - Header ID
 *
 * @return -
 */
int mime_hdr_is_known_present(const mime_header_t * hd, int token);

/**
 * @brief Check if unknown header is present
 *
 * @param hd - Header Context
 * @param name - Header Name
 * @param len - Length of Header Name
 *
 * @return -
 */
int mime_hdr_is_unknown_present(const mime_header_t * hd, const char *name,
				int len);

/**
 * @brief Check if Header present as name value pair
 *
 * @param hd - Header context
 * @param name - Header Name
 * @param len - Length of header
 *
 * @return -
 */
int mime_hdr_is_namevalue_present(const mime_header_t * hd,
				  const char *name, int len);

#define mime_hdr_get_known MIME_HDR_GET_KNOWN
#define MIME_HDR_GET_KNOWN(_hd, _token, _data, _len, _attributes, _header_cnt) \
	(((_token) >= MIME_HDR_MAX_DEFS || \
	 ((_hd)->known_header_map[(_token)] == 0)) ? 1 : \
	    int_mime_hdr_get_known((_hd), (_token), (_data), (_len), \
				   (_attributes), (_header_cnt)))
int int_mime_hdr_get_known(const mime_header_t * hd, int token,
		       const char **data, int *len, u_int32_t * attributes,
		       int *header_cnt);

// header_num is zero based
int mime_hdr_get_nth_known(const mime_header_t * hd, int token,
			   int header_num, const char **data, int *len,
			   u_int32_t * attributes);
int mime_hdr_get_unknown(const mime_header_t * hd, const char *name,
			 int namelen, const char **data, int *len,
			 u_int32_t * attributes);
// arg_num is zero based
int mime_hdr_get_nth_unknown(const mime_header_t * hd, int arg_num,
			     const char **name, int *name_len,
			     const char **val, int *val_len,
			     u_int32_t * attributes);

int mime_hdr_get_namevalue(const mime_header_t * hd, const char *name,
			   int namelen, const char **data, int *len,
			   u_int32_t * attributes);

// arg_num is zero based
int mime_hdr_get_nth_namevalue(const mime_header_t * hd, int arg_num,
			       const char **name, int *name_len,
			       const char **val, int *val_len,
			       u_int32_t * attributes);

int mime_hdr_get_query_arg(mime_header_t * hd, int arg_num,
			   const char **name, int *name_len,
			   const char **val, int *val_len);

int mime_hdr_get_query_arg_by_name(mime_header_t * hd, const char *name,
				   int namelen, const char **val,
				   int *val_len);

int mime_hdr_get_header_counts(const mime_header_t * hd,
			       int *cnt_known_headers,
			       int *cnt_unknown_headers,
			       int *cnt_namevalue_headers,
			       int *cnt_querystring_headers);

int mime_hdr_copy_headers(mime_header_t * dest, const mime_header_t * src);
int mime_hdr_copy_known_header(mime_header_t * dest, const mime_header_t * src,
			       int token);

int mime_hdr_serialize_datasize(const mime_header_t * hd, int *r_heap_int_size,
                                int *r_heap_ext_size, int *r_ext_data_size);

int mime_hdr_serialize(const mime_header_t * hd, char *outbuf,
		       int outbuf_size);

int mime_hdr_deserialize(const char *inbuf, int inbuf_size,
			 mime_header_t * hd, const char *extdata,
			 int extdatasz);

char *mk_rfc1123_time(const time_t * t, char *buf, int bufsz, int *datestr_len);

time_t parse_date(const char *buf, const char *buf_end);

int mime_hdr_get_known_cooked_data(mime_header_t * hd, int token,
				   const cooked_data_types_t ** data,
				   int *dlen, mime_hdr_datatype_t * type);

int canonicalize_url(const char *url, int url_len, char *output,
		     int output_bufsize, int *outbytesused);

int
escape_str(char *dst_str, int dst_strlen, const char *src_str,
	   int src_strlen, int *used_len);
// 
// Normalize uri_path by removing "." and ".." entries.
// Path is assumed to be absolute (i.e starts with "/").
//
// in: uri_path, uri_pathlen
// in: outbuf -- (outbuf == 0) => malloc buffer 
// in/out: outbuf_len -- return strlen(outbuf)
// out: modifications -- (modifications == 0) => no uri_path changes
// out: errcode -- (errcode == 0) => Success
//
char *normalize_uri_path(const char *uri_path, int uri_pathlen,
			 char *outbuf, int *outbuf_len,
			 int *modifications, int *errcode);
//
// Apply normalize_uri_path() on either MIME_HDR_X_NKN_URI or 
// MIME_HDR_X_NKN_DECODED_URI if defined.
//
//
int normalize_http_uri(mime_header_t * hdr);

const char *mime_hdr_dump_header(const mime_header_t * hd, char *outbuf,
				 int outbufsz);

int mapstr2upper(const char *in, char *out, int len);

int mapstr2lower(const char *in, char *out, int len);

long atol_len(const char *str, int len);

long long atoll_len(const char *str, int len);

long hextol_len(const char *str, int len);

// extract_suffix()
//   Given a [str,strlen] extract the suffix component where 'suffix_delimiter'
//   denotes the beginning of the suffix.
//
//   'suffix_delimiter' is not include in the specified output buffer.
//   'lowercase_on_output' denotes that the output is mapped to lower case
//   where appropriate.
//   'outbuf' is NULL terminated.
//   
// 	Returns: 0 => Success
//
int extract_suffix(const char *str, int slen, char *outbuf, int outbuflen,
		   char suffix_delimiter, int lowercase_on_output);

int copy_ipv6_host (char *dst, const char *src, int data_len);

int parse_host_header(const char *host, int hostlen,
                      int *domain_len, uint16_t *pport,
		      int network_byte_order_port);

char *concatenate_known_header_values(const mime_header_t * hd,
                                      int token, 
                                      char *buf, int bufsz) ;
int valid_FQDN(const char *name, int name_len);
int valid_IPaddr(const char *name, int name_len);

// 1 -> Cacheable, (expire_time == 0) => no expiry
// 0 -> No Cache
int compute_object_expiration(mime_header_t * hdr,
			      int ignore_s_maxage,
			      time_t request_send_time,
			      time_t request_response_time,
			      time_t now,
			      time_t * corrected_initial_age, /* output */
			      time_t * obj_expire_time /* output */);

#ifndef PROTO_HTTP_LIB
// Convert between nkn_attr_t and mime_header_t functions
//   Return 0 on success
int nkn_attr2mime_header(const nkn_attr_t * attr, int only_end2end_hdrs,
			 mime_header_t * hdr);

int mime_header2nkn_attr(const mime_header_t * hdr, int only_end2end_hdrs,
			 nkn_attr_t * attr);

int mime_header2nkn_buf_attr(const mime_header_t * hdr, int only_end2end_hdrs,
                             void * buf_nkn_attr,
			     int (*set_bufsize)(void *, int, int),
			     void *(*buf2attr)(void *));


// Generate nkn_objv from given mime_header_t
//   Return 0 on success
int compute_object_version(const mime_header_t *hdr, const char *uri,
                            int urilen, int encoding_type, nkn_objv_t *objvp);
int compute_object_version_use_etag(const mime_header_t *hdr, const char *uri,
                            int urilen, int encoding_type, nkn_objv_t *objvp);
int compute_object_version_use_lm(const mime_header_t *hdr, const char *uri,
                            int urilen, int encoding_type, nkn_objv_t *objvp);
#endif /* !PROTO_HTTP_LIB */

// Remove known hop2hop headers
int mime_hdr_remove_hop2hophdrs(mime_header_t *hdr);
int mime_hdr_remove_hop2hophdrs_except(mime_header_t *hdr, int token);
int mime_hdr_remove_hop2hophdrs_except_these(mime_header_t *hdr, int num_tokens, int *token);

// Subsystem system shutdown interface
int mime_hdr_shutdown_headers(void);

// Delete Invalid Warning Values
int delete_invalid_warning_values(mime_header_t *hdr);

#endif

/*
 * End of mime_header.h
 */

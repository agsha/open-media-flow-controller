/*
 *******************************************************************************
 * http_header.h -- HTTP protocol header processing
 *******************************************************************************
 */

#ifndef _HTTP_HEADER_H
#define _HTTP_HEADER_H

#include "http_def.h"
#include "mime_header.h"

/* 
 * Trace the request for diagnostics 
 * Currently treated as unknown header to avoid parser mods.
 */
#define MIME_HDR_X_NKN_TRACE_STR	"X-NKN-Trace"
#define MIME_HDR_X_NKN_TRACE_LEN	11


/*
 ********************************
 * Known HTTP header descriptor *
 ********************************
 */
extern mime_header_descriptor_t http_known_headers[MIME_HDR_MAX_DEFS];

/*
 *******************************
 * HTTP End to end header list *
 *******************************
 */
extern char http_end2end_header[MIME_HDR_MAX_DEFS];

/*
 ********************************
 * HTTP allowed 304 header list *
 ********************************
 */
extern char http_allowed_304_header[MIME_HDR_MAX_DEFS];

/*
 **********************************************************************
 * Nokeena - Customer removable headers if 304 response does not have *
 **********************************************************************
 */
extern char http_removable_304_header[MIME_HDR_MAX_DEFS];



/*
 ******************************************************************************
 * 		P U B L I C  F U N C T I O N S
 ******************************************************************************
 */
// Call after object allocation
int init_http_header (mime_header_t * hd, const char *ext_data,
		      int32_t ext_data_size);
// Call prior to object reuse
int reset_http_header (mime_header_t * hd, const char *ext_data,
		       int32_t ext_data_size);
// Call prior to object deallocation
int shutdown_http_header (mime_header_t * hd);

int string_to_known_header_enum (const char *name, int len, int *data_enum);

// Reference data if in [ext_data, len] else copy to heap
int add_known_header (mime_header_t * hd, int token, const char *val,
		      int len);

// Reference data if in [ext_data, len] else copy to heap
int add_unknown_header (mime_header_t * hd, const char *name, int namelen,
			const char *val, int vallen);
// Always allocates and copies data to heap
int add_namevalue_header (mime_header_t * hd, const char *name, int namelen,
			  const char *val, int vallen, u_int8_t user_attr);

int update_known_header (mime_header_t * hd, int token, const char *val,
			 int len,
			 int replace_value /* 1=Replace; 0=Update/Append */ );

int delete_known_header (mime_header_t * hd, int token);
int delete_all_known_headers (mime_header_t * hd);
int delete_unknown_header (mime_header_t * hd, const char *name, int namelen);
int delete_all_unknown_headers (mime_header_t * hd);
int delete_namevalue_header (mime_header_t * hd, const char *name,
			     int namelen);
int delete_all_namevalue_headers (mime_header_t * hd);

int is_known_header_present (const mime_header_t * hd, int token);
int is_unknown_header_present (const mime_header_t * hd, const char *name,
			       int len);
int is_namevalue_header_present (const mime_header_t * hd, const char *name,
				 int len);

#define get_known_header GET_KNOWN_HEADER
#define GET_KNOWN_HEADER(_hd, _token, _data, _len, _attributes, _header_cnt) \
	(((_token) >= MIME_HDR_MAX_DEFS || \
	 (_hd)->known_header_map[(_token)] == 0) ? 1 : \
	    int_get_known_header((_hd), (_token), (_data), (_len), \
	    			 (_attributes), (_header_cnt)))
int int_get_known_header (const mime_header_t * hd, int token, 
			  const char **data, int *len, u_int32_t * attributes, 
			  int *header_cnt);
// header_num is zero based
int get_nth_known_header (const mime_header_t * hd, int token, int header_num,
			  const char **data, int *len,
			  u_int32_t * attributes);
int get_unknown_header (const mime_header_t * hd, const char *name,
			int namelen, const char **data, int *len,
			u_int32_t * attributes);
// arg_num is zero based
int get_nth_unknown_header (const mime_header_t * hd, int arg_num,
			    const char **name, int *name_len,
			    const char **val, int *val_len,
			    u_int32_t * attributes);
int get_namevalue_header (const mime_header_t * hd, const char *name,
			  int namelen, const char **data, int *len,
			  u_int32_t * attributes);
// arg_num is zero based
int get_nth_namevalue_header (const mime_header_t * hd, int arg_num,
			      const char **name, int *name_len,
			      const char **val, int *val_len,
			      u_int32_t * attributes);

int get_query_arg (mime_header_t * hd, int arg_num, const char **name,
		   int *name_len, const char **val, int *val_len);

int get_query_arg_by_name (mime_header_t * hd, const char *name,
			   int namelen, const char **val, int *val_len);

int get_header_counts (const mime_header_t * hd, int *cnt_known_headers,
		       int *cnt_unknown_headers,
		       int *cnt_namevalue_headers,
		       int *cnt_querystring_headers);

int copy_http_headers (mime_header_t * dest, const mime_header_t * src);

int serialize_datasize (const mime_header_t * hd);

int serialize (const mime_header_t * hd, char *outbuf, int outbuf_size);

int deserialize (const char *inbuf, int inbuf_size, mime_header_t * hd,
		 const char *extdata, int extdatasz);

int get_cooked_known_header_data (mime_header_t * hd, int token,
				  const cooked_data_types_t ** data,
				  int *dlen, mime_hdr_datatype_t * type);

const char *dump_http_header (const mime_header_t * hd, char *outbuf,
			      int outbufsz, int pretty);

// Convert between nkn_attr_t and mime_header_t functions
//   Return 0 on success
int nkn_attr2http_header (const nkn_attr_t * attr, int only_end2end_hdrs,
			  mime_header_t * hdr);
int http_header2nkn_attr (const mime_header_t * hdr, int only_end2end_hdrs,
			  nkn_attr_t * attr);

// Subsystem system shutdown interface
int shutdown_http_headers (void);

int http_hdr_block_connection_token (mime_header_t * hd) ;
int count_known_header_values(const mime_header_t * hd, int token,
                                  int *num_values,
                                  int *total_val_bytecount) ;


#endif /* _HTTP_HEADER_H */

/*
 * End of http_header.h
 */

/*
 *******************************************************************************
 * http_header.c -- HTTP protocol header processing
 *******************************************************************************
 */
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdio.h>
#include <alloca.h>

#include "nkn_defs.h"
#include "nkn_debug.h"
#include "mime_header.h"
#include "http_header.h"

#include "http_def.h"
#include "http_data.c"

/*
 *******************************************************************************
 * init_http_header() -- Initialize an allocated http_header
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int init_http_header(mime_header_t * hd, const char *ext_data,
		     int32_t ext_data_size)
{
    return mime_hdr_init(hd, MIME_PROT_HTTP, ext_data, ext_data_size);
}

/*
 *******************************************************************************
 * reset_http_header() -- Reset http_header for reuse
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int reset_http_header(mime_header_t * hd, const char *ext_data,
		      int32_t ext_data_size)
{
    return mime_hdr_reset(hd, ext_data, ext_data_size);
}

/*
 *******************************************************************************
 * shutdown_http_header() -- Free malloc'ed http_header data, called prior to
 *   destruction of http_header.
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int shutdown_http_header(mime_header_t * hd)
{
    return reset_http_header(hd, 0, 0);
}

/*
 *******************************************************************************
 * shutdown_http_headers() -- Deallocate malloc'ed data prior to system shutdown
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
int shutdown_http_headers()
{
    return mime_hdr_shutdown_headers();
}

/*
 *******************************************************************************
 * string_to_known_header_enum() -- Determine if the given string is a 
 *   know header, if so return the corresponding enum token.
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int string_to_known_header_enum(const char *name, int len, int *data_enum)
{
    return mime_hdr_strtohdr(MIME_PROT_HTTP, name, len, data_enum);
}

/*
 *******************************************************************************
 * add_known_header() -- Add known header, if it exists and is a CSV header,
 *   append to value list otherwise overwrite existing value.
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int add_known_header(mime_header_t * hd, int token, const char *val,
		     int len)
{
    return mime_hdr_add_known(hd, token, val, len);
}

/*
 *******************************************************************************
 * add_unknown_header() -- Add unknown header, if it exists the current value
 *   is overwritten
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int add_unknown_header(mime_header_t * hd, const char *name, int namelen,
		       const char *val, int vallen)
{
    return mime_hdr_add_unknown(hd, name, namelen, val, vallen);
}

/*
 *******************************************************************************
 * add_namevalue_header() -- Add name/value header, if it exists the 
 *   current value is overwritten
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int add_namevalue_header(mime_header_t * hd, const char *name, int namelen,
			 const char *val, int vallen, u_int8_t user_attr)
{
    return mime_hdr_add_namevalue(hd, name, namelen, val, vallen,
				  user_attr);
}

/*
 *******************************************************************************
 * update_known_header() -- Update or add known header.
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int update_known_header(mime_header_t * hd, int token, const char *val,
			int len,
			int replace_value /* 1=Replace; 0=Update/Append */
			) {
    return mime_hdr_update_known(hd, token, val, len, replace_value);
}

/*
 *******************************************************************************
 * delete_known_header() -- Delete known header
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int delete_known_header(mime_header_t * hd, int token)
{
    return mime_hdr_del_known(hd, token);
}

/*
 *******************************************************************************
 * delete_all_known_headers() -- Delete all known headers
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int delete_all_known_headers(mime_header_t * hd)
{
    return mime_hdr_del_all_known(hd);
}

/*
 *******************************************************************************
 * delete_unknown_header() -- Delete unknown header by name
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int delete_unknown_header(mime_header_t * hd, const char *name,
			  int namelen)
{
    return mime_hdr_del_unknown(hd, name, namelen);
}

/*
 *******************************************************************************
 * delete_all_unknown_headers() -- Delete all unknown headers
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int delete_all_unknown_headers(mime_header_t * hd)
{
    return mime_hdr_del_all_unknown(hd);
}

/*
 *******************************************************************************
 * delete_namevalue_header() -- Delete name/value header by name
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int delete_namevalue_header(mime_header_t * hd, const char *name,
			    int namelen)
{
    return mime_hdr_del_namevalue(hd, name, namelen);
}

/*
 *******************************************************************************
 * delete_all_namevalue_headers() -- Delete all name/value headers
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int delete_all_namevalue_headers(mime_header_t * hd)
{
    return mime_hdr_del_all_namevalue(hd);
}

/*
 *******************************************************************************
 * is_known_header_present() -- Determine if known header exists
 *
 *   Return !=0 => Present
 *******************************************************************************
 */
int is_known_header_present(const mime_header_t * hd, int token)
{
    return mime_hdr_is_known_present(hd, token);
}

/*
 *******************************************************************************
 * is_unknown_header_present() -- Determine if unknown header exists
 *
 *   Return !=0 => Present
 *******************************************************************************
 */
int is_unknown_header_present(const mime_header_t * hd, const char *name,
			      int len)
{
    return mime_hdr_is_unknown_present(hd, name, len);
}

/*
 *******************************************************************************
 * is_namevalue_header_present() -- Determine if name value header exists
 *
 *   Return !=0 => Present
 *******************************************************************************
 */
int is_namevalue_header_present(const mime_header_t * hd, const char *name,
				int len)
{
    return mime_hdr_is_namevalue_present(hd, name, len);
}

/*
 *******************************************************************************
 * int_get_known_header() -- Get requested known header data
 *
 *   Return 0 => Success, value [data, len, attribute] corresponding to 
 *               value #0 with header_cnt reflecting the total number of 
 *               value headers
 *
 *  Note: Internal interface users should use GET_KNOWN_HEADER() interface
 *******************************************************************************
 */
int int_get_known_header(const mime_header_t * hd, int token,
		     const char **data, int *len, u_int32_t * attributes,
		     int *header_cnt)
{
    return int_mime_hdr_get_known(hd, token, data, len, attributes,
			      header_cnt);
}

/*
 *******************************************************************************
 * get_nth_known_header() -- Get the nth value data [data, len, attribute] 
 *   for the requested known header
 *
 *   Return 0 => Success
 *		
 *******************************************************************************
 */
int get_nth_known_header(const mime_header_t * hd, int token,
			 int header_num, const char **data, int *len,
			 u_int32_t * attributes)
{
    return mime_hdr_get_nth_known(hd, token, header_num, data, len,
				  attributes);
}

/*
 *******************************************************************************
 * get_unknown_header() -- Get the requested unknown header data
 *
 *   Return 0 => Success, value data [data, len, attribute]
 *******************************************************************************
 */
int get_unknown_header(const mime_header_t * hd, const char *name,
		       int namelen, const char **data, int *len,
		       u_int32_t * attributes)
{
    return mime_hdr_get_unknown(hd, name, namelen, data, len, attributes);
}

/*
 *******************************************************************************
 * get_namevalue_header() -- Get the requested name/value header data
 *
 *   Return 0 => Success, value data [data, len, attribute]
 *******************************************************************************
 */
int get_namevalue_header(const mime_header_t * hd, const char *name,
			 int namelen, const char **data, int *len,
			 u_int32_t * attributes)
{
    return mime_hdr_get_namevalue(hd, name, namelen, data, len,
				  attributes);
}

/*
 *******************************************************************************
 * get_nth_unknown_header() -- Get the nth unknown header
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int get_nth_unknown_header(const mime_header_t * hd, int arg_num,
			   const char **name, int *name_len,
			   const char **val, int *val_len,
			   u_int32_t * attributes)
{
    return mime_hdr_get_nth_unknown(hd, arg_num, name, name_len, val,
				    val_len, attributes);
}

/*
 *******************************************************************************
 * get_nth_namevalue_header() -- Get the nth name/value header
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int get_nth_namevalue_header(const mime_header_t * hd, int arg_num,
			     const char **name, int *name_len,
			     const char **val, int *val_len,
			     u_int32_t * attributes)
{
    return mime_hdr_get_nth_namevalue(hd, arg_num, name, name_len, val,
				      val_len, attributes);
}

/*
 *******************************************************************************
 * get_query_arg() -- Get the nth name/value element
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int get_query_arg(mime_header_t * hd, int arg_num,
		  const char **name, int *name_len,
		  const char **val, int *val_len)
{
    return mime_hdr_get_query_arg(hd, arg_num, name, name_len, val,
				  val_len);
}

/*
 *******************************************************************************
 * get_query_arg_by_name() -- Get the query value associated with the given name
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int get_query_arg_by_name(mime_header_t * hd, const char *name,
			  int namelen, const char **val, int *val_len)
{
    return mime_hdr_get_query_arg_by_name(hd, name, namelen, val, val_len);
}

/*
 *******************************************************************************
 * get_header_counts() -- Get header stat data
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int get_header_counts(const mime_header_t * hd,
		      int *cnt_known_headers, int *cnt_unknown_headers,
		      int *cnt_namevalue_headers,
		      int *cnt_querystring_headers)
{
    return mime_hdr_get_header_counts(hd, cnt_known_headers,
				      cnt_unknown_headers,
				      cnt_namevalue_headers,
				      cnt_querystring_headers);
}

/*
 *******************************************************************************
 * copy_http_headers() -- Copy headers from given src to dest mime_header_t
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int copy_http_headers(mime_header_t * dest, const mime_header_t * src)
{
    return mime_hdr_copy_headers(dest, src);
}

/*
 *******************************************************************************
 * serialize_datasize() -- Determine http_header serialization size in bytes
 *
 *   Return !=0 => Success
 *******************************************************************************
 */
int serialize_datasize(const mime_header_t * hd)
{
    return mime_hdr_serialize_datasize(hd, 0, 0, 0);
}

/*
 *******************************************************************************
 * serialize() -- Serialize http_header into given buffer
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int serialize(const mime_header_t * hd, char *outbuf, int outbuf_size)
{
    return mime_hdr_serialize(hd, outbuf, outbuf_size);
}

/*
 *******************************************************************************
 * deserialize() -- Deserialize buffer into http_header and given extdata.
 *   If (extdata == 0) place it in the local heap
 *   If (extdata == 1) use extdata directly from the serialized buffer
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int deserialize(const char *inbuf, int inbuf_size, mime_header_t * hd,
		const char *extdata, int extdatasz)
{
    return mime_hdr_deserialize(inbuf, inbuf_size, hd, extdata, extdatasz);
}


/*
 *******************************************************************************
 * cook_known_header_data() -- Cook the given known header data and place
 *   it in the local heap.
 *
 *   Return 0 => Success; type denotes the data type for [data, len] 
 *******************************************************************************
 */
int get_cooked_known_header_data(mime_header_t * hd, int token,
				 const cooked_data_types_t ** data,
				 int *dlen, mime_hdr_datatype_t * type)
{
    return mime_hdr_get_known_cooked_data(hd, token, data, dlen, type);
}

/*
 *******************************************************************************
 * nkn_attr2http_header() -- Convert from nkn_attr_t to mime_header_t
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int nkn_attr2http_header(const nkn_attr_t * nkn_attr,
			 int only_end2end_hdrs, mime_header_t * hdr)
{
    return nkn_attr2mime_header(nkn_attr, only_end2end_hdrs, hdr);
}

/*
 *******************************************************************************
 * http_header2nkn_attr() -- Convert from mime_header_t to nkn_attr_t
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int http_header2nkn_attr(const mime_header_t * hdr, int only_end2end_hdrs,
			 nkn_attr_t * nkn_attr)
{
    return mime_header2nkn_attr(hdr, only_end2end_hdrs, nkn_attr);
}


/*
 * This API blocks the fwding of 'Connection:' token values
 * across MFC as this is not allowed as per RFC-2616.
 * This API will be called by http module when a request comes
 * in or when a response enters MFC(at origin-mgr) from origin.
 */
int
http_hdr_block_connection_token (mime_header_t * hd)
{
    int                rv = 0;
    int                header_num = 0;
    const char       * hdr_value;
    int                hdr_len;
    u_int32_t          hdr_attrs;
 
   /* 
    * Search for all the values for the known header 'Connection:'.
    */
    while ((rv = mime_hdr_get_nth_known(hd, MIME_HDR_CONNECTION, 
                                        header_num,
                                        &hdr_value, &hdr_len, 
                                        &hdr_attrs)) == 0) {
        if (hdr_value && hdr_len) {
           /*
            * Delete the connection-token header from the
            * response's unknown header list.
            */
            if (delete_unknown_header(hd, hdr_value, hdr_len))
            {
                DBG_LOG(MSG, MOD_HTTP, "Connection header not in list");
            }
        }   
        header_num++;
    }
    // On Success
    return 0;
}

/*
 *   Get cache index from mime headers.
 *
 *   Return 0 => Success, value [data, len, attribute] corresponding to
 *               value #0 with header_cnt reflecting the total number of
 *               value headers
 *
 */
int mime_hdr_get_cache_index(const mime_header_t *hd,
        const char **data, int *datalen, u_int32_t *attributes,
        int *header_cnt)
{
    if (get_known_header(hd, MIME_HDR_X_NKN_CACHE_INDEX,
                         data, datalen, attributes, header_cnt)) {
        if (get_known_header(hd, MIME_HDR_X_NKN_REMAPPED_URI,
                             data, datalen, attributes, header_cnt)) {
            if (get_known_header(hd, MIME_HDR_X_NKN_DECODED_URI,
                                 data, datalen, attributes, header_cnt)) {
                if (get_known_header(hd, MIME_HDR_X_NKN_URI,
                                     data, datalen, attributes, header_cnt)) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

/*
 * End of http_header.c
 */

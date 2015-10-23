/* File : mime_header.c Author : 
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
/** 
 * @file mime_header.c
 * @brief General interface derived from previously developed code for http
 * @author
 * @version 1.00
 * @date 2009-09-08
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
#include <ctype.h>
#include <openssl/md5.h>
#include <arpa/inet.h>
#include <atomic_ops.h>

#include "nkn_defs.h"
#include "nkn_debug.h"
#include "cache_mgr.h"

#include "http_header.h"
#include "rtsp_header.h"
#include <uuid/uuid.h>
#if 0
#include "nkn_assert.h"
#else
/* The NKN_ASSERT macro definition is not working properly in this directory */
/* TBD:NKN_ASSERT */

#define NKN_ASSERT(_e)
#include <assert.h>
#endif


/* Global for enabling/disabling logging */
static int log_enabled = 1;	// Primarily used to disable from nknfqueue
void disable_http_headers_log(void);
static version_counter_t obj_ver_counter;

#define DBG(_fmt, ...) if (log_enabled) DBG_LOG(MSG, MOD_HTTPHDRS, _fmt, __VA_ARGS__)


/* 
 * Private Functions included from mime_hdr_private.c
 */
#include "mime_hdr_private.c"

/*
 *******************************************************************************
 *
 *		P U B L I C  F U N C T I O N S
 *
 *******************************************************************************
 */

int mime_hdr_startup(void)
{
    if (MIME_HDR_MAX_DEFS > MIME_MAX_HDR_DEFS) {
	assert(!"MIME_HDR_MAX_DEFS > MIME_MAX_HDR_DEFS,"
	       "adjust MIME_MAX_HDR_DEFS bump MIME_HEADER_VERSION");
    }

    if (RTSP_HDR_MAX_DEFS > MIME_MAX_HDR_DEFS) {
	assert(!"RTSP_HDR_MAX_DEFS > MIME_MAX_HDR_DEFS,"
	       "adjust MIME_MAX_HDR_DEFS bump MIME_HEADER_VERSION");
    }

    hash_table_init();
    return 0;
}
/*
 * function to initialize the incarnation and counter
 */
void MIME_init()
{
       uuid_generate(obj_ver_counter.ver.t_ver.incarnation);
       AO_store(&obj_ver_counter.ver.t_ver.counter, 0);
}

/*
 *******************************************************************************
 * init_http_header() -- Initialize an allocated http_header
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_init(mime_header_t * hd, mime_protocol_t protocol,
		  const char *ext_data, int32_t ext_data_size)
{
    assert((protocol == MIME_PROT_HTTP) || (protocol == MIME_PROT_RTSP));

    memset((void *) hd, 0, sizeof(*hd));
    hd->protocol = protocol;
    hd->version = MIME_HEADER_VERSION;
    hd->ext_data = ext_data;
    hd->ext_data_size = ext_data_size;

    return 0;
}

/*
 *******************************************************************************
 * reset_http_header() -- Reset http_header for reuse
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_reset(mime_header_t * hd, const char *ext_data,
		   int32_t ext_data_size)
{

    memset((void *) hd->known_header_map, 0, sizeof(hd->known_header_map));
    hd->unknown_header_head = 0;
    hd->query_string_head = 0;
    hd->name_value_head = 0;
    hd->cnt_known_headers = 0;
    hd->cnt_unknown_headers = 0;
    hd->cnt_namevalue_headers = 0;
    hd->cnt_querystring_headers = 0;
    hd->heap_free_index = 0;
    hd->heap_frees = 0;

    memset((void *)hd->heap, 0, sizeof(hd->heap));

    if (hd->heap_ext) {
	free(hd->heap_ext);
	hd->heap_ext = 0;
	hd->heap_ext_size = 0;
    }
    hd->ext_data = ext_data;
    hd->ext_data_size = ext_data_size;

    return 0;
}

/*
 *******************************************************************************
 * shutdown_http_header() -- Free malloc'ed http_header data, called prior to
 *   destruction of http_header.
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_shutdown(mime_header_t * hd)
{
    reset_http_header(hd, 0, 0);
    hd->protocol = -1;
    hd->version = -1;
    return 0;
}

/*
 *******************************************************************************
 * mime_hdr_shutdown_headers() -- Deallocate malloc'ed data prior to system shutdown
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
int mime_hdr_shutdown_headers()
{
    int rv;
    int prot;

    rv = (*ht_cache_control_vals.dealloc_func) (&ht_cache_control_vals);
    if (rv) {
	DBG("ht_cache_control_vals.dealloc_func() failed rv=%d", rv);
    }

    for (prot = MIME_PROT_HTTP; prot < MIME_PROT_MAX; prot++) {
	if (!ht_known_headers[prot].dealloc_func)
	    continue;
	rv = (*ht_known_headers[prot].
	      dealloc_func) (&ht_known_headers[prot]);
	if (rv) {
	    DBG("ht_known_headers[prot].dealloc_func() failed rv=%d", rv);
	}
    }
    return 0;
}

/*
 *******************************************************************************
 * string_to_known_header_enum() -- Determine if the given string is a 
 *   know header, if so return the corresponding enum token.
 *
 *   Return 0 => Success
 *******************************************************************************
 */

int mime_hdr_strtohdr(mime_protocol_t protocol, const char *name, int len,
		      int *data_enum)
{
    int rv;

    rv = (*ht_known_headers[protocol].
	  lookup_func) (&ht_known_headers[protocol], name, len, data_enum);
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_add_known() -- Add known header, if it exists and is a CSV header,
 *   append to value list otherwise overwrite existing value.
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_add_known(mime_header_t * hd, int token, const char *val,
		       int len)
{
    int rv = 0;
    dataddr_t hdptr;
    dataddr_t hdr_token;
    dataddr_t data_hdptr = 0;
    dataddr_t val_hdptr = 0;
    dataddr_t val_offset;
    heap_data_t *pheap_data;
    heap_data_t *pheap_data_first;

    if (token >= MIME_HDR_MAX_DEFS) {
	DBG("Invalid token=%d", token);
	rv = 1;
	goto err_exit;
    }

    /*
     * Compute value offset
     */
    rv = compute_data_offset(hd, val, len, &val_offset, &data_hdptr, 0);
    if (rv) {
	DBG("compute_data_offset() failed rv=%d", rv);
	rv = 2;
	goto err_exit;
    }
    hdr_token = hd->known_header_map[token];

    /*
     * Allocate value_data heap element
     */
    if (!hdr_token
        || http_known_headers[token].comma_hdr
       /* BZ: 3428.
        * Even though we dont set the 'comma_hdr' flag to 1
        * incase of 'Connection' header, we still need to
        * keep a list of all values for this header. 
        */
        || token == MIME_HDR_CONNECTION) {
	val_hdptr = get_heap_element(hd, F_VALUE_DATA, 0);
	if (!val_hdptr) {
	    DBG("get_heap_element(%p, F_VALUE_DATA, 0) failed", hd);
	    rv = 3;
	    goto err_exit;
	}
	pheap_data = OFF2PTR(hd, val_hdptr);
	pheap_data->flags |= (data_hdptr != 0) * F_VALUE_HEAPDATA_REF;
	pheap_data->u.v.value = val_offset;
	pheap_data->u.v.value_len = len;
    }

    /*
     * Set, append or replace value_data
     */
    if (!hdr_token) {
	hd->known_header_map[token] = val_hdptr;
	hd->cnt_known_headers++;
	// Update counter
	pheap_data_first = OFF2PTR(hd, val_hdptr);
	pheap_data_first->u.v.hdrcnt ++;
    } else {
        if (http_known_headers[token].comma_hdr
           /* Give exception to 'Connection:' header, since
            * we might need to keep this info for removing the
            * connection-token values.
            * Append the '#connection-token' name to the list.
            */  
            || token == MIME_HDR_CONNECTION) {
	    /* Append value_data */
	    hdptr = hdr_token;

	    while (hdptr) {
		pheap_data = OFF2PTR(hd, hdptr);
		if (pheap_data->u.v.value_next) {
		    hdptr = pheap_data->u.v.value_next;
		} else {
		    pheap_data->u.v.value_next = val_hdptr;
		    break;
		}
	    }
	    // Update counter
	    pheap_data_first = OFF2PTR(hd, hdr_token);
	    pheap_data_first->u.v.hdrcnt ++;
	} else {
	    /* Replace value_data */
	    hdptr = hdr_token;
	    pheap_data = OFF2PTR(hd, hdptr);
	    if (pheap_data->flags & F_VALUE_HEAPDATA_REF) {
		delete_heap_element(hd,
				    DATAOFF_TO_HEAP_ELEMENT(hd,
							    pheap_data->
							    u.v.value));
		pheap_data->flags &= ~F_VALUE_HEAPDATA_REF;
	    }
	    pheap_data->flags |= (data_hdptr != 0) * F_VALUE_HEAPDATA_REF;
	    pheap_data->u.v.value = val_offset;
	    pheap_data->u.v.value_len = len;
	}

	/* Delete cooked data */
	hdptr = hdr_token;
	pheap_data = OFF2PTR(hd, hdptr);
	if (pheap_data->u.v.cooked_data) {	/* Always F_DATA */
	    delete_heap_element(hd,
				DATAOFF_TO_HEAP_ELEMENT(hd,
							pheap_data->u.
							v.cooked_data));
	    pheap_data->u.v.cooked_data = 0;
	}
    }

    return rv;

  err_exit:

    if (data_hdptr) {
	delete_heap_element(hd, OFF2PTR(hd, data_hdptr));
    }
    if (val_hdptr) {
	delete_heap_element(hd, OFF2PTR(hd, val_hdptr));
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_add_unknown() -- Add unknown header, if it exists the current value
 *   is overwritten
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_add_unknown(mime_header_t * hd, const char *name, int namelen,
			 const char *val, int vallen)
{
    int rv;
    rv = add_name_value(hd, &hd->unknown_header_head,
			&hd->cnt_unknown_headers, name, namelen, val,
			vallen, 0, 0, 0);
    if (rv) {
	DBG("add_name_value() failed rv=%d", rv);
	rv = 1;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_add_namevalue() -- Add name/value header, if it exists the 
 *   current value is overwritten
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_add_namevalue(mime_header_t * hd, const char *name,
			   int namelen, const char *val, int vallen,
			   u_int8_t user_attr)
{
    int rv;
    rv = add_name_value(hd, &hd->name_value_head,
			&hd->cnt_namevalue_headers, name, namelen, val,
			vallen, 1, user_attr, 0);
    if (rv) {
	DBG("add_name_value() failed rv=%d", rv);
	rv = 1;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_update_known() -- Update or add known header.
 *
 *   Return 0 => Success
 *******************************************************************************
 */
static void copy_no_spaces(char *dest, const char *src, int srclen)
{
    // Note: dest is always srclen+1 bytes in length
    int len = srclen;

    if (len) {
	// Ignore leading spaces
	while (*src == ' ' && len) {
	    src++;
	    len--;
	}

	if (len) {
	    // Copy until space or len=0
	    while (*src != ' ' && len) {
		*(dest++) = *(src++);
		len--;
	    }
	}
    }
    *dest = 0;
}

#define TOKENIZE(_input_str, _list_nv, _list_nv_size, _list_nv_used, \
		 _exit_status) { \
    /* Note: Must be a macro due to alloca() usage */ \
    char *_ptoken; \
    char *_saveptr; \
    char *_name; \
    char *_value; \
    char *_p; \
    int _len; \
    int _list_nv_ix = 0; \
    name_value_t *_t_nv; \
    (_exit_status) = 0; \
 \
    _ptoken = strtok_r((_input_str), ",", &_saveptr); \
    while (_ptoken) { \
		_p = strchr(_ptoken, '='); \
	if (_p) { \
		_len = (_p-_ptoken); \
		_name = alloca(_len+1); \
		copy_no_spaces(_name, _ptoken, _len); \
 \
		_len = strlen(_p+1); \
		_value = alloca(_len+1); \
		copy_no_spaces(_value, (_p+1), _len); \
	} else { \
		_len = strlen(_ptoken); \
		_name = alloca(_len+1); \
		copy_no_spaces(_name, _ptoken, _len); \
		_value = 0; \
	} \
	if (_list_nv_ix >= (_list_nv_size)) { \
		(_list_nv_size) *= 2; \
		_t_nv = nkn_realloc_type(list_nv, sizeof(name_value_t)*(_list_nv_size), mod_http_name_value_t); \
		if (_t_nv) { \
			(_list_nv) = _t_nv; \
		} else { \
			DBG("realloc() failed, list_nv_size=%d", (_list_nv_size)); \
		(_exit_status) = 1; \
		break; \
		} \
	} \
	if (*_name) { \
		(_list_nv)[_list_nv_ix].name = _name; \
		(_list_nv)[_list_nv_ix].value = _value; \
		_list_nv_ix++; \
	} \
	_ptoken = strtok_r(0, ",", &_saveptr); \
    } \
    (_list_nv_used) = _list_nv_ix; \
}

int mime_hdr_update_known(mime_header_t * hd, int token, const char *val,
			  int len,
			  int replace_value
			  /* 1=Replace; 0=Update/Append */ ) {
    char *csv_list;
    int csv_listsz;
    char *valbuf;

    name_value_t *list_nv = 0;
    int size_list_nv = 64;
    int list_nv_used = 0;

    name_value_t *new_list_nv = 0;
    int size_new_list_nv = 64;
    int new_list_nv_used = 0;

    int status;
    int rv = 0;
    int n;
    char *resvalp;
    int resvalsz;
    int bytes;
    int total_bytes;
    int cc_max_age = 0;

    if (replace_value) {
	if (!http_known_headers[token].comma_hdr) {
	    return mime_hdr_add_known(hd, token, val, len);
	} else {
	    rv = delete_known_header(hd, token);
	    if (rv) {
		DBG("delete_known_header() failed, rv=%d", rv);
	    }
	    return mime_hdr_add_known(hd, token, val, len);
	}
    }

    csv_listsz = 4096;
    csv_list = alloca(csv_listsz);

    if (!concatenate_known_header_values(hd, token, csv_list, csv_listsz)) {
	DBG("concatenate_known_header_values() failed, "
	    "token=%d csv_list=%p csv_listsz=%d", token, csv_list,
	    csv_listsz);
	rv = 1;
	goto exit;
    }

    if (http_known_headers[token].comma_hdr < 0) {
	// Append only
	goto exit;
    }

    valbuf = alloca(len + 1);
    memcpy(valbuf, val, len);
    valbuf[len] = '\0';

    // Create list_nv
    list_nv =
	(name_value_t *) nkn_malloc_type(sizeof(name_value_t) *
					 size_list_nv,
					 mod_http_name_value_t);
    if (!list_nv) {
	DBG("nkn_malloc_type() failed, size_list_nv=%d", size_list_nv);
	rv = 2;
	goto exit;
    }

    TOKENIZE(csv_list, list_nv, size_list_nv, list_nv_used, status);
    if (status) {
	DBG("Unable to tokenize 'csv_list', status=%d", status);
	rv = 3;
	goto exit;
    }
    // Create new_list_nv
    new_list_nv =
	(name_value_t *) nkn_malloc_type(sizeof(name_value_t) *
					 size_new_list_nv,
					 mod_http_name_value_t);
    if (!new_list_nv) {
	DBG("nkn_malloc_type() failed, size_new_list_nv=%d",
	    size_new_list_nv);
	rv = 4;
	goto exit;
    }

    TOKENIZE(valbuf, new_list_nv, size_new_list_nv, new_list_nv_used,
	     status);
    if (status) {
	DBG("Unable to tokenize 'valbuf', status=%d", status);
	rv = 5;
	goto exit;
    }

    status = update_nv_list(&list_nv, &list_nv_used, &size_list_nv,
			    new_list_nv, new_list_nv_used);
    if (status) {
	DBG("update_nv_list() failed, status=%d", status);
	rv = 6;
	goto exit;
    }
    // Create comma separated header value

    resvalsz = 4096;
    resvalp = alloca(resvalsz + 1);
    total_bytes = 0;

    for (n = 0; n < list_nv_used; n++) {

        // BZ-9789 - B
        if ((MIME_HDR_CACHE_CONTROL == token) && (0 == strncasecmp (list_nv[n].name, "max-age", 
            strlen (list_nv[n].name))))
        {
            if (cc_max_age)
                continue;

            cc_max_age = 1;
        }
        // BZ-9789 - E

	if (list_nv[n].value) {
	    bytes = snprintf(resvalp + total_bytes, resvalsz - total_bytes,
			     "%s=%s,", list_nv[n].name, list_nv[n].value);
	} else {
	    bytes =
		snprintf(resvalp + total_bytes, resvalsz - total_bytes,
			 "%s,", list_nv[n].name);
	}
	total_bytes += bytes;
	if (bytes <= 0) {
	    break;
	}
    }
    resvalp[total_bytes - 1] = '\0';	// remove trailing ','
    resvalp[resvalsz] = '\0';

    status = delete_known_header(hd, token);
    if (status) {
	DBG("delete_known_header() failed, status=%d", status);
	rv = 7;
	goto exit;
    }

    status = mime_hdr_add_known(hd, token, resvalp, strlen(resvalp));
    if (status) {
	DBG("mime_hdr_add_known() failed, status=%d", status);
	rv = 8;
	goto exit;
    }

  exit:

    if (list_nv) {
	free(list_nv);
    }
    if (new_list_nv) {
	free(new_list_nv);
    }
    return rv;
}

#undef TOKENIZE

/*
 *******************************************************************************
 * delete_known_header() -- Delete known header
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_del_known(mime_header_t * hd, int token)
{
    int rv = 0;
    dataddr_t hdptr;
    dataddr_t valptr;
    dataddr_t next_valptr;
    heap_data_t * pheap_data;
    heap_data_t * pheap_data_valptr;

    if ((token < MIME_HDR_MAX_DEFS) && hd->known_header_map[token]) {
	hdptr = hd->known_header_map[token];
	pheap_data = OFF2PTR(hd, hdptr);
	valptr = pheap_data->u.v.value_next;

	while (valptr) {
	    pheap_data_valptr = OFF2PTR(hd, valptr);
	    next_valptr = pheap_data_valptr->u.v.value_next;
	    if (pheap_data_valptr->flags & F_VALUE_HEAPDATA_REF) {
		delete_heap_element(hd,
				    DATAOFF_TO_HEAP_ELEMENT(hd,
							    pheap_data_valptr->
							    u.v.value));
		pheap_data_valptr->flags &= ~F_VALUE_HEAPDATA_REF;
	    }
	    delete_heap_element(hd, pheap_data_valptr);
	    valptr = next_valptr;
	}

	if (pheap_data->flags & F_VALUE_HEAPDATA_REF) {
	    delete_heap_element(hd,
				DATAOFF_TO_HEAP_ELEMENT(hd,
							pheap_data->u.
							v.value));
	}

	/* Delete cooked data */
	if (pheap_data->u.v.cooked_data) {	/* Always F_DATA */
	    delete_heap_element(hd,
				DATAOFF_TO_HEAP_ELEMENT(hd,
							pheap_data->u.
							v.cooked_data));
	}

	delete_heap_element(hd, pheap_data);
	hd->known_header_map[token] = 0;
	hd->cnt_known_headers--;

    } else {
	DBG("Not found, token=%d", token);
	rv = 1;			// Not found
    }
    return rv;
}

/*
 *******************************************************************************
 * delete_all_known_headers() -- Delete all known headers
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_del_all_known(mime_header_t * hd)
{
    int n;

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	delete_known_header(hd, n);
    }
    return 0;
}

/*
 *******************************************************************************
 * mime_hdr_del_unknown() -- Delete unknown header by name
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_del_unknown(mime_header_t * hd, const char *name, int namelen)
{

    int rv;
    rv = delete_name_value_by_name(hd, &hd->unknown_header_head,
				   &hd->cnt_unknown_headers, name,
				   namelen);
    if (rv) {
	DBG("delete_name_value_by_name() failed rv=%d", rv);
	rv = 1;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_del_all_unknown() -- Delete all unknown headers
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_del_all_unknown(mime_header_t * hd)
{
    int rv;
    const char *name;
    int namelen;
    const char *val;
    int vallen;
    u_int32_t attr;

    while ((rv =
	    mime_hdr_get_nth_unknown(hd, 0, &name, &namelen, &val, &vallen,
				     &attr)) == 0) {
	mime_hdr_del_unknown(hd, name, namelen);
    }
    return 0;
}

/*
 *******************************************************************************
 * mime_hdr_del_all_namevalue() -- Delete name/value header by name
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_del_namevalue(mime_header_t * hd, const char *name,
			   int namelen)
{

    int rv;
    rv = delete_name_value_by_name(hd, &hd->name_value_head,
				   &hd->cnt_namevalue_headers, name,
				   namelen);
    if (rv) {
	DBG("delete_name_value_by_name() failed rv=%d", rv);
	rv = 1;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_del_all_namevalue() -- Delete all name/value headers
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_del_all_namevalue(mime_header_t * hd)
{
    int rv;
    const char *name;
    int name_len;
    const char *val;
    int val_len;
    u_int32_t attributes;

    while ((rv = mime_hdr_get_nth_namevalue(hd, 0, &name, &name_len,
					    &val, &val_len,
					    &attributes)) == 0) {
	rv = mime_hdr_del_namevalue(hd, name, name_len);
	if (rv) {
	    DBG("delete_name_value_by_name() failed rv=%d", rv);
	    return 1;
	}
    }
    return 0;
}

/*
 *******************************************************************************
 * mime_hdr_is_known_present() -- Determine if known header exists
 *
 *   Return !=0 => Present
 *******************************************************************************
 */
inline int mime_hdr_is_known_present(const mime_header_t * hd, int token)
{

	return (((token < MIME_HDR_MAX_DEFS) &&
			hd->known_header_map[token]));
}

/*
 *******************************************************************************
 * mime_hdr_is_unknown_present() -- Determine if unknown header exists
 *
 *   Return !=0 => Present
 *******************************************************************************
 */
int mime_hdr_is_unknown_present(const mime_header_t * hd, const char *name,
				int len)
{
    int rv;
    dataddr_t heap_data;

    rv = find_name_value_by_name(hd, &hd->unknown_header_head,
				 name, len, &heap_data);
    if (!rv) {
	return 1;
    } else {
	return 0;
    }
}

/*
 *******************************************************************************
 * mime_hdr_is_namevalue_present() -- Determine if name value header exists
 *
 *   Return !=0 => Present
 *******************************************************************************
 */
int mime_hdr_is_namevalue_present(const mime_header_t * hd,
				  const char *name, int len)
{
    int rv;
    dataddr_t heap_data;

    rv = find_name_value_by_name(hd, &hd->name_value_head,
				 name, len, &heap_data);
    if (!rv) {
	return 1;
    } else {
	return 0;
    }
}

/*
 *******************************************************************************
 * int_mime_hdr_get_known() -- Get requested known header data
 *
 *   Return 0 => Success, value [data, len, attribute] corresponding to 
 *               value #0 with header_cnt reflecting the total number of 
 *               value headers
 *
 *  Note: Internal interface users should use MIME_HDR_GET_KNOWN() interface
 *******************************************************************************
 */
int int_mime_hdr_get_known(const mime_header_t * hd, int token,
		       const char **data, int *len, u_int32_t * attributes,
		       int *header_cnt)
{
    int rv = 0;
    heap_data_t *pheap_data;
#if 0
    if ((token >= MIME_HDR_MAX_DEFS) || (hd->known_header_map[token]==0)) {
	rv = 1;			// Invalid token
	goto exit;
    }
#endif

    pheap_data = OFF2PTR(hd, hd->known_header_map[token]);

    *data = pheap_data->flags & F_VALUE_HEAPDATA_REF ?
            OFF2CHAR_PTR(hd, pheap_data->u.v.value) :
            EXT_OFF2CHAR_PTR(hd, pheap_data->u.v.value);
    NKN_ASSERT(*data == CHAR_PTR(hd, valptr, F_VALUE_HEAPDATA_REF));

    *len = pheap_data->u.v.value_len;
    *attributes = pheap_data->flags;
    *header_cnt = pheap_data->u.v.hdrcnt;

#if 0
  exit:
#endif
    if (rv) {
	*data = 0;
	*len = 0;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_get_nth_known() -- Get the nth value data [data, len, attribute] 
 *   for the requested known header
 *
 *   Return 0 => Success
 *
 *******************************************************************************
 */
int mime_hdr_get_nth_known(const mime_header_t * hd, int token,
			   int header_num, const char **data, int *len,
			   u_int32_t * attributes)
{
    int rv;
    rv = mime_hdr_get_known_nth_value(hd, token, header_num,
				      data, len, attributes);
    if (rv) {
	DBG("mime_hdr_get_known_nth_value() failed rv=%d", rv);
	*data = 0;
	*len = 0;
	rv = 1;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_get_unknown() -- Get the requested unknown header data
 *
 *   Return 0 => Success, value data [data, len, attribute]
 *******************************************************************************
 */
int mime_hdr_get_unknown(const mime_header_t * hd, const char *name,
			 int namelen, const char **data, int *len,
			 u_int32_t * attributes)
{
    int rv;
    dataddr_t valptr;
    heap_data_t * pheap_data;

    rv = find_name_value_by_name(hd, &hd->unknown_header_head,
				 name, namelen, &valptr);
    if (!rv) {
	pheap_data = OFF2PTR(hd, valptr);
	*data = pheap_data->flags & F_VALUE_HEAPDATA_REF ?
	    OFF2CHAR_PTR(hd, pheap_data->u.nv.value) :
	    EXT_OFF2CHAR_PTR(hd, pheap_data->u.nv.value);
	NKN_ASSERT(*data == CHAR_PTR(hd, valptr, F_VALUE_HEAPDATA_REF));

	*len = pheap_data->u.nv.value_len;
	*attributes = pheap_data->flags;
    } else {
	DBG("find_name_value_by_name() failed rv=%d", rv);
	*data = 0;
	*len = 0;
	rv = 1;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_get_namevalue() -- Get the requested name/value header data
 *
 *   Return 0 => Success, value data [data, len, attribute]
 *******************************************************************************
 */
int mime_hdr_get_namevalue(const mime_header_t * hd, const char *name,
			   int namelen, const char **data, int *len,
			   u_int32_t * attributes)
{
    int rv;
    dataddr_t valptr;
    heap_data_t * pheap_data;

    rv = find_name_value_by_name(hd, &hd->name_value_head,
				 name, namelen, &valptr);
    if (!rv) {
	pheap_data = (heap_data_t *)OFF2PTR(hd, valptr);
	*data = pheap_data->flags & F_VALUE_HEAPDATA_REF ?
	    OFF2CHAR_PTR(hd, pheap_data->u.nv.value) :
	    EXT_OFF2CHAR_PTR(hd, pheap_data->u.nv.value);
	NKN_ASSERT(*data == CHAR_PTR(hd, valptr, F_VALUE_HEAPDATA_REF));

	*len = pheap_data->u.nv.value_len;
	*attributes = pheap_data->flags;
    } else {
	DBG("find_name_value_by_name() failed rv=%d", rv);
	*data = 0;
	*len = 0;
	rv = 1;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_get_nth_unknown() -- Get the nth unknown header
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_get_nth_unknown(const mime_header_t * hd, int arg_num,
			     const char **name, int *name_len,
			     const char **val, int *val_len,
			     u_int32_t * attributes)
{
    int rv;

    rv = get_nth_list_element(hd, &hd->unknown_header_head,
			      arg_num, name, name_len, val, val_len,
			      attributes);
    if (rv) {
	DBG("get_nth_list_element() failed rv=%d", rv);
	rv = 1;
	*name = 0;
	*name_len = 0;
	*val = 0;
	*val_len = 0;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_get_nth_namevalue() -- Get the nth name/value header
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_get_nth_namevalue(const mime_header_t * hd, int arg_num,
			       const char **name, int *name_len,
			       const char **val, int *val_len,
			       u_int32_t * attributes)
{
    int rv;

    rv = get_nth_list_element(hd, &hd->name_value_head,
			      arg_num, name, name_len, val, val_len,
			      attributes);
    if (rv) {
	DBG("get_nth_list_element() failed rv=%d", rv);
	rv = 1;
	*name = 0;
	*name_len = 0;
	*val = 0;
	*val_len = 0;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_get_query_arg() -- Get the nth name/value element
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_get_query_arg(mime_header_t * hd, int arg_num,
			   const char **name, int *name_len,
			   const char **val, int *val_len)
{
    int rv;
    u_int32_t attr;

    if ((rv = parse_query_string(hd))) {
	DBG("parse_query_string() failed rv=%d", rv);
	rv = 1;
	goto exit;
    }

    rv = get_nth_list_element(hd, &hd->query_string_head,
			      arg_num, name, name_len, val, val_len,
			      &attr);
    if (rv) {
	DBG("get_nth_list_element() failed rv=%d", rv);
	rv = 2;
	goto exit;
    }
  exit:
    if (rv) {
	*name = 0;
	*name_len = 0;
	*val = 0;
	*val_len = 0;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_get_query_arg_by_name() -- Get the query value associated with the given name
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_get_query_arg_by_name(mime_header_t * hd, const char *name,
				   int namelen, const char **val,
				   int *val_len)
{
    int rv;
    dataddr_t valptr;
    heap_data_t * pheap_data;

    if ((rv = parse_query_string(hd))) {
	DBG("parse_query_string() failed rv=%d", rv);
	rv = 1;
	goto exit;
    }

    rv = find_name_value_by_name(hd, &hd->query_string_head,
				 name, namelen, &valptr);
    if (!rv) {
	pheap_data = OFF2PTR(hd, valptr);
	*val = pheap_data->flags & F_VALUE_HEAPDATA_REF ?
	    OFF2CHAR_PTR(hd, pheap_data->u.nv.value) :
	    EXT_OFF2CHAR_PTR(hd, pheap_data->u.nv.value);
	NKN_ASSERT(*val == CHAR_PTR(hd, valptr, F_VALUE_HEAPDATA_REF));

	*val_len = pheap_data->u.nv.value_len;
    } else {
	DBG("find_name_value_by_name() failed rv=%d", rv);
	rv = 2;
    }
  exit:
    if (rv) {
	*val = 0;
	*val_len = 0;
    }
    return rv;
}

/*
 *******************************************************************************
 * get_header_counts() -- Get header stat data
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_get_header_counts(const mime_header_t * hd,
			       int *cnt_known_headers,
			       int *cnt_unknown_headers,
			       int *cnt_namevalue_headers,
			       int *cnt_querystring_headers)
{
    *cnt_known_headers = hd->cnt_known_headers;
    *cnt_unknown_headers = hd->cnt_unknown_headers;
    *cnt_namevalue_headers = hd->cnt_namevalue_headers;
    *cnt_querystring_headers = hd->cnt_querystring_headers;
    return 0;
}

/*
 *******************************************************************************
 * copy_http_headers() -- Copy headers from given src to dest mime_header_t
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_copy_headers(mime_header_t * dest, const mime_header_t * src)
{
    int rv;
    int n;
    int nth;
    const char *name;
    int namelen;
    const char *data;
    int datalen;
    u_int32_t attr;
    int hdrcnt;

    // Copy known headers

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = mime_hdr_get_known(src, n, &data, &datalen, &attr, &hdrcnt);
	if (!rv) {
	    rv = mime_hdr_add_known(dest, n, data, datalen);
	    if (rv) {
		DBG("mime_hdr_add_known() failed rv=%d", rv);
	    	return 1;
	    }
	} else {
	    continue;
	}
	for (nth = 1; nth < hdrcnt; nth++) {
	    rv = mime_hdr_get_nth_known(src, n, nth, &data, &datalen,
					&attr);
	    if (!rv) {
		rv = mime_hdr_add_known(dest, n, data, datalen);
		if (rv) {
		    DBG("mime_hdr_add_known() failed rv=%d", rv);
		    return 2;
		}
	    } else {
		DBG("mime_hdr_get_nth_known() failed rv=%d nth=%d", rv, nth);
	    }
	}
    }

    // Copy unknown headers

    n = 0;
    while ((rv = mime_hdr_get_nth_unknown(src, n, &name, &namelen,
					  &data, &datalen, &attr)) == 0) {
	rv = mime_hdr_add_unknown(dest, name, namelen, data, datalen);
	if (rv) {
	    DBG("mime_hdr_add_unknown() failed rv=%d", rv);
	    return 3;
	}
	n++;
    }

    return 0;
}

/*
 *******************************************************************************
 * mime_hdr_copy_known_header() -- Copy noted known header from src to dest 
 *				mime_header_t
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_copy_known_header(mime_header_t * dest, const mime_header_t * src,
			       int token)
{
    int rv;
    int nth;
    const char *data;
    int datalen;
    u_int32_t attr;
    int hdrcnt;

    // Copy known header

    rv = mime_hdr_get_known(src, token, &data, &datalen, &attr, &hdrcnt);
    if (!rv) {
	rv = mime_hdr_add_known(dest, token, data, datalen);
	if (rv) {
	    DBG("mime_hdr_add_known() failed rv=%d", rv);
	    return 1;
	}
    } else {
	return 0; // nothing to copy
    }

    for (nth = 1; nth < hdrcnt; nth++) {
	rv = mime_hdr_get_nth_known(src, token, nth, &data, &datalen,
				    &attr);
	if (!rv) {
	    rv = mime_hdr_add_known(dest, token, data, datalen);
	    if (rv) {
	    	DBG("mime_hdr_add_known() failed rv=%d", rv);
	    	return 2;
	    }
	} else {
	    DBG("mime_hdr_get_nth_known() failed rv=%d nth=%d", rv, nth);
	}
    }

    return 0;
}

/*
 *******************************************************************************
 * mime_hdr_serialize_datasize() -- Determine http_header serialization size 
 *   in bytes.
 *
 *   Return !=0 => Success
 *******************************************************************************
 */
STATIC
int int_mime_hdr_serialize_datasize(const mime_header_t * hd, 
				int *r_heap_int_size,
				int *r_heap_ext_size, int *r_ext_data_size)
{
    int rv;
    int sparse_known_header_map_size;
    int heap_int_size;
    int heap_ext_size;
    int ext_data_size;

    sparse_known_header_map_size = hd->cnt_known_headers * sizeof(dataddr_t);

    if (hd->heap_free_index <= 
    	(int)(MIME_HEADER_HEAPSIZE/sizeof(heap_data_t))) {
    	heap_int_size = hd->heap_free_index * sizeof(heap_data_t);
	heap_ext_size = 0;
    } else {
    	heap_int_size = MIME_HEADER_HEAPSIZE;
	heap_ext_size = 
	    (hd->heap_free_index - (MIME_HEADER_HEAPSIZE/sizeof(heap_data_t))) *
	    sizeof(heap_data_t);
    }

    if (hd->ext_data && hd->ext_data_size) {
    	rv = mime_hdr_pack_ext_data((mime_header_t *)hd, 0, 0, 0, 0, 
				    0, 0, &ext_data_size);
    	if (rv) {
	    DBG("mime_hdr_pack_ext_data() failed, rv=%d", rv);
	    return 0;
    	}
    } else {
    	ext_data_size = 0;
    }

    if (r_heap_int_size) {
    	*r_heap_int_size = heap_int_size;
    }

    if (r_heap_ext_size) {
    	*r_heap_ext_size = heap_ext_size;
    }

    if (r_ext_data_size) {
    	*r_ext_data_size = ext_data_size;
    }
    return (sizeof(serial_mime_header_t) + sparse_known_header_map_size +
	    heap_int_size + heap_ext_size + ext_data_size);
}

int mime_hdr_serialize_datasize(const mime_header_t * input_hd, 
				int *r_heap_int_size,
				int *r_heap_ext_size, int *r_ext_data_size)
{
    int rv = 0;
    int retval;

    mime_header_t compressed_hd;
    int init_compressed_hd = 0;
    const mime_header_t *hd;
    const char *ext_data_2;
    int ext_data_2_length;
    const char *ext_data_3;
    int ext_data_3_length;

    if (input_hd->ext_data && input_hd->ext_data_size) {
    	mime_hdr_init(&compressed_hd, MIME_PROT_HTTP, 0, 0);
    	init_compressed_hd = 1;
    	retval = mime_hdr_compress(input_hd, &compressed_hd,
					&ext_data_2, &ext_data_2_length,
			       		&ext_data_3, &ext_data_3_length);
    	if (retval) {
	    DBG("mime_hdr_compress() failed, rv=%d", retval);
	    rv = 0;
	    goto exit;
    	}
	hd = &compressed_hd;
    } else {
    	hd = input_hd;
    }
    rv = int_mime_hdr_serialize_datasize(hd, r_heap_int_size, 
					 r_heap_ext_size, r_ext_data_size);
exit:
    if (init_compressed_hd) {
    	mime_hdr_shutdown(&compressed_hd);
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_serialize() -- Serialize http_header into given buffer
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_serialize(const mime_header_t * input_hd, char *outbuf,
		       int outbuf_size)
{
    int rv = 0;
    int retval;
    int heap_int_size;
    int heap_ext_size; 
    int ext_data_size;
    int req_bufsize;
    serial_mime_header_t *pshdr;
    int n, khcnt;
    char *pdest;
    int bytes_used;

    mime_header_t compressed_hd;
    int init_compressed_hd = 0;
    const mime_header_t *hd;
    const char *ext_data_2;
    int ext_data_2_length;
    const char *ext_data_3;
    int ext_data_3_length;

    if (input_hd->ext_data && input_hd->ext_data_size) {
    	mime_hdr_init(&compressed_hd, MIME_PROT_HTTP, 0, 0);
    	init_compressed_hd = 1;
    	retval = mime_hdr_compress(input_hd, &compressed_hd,
					&ext_data_2, &ext_data_2_length,
			       		&ext_data_3, &ext_data_3_length);
    	if (retval) {
	    DBG("mime_hdr_compress() failed, rv=%d", retval);
	    rv = 1;
	    goto exit;
    	}
	hd = &compressed_hd;
    } else {
    	hd = input_hd;
    }

    req_bufsize = int_mime_hdr_serialize_datasize(hd, 
    				&heap_int_size, &heap_ext_size, &ext_data_size);

    if (!outbuf || !req_bufsize || (outbuf_size < req_bufsize)) {
	DBG("Invalid parameters outbuf=%p outbuf_size=%d "
	    "req bufsize=%d",
	    outbuf, outbuf_size, req_bufsize);
	rv = 2;
    } else {
	pshdr = (serial_mime_header_t *) outbuf;
	pshdr->magic = SERIAL_MIME_HEADER_MAGIC;
	pshdr->total_size = req_bufsize;

	pshdr->protocol = hd->protocol;
	pshdr->version = hd->version;
	pshdr->cnt_known_headers = hd->cnt_known_headers;
	pshdr->cnt_unknown_headers = hd->cnt_unknown_headers;
	pshdr->cnt_namevalue_headers = hd->cnt_namevalue_headers;
	pshdr->cnt_querystring_headers = hd->cnt_querystring_headers;
	pshdr->unknown_header_head = hd->unknown_header_head;
	pshdr->query_string_head = hd->query_string_head;
	pshdr->name_value_head = hd->name_value_head;
	pshdr->heap_free_index = hd->heap_free_index;
	pshdr->heap_frees = hd->heap_frees;

	memset(pshdr->known_header_bitmap, 0, 
	       sizeof(pshdr->known_header_bitmap));
	for (n = 0, khcnt = 0; n < MIME_MAX_HDR_DEFS; n++) {
	    if (hd->known_header_map[n]) {
	    	setbit(pshdr->known_header_bitmap, n);
    		((dataddr_t *)
		 (outbuf + sizeof(serial_mime_header_t)))[khcnt++] = 
		    hd->known_header_map[n];
	    }
	}
	pshdr->heap_size = heap_int_size;
	pshdr->heap_ext_size = heap_ext_size;
	pshdr->ext_data_size = ext_data_size;

	/* External data */
	if (hd->ext_data && pshdr->ext_data_size) {
	    pdest = outbuf + sizeof(serial_mime_header_t) + 
		    (khcnt * sizeof(dataddr_t)) + pshdr->heap_size +
		    pshdr->heap_ext_size;
	    /*
	     * Indicate pdest is hd->ext_data_size in length to 
	     * keep mime_hdr_pack_ext_data() happy even though pdest is
	     * smaller.   
	     * No chance of overflow since we obtained the required 
	     * buffer size in the previous call to mime_hdr_pack_ext_data()
	     */
	    rv = mime_hdr_pack_ext_data((mime_header_t *)hd, 
	    				ext_data_2, ext_data_2_length,
	    				ext_data_3, ext_data_3_length,
	    				pdest, 
	    				hd->ext_data_size, &bytes_used);
	    if (rv || (bytes_used != ext_data_size)) {
		DBG("mime_hdr_pack_ext_data() failed, rv=%d "
		    "bytes_used=%d ext_data_size=%d", 
		    rv, bytes_used, ext_data_size);
		rv = 3;
	    }
	}

	/*
	 * Note: Heap copy must come after external data copy since
	 *       mime_hdr_pack_ext_data() alters heap entries.
	 */

	/* Internal heap */
	if (pshdr->heap_size) {
	    pdest = outbuf + sizeof(serial_mime_header_t) + 
		    (khcnt * sizeof(dataddr_t));
	    memcpy(pdest, hd->heap, pshdr->heap_size);
	}

	/* External heap */
	if (hd->heap_ext && pshdr->heap_ext_size) {
	    pdest = outbuf + sizeof(serial_mime_header_t) + 
		    (khcnt * sizeof(dataddr_t)) + pshdr->heap_size;
	    memcpy(pdest, hd->heap_ext, pshdr->heap_ext_size);
	}
    }
exit:
    if (init_compressed_hd) {
    	mime_hdr_shutdown(&compressed_hd);
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_deserialize() -- Deserialize buffer into http_header and given 
 *   extdata.
 *   If (extdata == 0) place it in the local heap
 *   If (extdata == 1) use extdata directly from the serialized buffer
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_deserialize(const char *inbuf, int inbuf_size,
			 mime_header_t * hd, const char *extdatabuf,
			 int extdatabufsz)
{
    int rv = 0;
    serial_mime_header_t shdr;
    int data_totalsize;
    dataddr_t hpd;
    dataddr_t *sparse_known_header_map;
    char *heap;
    char *heap_ext;
    char *ext_data;

    while (1) {
	if (!inbuf || !inbuf_size || !hd) {
	    DBG("Invalid parameters, inbuf=%p inbufsz=%d hd=%p",
		inbuf, inbuf_size, hd);
	    rv = 1;
	    break;
	}

	if (inbuf_size < (int) sizeof(serial_mime_header_t)) {
	    DBG("inbuf_size(%d) < sizeof(serial_mime_header_t)(%ld)",
		inbuf_size, sizeof(serial_mime_header_t));
	    rv = 2;
	    break;
	}

	memcpy((void *) &shdr, (void *) inbuf,
	       sizeof(serial_mime_header_t));
	if (shdr.magic != SERIAL_MIME_HEADER_MAGIC) {
	    DBG("shdr.magic(0x%x) != SERIAL_MIME_HEADER_MAGIC(0x%x)",
		shdr.magic, SERIAL_MIME_HEADER_MAGIC);
	    rv = 3;
	    break;
	}

	if (shdr.version != MIME_HEADER_VERSION) {
	    DBG("shdr.version(0x%x) != MIME_HEADER_VERSION(0x%x)",
		shdr.version, MIME_HEADER_VERSION);
	    rv = 4;
	    break;
	}

	data_totalsize = sizeof(serial_mime_header_t) +
	    (shdr.cnt_known_headers * sizeof(dataddr_t)) +
	    shdr.heap_size + shdr.heap_ext_size + shdr.ext_data_size;

	if (shdr.total_size != data_totalsize) {
	    DBG("shdr.total_size(%d) != data_totalsize(%d)",
		shdr.total_size, data_totalsize);
	    rv = 5;
	    break;
	}

	if (data_totalsize > inbuf_size) {
	    DBG("data_totalsize(%d) > inbuf_size(%d)",
		data_totalsize, inbuf_size);
	    rv = 6;
	    break;
	}

	if (extdatabuf && (extdatabuf != (char *)1) && 
	    (shdr.ext_data_size > extdatabufsz)) {
	    DBG("shdr.ext_data_size(%d) > extdatabufsz(%d)",
		shdr.ext_data_size, extdatabufsz);
	    rv = 7;
	    break;
	}

	/* Compute pointers to variable length data */
	sparse_known_header_map = 
	    (dataddr_t *)(inbuf + sizeof(serial_mime_header_t));
	heap = (char *)sparse_known_header_map + 
	       (shdr.cnt_known_headers * sizeof(dataddr_t));
	heap_ext = heap + shdr.heap_size;
	ext_data = heap_ext + shdr.heap_ext_size;

	/* Initialize the given mime_header_t ext_data */
	if (extdatabuf) {
	    if (extdatabuf == (char *)1) {
		hd->ext_data = ext_data;
		hd->ext_data_size = shdr.ext_data_size;
	    } else {
		hd->ext_data = extdatabuf;
		hd->ext_data_size = extdatabufsz;
	    }
	} else { 
	    hd->ext_data = 0;
	    hd->ext_data_size = 0;
	}

	mime_header_check_and_copy((char *)&shdr.known_header_bitmap,
				   hd->known_header_map,
				   sparse_known_header_map);
	hd->cnt_known_headers = shdr.cnt_known_headers;
	hd->cnt_unknown_headers = shdr.cnt_unknown_headers;
	hd->cnt_namevalue_headers = shdr.cnt_namevalue_headers;
	hd->cnt_querystring_headers = shdr.cnt_querystring_headers;
	hd->unknown_header_head = shdr.unknown_header_head;
	hd->query_string_head = shdr.query_string_head;
	hd->name_value_head = shdr.name_value_head;

	memcpy(hd->heap, heap, shdr.heap_size);
	hd->heap_free_index = shdr.heap_free_index;
	hd->heap_frees = shdr.heap_frees;

	if (shdr.heap_ext_size) {
	    hd->heap_ext =
		nkn_malloc_type(shdr.heap_ext_size, mod_http_heap_ext);
	    if (!hd->heap_ext) {
		DBG("nkn_malloc_type(%d) failed",
		    shdr.heap_ext_size);
		rv = 8;
		break;
	    }
	    memcpy(hd->heap_ext, heap_ext, shdr.heap_ext_size);
	    hd->heap_ext_size = shdr.heap_ext_size;
	}

	/*
	 * Heap allocation can only occur after the internal/external 
	 * heaps have been initialized
	 */
	if (!extdatabuf && shdr.ext_data_size) {
	    /* Allocate external data from heap */
	    hpd = get_heap_element(hd, F_DATA, shdr.ext_data_size);
	    if (hpd) {
		hd->ext_data = OFF2PTR(hd, hpd)->u.d.data;
	    } else {
		DBG("get_heap_element(F_DATA, %d) failed", shdr.ext_data_size);
		rv = 9;
		break;
	    }
	}
	hd->ext_data_size = shdr.ext_data_size;

	if (hd->ext_data_size) {
	    if (hd->ext_data != ext_data) {
	    	memcpy((char *)hd->ext_data, ext_data, hd->ext_data_size);
	    }
	}

	/*
	 * Update hdrcnt
	 */
	mime_header_update_hdrcnt(hd);
	break;
    } /* End of while */

    return rv;
}

/*
 ******************************************************************************
 * mk_rfc1123_time -- Create RFC 1123 from time_t
 *
 *  Return: Pointer to string in given buffer
 ******************************************************************************
 */
char *mk_rfc1123_time(const time_t * t, char *buf, int bufsz, int *datestr_len)
{
#define STRFTIME_FMT_RFC1123 "%a, %d %b %Y %H:%M:%S GMT"
    struct tm gmt;
    int ret;

    gmtime_r(t, &gmt);
    if (!buf) {
	bufsz = 128;
	buf = nkn_malloc_type(bufsz, mod_http_charbuf);
    }
    *buf = '\0';
    ret = strftime(buf, bufsz, STRFTIME_FMT_RFC1123, &gmt);
    if (datestr_len) *datestr_len = ret;

    return buf;
}

/*
 *******************************************************************************
 * parse_date() -- Parse the following date formats
 *
 * Sat, 06 Nov 1999 11:47:37 GMT  [RFC 1123]
 * Sunday, 06-Nov-99 11:47:37 GMT [RFC 850]
 * Sun Nov  6 11:47:37 1999       [asctime]
 *
 * Return: time_t
 *******************************************************************************
 */
time_t parse_date(const char *buf, const char *buf_end)
{
    int rv;
    struct tm tp;

    time_t t;
    if (!buf) {
	return (time_t) 0;
    }

    /* Remove white space */
    while ((buf != buf_end) && IS_WS(*buf)) {
	buf++;
    }

    if ((buf_end && ((buf_end - buf + 1) >= 29)) && (buf[3] == ',')) {
	rv = fast_parse_rfc1122_date(buf, buf_end - buf + 1, &tp);
	if (rv) {
	    DBG("fast_parse_rfc1122_date() failed rv=%d", rv);
	    goto slow_path;
	}
    } else {

      slow_path:

	rv = parse_day(&buf, buf_end, &tp.tm_wday);
	if (rv) {
	    DBG("parse_day() failed rv=%d", rv);
	    return (time_t) 0;
	}

	/* Remove white space */
	while ((buf != buf_end) && IS_WS(*buf)) {
	    buf++;
	}

	if ((buf != buf_end) && ((*buf == ',') || IS_DIGIT(*buf))) {
	    /* RFC 1123 or RFC 850 */
	    rv = parse_mday(&buf, buf_end, &tp.tm_mday);
	    if (rv) {
		DBG("parse_mday() failed rv=%d", rv);
		return (time_t) 0;
	    }
	    rv = parse_month(&buf, buf_end, &tp.tm_mon);
	    if (rv) {
		DBG("parse_month() failed rv=%d", rv);
		return (time_t) 0;
	    }
	    rv = parse_year(&buf, buf_end, &tp.tm_year);
	    if (rv) {
		DBG("parse_year() failed rv=%d", rv);
		return (time_t) 0;
	    }
	    rv = parse_time(&buf, buf_end, &tp.tm_hour,
			    &tp.tm_min, &tp.tm_sec);
	    if (rv) {
		DBG("parse_time() failed rv=%d", rv);
		return (time_t) 0;
	    }
	} else {
	    /* ascii time format */
	    rv = parse_month(&buf, buf_end, &tp.tm_mon);
	    if (rv) {
		DBG("parse_month() failed rv=%d", rv);
		return (time_t) 0;
	    }
	    rv = parse_mday(&buf, buf_end, &tp.tm_mday);
	    if (rv) {
		DBG("parse_mday() failed rv=%d", rv);
		return (time_t) 0;
	    }
	    rv = parse_time(&buf, buf_end, &tp.tm_hour,
			    &tp.tm_min, &tp.tm_sec);
	    if (rv) {
		DBG("parse_time() failed rv=%d", rv);
		return (time_t) 0;
	    }
	    rv = parse_year(&buf, buf_end, &tp.tm_year);
	    if (rv) {
		DBG("parse_year() failed rv=%d", rv);
		return (time_t) 0;
	    }
	}
    }
    tp.tm_isdst = 0;

    if (!have_local_tm) {
	t = time(0);
	localtime_r(&t, &local_tm);
    }
    t = mktime(&tp);
    /* Note: For now, assume DST offset = 3600 secs */
    t = t + local_tm.tm_gmtoff - (local_tm.tm_isdst > 0 ? 60 * 60 : 0);

    return t;
}

/*
 *******************************************************************************
 * cook_known_header_data() -- Cook the given known header data and place
 *   it in the local heap.
 *
 *   Return 0 => Success; type denotes the data type for [data, len] 
 *******************************************************************************
 */
int mime_hdr_get_known_cooked_data(mime_header_t * hd, int token,
				   const cooked_data_types_t ** data,
				   int *dlen, mime_hdr_datatype_t * type)
{
    int rv = 0;
    char *p;
    int len;
    char *outbuf;
    int outbufsz;
    int num_vals;
    int total_val_bytecount;
    dataddr_t hpd;
    cooked_data_types_t t_cd;
    heap_data_t * pheap_data;

    if ((token >= MIME_HDR_MAX_DEFS) || !hd->known_header_map[token]) {
	DBG("Invalid token=%d", token);
	rv = 1;
	goto exit;
    }

    hpd = hd->known_header_map[token];
    pheap_data = OFF2PTR(hd, hpd);
    if (!(pheap_data->u.v.cooked_data)) {
	p = pheap_data->flags & F_VALUE_HEAPDATA_REF ?
	    OFF2CHAR_PTR(hd, pheap_data->u.v.value) :
	    EXT_OFF2CHAR_PTR(hd, pheap_data->u.v.value);
	NKN_ASSERT(p == CHAR_PTR(hd, hpd, F_VALUE_HEAPDATA_REF));

	len = pheap_data->u.v.value_len;

	switch (http_known_headers[token].type) {
	case DT_INT:
	    if (len) {
		t_cd.u.dt_int.l = atol_len(p, len);
	    } else {
		t_cd.u.dt_int.l = 0;
	    }
	    rv = add_cooked_heap_value_data(hd, hpd,
					    (char *) &t_cd.u.dt_int,
					    sizeof(t_cd.u.dt_int));
	    break;

	case DT_DATE_1123:
	    if (len) {
		t_cd.u.dt_date_1123.t = parse_date(p, p + (len - 1));
	    } else {
		t_cd.u.dt_date_1123.t = 0;
	    }
	    rv = add_cooked_heap_value_data(hd, hpd,
					    (char *) &t_cd.u.dt_date_1123,
					    sizeof(t_cd.u.dt_date_1123));
	    break;

	case DT_CACHECONTROL_ENUM:
	    if (pheap_data->u.v.value_next) {
		rv = count_known_header_values(hd, token, &num_vals,
					       &total_val_bytecount);
		outbufsz = total_val_bytecount + num_vals + 1;
		outbuf = (char *) alloca(outbufsz);
		p = concatenate_known_header_values(hd, token, outbuf,
						    outbufsz);
		if (!p) {
		    DBG("concatenate_known_header_values(token=%d) failed",
			token);
		    rv = 2;
		    goto exit;
		}
		len = strlen(p);
	    }
	    rv = parse_cache_control_enum(p, p + (len - 1), &t_cd);
	    if (rv) {
		DBG("parse_cache_control_enum() failed rv=%d", rv);
		rv = 3;
		goto exit;
	    }
	    rv = add_cooked_heap_value_data(hd, hpd,
					    (char *) &t_cd.u.
					    dt_cachecontrol_enum,
					    sizeof(t_cd.u.
						   dt_cachecontrol_enum));
	    break;

	case DT_CONTENTRANGE:
	    rv = parse_contentrange(p, p + (len - 1), &t_cd);
	    if (rv) {
		DBG("parse_contentrange() failed rv=%d", rv);
		rv = 4;
		goto exit;
	    }
	    rv = add_cooked_heap_value_data(hd, hpd,
					    (char *) &t_cd.u.
					    dt_contentrange,
					    sizeof(t_cd.u.
						   dt_contentrange));
	    break;

	case DT_RANGE:
	    rv = parse_range(p, p + (len - 1), &t_cd);
	    if (rv) {
		DBG("parse_range() failed rv=%d", rv);
		rv = 5;
		goto exit;
	    }
	    rv = add_cooked_heap_value_data(hd, hpd,
					    (char *) &t_cd.u.dt_range,
					    sizeof(t_cd.u.dt_range));
	    break;

	case DT_SIZE:
	    if (pheap_data->u.v.value_len) {
		t_cd.u.dt_size.ll = atoll_len((pheap_data->
					       flags & F_VALUE_HEAPDATA_REF
					       ? OFF2CHAR_PTR(hd,
							      pheap_data->
							      u.v.
							      value) :
					       EXT_OFF2CHAR_PTR(hd,
							      pheap_data->
								u.v.
								value)),
					      (int) pheap_data->u.v.
					      value_len);
	    } else {
		t_cd.u.dt_size.ll = 0;
	    }
	    rv = add_cooked_heap_value_data(hd, hpd,
					    (char *) &t_cd.u.dt_size,
					    sizeof(t_cd.u.dt_size));
	    break;

	case DT_STRING:
	case DT_ETAG:
	case DT_DATE_1123_OR_ETAG:
	default:
	    /* No action, just leave it as a [str,len] type */
	    break;
	}

	if (rv) {
	    DBG("add_cooked_heap_value_data() failed rv=%d type=%d",
		rv, http_known_headers[token].type);
	    rv = 6;
	    goto exit;
	}
    }

    pheap_data = OFF2PTR(hd, hpd);	// Recalculate the offset, as buffer might have been realloced

    switch (http_known_headers[token].type) {
    case DT_INT:
	*dlen = sizeof(((cooked_data_types_t *) (0))->u.dt_int);
	break;
    case DT_DATE_1123:
	*dlen = sizeof(((cooked_data_types_t *) (0))->u.dt_date_1123);
	break;
    case DT_CACHECONTROL_ENUM:
	*dlen =
	    sizeof(((cooked_data_types_t *) (0))->u.dt_cachecontrol_enum);
	break;
    case DT_CONTENTRANGE:
	*dlen = sizeof(((cooked_data_types_t *) (0))->u.dt_contentrange);
	break;
    case DT_RANGE:
	*dlen = sizeof(((cooked_data_types_t *) (0))->u.dt_range);
	break;
    case DT_SIZE:
	*dlen = sizeof(((cooked_data_types_t *) (0))->u.dt_size);
	break;
    case DT_STRING:
    case DT_ETAG:
    case DT_DATE_1123_OR_ETAG:
    default:
	*dlen = pheap_data->u.v.value_len;
	break;
    }

    switch (http_known_headers[token].type) {
    case DT_INT:
    case DT_DATE_1123:
    case DT_CACHECONTROL_ENUM:
    case DT_CONTENTRANGE:
    case DT_RANGE:
    case DT_SIZE:
	*data = (cooked_data_types_t *)
	    OFF2CHAR_PTR(hd, pheap_data->u.v.cooked_data);
	break;

    case DT_STRING:
    case DT_ETAG:
    case DT_DATE_1123_OR_ETAG:
    default:
	p = pheap_data->flags & F_VALUE_HEAPDATA_REF ?
	    	OFF2CHAR_PTR(hd, pheap_data->u.v.value) :
	    	EXT_OFF2CHAR_PTR(hd, pheap_data->u.v.value);
	*data = (cooked_data_types_t *) p;
	break;
    }

    *type = http_known_headers[token].type;

  exit:

    return rv;
}

/** 
 * @brief HEX to char Mapping
 */
static const char HEX_CHAR_TABLE[0x0F + 1] = {
    '0', '1', '2', '3', '4', '5', '6', '7', 
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' 
};

#define HEX_CHAR(x_nibble) (HEX_CHAR_TABLE[x_nibble&0x0F])

/** 
 * @brief Does escape encoding for a string (RFC 2396)
 * 
 * @param dst_str - Destination memory location to write encoded str
 * @param dst_strlen - Length or size of destination memory
 * @param src_str - Source string to be encoded
 * @param src_strlen - Source String length
 * @param used_len - Size used in the Destination buffer
 * 
 * @return 0 - success
 *         < 0 - failure
 */
int
escape_str(char *dst_str, int dst_strlen, const char *src_str,
	   int src_strlen, int *used_len)
{
    const char *scan = src_str;
    const char *end = src_str + src_strlen;
    char *dst_scan = dst_str;
    char *dst_end = dst_str + dst_strlen;
    int len = 0;
    if (!src_str || !dst_str)
	return -1;
    while (scan < end) {
	if (dst_scan >= dst_end) {
	    return -2;
	}
	if (is_escaped(*scan)) {
	    if ((dst_end - dst_scan) < 3) {
		return (-2);
	    }
	    *dst_scan++ = '%';
	    *dst_scan++ = HEX_CHAR(((*scan)>>0x04)& 0x0F);
	    *dst_scan++ = HEX_CHAR((*scan)& 0x0F);
	} else {
	    *dst_scan++ = *scan;
	}
	scan++;
    }
    len = dst_scan - dst_str;
    dst_scan[len] = '\0';
    if(used_len){
	*used_len = len;
    }
    return (0);
}

/*
 *******************************************************************************
 * canonicalize_url() -- Translate URL into an intermediate form allowing
 *   URL comparison.
 *
 *   Return 0 => Success, output buf is NULL terminated
 *******************************************************************************
 */
int canonicalize_url(const char *url, int url_len, char *output,
		     int output_bufsize, int *output_bytesused)
{
    char *p;

    if (!url || !url_len || !output || !output_bufsize) {
	DBG("Invalid parameters: url=%p url_len=%d "
	    "output=%p output_bufsize=%d",
	    url, url_len, output, output_bufsize);
	return 1;
    }

    /*
     *  Input cases:
     *    1) Forward proxy
     *       http://xyz.com:80/index.html
     *
     *    2) Reverse proxy
     *       /index.html
     */
#if 0
    p = memchr(url, ':', MIN(url_len, 16));
    if (p) {
	TSTR(url, url_len, tp);
	DBG("Forward proxy URL [%s] not supported", tp);
	return 2;
    } else {
#else
    {
#endif
	/* Case #2 */
	p = unescape_str(url, url_len, 0, output, output_bufsize,
			 output_bytesused);
    }
    return 0;
}

/*
 *******************************************************************************
 * Normalize uri_path by removing "." and ".." entries.
 * Path is assumed to be absolute (i.e starts with "/").
 *
 * in: uri_path, uri_pathlen
 * in: outbuf -- (outbuf == 0) => malloc buffer 
 * in/out: outbuf_len -- return strlen(outbuf)
 * out: modifications -- (modifications == 0) => no uri_path changes
 * out: errcode -- (errcode == 0) => Success
 *
 *******************************************************************************
 */
char *normalize_uri_path(const char *uri_path, int uri_pathlen,
			 char *outbuf, int *outbuf_len,
			 int *modifications, int *errcode)
{
    typedef struct centry {
	struct centry *next;
	struct centry *prev;
	int len;
	char cname[];		// Null terminated
    } centry_t;

    centry_t list_head;
    centry_t *p_centry;
    centry_t *p_cur_centry;

    const char *p_start;
    const char *p_end;
    const char *p_query;
    char *malloced_outbuf = 0;
    char *p_out;
    int valid_bytes;
    int clen;
    int mods;
    int plen, qlen;

    int error;

    /* Input validation */

    if (!uri_path || !(*uri_path)) {
	error = 1;		// Invalid input buffer specified
	goto err;
    }

    if (*uri_path != '/') {	// Only allow absolute URIs
	error = 2;
	goto err;
    }

    if (!uri_pathlen) {
	error = 3;		// Input buffer len is zero
	goto err;
    }

    if (outbuf) {
	if (!outbuf_len || !(*outbuf_len)) {
	    error = 4;		// Output buf specified, but no length
	    goto err;
	}

	if (*outbuf_len < (uri_pathlen + 1)) {
	    error = 5;		// Output buf specified is to small
	    goto err;
	}
    } else {
	malloced_outbuf =
	    nkn_malloc_type(uri_pathlen + 1, mod_http_charbuf);
	if (!malloced_outbuf) {
	    error = 6;		// nkn_malloc_type() failed
	    goto err;
	}
	if (outbuf_len) {
	    *outbuf_len = uri_pathlen + 1;
	}
    }

    /*
     * Create a doubly linked list of each of the URI path components and
     * process the "." (remove) and the ".." (delete previous entry)
     * components.
     */
    list_head.next = &list_head;
    list_head.prev = &list_head;

    p_start = uri_path;
    p_query = memchr(p_start, '?', uri_pathlen);
    qlen = mods = 0;
    if ( p_query != NULL ) {
	plen = p_query - p_start;
	qlen = uri_pathlen - plen;
    }
    else
    	plen = uri_pathlen;

    while (p_start && (p_start < (uri_path + plen))) {
	valid_bytes = plen - ((p_start + 1) - uri_path);

	// Note: memchr() with cnt=0 returns NULL
	p_end = memchr(p_start + 1, '/', valid_bytes);
	if (!p_end) {
	    clen = valid_bytes ? (uri_path + plen) - p_start : 1;
	} else {
	    clen = p_end - p_start;
	}

	if (((clen == 1) && valid_bytes) || ((clen == 2) && (p_start[1] == '.'))) {
	    /* Drop entry */
	    mods++;
	} else if ((clen == 3) &&
		   (p_start[1] == '.')
		   && (p_start[2] == '.')) {
	    /* Delete previous entry */
	    mods++;
	    if (list_head.prev != &list_head) {
		p_cur_centry = list_head.prev;
		p_cur_centry->prev->next = &list_head;
		list_head.prev = p_cur_centry->prev;
	    } else {
		/* No previous entry, ".." beyond given root */
		error = 7;
		goto err;
	    }
	} else {
		p_centry = alloca(sizeof(centry_t) + clen + 1);
		p_centry->len = clen;	// strlen()
		memcpy(p_centry->cname, p_start, clen);	// Copy from '/'
		p_centry->cname[clen] = '\0';

		/* Add to tail of list */
		p_centry->next = &list_head;
		p_centry->prev = list_head.prev;
		list_head.prev->next = p_centry;
		list_head.prev = p_centry;
	}

	p_start = p_end;
    }

    /*
     * Build the resultant URI path by walking the linked list.
     */
    p_out = outbuf ? outbuf : malloced_outbuf;
    p_centry = list_head.next;

    if (p_centry != &list_head) {
	while (p_centry != &list_head) {
	    memcpy(p_out, p_centry->cname, p_centry->len);
	    p_out += p_centry->len;
	    p_centry = p_centry->next;
	}
    } else {
	*p_out = '/';		// no elements, just '/' case
	p_out += 1;
    }
    if ( p_query ) {
	memcpy( p_out, p_query, qlen );
	p_out += qlen;
    }
    *p_out = '\0';

    if (outbuf_len) {
	*outbuf_len = (p_out - (outbuf ? outbuf : malloced_outbuf));
    }

    if (modifications) {
	*modifications = mods;
    }

    if (errcode) {
	*errcode = 0;
    }
    return outbuf ? outbuf : malloced_outbuf;

  err:

    if (malloced_outbuf) {
	free(malloced_outbuf);
    }

    if (errcode) {
	*errcode = error;
    }
    return 0;
}

/*
 *******************************************************************************
 * normalize_http_uri() -- Apply normalize_uri_path() on either MIME_HDR_X_NKN_URI
 *			   or MIME_HDR_X_NKN_DECODED_URI if defined.
 *******************************************************************************
 */
int normalize_http_uri(mime_header_t * hdr)
{
    const char *uri_path;
    int uri_len;
    u_int32_t attrs;
    int hdrcnt;
    int token;

    char *p;
    char *outbuf;
    int outbuf_len;
    int pathname_mods;
    int rv;

    if (!hdr) {
	return 1;
    }

    if (mime_hdr_get_known(hdr, MIME_HDR_X_NKN_DECODED_URI,
			   &uri_path, &uri_len, &attrs, &hdrcnt)) {
	if (mime_hdr_get_known(hdr, MIME_HDR_X_NKN_URI,
			       &uri_path, &uri_len, &attrs, &hdrcnt)) {
	    return 2;		// Should never happen
	} else {
	    token = MIME_HDR_X_NKN_URI;
	}
    } else {
	token = MIME_HDR_X_NKN_DECODED_URI;
    }

	outbuf_len = uri_len + 1;
	if ( outbuf_len > 512 )
		outbuf = nkn_malloc_type(outbuf_len, mod_http_charbuf);
	else
		outbuf = alloca(outbuf_len);
	p = normalize_uri_path(uri_path, uri_len, outbuf, &outbuf_len,
			&pathname_mods, &rv);
	if (!p || rv) {
		DBG("normalize_uri_path() failed, rv=%d p=%p", rv, p);
		if ( outbuf_len > 512 ) 
			free(outbuf);
		return 3;
	}

	if (pathname_mods) {
		rv = mime_hdr_add_known(hdr, token, outbuf, outbuf_len);
		if (rv) {
			DBG("normalize_uri_path() failed, rv=%d p=%p", rv, p);
			if ( outbuf_len > 512 ) 
				free(outbuf);
			return 4;
		}
	}
	if ( outbuf_len > 512 ) 
		free(outbuf);
	return 0;
}


/*
 *******************************************************************************
 * compute_object_corrected_initial_age() -- Per RFC 2616 13.2.3
 *
 *   Return 0 => Success
 *******************************************************************************
 */
static time_t compute_object_corrected_initial_age(mime_header_t *hdr,
					    time_t request_send_time,
					    time_t request_response_time,
					    time_t now)
{
    UNUSED_ARGUMENT(now);
    int rv;
    time_t origin_date;
    time_t apparent_age;
    time_t corrected_received_age;
    time_t corrected_initial_age;
    time_t response_delay;


    const char *data;
    int datalen;
    u_int32_t hattrs;
    int hdrcnt;

    const cooked_data_types_t *cd;
    int cd_len;
    mime_hdr_datatype_t dtype;
    time_t age_value;

    // Compute apparent_age = MAX(0, (response_time - date_value))

    rv = mime_hdr_get_known(hdr, MIME_HDR_DATE, &data, &datalen, &hattrs, &hdrcnt);
    if (!rv) {
    	rv = mime_hdr_get_known_cooked_data(hdr, MIME_HDR_DATE, 
					    &cd, &cd_len, &dtype);
	if (!rv && (dtype == DT_DATE_1123)) {
	    origin_date = cd->u.dt_date_1123.t;
	} else {
	    origin_date = request_response_time;
	}
    	apparent_age = MAX(0, (request_response_time - origin_date));
    } else {
    	apparent_age = 0;
    }

    // Compute corrected_received_age = MAX(apparent_age, age_value)

    rv = mime_hdr_get_known(hdr, MIME_HDR_AGE, &data, &datalen, &hattrs, &hdrcnt);
    if (!rv) {
    	rv = mime_hdr_get_known_cooked_data(hdr, MIME_HDR_AGE, 
					    &cd, &cd_len, &dtype);
	if (!rv && (dtype == DT_INT)) {
	    age_value = MAX(0, cd->u.dt_int.l);
	} else {
	    age_value = 0;
	}
    } else {
	age_value = 0;
    }
    corrected_received_age = MAX(apparent_age, age_value);

    response_delay = request_response_time - request_send_time;
    corrected_initial_age = corrected_received_age + response_delay;

    DBG("apparent_age=%ld age_value=%ld response_delay=%ld "
    	"corrected_initial_age=%ld", 
	apparent_age, age_value, response_delay, corrected_initial_age);

    return corrected_initial_age;
}

/*
 *******************************************************************************
 * compute_object_expiration() -- Compute (1) if cacheable; (2) expiration.
 *
 *   1 -> Cacheable, (expire_time == NKN_EXPIRY_FOREVER) => no expiry
 *   0 -> No Cache
 *******************************************************************************
 */
int compute_object_expiration(mime_header_t * hdr,
			      int s_maxage_ignore,
			      time_t request_send_time,
			      time_t request_response_time,
			      time_t now,
			      time_t * obj_corrected_initial_age /* output */,
			      time_t * obj_expire_time /* output */)
{
    UNUSED_ARGUMENT(now);

    int rv;
    const cooked_data_types_t *cd;
    int cd_len;
    int x_accel_cache = 0;
    mime_hdr_datatype_t dtype;

    int no_cache = 0;
    time_t expire_time = NKN_EXPIRY_FOREVER;
    time_t os_date;

    *obj_corrected_initial_age = 
    	compute_object_corrected_initial_age(hdr,
					    request_send_time,
					    request_response_time, now);

    if (mime_hdr_is_known_present(hdr, MIME_HDR_DATE)) {
	rv = mime_hdr_get_known_cooked_data(hdr, MIME_HDR_DATE,
					    &cd, &cd_len, &dtype);
	if (!(rv || (dtype != DT_DATE_1123))) {
	    os_date = cd->u.dt_date_1123.t;
	} else {
	    DBG("mime_hdr_get_known_cooked_data(MIME_HDR_DATE) "
		"failed, rv=%d type=%d, ignoring header", rv, dtype);
	    os_date = now;
	}
    } else {
	os_date = now;
    }

    if (mime_hdr_is_known_present(hdr, MIME_HDR_X_ACCEL_CACHE_CONTROL) || 
	mime_hdr_is_known_present(hdr, MIME_HDR_CACHE_CONTROL)) {
	/* 
	 * MIME_HDR_X_ACCEL_CACHE_CONTROL has high precedence
	 */
	if (mime_hdr_is_known_present(hdr, MIME_HDR_X_ACCEL_CACHE_CONTROL)) {
	    rv = mime_hdr_get_known_cooked_data(hdr, 
		    MIME_HDR_X_ACCEL_CACHE_CONTROL, &cd, &cd_len, &dtype);
	    x_accel_cache = 1;
	} else {
cache_control:
	    rv = mime_hdr_get_known_cooked_data(hdr, MIME_HDR_CACHE_CONTROL,
							&cd, &cd_len, &dtype);
	}
	if (!(rv || (dtype != DT_CACHECONTROL_ENUM))) {
	    if ((cd->u.dt_cachecontrol_enum.mask & (1 << CT_PRIVATE)) ||
		(cd->u.dt_cachecontrol_enum.mask & (1 << CT_NO_CACHE)) ||
		(cd->u.dt_cachecontrol_enum.mask & (1 << CT_NO_STORE))) {
		no_cache = 1;
	    } 

	    /*
	     * If max-age or s-max-age is present calculate the expiry time, so
	     * that it can be used if caching is forced by user policy
	     */
	    if (cd->u.dt_cachecontrol_enum.mask & (1 << CT_MAX_AGE)) {
		expire_time = cd->u.dt_cachecontrol_enum.max_age - 
			(*obj_corrected_initial_age + 
				(now - request_response_time));
		if (expire_time > 0) {
		    expire_time += now;
		} else {
		    expire_time = now;
		}
		/* BZ 3204, 3362, if max-age=0 don't cache the file
		 */
		if (cd->u.dt_cachecontrol_enum.max_age <= 0) {
			no_cache = 1;
		}
		DBG("CC based expiry=%ld", expire_time);
	    }
	    /* Consider s_maxage only if nobody has asked us to ignore it */
	    if ((s_maxage_ignore == 0) && 
			cd->u.dt_cachecontrol_enum.mask & (1 << CT_S_MAXAGE)) {
		expire_time = cd->u.dt_cachecontrol_enum.s_maxage - 
			(*obj_corrected_initial_age + 
				(now - request_response_time));
		if (expire_time > 0) {
		    expire_time += now;
		} else {
		    expire_time = now;
		}
		/* BZ 2434 if s_maxage is present , that is given precedence over max-age
		 */
		if (cd->u.dt_cachecontrol_enum.s_maxage == 0) {
			no_cache = 1;
		}
		DBG("CC based expiry=%ld", expire_time);

	    }
	} else {
	    if (x_accel_cache == 1) {
		x_accel_cache = 0;
		DBG("mime_hdr_get_known_cooked_data(MIME_HDR_X_ACCEL_CACHE_CONTROL) "
			"failed, rv=%d type=%d, ignoring header", rv, dtype);
		goto cache_control;
	    }
	    DBG("mime_hdr_get_known_cooked_data(MIME_HDR_CACHE_CONTROL) "
		"failed, rv=%d type=%d, ignoring header", rv, dtype);
	}
    }
    // Cache-Control: max-age takes precedence over Expires: <date>
    if ((expire_time == NKN_EXPIRY_FOREVER)
	&& mime_hdr_is_known_present(hdr, MIME_HDR_EXPIRES)) {
	rv = mime_hdr_get_known_cooked_data(hdr, MIME_HDR_EXPIRES, &cd, &cd_len,
					    &dtype);
	if (!(rv || (dtype != DT_DATE_1123))) {
	    expire_time = MAX(0, (cd->u.dt_date_1123.t - os_date)) -
	    	(*obj_corrected_initial_age + (now - request_response_time));
	    if (expire_time > 0) {
	    	expire_time += now;
	    } else {
	    	expire_time = now;
                no_cache = 1;
	    }
	    DBG("Expiry based expiry=%ld", expire_time);
	} else {
	    DBG("mime_hdr_get_known_cooked_data(MIME_HDR_EXPIRES) "
		"failed, rv=%d type=%d, ignoring header", rv, dtype);
	}
    }
    // Pragma: no-cache is equivalent to Cache-Control: no-cache
    if (x_accel_cache == 0 && 
		mime_hdr_is_known_present(hdr, MIME_HDR_PRAGMA)) {
	rv = mime_hdr_get_known_cooked_data(hdr, MIME_HDR_PRAGMA,
					    &cd, &cd_len, &dtype);
	if (!(rv || (dtype != DT_STRING))) {
	    TSTR(cd->u.dt_string.str, cd_len, pragma_str);
	    if (strstr(pragma_str, "no-cache")) {
		no_cache = 1;
	    }
	} else {
	    DBG("mime_hdr_get_known_cooked_data(MIME_HDR_PRAGMA) "
		"failed, rv=%d type=%d, ignoring header", rv, dtype);
	}
    }

    *obj_expire_time = expire_time;
    return no_cache ? 0 : 1;
}

/*
 *******************************************************************************
 * nkn_attr2mime_header() -- Convert from nkn_attr_t to mime_header_t
 *
 *   Caller passes an initialized mime_header_t
 *   Return 0 => Success
 *******************************************************************************
 */
int nkn_attr2mime_header(const nkn_attr_t * nkn_attr,
			 int only_end2end_hdrs, mime_header_t * hdr)
{
    int rv = 0;
    nkn_attr_id_t id;
    char *serialbuf;
    int serialbufsz;

    if (!nkn_attr || !hdr) {
	DBG("Invalid parameters: nkn_attr=%p hdr=%p", nkn_attr, hdr);
	rv = 1;
	goto exit;
    }
    
    if ((hdr->version != MIME_HEADER_VERSION) ||
        ((hdr->protocol != MIME_PROT_HTTP) && 
	 (hdr->protocol != MIME_PROT_RTSP))) {
	DBG("hdr not initialized, version=%d protocol=%d",
	    hdr->version, hdr->protocol);
	rv = 2;
	goto exit;
    }

    id.u.attr.protocol_id = NKN_PROTO_HTTP;
    id.u.attr.id = NKN_ATTR_MIME_HDR_ID;

    serialbuf = nkn_attr_get((nkn_attr_t *)nkn_attr, id, 
			     (uint32_t *)&serialbufsz);
    if (!serialbuf) {
	DBG("Bad attribute data, nkn_attr_get(NKN_PROTO_HTTP, "
	    "NKN_ATTR_MIME_HDR_ID) failed %s", " ");
	rv = 3;
	goto exit;
    }

    rv = mime_hdr_deserialize(serialbuf, serialbufsz, hdr, (char *)1, 0);
    if (rv) {
	DBG("mime_hdr_deserialize() failed, rv=%d", rv);
	rv = 4;
	goto exit;
    }

    if (only_end2end_hdrs) {
    	mime_hdr_remove_hop2hophdrs(hdr);
    }

exit:

    return rv;
}

static
int int_mime_header2nkn_attr(const mime_header_t * hdr, int only_end2end_hdrs,
			     nkn_attr_t * nkn_attr, nkn_buffer_t *buf_nkn_attr,
			     int (*set_bufsize)(void *, int, int),
			     void *(*buf2attr)(void *))
{
#define MAX_ALLOCA (64 * 1024)
    int rv = 0;
    int serialbufsz;
    char *serialbuf;
    nkn_attr_id_t id;
    mime_header_t thdr;
    int thdr_init = 0;
    char *serialbuf_malloc = 0;
    char *serialbuf_malloc_2 = 0;

    serialbufsz = mime_hdr_serialize_datasize(hdr, 0, 0, 0);
    if (!serialbufsz) {
	DBG("mime_hdr_serialize_datasize() failed, hdr=%p", hdr);
	rv = 1;
	goto exit;
    }

    if (serialbufsz <= MAX_ALLOCA) {
    	serialbuf = alloca(serialbufsz);
    } else {
    	serialbuf_malloc = nkn_malloc_type(serialbufsz, mod_httphdrs_serialbuf);
	if (!serialbuf_malloc) {
	    DBG("nkn_malloc_type() failed, size=%d", serialbufsz);
	    rv = 2;
	    goto exit;
	}
	serialbuf = serialbuf_malloc;
    }
    rv = mime_hdr_serialize(hdr, serialbuf, serialbufsz);
    if (rv) {
	DBG("mime_hdr_serialize() failed, rv=%d "
	    "serialbufsz=%d", rv, serialbufsz);
	rv = 3;
	goto exit;
    }

    if (only_end2end_hdrs) {
     	// Create temp mime_header_t, remove hop2hop hdrs and serialize
    	mime_hdr_init(&thdr, hdr->protocol, 0, 0);
	thdr_init = 1;

    	rv = mime_hdr_deserialize(serialbuf, serialbufsz, &thdr, (char *)1, 0);
	if (rv) {
	    DBG("mime_hdr_deserialize() failed, rv=%d serialbufsz=%d",
	    	rv, serialbufsz);
	    rv = 4;
	    goto exit;
	}
    	mime_hdr_remove_hop2hophdrs(&thdr);

    	serialbufsz = mime_hdr_serialize_datasize(&thdr, 0, 0, 0);
	if (!serialbufsz) {
	    DBG("mime_hdr_serialize_datasize() failed, hdr=%p", hdr);
	    rv = 5;
	    goto exit;
	}

    	if (serialbufsz <= MAX_ALLOCA) {
	    serialbuf = alloca(serialbufsz);
	} else {
	    serialbuf_malloc_2 = 
	    	nkn_malloc_type(serialbufsz, mod_httphdrs_serialbuf);
	    if (!serialbuf_malloc_2) {
	    	DBG("nkn_malloc_type() failed, size=%d", serialbufsz);
	    	rv = 6;
	    	goto exit;
	    }
	    serialbuf = serialbuf_malloc_2;
	}
    	rv = mime_hdr_serialize(&thdr, serialbuf, serialbufsz);
    	if (rv) {
	    DBG("mime_hdr_serialize() failed, rv=%d "
	    	"serialbufsz=%d", rv, serialbufsz);
	    rv = 7;
	    goto exit;
    	}
    }

    if (!nkn_attr) {
    	rv = (*set_bufsize)(buf_nkn_attr, 1, serialbufsz);
	if (rv) {
	    DBG("(*set_bufsize)() failed, rv=%d size=%d", rv, serialbufsz);
	    rv = 8;
	    goto exit;
	}
	nkn_attr = (nkn_attr_t *)(*buf2attr)(buf_nkn_attr);
	if (!nkn_attr) {
	    DBG("buf2attr() failed, buf_nkn_attr=%p", buf_nkn_attr);
	    rv = 9;
	    goto exit;
	}
    } else {
	nkn_attr_reset_blob(nkn_attr, 1);
    }

    id.u.attr.protocol_id = NKN_PROTO_HTTP;
    id.u.attr.id = NKN_ATTR_MIME_HDR_ID;
    rv = nkn_attr_set(nkn_attr, id, serialbufsz, serialbuf);
    if (rv) {
	DBG("nkn_attr_set(mime_header_t) failed rv=%d", rv);
	rv = 10;
	goto exit;
    }

exit:

    if (thdr_init) {
    	mime_hdr_shutdown(&thdr);
    }

    if (serialbuf_malloc) {
    	free(serialbuf_malloc);
    }

    if (serialbuf_malloc_2) {
    	free(serialbuf_malloc_2);
    }
    return rv;
#undef MAX_ALLOCA
}

/*
 *******************************************************************************
 * mime_header2nkn_attr() -- Convert from mime_header_t to nkn_attr_t
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_header2nkn_attr(const mime_header_t * hdr, int only_end2end_hdrs,
			 nkn_attr_t * nkn_attr)
{
    return int_mime_header2nkn_attr(hdr, only_end2end_hdrs, nkn_attr, 0, 0, 0);
}

/*
 *******************************************************************************
 * mime_header2nkn_buf_attr() -- Convert from mime_header_t to nkn_attr_t
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_header2nkn_buf_attr(const mime_header_t * hdr, int only_end2end_hdrs,
			     void * buf_nkn_attr, 
			     int (*set_bufsize)(void *, int, int),
			     void *(*buf2attr)(void *))
{
    int rv;
    nkn_attr_t *pnknattr;

    if (set_bufsize) {
    	if (buf_nkn_attr) {
	    return int_mime_header2nkn_attr(hdr, only_end2end_hdrs, 0, 
					    buf_nkn_attr, 
					    set_bufsize, buf2attr);
	} else {
	    rv = 100;
	}
    } else {
    	pnknattr = (nkn_attr_t *)(*buf2attr)(buf_nkn_attr);
	if (pnknattr) {
	    return int_mime_header2nkn_attr(hdr, only_end2end_hdrs, 
					    pnknattr, 0, 0, 0);
	} else {
	    rv = 101;
	}
    }
    return rv;
}

/*
 *******************************************************************************
 * compute_object_version() -- Generate nkn_objv from given mime_header_t
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int compute_object_version(const mime_header_t *hdr, const char *uri, 
			    int urilen, int encoding_type, nkn_objv_t *objvp,
			    int *nonmatch_type)
{
    int rv;
    const char *val = NULL;
    int vallen;
    const char *date;
    int datelen;
    u_int32_t attr;
    int header_cnt;
    MD5_CTX ctx;
    int encoding_str_len;
    char encoding_str[32];
    unsigned char MD5hash[MD5_DIGEST_LENGTH];
    time_t tlm, tdate;

    if (!hdr || !uri || !urilen || !objvp) {
	DBG("Invalid input, hdr=%p uri=%p urilen=%d objvp=%p", 
	    hdr, uri, urilen, objvp);
    	return 1;
    }
    *nonmatch_type = 0;

    while (1) {
    	rv = mime_hdr_get_known(hdr, MIME_HDR_ETAG, &val, &vallen, &attr, 
				&header_cnt);
    	if (!rv && vallen) {
    	    /* Ignore Week ETag indicator while computing version.
    	     * BZ 3833, 3940
    	     */
	    if ( (*val == 'W' || *val == 'w') && (*(val+1) == '/') ) {
	    	val += 2;
	    	vallen -= 2;
	    }
	    break; // Use Etag
    	}

	// Only use LM if it is a strong validator
    	rv = mime_hdr_get_known(hdr, MIME_HDR_LAST_MODIFIED, &val, &vallen, &attr, 
				&header_cnt);
    	if (!rv && vallen) {
	    rv = mime_hdr_get_known(hdr, MIME_HDR_DATE, &date, &datelen, &attr, 
				&header_cnt);
	    if (!rv && datelen) {
		tlm = parse_date(val, val+(vallen-1));
	    	tdate = parse_date(date, date+(datelen-1));
		if ((tdate - tlm) >= 60) {
		    break; // Use LM
		}
	    }
    	}

	// use uri name
    	*nonmatch_type = 1;
	break;
    } /* end while */

    MD5_Init(&ctx);
    MD5_Update(&ctx, uri, urilen);
    if (val && vallen) {
    	MD5_Update(&ctx, val, vallen);
    }
    if (encoding_type) {
	encoding_str_len = snprintf(encoding_str, 
				sizeof(encoding_str), "%d", encoding_type);
    	MD5_Update(&ctx, encoding_str, encoding_str_len);
    }
    MD5_Final(MD5hash, &ctx);
    memcpy(objvp->objv_c, MD5hash, MIN(sizeof(MD5hash), sizeof(objvp->objv_c)));

    return 0;
}

/*
 *******************************************************************************
 * compute_object_version_use_etag() -- Generate nkn_objv from given mime_header_t
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int compute_object_version_use_etag(const mime_header_t *hdr, const char *uri, 
			    int urilen, int encoding_type, nkn_objv_t *objvp,
			    int *nonmatch_type)
{
    int rv;
    const char *val = NULL;
    int vallen;
    u_int32_t attr;
    int header_cnt;
    MD5_CTX ctx;
    int encoding_str_len;
    char encoding_str[32];
    unsigned char MD5hash[MD5_DIGEST_LENGTH];

    if (!hdr || !uri || !urilen || !objvp) {
	DBG("Invalid input, hdr=%p uri=%p urilen=%d objvp=%p", 
	    hdr, uri, urilen, objvp);
    	return 1;
    }

    *nonmatch_type = 0;
    while (1) {
    	rv = mime_hdr_get_known(hdr, MIME_HDR_ETAG, &val, &vallen, &attr, 
				&header_cnt);
    	if (!rv && vallen) {
    	    /* Ignore Week ETag indicator while computing version.
    	     * BZ 3833, 3940
    	     */
	    if ( (*val == 'W' || *val == 'w') && (*(val+1) == '/') ) {
	    	val += 2;
	    	vallen -= 2;
	    }
	    break; // Use Etag
    	}
	
	// use uri name 
	*nonmatch_type = 1;
	break;
    } /* end while */

    MD5_Init(&ctx);
    MD5_Update(&ctx, uri, urilen);
    if (val && vallen) {
    	MD5_Update(&ctx, val, vallen);
    }
    if (encoding_type) {
	encoding_str_len = snprintf(encoding_str, 
				sizeof(encoding_str), "%d", encoding_type);
    	MD5_Update(&ctx, encoding_str, encoding_str_len);
    }
    MD5_Final(MD5hash, &ctx);
    memcpy(objvp->objv_c, MD5hash, MIN(sizeof(MD5hash), sizeof(objvp->objv_c)));

    return 0;
}

/*
 *******************************************************************************
 * compute_object_version_use_lm() -- Generate nkn_objv from given mime_header_t
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int compute_object_version_use_lm(const mime_header_t *hdr, const char *uri, 
			    int urilen, int encoding_type, nkn_objv_t *objvp,
			    int *nonmatch_type)
{
    int rv;
    const char *val = NULL;
    int vallen;
    const char *date;
    int datelen;
    u_int32_t attr;
    int header_cnt;
    MD5_CTX ctx;
    int encoding_str_len;
    char encoding_str[32];
    unsigned char MD5hash[MD5_DIGEST_LENGTH];
    time_t tlm, tdate;

    if (!hdr || !uri || !urilen || !objvp) {
	DBG("Invalid input, hdr=%p uri=%p urilen=%d objvp=%p", 
	    hdr, uri, urilen, objvp);
    	return 1;
    }

    *nonmatch_type = 0;
    while (1) {
	// Only use LM if it is a strong validator
    	rv = mime_hdr_get_known(hdr, MIME_HDR_LAST_MODIFIED, &val, &vallen, &attr, 
				&header_cnt);
    	if (!rv && vallen) {
	    rv = mime_hdr_get_known(hdr, MIME_HDR_DATE, &date, &datelen, &attr, 
				&header_cnt);
	    if (!rv && datelen) {
		tlm = parse_date(val, val+(vallen-1));
	    	tdate = parse_date(date, date+(datelen-1));
		if ((tdate - tlm) >= 60) {
		    break; // Use LM
		}
	    }
    	}

	// use uri name
    	*nonmatch_type = 1;
	break;
    } /* end while */

    MD5_Init(&ctx);
    MD5_Update(&ctx, uri, urilen);
    if (val && vallen) {
    	MD5_Update(&ctx, val, vallen);
    }
    if (encoding_type) {
	encoding_str_len = snprintf(encoding_str, 
				sizeof(encoding_str), "%d", encoding_type);
    	MD5_Update(&ctx, encoding_str, encoding_str_len);
    }
    MD5_Final(MD5hash, &ctx);
    memcpy(objvp->objv_c, MD5hash, MIN(sizeof(MD5hash), sizeof(objvp->objv_c)));

    return 0;
}

/*
 *******************************************************************************
 * mime_hdr_remove_hop2hophdr() -- Remove known hop2hop headers
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_remove_hop2hophdrs(mime_header_t *hdr)
{
    int n;

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	if (hdr->known_header_map[n] && !http_end2end_header[n]) {
	    mime_hdr_del_known(hdr, n);
	} else {
	    continue;
	}
    }
    return 0;
}

/*
 *******************************************************************************
 * mime_hdr_remove_hop2hophdr_except() -- Remove known hop2hop headers
 *			except for specified header
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_remove_hop2hophdrs_except(mime_header_t *hdr, int token)
{
    int n;

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	if (hdr->known_header_map[n] && 
	    ((n != token) && !http_end2end_header[n])) {
	    mime_hdr_del_known(hdr, n);
	} else {
	    continue;
	}
    }
    return 0;
}

/*
 *******************************************************************************
 * mime_hdr_remove_hop2hophdr_except() -- Remove known hop2hop headers
 *			except for specified header
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mime_hdr_remove_hop2hophdrs_except_these(mime_header_t *hdr, int num_tokens, int *token)
{
    int n, i, skip;

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	if (hdr->known_header_map[n] && !http_end2end_header[n]) {
	    skip = 0;
	    for (i = 0; i < num_tokens; i++ ) {
		if (n == token[i])
		    skip = 1;
	    }
	    if (!skip)
		mime_hdr_del_known(hdr, n);
	} else {
	    continue;
	}
    }
    return 0;
}

int copy_ipv6_host (char *dst, const char *src, int data_len)
{
    int n = 0;
    int len = 0;

    if (!dst || !src || !data_len)
        return 0;

    for (n = 0; n < data_len; n++)
    {
        if (('[' == src[n]) || (']' == src[n]))
            continue;

        dst[len] = src[n];

        len++;
    }

    dst[len] = '\0';

    return len;
}

/*
 *******************************************************************************
 * parse_host_header() -- Parse host header "<domain>:<port>"
 *
 *   Return 0 => Success
 *   Return 1 => Fail
 *   domain name could be empty or string can have null char, then fail
 *******************************************************************************
 */
int parse_host_header(const char *host, int hostlen, 
		      int *domain_len, uint16_t *pport, 
		      int network_byte_order_port)
{
    char *p;
    long val;
    int bytes;
    uint16_t port;
    int n = 0;
    int is_ipv6 = 0;

    if (hostlen == 0 || memchr(host, 0, hostlen) != NULL) // Check for NULL char in domain name
	return 1;

    if ('[' == host[0])
    {
        for (n = 0; n < hostlen; n++)
        {
            if (']' == host[n])
            {
                is_ipv6 = 1;
                n++;
                break;
            }
        }

        if (!is_ipv6)
        {
            // No close ']' is seen
            return 1;  // Invalid
        }
    }
    else
    {
        // Okay. while testing I found another variant. Curl can't understand '[' and ']'. Huh! 
        // It is possible to get IPv6 literal with no '[' delims.
        p = memchr(host, ':', hostlen);

        // If no '[' delim, you can't send port option.
        if (p && (p != memrchr(host, ':', hostlen)))
        {
            *domain_len = hostlen;
            *pport = network_byte_order_port ? htons(80) : 80;

            return 0;
        }
    }


    if (is_ipv6)
    {
        p = memchr (host + n, ':', hostlen - n);
    }
    else
    {
        p = memchr(host, ':', hostlen);
    }

    *domain_len = p ? (p - host) : hostlen;
    if (*domain_len == 0)
	return 1;
    bytes = hostlen - (*domain_len + 1);
    if (p && (bytes > 0)) {
    	val = atol_len(p+1, bytes);
	port = (uint16_t) val;
    	*pport = network_byte_order_port ? htons(port) : port;
    } else {
    	*pport = network_byte_order_port ? htons(80) : 80;
    }
    return 0;
}

/*
 *******************************************************************************
 * valid_FQDN() -- Validate FQDN per RFC 952 but ignore port specification if
 *	present.   <host> or <host>:<port> are legal inputs.
 *	<host> can be an IP address
 *
 *   Return 0 => Success
 *******************************************************************************
 */
STATIC char valid_FQDN_name_chars[128] = {
      0,      // 000    000   00000000      NUL    (Null char.)
      0,      // 001    001   00000001      SOH    (Start of Header)
      0,      // 002    002   00000010      STX    (Start of Text)
      0,      // 003    003   00000011      ETX    (End of Text)
      0,      // 004    004   00000100      EOT    (End of Transmission)
      0,      // 005    005   00000101      ENQ    (Enquiry)
      0,      // 006    006   00000110      ACK    (Acknowledgment)
      0,      // 007    007   00000111      BEL    (Bell)
      0,      // 010    008   00001000       BS    (Backspace)
      0,      // 011    009   00001001       HT    (Horizontal Tab)
      0,      // 012    00A   00001010       LF    (Line Feed)
      0,      // 013    00B   00001011       VT    (Vertical Tab)
      0,      // 014    00C   00001100       FF    (Form Feed)
      0,      // 015    00D   00001101       CR    (Carriage Return)
      0,      // 016    00E   00001110       SO    (Shift Out)
      0,      // 017    00F   00001111       SI    (Shift In)
      0,      // 020    010   00010000      DLE    (Data Link Escape)
      0,      // 021    011   00010001      DC1 (XON) (Device Control 1)
      0,      // 022    012   00010010      DC2       (Device Control 2)
      0,      // 023    013   00010011      DC3 (XOFF)(Device Control 3)
      0,      // 024    014   00010100      DC4       (Device Control 4)
      0,      // 025    015   00010101      NAK (Negativ Acknowledgemnt)
      0,      // 026    016   00010110      SYN    (Synchronous Idle)
      0,      // 027    017   00010111      ETB    (End of Trans. Block)
      0,      // 030    018   00011000      CAN    (Cancel)
      0,      // 031    019   00011001       EM    (End of Medium)
      0,      // 032    01A   00011010      SUB    (Substitute)
      0,      // 033    01B   00011011      ESC    (Escape)
      0,      // 034    01C   00011100       FS    (File Separator)
      0,      // 035    01D   00011101       GS    (Group Separator)
      0,      // 036    01E   00011110       RS (Reqst to Send)(Rec. Sep.)
      0,      // 037    01F   00011111       US    (Unit Separator)
      0,      // 040    020   00100000       SP    (Space)
      0,      // 041    021   00100001        !    (exclamation mark)
      0,      // 042    022   00100010        "    (double quote)
      0,      // 043    023   00100011        #    (number sign)
      0,      // 044    024   00100100        $    (dollar sign)
      0,      // 045    025   00100101        %    (percent)
      0,      // 046    026   00100110        &    (ampersand)
      0,      // 047    027   00100111        '    (single quote)
      0,      // 050    028   00101000        (  (left/open parenthesis)
      0,      // 051    029   00101001        )  (right/closing parenth.)
      0,      // 052    02A   00101010        *    (asterisk)
      0,      // 053    02B   00101011        +    (plus)
      0,      // 054    02C   00101100        ,    (comma)
      1,      // 055    02D   00101101        -    (minus or dash)
      1,      // 056    02E   00101110        .    (dot)
      0,      // 057    02F   00101111        /    (forward slash)
      1,      // 060    030   00110000        0
      1,      // 061    031   00110001        1
      1,      // 062    032   00110010        2
      1,      // 063    033   00110011        3
      1,      // 064    034   00110100        4
      1,      // 065    035   00110101        5
      1,      // 066    036   00110110        6
      1,      // 067    037   00110111        7
      1,      // 070    038   00111000        8
      1,      // 071    039   00111001        9
      0,      // 072    03A   00111010        :    (colon)
      0,      // 073    03B   00111011        ;    (semi-colon)
      0,      // 074    03C   00111100        <    (less than)
      0,      // 075    03D   00111101        =    (equal sign)
      0,      // 076    03E   00111110        >    (greater than)
      0,      // 077    03F   00111111        ?    (question mark)
      0,      // 100    040   01000000        @    (AT symbol)
      1,      // 101    041   01000001        A
      1,      // 102    042   01000010        B
      1,      // 103    043   01000011        C
      1,      // 104    044   01000100        D
      1,      // 105    045   01000101        E
      1,      // 106    046   01000110        F
      1,      // 107    047   01000111        G
      1,      // 110    048   01001000        H
      1,      // 111    049   01001001        I
      1,      // 112    04A   01001010        J
      1,      // 113    04B   01001011        K
      1,      // 114    04C   01001100        L
      1,      // 115    04D   01001101        M
      1,      // 116    04E   01001110        N
      1,      // 117    04F   01001111        O
      1,      // 120    050   01010000        P
      1,      // 121    051   01010001        Q
      1,      // 122    052   01010010        R
      1,      // 123    053   01010011        S
      1,      // 124    054   01010100        T
      1,      // 125    055   01010101        U
      1,      // 126    056   01010110        V
      1,      // 127    057   01010111        W
      1,      // 130    058   01011000        X
      1,      // 131    059   01011001        Y
      1,      // 132    05A   01011010        Z
      0,      // 133    05B   01011011        [    (left/opening bracket)
      0,      // 134    05C   01011100        \    (back slash)
      0,      // 135    05D   01011101        ]    (right/closing bracket)
      0,      // 136    05E   01011110        ^    (caret/circumflex)
      1,      // 137    05F   01011111        _    (underscore)
      0,      // 140    060   01100000        `
      1,      // 141    061   01100001        a
      1,      // 142    062   01100010        b
      1,      // 143    063   01100011        c
      1,      // 144    064   01100100        d
      1,      // 145    065   01100101        e
      1,      // 146    066   01100110        f
      1,      // 147    067   01100111        g
      1,      // 150    068   01101000        h
      1,      // 151    069   01101001        i
      1,      // 152    06A   01101010        j
      1,      // 153    06B   01101011        k
      1,      // 154    06C   01101100        l
      1,      // 155    06D   01101101        m
      1,      // 156    06E   01101110        n
      1,      // 157    06F   01101111        o
      1,      // 160    070   01110000        p
      1,      // 161    071   01110001        q
      1,      // 162    072   01110010        r
      1,      // 163    073   01110011        s
      1,      // 164    074   01110100        t
      1,      // 165    075   01110101        u
      1,      // 166    076   01110110        v
      1,      // 167    077   01110111        w
      1,      // 170    078   01111000        x
      1,      // 171    079   01111001        y
      1,      // 172    07A   01111010        z
      0,      // 173    07B   01111011        {    (left/opening brace)
      0,      // 174    07C   01111100        |    (vertical bar)
      0,      // 175    07D   01111101        }    (right/closing brace)
      0,      // 176    07E   01111110        ~    (tilde)
      0       // 177    07F   01111111      DEL    (delete)
};

int
valid_FQDN(const char *name, int name_len)
{
    int n;
    char *p;
    int is_ipaddr = 0;
    int is_ipv6 = 0;

    if (!name || !name_len) {
    	return 0; // Invalid
    }

#if 0
    if (isalpha(name[0])) {
        is_ipaddr = 0;
    } else if (isdigit(name[0])) {
        is_ipaddr = 1;
    } else {
    	return 0; // Invalid
    }
#endif

    // IPv6 IP format check per RFC 2732
    // TODO: Should we handle IPv6 literal without '[' and ']' delims?
    if ('[' == name[0])
    {
        for (n = 1; n < name_len; n++)
        {
            if (']' == name[n])
            {
                is_ipv6 = 1;
                n++;
                break;
            }

            // The enclosed IPv6 literal can have colons ':', dot '.', hexdigits, and dec-digits
            // '.' is valid in IPv4 mapped or compat IPv6 format

            if ((':' != name[n]) && ('.' != name[n]) && !isdigit(name[n]) && !isxdigit(name[n]))
            {
                return 0; // Invalid
            }
        }

        if (!is_ipv6)
        {
            // No close ']' is seen
            return 0; // Invalid
        }

        return 1;
    }
    else
    {
        // Another variant of IPv6 literal - with no delims '['. Curl is no good :(
        p = memchr(name, ':', name_len);

        if (p && (p != memrchr(name, ':', name_len)))
        {
            return 1;
        }
    }

    if (p) {
    	name_len = p - name;
    }

    if ((name[name_len-1] == '-') || (name[name_len-1] == '.')) {
    	return 0; // Invalid, last char must not be '-' or '.'
    }

    for (n = 0; n < name_len; n++) {
    	if (is_ipaddr ? 
		!(isdigit(name[n]) || (name[n] == '.')) : 
		!valid_FQDN_name_chars[(int)name[n]]) {
	    return 0; // Invalid char
	}
	// Note: First char is never '.' so name[n-1] is legal
	if ((name[n] == '.') && (name[n-1] == '.')) {
	    return 0; // ".." not allowed
	}
    }
    return 1;
}

/*
 *******************************************************************************
 * valid_IPaddr() -- Validate given IP address but ignore port specification if
 *	present.   <IP> or <IP>:<port> are legal inputs.
 *
 *   Return 0 => Success
 *******************************************************************************
 */
STATIC char valid_IP_name_chars[128] = {
      0,      // 000    000   00000000      NUL    (Null char.)
      0,      // 001    001   00000001      SOH    (Start of Header)
      0,      // 002    002   00000010      STX    (Start of Text)
      0,      // 003    003   00000011      ETX    (End of Text)
      0,      // 004    004   00000100      EOT    (End of Transmission)
      0,      // 005    005   00000101      ENQ    (Enquiry)
      0,      // 006    006   00000110      ACK    (Acknowledgment)
      0,      // 007    007   00000111      BEL    (Bell)
      0,      // 010    008   00001000       BS    (Backspace)
      0,      // 011    009   00001001       HT    (Horizontal Tab)
      0,      // 012    00A   00001010       LF    (Line Feed)
      0,      // 013    00B   00001011       VT    (Vertical Tab)
      0,      // 014    00C   00001100       FF    (Form Feed)
      0,      // 015    00D   00001101       CR    (Carriage Return)
      0,      // 016    00E   00001110       SO    (Shift Out)
      0,      // 017    00F   00001111       SI    (Shift In)
      0,      // 020    010   00010000      DLE    (Data Link Escape)
      0,      // 021    011   00010001      DC1 (XON) (Device Control 1)
      0,      // 022    012   00010010      DC2       (Device Control 2)
      0,      // 023    013   00010011      DC3 (XOFF)(Device Control 3)
      0,      // 024    014   00010100      DC4       (Device Control 4)
      0,      // 025    015   00010101      NAK (Negativ Acknowledgemnt)
      0,      // 026    016   00010110      SYN    (Synchronous Idle)
      0,      // 027    017   00010111      ETB    (End of Trans. Block)
      0,      // 030    018   00011000      CAN    (Cancel)
      0,      // 031    019   00011001       EM    (End of Medium)
      0,      // 032    01A   00011010      SUB    (Substitute)
      0,      // 033    01B   00011011      ESC    (Escape)
      0,      // 034    01C   00011100       FS    (File Separator)
      0,      // 035    01D   00011101       GS    (Group Separator)
      0,      // 036    01E   00011110       RS (Reqst to Send)(Rec. Sep.)
      0,      // 037    01F   00011111       US    (Unit Separator)
      0,      // 040    020   00100000       SP    (Space)
      0,      // 041    021   00100001        !    (exclamation mark)
      0,      // 042    022   00100010        "    (double quote)
      0,      // 043    023   00100011        #    (number sign)
      0,      // 044    024   00100100        $    (dollar sign)
      0,      // 045    025   00100101        %    (percent)
      0,      // 046    026   00100110        &    (ampersand)
      0,      // 047    027   00100111        '    (single quote)
      0,      // 050    028   00101000        (  (left/open parenthesis)
      0,      // 051    029   00101001        )  (right/closing parenth.)
      0,      // 052    02A   00101010        *    (asterisk)
      0,      // 053    02B   00101011        +    (plus)
      0,      // 054    02C   00101100        ,    (comma)
      1,      // 055    02D   00101101        -    (minus or dash)
      1,      // 056    02E   00101110        .    (dot)
      0,      // 057    02F   00101111        /    (forward slash)
      1,      // 060    030   00110000        0
      1,      // 061    031   00110001        1
      1,      // 062    032   00110010        2
      1,      // 063    033   00110011        3
      1,      // 064    034   00110100        4
      1,      // 065    035   00110101        5
      1,      // 066    036   00110110        6
      1,      // 067    037   00110111        7
      1,      // 070    038   00111000        8
      1,      // 071    039   00111001        9
      0,      // 072    03A   00111010        :    (colon)
      0,      // 073    03B   00111011        ;    (semi-colon)
      0,      // 074    03C   00111100        <    (less than)
      0,      // 075    03D   00111101        =    (equal sign)
      0,      // 076    03E   00111110        >    (greater than)
      0,      // 077    03F   00111111        ?    (question mark)
      0,      // 100    040   01000000        @    (AT symbol)
      0,      // 101    041   01000001        A
      0,      // 102    042   01000010        B
      0,      // 103    043   01000011        C
      0,      // 104    044   01000100        D
      0,      // 105    045   01000101        E
      0,      // 106    046   01000110        F
      0,      // 107    047   01000111        G
      0,      // 110    048   01001000        H
      0,      // 111    049   01001001        I
      0,      // 112    04A   01001010        J
      0,      // 113    04B   01001011        K
      0,      // 114    04C   01001100        L
      0,      // 115    04D   01001101        M
      0,      // 116    04E   01001110        N
      0,      // 117    04F   01001111        O
      0,      // 120    050   01010000        P
      0,      // 121    051   01010001        Q
      0,      // 122    052   01010010        R
      0,      // 123    053   01010011        S
      0,      // 124    054   01010100        T
      0,      // 125    055   01010101        U
      0,      // 126    056   01010110        V
      0,      // 127    057   01010111        W
      0,      // 130    058   01011000        X
      0,      // 131    059   01011001        Y
      0,      // 132    05A   01011010        Z
      0,      // 133    05B   01011011        [    (left/opening bracket)
      0,      // 134    05C   01011100        \    (back slash)
      0,      // 135    05D   01011101        ]    (right/closing bracket)
      0,      // 136    05E   01011110        ^    (caret/circumflex)
      0,      // 137    05F   01011111        _    (underscore)
      0,      // 140    060   01100000        `
      0,      // 141    061   01100001        a
      0,      // 142    062   01100010        b
      0,      // 143    063   01100011        c
      0,      // 144    064   01100100        d
      0,      // 145    065   01100101        e
      0,      // 146    066   01100110        f
      0,      // 147    067   01100111        g
      0,      // 150    068   01101000        h
      0,      // 151    069   01101001        i
      0,      // 152    06A   01101010        j
      0,      // 153    06B   01101011        k
      0,      // 154    06C   01101100        l
      0,      // 155    06D   01101101        m
      0,      // 156    06E   01101110        n
      0,      // 157    06F   01101111        o
      0,      // 160    070   01110000        p
      0,      // 161    071   01110001        q
      0,      // 162    072   01110010        r
      0,      // 163    073   01110011        s
      0,      // 164    074   01110100        t
      0,      // 165    075   01110101        u
      0,      // 166    076   01110110        v
      0,      // 167    077   01110111        w
      0,      // 170    078   01111000        x
      0,      // 171    079   01111001        y
      0,      // 172    07A   01111010        z
      0,      // 173    07B   01111011        {    (left/opening brace)
      0,      // 174    07C   01111100        |    (vertical bar)
      0,      // 175    07D   01111101        }    (right/closing brace)
      0,      // 176    07E   01111110        ~    (tilde)
      0       // 177    07F   01111111      DEL    (delete)
};

int
valid_IPaddr(const char *name, int name_len)
{
    int n;
    char *p;

    if (!name || !name_len) {
    	return 0; // Invalid
    }

    if (name_len > 15) { // Only IPV4 XXX.YYY.ZZZ.XXX
    	return 0; // Invalid
    }

    if (!isdigit(name[0])) {
    	return 0; // Invalid
    }

    p = memchr(name, ':', name_len);
    if (p) {
    	name_len = p - name;
    }

    for (n = 0; n < name_len; n++) {
    	if (!valid_IP_name_chars[(int)name[n]]) {
	    return 0; // Invalid char
	}
	// Note: First char is never '.' so name[n-1] is legal
	if ((name[n] == '.') && (name[n-1] == '.')) {
	    return 0; // ".." not allowed
	}
    }
    return 1;
}

/*
 *******************************************************************************
 * delete_invalid_warnig_values() --  Delete Warning Value if Warning-Date 
 *                                    differs to Response-Date. if 
 *                                    Warning-Date is not there, Warning
 *                                    Value wont be deleted.
 *   Return 0 => Success
 *******************************************************************************
 */
int delete_invalid_warning_values(mime_header_t *hdr)
{
#define DOUBLE_QUOTE '"'
#define SPACE ' '
#define MAX_SPACE (3)
#define COMMA ','
#define MAX_COMMA (1)
	int data_len;
	int hdr_cnt;
	int data_index;
	int hdr_index;
	int rv;
	u_int32_t attr;
	time_t ref_tm;
	const char *data;

	typedef struct warning_data_t {
	    const char *data; /* Warning value(s) ptr */
	    int data_len; /* Warning value(s) length */
	    const char *valid_data; /* Valid Warning value(s) ptr */
	    int valid_data_len; /* Valid Warning value(s) length */
	} warning_data;

	warning_data *w_data = NULL;

	if (!is_known_header_present(hdr, MIME_HDR_WARNING) ||
			!is_known_header_present(hdr, MIME_HDR_DATE)) {
	    return 0;
	}

	rv = get_known_header(hdr, MIME_HDR_DATE, &data,
					&data_len, &attr, &hdr_cnt);
	if (rv) {
	    DBG("get_known_header() failed \
				for getting date, rv=%d", rv);
	    return 1;
	}
	ref_tm = parse_date(data, data + data_len);

	rv = get_known_header(hdr, MIME_HDR_WARNING, &data,
					&data_len, &attr, &hdr_cnt);
	if (rv) {
	    DBG("get_known_header() failed  for \
			    getting warning headers, rv=%d", rv);
	    return 2;
	}

	w_data = (warning_data *)
			alloca(sizeof(warning_data) * hdr_cnt);
	if (!w_data) {
	    DBG("alloca() failed, requested_size=%lu",
			    ((sizeof(warning_data)) * hdr_cnt));
	    return 3;
	}

	/* Delete all Warning Values and add valid Warnig Values.
	   It should be modified if there is an api added to delete
	   element in MIME headers */

	/* Store Warning value(s) ptr and length before deleting */
	hdr_index = 0;
	w_data[hdr_index].data = data;
	w_data[hdr_index].data_len = data_len;
	w_data[hdr_index].valid_data = NULL;
	w_data[hdr_index].valid_data_len = 0;
	hdr_index++;

	while ((hdr_cnt - hdr_index) >= 1) {
	    rv = get_nth_known_header(hdr, MIME_HDR_WARNING,
				hdr_index, &data, &data_len, &attr);
	    if (rv) {
		DBG("get_nth_known_header failed for getting \
		    warning header, rv=%d, requested_header=%d, \
		    available_header=%d", rv, hdr_index, hdr_cnt);
		return 4;
	    }

	    w_data[hdr_index].data = data;
	    w_data[hdr_index].data_len = data_len;
	    w_data[hdr_index].valid_data = NULL;
	    w_data[hdr_index].valid_data_len = 0;
	    hdr_index++;
	}

	/* Add all valid Warning values */
	hdr_index = 0;

	while (hdr_cnt - hdr_index > 0) {
	    int msg_start = 0;
	    int valid_data_index = 0;
	    char *valid_data = NULL;

	    data_index = 0;
	    data = w_data[hdr_index].data;
	    data_len = w_data[hdr_index].data_len;

	    valid_data = (char *)alloca(data_len);

	    while (data_len) {
		int quote_in = 0;
		int date_start = 0;
		int field_add = 1;
		int space_cnt = 0;
		int comma_cnt = 0;

		msg_start = data_index;

		while (data_len && IS_WS(data[data_index])) {
		    data_index++;
		    data_len--;
		}

		while (data_len && comma_cnt != MAX_COMMA
				    && space_cnt != MAX_SPACE) {
		    if (data[data_index] == DOUBLE_QUOTE) {
			quote_in = ~quote_in;
		    }
		    if (!quote_in && data[data_index] == COMMA) {
			comma_cnt++;
		    }
		    if (!quote_in && data[data_index] == SPACE) {
			space_cnt++;
		    }
		    data_index++;
		    data_len--;
		}

		if (!data_len || comma_cnt == MAX_COMMA) {
		    // data_index is already moved
		    memmove(valid_data + valid_data_index,
				data + msg_start, data_index - msg_start);
		    valid_data_index += data_index - msg_start - 1;
		} else {
		    if (data[data_index] == DOUBLE_QUOTE) {
			time_t tm = 0;
			data_index++;
			data_len--;
			date_start = data_index;
			while (data_len && data[data_index] != DOUBLE_QUOTE) {
			    data_index++;
			    data_len--;
			}
			if (data[data_index] == DOUBLE_QUOTE) {
			    tm = parse_date(data + date_start,
					    data + data_index - 1);
			    if (ref_tm != tm) {
				field_add = 0;
			    }
			}
		    }
		    while (data_len && data[data_index] != COMMA) {
			data_index++;
			data_len--;
		    }
		    if (field_add) {
			if (!data_len) {
			    memmove(valid_data + valid_data_index,
				    data + msg_start, data_index - msg_start);
			    valid_data_index += data_index - msg_start - 1;
			} else {
			    // copy the comma character also
			    memmove(valid_data + valid_data_index,
				    data + msg_start, data_index + 1 - msg_start);
			    valid_data_index += data_index + 1 - msg_start;
			    data_index++;
			    data_len--;
			}
		    }
		}
	    }

	    if (valid_data[valid_data_index] == COMMA) {
		valid_data_index--;
	    }

	    if (valid_data_index > 0) {
		w_data[hdr_index].valid_data = valid_data;
		w_data[hdr_index].valid_data_len = valid_data_index + 1;
	    }

	    hdr_index++;
	}

	/* Delete all Warning Headers */
	delete_known_header(hdr, MIME_HDR_WARNING);

	hdr_index = 0;
	while (hdr_cnt - hdr_index > 0) {
		if (w_data[hdr_index].valid_data) {
			add_known_header(hdr, MIME_HDR_WARNING,
				w_data[hdr_index].valid_data,
				    w_data[hdr_index].valid_data_len);
		}
		hdr_index++;
	}

	return 0;
#undef DOUBLE_QUOTE
#undef SPACE
#undef MAX_SPACE
#undef COMMA
#undef MAX_COMMA
}

/*
 * End of mime_header.c
 */

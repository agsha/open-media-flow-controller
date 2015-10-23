/*
 * test_http_header.c -- Unit tests
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <alloca.h>
#include "http_header.h"
#include "nkn_memalloc.h"

#if 0
int32_t dbg_log_level;
int32_t dbg_log_mod;
int errorlog_socket;
#endif 

typedef struct hdrname {
    char name[128];
} hdrname_t;

typedef struct hdrval {
    char name[128];
    char val[128];
} hdrval_t;

//void nkn_errorlog(char * fmt, ...);
hdrname_t *known_headers_to_buffer(int map, int *bufsize);
int test_11(void);
int test_10(void);
int test_10a(void);
int test_09(hdrname_t *hnames, int hnames_sz);
int test_08(void);
int test_07(void);
int test_06(void);
int test_05(void);
int test_04(hdrname_t *hnames, int hnames_sz);
int test_03(hdrname_t *hnames, int hnames_sz);
int test_02(hdrname_t *hnames, int hnames_sz);
int test_01(hdrname_t *hnames, int hnames_sz);

int console_log_level;
int console_log_mod;


void * nkn_calloc_type(size_t n, size_t size, nkn_obj_type_t type){
	return calloc(n,size);
}

void * nkn_malloc_type(size_t size, nkn_obj_type_t type){
	return malloc(size);
}

void * nkn_realloc_type(void *p, size_t size, nkn_obj_type_t type){
	return realloc(p, size);
}

int
nkn_attr_get_nth(nkn_attr_t	*ap,
		 const int	nth,
		 nkn_attr_id_t	*id,
		 void		**data,
		 uint32_t *len)
{
    assert(!"Not Supported");
    return 0;
}

void *nkn_attr_get(nkn_attr_t *ap, nkn_attr_id_t id, uint32_t *len)
{
    assert(!"Not Supported");
    return 0;
}

void
nkn_attr_init(nkn_attr_t *ap, int maxlen)
{
    assert(!"Not Supported");
}

int
nkn_attr_set(nkn_attr_t *ap, nkn_attr_id_t id, uint32_t len, void *value)
{
    assert(!"Not Supported");
    return 0;
}

void
nkn_attr_reset_blob(nkn_attr_t *ap, int zero_used_blob)
{
    assert(!"Not Supported");
    return;
}

hdrname_t *known_headers_to_buffer(int map, int *bufsize)
{
    /* map < 0 (lowercase); map == 0 (no map); map > 0 (uppercase) */
    int size;
    int n;
    hdrname_t *hn;
    char *p;

    size = MIME_HDR_MAX_DEFS * sizeof(hdrname_t);
    hn = (hdrname_t *)malloc(size);

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	strncpy((void *)hn[n].name, 
		(void *)http_known_headers[n].name, sizeof(hdrname_t));
	if (map > 0) {
	    p = hn[n].name;
	    while (*p) {
	    	*p = (char)toupper(*p);
		p++;
	    }
	} else if (map < 0) {
	    p = hn[n].name;
	    while (*p) {
	    	*p = (char)tolower(*p);
		p++;
	    }
	}
    }
    *bufsize = size;
    return hn;
}

void log_debug_msg(int dlevel, uint64_t dmodule, const char * fmt, ...)
{
}

int test_14(void)
{
    int n;
    mime_header_t hd;
    int rv;
    const char *data;
    int datalen;
    uint32_t attrs;
    int hcnt;
    int cnt;
    char *strval = "/abc/def/ghi/jkl.html";

    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    for (n = 0; n < 500000; n++) {
    	rv = add_known_header(&hd, MIME_HDR_X_NKN_REMAPPED_URI, 
			      strval, strlen(strval));
    	assert(rv == 0);

    	rv = get_known_header(&hd, MIME_HDR_X_NKN_REMAPPED_URI, 
			      &data, &datalen, &attrs, &hcnt);
    	assert(rv == 0);
    	assert(strncmp(data, strval, datalen) == 0);
    	assert(datalen == strlen(strval));

    	rv = add_known_header(&hd, MIME_HDR_X_NKN_REMAPPED_URI, 
			      data, datalen);
    	assert(rv == 0);

    	rv = get_known_header(&hd, MIME_HDR_X_NKN_REMAPPED_URI, 
			      &data, &datalen, &attrs, &hcnt);
    	assert(rv == 0);
    	assert(strncmp(data, strval, datalen) == 0);
    	assert(datalen == strlen(strval));
    }

    return 0;
}

int test_13b(void)
{
    mime_header_t hd;
    mime_header_t deserialized_hd;
    int rv;
    const char *name;
    int namelen;
    const char *data;
    int datalen;
    uint32_t attrs;
    int hcnt;
    int cnt;
    int n;
    char *p, *p_end;

    char *serialbuf;
    int serialbuf_size;
    int last_serialbuf_size;

    char unk_hdr_name[1024];
    int unk_hdr_name_len;
    char unk_hdr_value[1024];
    int unk_hdr_value_len;

    char *pbuf = 
    "Host:myhost\r\n "
    "User-Agent:myagent\r\n "
    "Date:Thu, 17 Dec 2009 22:17:14 GMT\r\n";

    int pbuf_len = strlen(pbuf);

    rv = init_http_header(&hd, pbuf, pbuf_len);
    assert(rv == 0);

    p = strstr(pbuf, "Host:");
    p += strlen("Host:");
    p_end = strchr(p, '\r');
    rv = add_known_header(&hd, MIME_HDR_HOST, p, p_end-p);
    assert(rv == 0);

    p = strstr(pbuf, "User-Agent:");
    p += strlen("User-Agent:");
    p_end = strchr(p, '\r');
    rv = add_known_header(&hd, MIME_HDR_USER_AGENT, p, p_end-p);
    assert(rv == 0);

    p = strstr(pbuf, "Date:");
    p += strlen("Date:");
    p_end = strchr(p, '\r');
    rv = add_known_header(&hd, MIME_HDR_DATE, p, p_end-p);
    assert(rv == 0);

    for (n = 0; n < 512; n++) {
        unk_hdr_name_len = snprintf(unk_hdr_name, sizeof(unk_hdr_name), 
				    "unk-hdr-%d", n);
	assert(unk_hdr_name_len > 0);
        unk_hdr_value_len = snprintf(unk_hdr_value, sizeof(unk_hdr_value), 
				     "unk-hdr-data-%d", n);
	assert(unk_hdr_value_len > 0);
    	rv = add_unknown_header(&hd, unk_hdr_name, unk_hdr_name_len,
				unk_hdr_value, unk_hdr_value_len);
        assert(rv == 0);
    }

    serialbuf_size = mime_hdr_serialize_datasize(&hd, 0, 0, 0);
    assert(serialbuf_size > 0);
    serialbuf = alloca(serialbuf_size);

    rv = mime_hdr_serialize(&hd, serialbuf, serialbuf_size);
    assert(rv == 0);

    last_serialbuf_size = serialbuf_size;

    ////////////////////////////////////////////////////////////////////////////
    // Deserialize, verify data
    ////////////////////////////////////////////////////////////////////////////

    rv = init_http_header(&deserialized_hd, 0, 0);
    assert(rv == 0);

    rv = mime_hdr_deserialize(serialbuf, serialbuf_size, &deserialized_hd, 
    			      (char *)1, 0);
    assert(rv == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_HOST, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "Host:");
    p += strlen("Host:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_USER_AGENT, 
    			  &data, &datalen, &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "User-Agent:");
    p += strlen("User-Agent:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_DATE, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "Date:");
    p += strlen("Date:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    for (n = 0; n < 512; n++) {
        unk_hdr_name_len = snprintf(unk_hdr_name, sizeof(unk_hdr_name), 
				    "unk-hdr-%d", n);
	assert(unk_hdr_name_len > 0);
        unk_hdr_value_len = snprintf(unk_hdr_value, sizeof(unk_hdr_value), 
				     "unk-hdr-data-%d", n);
	assert(unk_hdr_value_len > 0);

    	rv = get_nth_unknown_header(&deserialized_hd, n, &name, &namelen,
				    &data, &datalen, &attrs);
        assert(rv == 0);
	assert(namelen == unk_hdr_name_len);
	assert(datalen == unk_hdr_value_len);
	assert(strncmp(name, unk_hdr_name, unk_hdr_name_len) == 0);
	assert(strncmp(data, unk_hdr_value, unk_hdr_value_len) == 0);
    }

    // Verify required buffer requirements are unchanged
    serialbuf_size = mime_hdr_serialize_datasize(&deserialized_hd, 0, 0, 0);
    assert(serialbuf_size == last_serialbuf_size);

    return 0;
}

int test_13a(void)
{
    mime_header_t hd;
    mime_header_t deserialized_hd;
    int rv;
    const char *data;
    int datalen;
    uint32_t attrs;
    int hcnt;
    int cnt;
    int n;
    char *p, *p_end;

    char *serialbuf;
    int serialbuf_size;
    int last_serialbuf_size;

    char *pbuf = 
    "Host:myhost\r\n "
    "User-Agent:myagent\r\n "
    "Date:Thu, 17 Dec 2009 22:17:14 GMT\r\n";

    char *cookie_data = "x="
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789";

    char *unknown_data = 
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789";

    int pbuf_len = strlen(pbuf);

    rv = init_http_header(&hd, pbuf, pbuf_len);
    assert(rv == 0);

    p = strstr(pbuf, "Host:");
    p += strlen("Host:");
    p_end = strchr(p, '\r');
    rv = add_known_header(&hd, MIME_HDR_HOST, p, p_end-p);
    assert(rv == 0);

    p = strstr(pbuf, "User-Agent:");
    p += strlen("User-Agent:");
    p_end = strchr(p, '\r');
    rv = add_known_header(&hd, MIME_HDR_USER_AGENT, p, p_end-p);
    assert(rv == 0);

    p = strstr(pbuf, "Date:");
    p += strlen("Date:");
    p_end = strchr(p, '\r');
    rv = add_known_header(&hd, MIME_HDR_DATE, p, p_end-p);
    assert(rv == 0);

    p = strstr(pbuf, "Cookie:");
    p += strlen("Cookie:");
    rv = add_known_header(&hd, MIME_HDR_COOKIE, cookie_data, strlen(cookie_data));
    assert(rv == 0);

    rv = add_unknown_header(&hd,
    			"unknown_hdr", strlen("unknown_hdr"),
    			unknown_data, strlen(unknown_data));
    assert(rv == 0);

    serialbuf_size = mime_hdr_serialize_datasize(&hd, 0, 0, 0);
    assert(serialbuf_size > 0);
    serialbuf = alloca(serialbuf_size);

    rv = mime_hdr_serialize(&hd, serialbuf, serialbuf_size);
    assert(rv == 0);

    last_serialbuf_size = serialbuf_size;

    ////////////////////////////////////////////////////////////////////////////
    // Deserialize, verify and update header
    ////////////////////////////////////////////////////////////////////////////

    rv = init_http_header(&deserialized_hd, 0, 0);
    assert(rv == 0);

    rv = mime_hdr_deserialize(serialbuf, serialbuf_size, &deserialized_hd, 
    			      (char *)1, 0);
    assert(rv == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_HOST, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "Host:");
    p += strlen("Host:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    rv = add_known_header(&deserialized_hd, MIME_HDR_HOST, p, p_end-p);
    assert(rv == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_USER_AGENT, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "User-Agent:");
    p += strlen("User-Agent:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    rv = add_known_header(&deserialized_hd, MIME_HDR_USER_AGENT, p, p_end-p);
    assert(rv == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_DATE, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "Date:");
    p += strlen("Date:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    rv = add_known_header(&deserialized_hd, MIME_HDR_DATE, p, p_end-p);
    assert(rv == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_COOKIE, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    assert(datalen = strlen(cookie_data));
    assert(strncmp(data, cookie_data, datalen) == 0);

    rv = delete_known_header(&deserialized_hd, MIME_HDR_COOKIE);
    assert(rv == 0);

    rv = add_known_header(&deserialized_hd, MIME_HDR_COOKIE, 
			  cookie_data, strlen(cookie_data));
    assert(rv == 0);

    rv = get_unknown_header(&deserialized_hd, 
    			    "unknown_hdr", strlen("unknown_hdr"),
			    &data, &datalen, &attrs);
    assert(rv == 0);
    assert(datalen == strlen(unknown_data));
    assert(strncmp(data, unknown_data, datalen) == 0);

    rv = delete_unknown_header(&deserialized_hd, 
    			       "unknown_hdr", strlen("unknown_hdr"));
    assert(rv == 0);

    rv = add_unknown_header(&deserialized_hd, 
    			"unknown_hdr", strlen("unknown_hdr"),
    			unknown_data, strlen(unknown_data));
    assert(rv == 0);

    // Verify required buffer requirements are unchanged
    serialbuf_size = mime_hdr_serialize_datasize(&deserialized_hd, 0, 0, 0);
    assert(serialbuf_size == last_serialbuf_size);

    return 0;
}

int test_13(void)
{
    mime_header_t hd;
    mime_header_t deserialized_hd;
    int rv;
    const char *data;
    int datalen;
    uint32_t attrs;
    int hcnt;
    int cnt;
    int n;
    char *p, *p_end;

    char *serialbuf;
    int serialbuf_size;
    int last_serialbuf_size;

    char *pbuf = 
    "Host:myhost\r\n "
    "User-Agent:myagent\r\n "
    "Date:Thu, 17 Dec 2009 22:17:14 GMT\r\n";

    char *unknown_data = 
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789"
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789";

    int pbuf_len = strlen(pbuf);

    rv = init_http_header(&hd, pbuf, pbuf_len);
    assert(rv == 0);

    p = strstr(pbuf, "Host:");
    p += strlen("Host:");
    p_end = strchr(p, '\r');
    rv = add_known_header(&hd, MIME_HDR_HOST, p, p_end-p);
    assert(rv == 0);

    p = strstr(pbuf, "User-Agent:");
    p += strlen("User-Agent:");
    p_end = strchr(p, '\r');
    rv = add_known_header(&hd, MIME_HDR_USER_AGENT, p, p_end-p);
    assert(rv == 0);

    p = strstr(pbuf, "Date:");
    p += strlen("Date:");
    p_end = strchr(p, '\r');
    rv = add_known_header(&hd, MIME_HDR_DATE, p, p_end-p);
    assert(rv == 0);

    rv = add_unknown_header(&hd,
    			"unknown_hdr", strlen("unknown_hdr"),
    			unknown_data, strlen(unknown_data));
    assert(rv == 0);

    serialbuf_size = mime_hdr_serialize_datasize(&hd, 0, 0, 0);
    assert(serialbuf_size > 0);
    serialbuf = alloca(serialbuf_size);

    rv = mime_hdr_serialize(&hd, serialbuf, serialbuf_size);
    assert(rv == 0);

    last_serialbuf_size = serialbuf_size;

    // Deserialize, verify and update header

    rv = init_http_header(&deserialized_hd, 0, 0);
    assert(rv == 0);

    rv = mime_hdr_deserialize(serialbuf, serialbuf_size, &deserialized_hd, 
    			      (char *)1, 0);
    assert(rv == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_HOST, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "Host:");
    p += strlen("Host:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    rv = add_known_header(&deserialized_hd, MIME_HDR_HOST, p, p_end-p);
    assert(rv == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_USER_AGENT, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "User-Agent:");
    p += strlen("User-Agent:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    rv = add_known_header(&deserialized_hd, MIME_HDR_USER_AGENT, p, p_end-p);
    assert(rv == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_DATE, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "Date:");
    p += strlen("Date:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    rv = add_known_header(&deserialized_hd, MIME_HDR_DATE, p, p_end-p);
    assert(rv == 0);

    rv = get_unknown_header(&deserialized_hd, 
    			    "unknown_hdr", strlen("unknown_hdr"),
			    &data, &datalen, &attrs);
    assert(rv == 0);
    assert(datalen == strlen(unknown_data));
    assert(strncmp(data, unknown_data, datalen) == 0);

    rv = delete_unknown_header(&deserialized_hd, 
    			       "unknown_hdr", strlen("unknown_hdr"));
    assert(rv == 0);

    rv = add_unknown_header(&deserialized_hd, 
    			"unknown_hdr", strlen("unknown_hdr"),
    			unknown_data, strlen(unknown_data));
    assert(rv == 0);

    // Verify required buffer requirements are unchanged
    serialbuf_size = mime_hdr_serialize_datasize(&deserialized_hd, 0, 0, 0);
    assert(serialbuf_size == last_serialbuf_size);

    // Serialize/Deserialize and verify data
    memset(serialbuf, 0, serialbuf_size);
    rv = mime_hdr_serialize(&deserialized_hd, serialbuf, serialbuf_size);
    assert(rv == 0);

    rv = init_http_header(&deserialized_hd, 0, 0);
    assert(rv == 0);

    rv = mime_hdr_deserialize(serialbuf, serialbuf_size, &deserialized_hd, 
    			(char *)1, 0);
    assert(rv == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_HOST, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "Host:");
    p += strlen("Host:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_USER_AGENT, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "User-Agent:");
    p += strlen("User-Agent:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    rv = get_known_header(&deserialized_hd, MIME_HDR_DATE, &data, &datalen, 
    			  &attrs, &hcnt);
    assert(rv == 0);
    p = strstr(pbuf, "Date:");
    p += strlen("Date:");
    p_end = strchr(p, '\r');
    assert(strncmp(p, data, datalen) == 0);

    rv = add_known_header(&deserialized_hd, MIME_HDR_DATE, p, p_end-p);
    assert(rv == 0);

    rv = get_unknown_header(&deserialized_hd, 
    			    "unknown_hdr", strlen("unknown_hdr"),
			    &data, &datalen, &attrs);
    assert(rv == 0);
    assert(datalen == strlen(unknown_data));
    assert(strncmp(data, unknown_data, datalen) == 0);

    return 0;
}

int test_12(void)
{
    mime_header_t hd;
    int rv;
    const char *data;
    int datalen;
    uint32_t attrs;
    int hcnt;
    char *pbuf = "0123456789abcdefghijklmnopqrstuvwxyz0123456789ABCDEFG";
    int pbuf_len = strlen(pbuf);
    int cnt;
    int n;

    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    while (1) {
    	rv = add_known_header(&hd, MIME_HDR_X_NKN_URI, pbuf, pbuf_len);
	assert(rv == 0);
	if (++cnt > 1000000) {
            break;
	}
    }
    rv = shutdown_http_header(&hd);
    assert(rv == 0);

    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_namevalue_header(&hd, 
				  http_known_headers[n].name, 
				  http_known_headers[n].namelen,
				  http_known_headers[n].name, 
				  http_known_headers[n].namelen,
				 (u_int8_t)(n & 0xff));
	assert(rv == 0);
    }

    rv = delete_all_namevalue_headers(&hd);
    assert(rv == 0);
    assert(hd.cnt_namevalue_headers == 0);

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_namevalue_header(&hd, 
				  http_known_headers[n].name, 
				  http_known_headers[n].namelen,
				  http_known_headers[n].name, 
				  http_known_headers[n].namelen,
				 (u_int8_t)(n & 0xff));
	assert(rv == 0);
    }
    rv = delete_all_namevalue_headers(&hd);
    assert(rv == 0);
    assert(hd.cnt_namevalue_headers == 0);

    rv = shutdown_http_header(&hd);
    assert(rv == 0);
}

int test_11(void)
{
    mime_header_t hd;
    int rv;
    const char *data;
    int datalen;
    uint32_t attrs;
    int hcnt;

    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    char *pbuf = malloc(2048+1);
    pbuf[2048] = '\0';
    memset(pbuf, '*', 2048);

    rv = add_known_header(&hd, 1, pbuf, 2048);
    assert(rv == 0);

    rv = get_known_header(&hd, 1, &data, &datalen, &attrs, &hcnt);
    assert(rv == 0);
    assert(data);
    assert(datalen == 2048);
    assert(bcmp(pbuf, data, 2048) == 0);

    rv = shutdown_http_header(&hd);
    assert(rv == 0);

    free(pbuf);

    return 0;
}

int test_10a(void)
{
    typedef struct test_string_data {
        const char *in;
        const char *out;
	int mods;
    } test_string_data_t;

    int n;
    char *p;
    char *localbuf;
    int outbuf_len;
    int mods;
    int error;


    test_string_data_t test_data[] = {
    	"/..", 0, 0,
    	"/../", 0, 0,
    	"/./..", 0, 0,
    	"/./../", 0, 0,
    	"/.../..", "/", 1,
    	"/.../../", "/", 1,
    	"/.../a", "/.../a", 0,
    	"/.../a/", "/.../a/", 0,
    	"/a/b/c/../..", "/a", 2,
    	"/a/b/c/../../", "/a/", 2,
    	"/a/b/c/../../index.html", "/a/index.html", 2,
    	"/./././a/b/c/index.html", "/a/b/c/index.html", 3,
    	"/./a/./b/./c/A/B/C/index.html", "/a/b/c/A/B/C/index.html", 3,
    	"../a/./b/./c/A/B/C/index.html", 0, 0,
    	"/a/b/../../..", 0, 0,
    	"/a/b/../../../", 0, 0,
    	"///////////////////////a/b/c", "/a/b/c", 22,
    	"///////////////////////a/b/c/", "/a/b/c/", 22,
	"/x/.././y/z", "/y/z", 2,
	"/x/.././y/z/", "/y/z/", 2,
	"/x/y/z/.././././.", "/x/y", 5,
	"/x/y/z/../././././", "/x/y/", 5,
	"/.x/.y/.z/", "/.x/.y/.z/", 0,
	"/.x/.y/.z", "/.x/.y/.z", 0,
	"/..x/..y/..z/", "/..x/..y/..z/", 0,
	"/..x/..y/..z", "/..x/..y/..z", 0,
	"/a.b/c..d/x.html", "/a.b/c..d/x.html", 0,
    	0, 0, 0,
    };

    for (n = 0; test_data[n].in; n++) {
        p = normalize_uri_path(test_data[n].in, strlen(test_data[n].in),
			 	0, &outbuf_len, &mods, &error);
	if (test_data[n].out) {
	    assert(!error);
	    assert(outbuf_len == strlen(test_data[n].out));
	    assert(mods == test_data[n].mods);
	    assert(!strcmp(p, test_data[n].out));
	    free(p);
	} else {
	    assert(error);
	    assert(!p);
	}
    }

    outbuf_len = 4096;
    localbuf = alloca(outbuf_len);

    for (n = 0; test_data[n].in; n++) {
        outbuf_len = 4096;
        p = normalize_uri_path(test_data[n].in, strlen(test_data[n].in),
			       localbuf, &outbuf_len, &mods, &error);
	if (test_data[n].out) {
	    assert(p == localbuf);
	    assert(!error);
	    assert(outbuf_len == strlen(test_data[n].out));
	    assert(mods == test_data[n].mods);
	    assert(!strcmp(p, test_data[n].out));
	} else {
	    assert(error);
	    assert(!p);
	}
    }

    return 0;
}

int test_10(void)
{
#if 0
%41%42%43%44%45%46%47%48%49%4a%4b%4c%4d%4e%4f%50%51%52%53%54%55%56%57%58%59%5a
%61%62%63%64%65%66%67%68%69%6a%6b%6c%6d%6e%6f%70%71%72%73%74%75%76%77%78%79%7a
%30%31%32%33%34%35%36%37%38%39
#endif
    int rv;
    char buf[1024];
    const char *url =
"/home/%41%42%43%44%45%46%47%48%49%4a%4b%4c%4d%4e%4f%50%51%52%53%54%55%56%57%58%59%5a/"
"%61%62%63%64%65%66%67%68%69%6a%6b%6c%6d%6e%6f%70%71%72%73%74%75%76%77%78%79%7a/"
"%30%31%32%33%34%35%36%37%38%39/";
    int bytesused;

    rv = canonicalize_url(url, strlen(url), buf, sizeof(buf), &bytesused);
    printf("url[%s] => [%s]\n", url, buf);
    assert(rv == 0);
    assert(bytesused-1 == (int)strlen(buf));

    return 0;
}

/* Serialize/Deserialize tests */
int test_09(hdrname_t *hnames, int hnames_sz)
{
    int rv;
    int n;
    mime_header_t hd;
    mime_header_t hd2;
    int serial_bufsz;
    char *sbuf;
    char *outbuf;

    const char *data;
    int len;
    uint32_t attrs;
    int hcnt;

    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = add_known_header(&hd, http_known_headers[n].id, 
			hnames[n].name, strlen(hnames[n].name));
	assert(rv == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name),
                        hnames[n].name, strlen(hnames[n].name));
	assert(rv == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_namevalue_header(&hd, hnames[n].name, strlen(hnames[n].name),
                        hnames[n].name, strlen(hnames[n].name), 
			(u_int8_t)(n & 0xff));
	assert(rv == 0);
    }

    rv = init_http_header(&hd2, 0, 0);
    assert(rv == 0);

    serial_bufsz = serialize_datasize(&hd);
    assert(serial_bufsz);

    sbuf = alloca(serial_bufsz);
    assert(sbuf);

    rv = serialize(&hd, sbuf, serial_bufsz);
    assert(rv == 0);

    outbuf = alloca(serial_bufsz);
    assert(outbuf);

    rv = deserialize(sbuf, serial_bufsz, &hd2, outbuf, serial_bufsz);
    assert(rv == 0);

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_known_header(&hd2, http_known_headers[n].id, &data, &len,
			&attrs, &hcnt);
	assert(rv == 0);
	assert(strncmp(data, hnames[n].name, len) == 0);
	assert(hcnt == 1);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_unknown_header(&hd2, hnames[n].name, strlen(hnames[n].name),
			&data, &len, &attrs);
	assert(rv == 0);
	assert(strncmp(data, hnames[n].name, len) == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_namevalue_header(&hd2, hnames[n].name, strlen(hnames[n].name),
			&data, &len, &attrs);
	assert(rv == 0);
	assert(ATTR2USERATTR(attrs) == (n & 0xff));
	assert(strncmp(data, hnames[n].name, len) == 0);
    }

    rv = shutdown_http_header(&hd);
    assert(rv == 0);

    rv = shutdown_http_header(&hd2);
    assert(rv == 0);

    return 0;
}

int test_09a(hdrname_t *hnames, int hnames_sz)
{
    char *extdata;
    int rv;
    int n;
    mime_header_t hd;
    mime_header_t hd2;
    int serial_bufsz;
    char *sbuf;

    const char *data;
    int len;
    uint32_t attrs;
    int hcnt;

    extdata = alloca(2048);

    for (n = 0; n < 26; n++) {
        memset(&extdata[n * 10], 'a' + n, 10);
    }
    rv = init_http_header(&hd, extdata, 2048);
    assert(rv == 0);

    for (n = 0; n < 26; n++) {
        if (n % 2) {
	    rv = add_known_header(&hd, http_known_headers[n].id, 
				  &extdata[n * 10], 10);
	    assert(rv == 0);
	}
    }

    for (n = 0; n < 26; n++) {
        if (n % 2) {
	    rv = add_unknown_header(&hd, &extdata[n * 10], 10, 
				    &extdata[n * 10], 10);
	    assert(rv == 0);
	}
    }

    serial_bufsz = serialize_datasize(&hd);
    assert(serial_bufsz);

    sbuf = alloca(serial_bufsz);
    assert(sbuf);

    rv = serialize(&hd, sbuf, serial_bufsz);
    assert(rv == 0);

    rv = deserialize(sbuf, serial_bufsz, &hd2, (char *)1, 0);
    assert(rv == 0);

    for (n = 0; n < 26; n++) {
        if (n % 2) {
	    rv = get_known_header(&hd2, http_known_headers[n].id, 
				  &data, &len, &attrs, &hcnt);
	    assert(rv == 0);
	    assert(len == 10);
	    assert(strncmp(&extdata[n * 10], data, 10) == 0);
	}
    }

    for (n = 0; n < 26; n++) {
        if (n % 2) {
	    rv = get_unknown_header(&hd2, &extdata[n * 10], 10, 
				    &data, &len, &attrs);
	    assert(rv == 0);
	    assert(len == 10);
	    assert(strncmp(&extdata[n * 10], data, 10) == 0);
	}
    }

    rv = shutdown_http_header(&hd);
    assert(rv == 0);

    rv = shutdown_http_header(&hd2);
    assert(rv == 0);

    return 0;
}

/** Date related headers */
int test_08(void)
{
    int rv;
    mime_header_t hd;
    const cooked_data_types_t *pcd;
    int cd_len;
    mime_hdr_datatype_t type;
    char datebuf[64];
    char *p;

    const char *str_date1 = "Sat, 06 Nov 1999 11:47:37 GMT";
    const char *str_date2 = "Sun, 07 Nov 1999 12:47:38 GMT";
    const char *str_date3 = "Mon, 08 Nov 1999 13:47:39 GMT";
    const char *str_date4 = "Tue, 09 Nov 1999 14:47:40 GMT";
    const char *str_date5 = "Wed, 10 Nov 1999 15:47:41 GMT";
    const char *str_date6 = "Thu, 11 Nov 1999 16:47:42 GMT";
    const char *str_date7 = "Fri, 12 Nov 1999 17:47:43 GMT";

    const char **str;

    const char *str_date_data[7+1] = {str_date1, str_date2, str_date3, 
	str_date4, str_date5, str_date6, str_date7, 0};

    str = &str_date_data[0];

    /**************************************************************************/

    while (*str) {
    	rv = init_http_header(&hd, 0, 0);
    	assert(rv == 0);

    	rv = add_known_header(&hd, MIME_HDR_DATE, *str, strlen(*str));
    	assert(rv == 0);

    	rv = get_cooked_known_header_data(&hd, MIME_HDR_DATE, &pcd, 
    			&cd_len, &type);
    	assert(rv == 0);
	p = mk_rfc1123_time(&pcd->u.dt_date_1123.t, 
			    datebuf, sizeof(datebuf), NULL);
	printf("[%s] => [%s]\n", *str, p);
	assert(!strncasecmp(*str, p, strlen(*str)));

    	rv = delete_known_header(&hd, MIME_HDR_DATE);
    	assert(rv == 0);
    	str++;
    }

    rv = shutdown_http_header(&hd);
    assert(rv == 0);

    return 0;
}

/** Byte range related headers */
int test_07(void)
{
    int rv;
    mime_header_t hd;
    const cooked_data_types_t *pcd;
    int cd_len;
    mime_hdr_datatype_t type;

    const char *str_bad_range_off = "bytes 900-100";
    const char *str_range_off = "bytes 1000-90000";

    const char *str_bad_content_range_off = "bytes 1900-100/50000";
    const char *str_content_range_off = "bytes 250000-500000/700000";
    const char *str_content_len = "221234567890";

    /**************************************************************************/
    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    rv = add_known_header(&hd, MIME_HDR_RANGE, str_bad_range_off, 
    		strlen(str_bad_range_off));
    assert(rv == 0);

    rv = get_cooked_known_header_data(&hd, MIME_HDR_RANGE, &pcd, &cd_len, &type);
    assert(rv);

    rv = delete_known_header(&hd, MIME_HDR_RANGE);
    assert(rv == 0);

    /**************************************************************************/
    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    rv = add_known_header(&hd, MIME_HDR_RANGE, str_range_off, 
    		strlen(str_range_off));
    assert(rv == 0);

    rv = get_cooked_known_header_data(&hd, MIME_HDR_RANGE, &pcd, &cd_len, &type);
    assert(rv == 0);

    rv = delete_known_header(&hd, MIME_HDR_RANGE);
    assert(rv == 0);



    /**************************************************************************/
    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    rv = add_known_header(&hd, MIME_HDR_CONTENT_RANGE, str_bad_content_range_off, 
    		strlen(str_bad_content_range_off));
    assert(rv == 0);

    rv = get_cooked_known_header_data(&hd, MIME_HDR_CONTENT_RANGE, &pcd, 
    		&cd_len, &type);
    assert(rv);

    rv = delete_known_header(&hd, MIME_HDR_CONTENT_RANGE);
    assert(rv == 0);

    /**************************************************************************/
    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    rv = add_known_header(&hd, MIME_HDR_CONTENT_RANGE, str_content_range_off, 
    		strlen(str_content_range_off));
    assert(rv == 0);

    rv = get_cooked_known_header_data(&hd, MIME_HDR_CONTENT_RANGE, &pcd, 
    		&cd_len, &type);
    assert(rv == 0);

    rv = delete_known_header(&hd, MIME_HDR_CONTENT_RANGE);
    assert(rv == 0);

    /**************************************************************************/
    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    rv = add_known_header(&hd, MIME_HDR_CONTENT_LENGTH, str_content_len, 
    		strlen(str_content_len));
    assert(rv == 0);

    rv = get_cooked_known_header_data(&hd, MIME_HDR_CONTENT_LENGTH, &pcd, 
    		&cd_len, &type);
    assert(rv == 0);

    assert(type == DT_SIZE);
    assert(pcd->u.dt_size.ll == 221234567890L);


    rv = shutdown_http_header(&hd);
    assert(rv == 0);

    return 0;
}

/** Cache control header */
int test_06(void)
{
    int rv;
    mime_header_t hd;
    const cooked_data_types_t *pcd;
    cooked_data_types_t last_pcd;
    int cd_len;
    mime_hdr_datatype_t type;
    mime_hdr_datatype_t last_type;
    const char **str;
    char *tbuf;

    const char *cc_1 = "public";
    const char *cc_2 = "private=field1";
    const char *cc_3 = "no-cache=field2";
    const char *cc_4 = "no-store";
    const char *cc_5 = "no-transform";
    const char *cc_6 = "must-revalidate";
    const char *cc_7 = "proxy-revalidate";
    const char *cc_8 = "max-age=60";
    const char *cc_9 = "max-stale=120";
    const char *cc_10 = "min-fresh=180";
    const char *cc_11 = "only-if-cached";

    const char *strlist[12] = {cc_1, cc_2, cc_3, cc_4, cc_5, cc_6, cc_7, cc_8,
    			cc_9, cc_10, cc_11, 0};

    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, cc_1, strlen(cc_1));
    assert(rv == 0);
    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, cc_2, strlen(cc_2));
    assert(rv == 0);
    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, cc_3, strlen(cc_3));
    assert(rv == 0);
    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, cc_4, strlen(cc_4));
    assert(rv == 0);
    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, cc_5, strlen(cc_5));
    assert(rv == 0);
    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, cc_6, strlen(cc_6));
    assert(rv == 0);
    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, cc_7, strlen(cc_7));
    assert(rv == 0);
    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, cc_8, strlen(cc_8));
    assert(rv == 0);
    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, cc_9, strlen(cc_9));
    assert(rv == 0);
    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, cc_10, strlen(cc_10));
    assert(rv == 0);
    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, cc_11, strlen(cc_11));
    assert(rv == 0);

    rv = get_cooked_known_header_data(&hd, MIME_HDR_CACHE_CONTROL, &pcd, &cd_len,
    					&type);
    assert(rv == 0);

    rv = get_cooked_known_header_data(&hd, MIME_HDR_CACHE_CONTROL, &pcd, &cd_len,
    					&type);
    assert(rv == 0);
    last_pcd = *pcd;
    last_type = type;

    rv = delete_known_header(&hd, MIME_HDR_CACHE_CONTROL);
    assert(rv == 0);
    //assert(hd.heap_frees == hd.heap_free_index);

    /**************************************************************************/
    rv = init_http_header(&hd, 0, 0);
    assert(rv == 0);

    tbuf = (char *)alloca(2048);
    tbuf[0] = 0;

    str = strlist;
    while (*str) {
        strcat(tbuf, *str);
	str++;
	if (*str) {
            strcat(tbuf, ",");
	}
    }
    rv = add_known_header(&hd, MIME_HDR_CACHE_CONTROL, tbuf, strlen(tbuf));
    assert(rv == 0);

    rv = get_cooked_known_header_data(&hd, MIME_HDR_CACHE_CONTROL, &pcd, &cd_len,
    					&type);
    assert(rv == 0);
    assert(!bcmp((void *)&last_pcd.u.dt_cachecontrol_enum, 
    			(void *)&pcd->u.dt_cachecontrol_enum, cd_len));
    assert(type == last_type);

    rv = shutdown_http_header(&hd);
    assert(rv == 0);

    return 0;
}

/** Query string tests */
int test_05(void)
{
    int rv, rv2;
    mime_header_t hd;
    int n;
    const char *name;
    int namelen;
    const char *val, *val_tmp;
    int vallen, vallen_tmp;
    const char **tst_qs;
    char *p1;
    char *p2;

    const char *bad_str1 = "";
    const char *bad_str2 = "x";
    const char *bad_str3 = "?&";
    const char *bad_str4 = "?=";
    const char *bad_str5 = "?=&";
    const char *bad_str6 = "?&&&&&&&&&&&&&&&";
    const char *bad_str7 = "?=================";
    const char *bad_str8 = "&=";

    const char *bad_strlist[8+1] = { bad_str1, bad_str2, bad_str3, bad_str4, 
    			bad_str5, bad_str6, bad_str7, bad_str8, 0 };

    const char *valid_str1 = "?x============";
    const char *valid_str2 = "?abcdefg";
    const char *valid_str3 = "?a=arg1&b=arg2&c=arg3";

    const char *valid_str4 = "?%61=%61%72%67%31&%62=%61%72%67%32&%63=%61%72%67%33";
    
    const char *valid_str5 = "?[AQB]&pageName=www.movenetworks.com&v5=www.movenetworks.com&server=http%3A%2F%2Fplayer.movenetworks.com%2Fpub%2FBBB87026%2Fmovenetworks.js&v6=http%3A%2F%2Fplayer.movenetworks.com%2Fpub%2FBBB87026%2Fmovenetworks.js&v7=BBB87026&events=event2&v1=071101000026&c1=071101000026&v2=play%20071101000055&c2=play%20071101000055&c5=event2&[AQE]";

    const char *valid_strlist[4+1] = { valid_str1, valid_str2, valid_str3, 
    			valid_str4,  0};

    const char *break_qs = "?rs=80&ri=5000&h=7c274a769e9a5f4ad0896851154948bb&ext=.flv&fs=1820792";


    tst_qs = bad_strlist;
    while (*tst_qs) {
    	rv = init_http_header(&hd, 0, 0);
    	assert(rv == 0);

    	rv = add_known_header(&hd, MIME_HDR_X_NKN_QUERY, *tst_qs, strlen(*tst_qs));
    	assert(rv == 0);

    	rv = get_query_arg(&hd, 0, &name, &namelen, &val, &vallen);
    	assert(rv);

    	tst_qs++;
    }

    tst_qs = valid_strlist;
    while (*tst_qs) {
    	rv = init_http_header(&hd, 0, 0);
    	assert(rv == 0);

    	rv = add_known_header(&hd, MIME_HDR_X_NKN_QUERY, *tst_qs, strlen(*tst_qs));
    	assert(rv == 0);

    	rv = get_query_arg(&hd, 0, &name, &namelen, &val, &vallen);
    	assert(rv == 0);

    	tst_qs++;
    }


    /**************************************************************************/
    rv = reset_http_header(&hd, 0, 0);
    assert(rv == 0);

    rv = add_known_header(&hd, MIME_HDR_X_NKN_QUERY, valid_str3, strlen(valid_str3));
    assert(rv == 0);

    rv = get_query_arg(&hd, 0, &name, &namelen, &val, &vallen);
    assert(rv == 0);
    assert(!strncmp(name, "a", namelen));
    assert(!strncmp(val, "arg1", vallen));

    rv = get_query_arg(&hd, 1, &name, &namelen, &val, &vallen);
    assert(rv == 0);
    assert(!strncmp(name, "b", namelen));
    assert(!strncmp(val, "arg2", vallen));

    rv = get_query_arg(&hd, 2, &name, &namelen, &val, &vallen);
    assert(rv == 0);
    assert(!strncmp(name, "c", namelen));
    assert(!strncmp(val, "arg3", vallen));

    /**************************************************************************/
    rv = reset_http_header(&hd, 0, 0);
    assert(rv == 0);

    rv = add_known_header(&hd, MIME_HDR_X_NKN_QUERY, valid_str4, strlen(valid_str4));
    assert(rv == 0);

    rv = get_query_arg(&hd, 0, &name, &namelen, &val, &vallen);
    assert(rv == 0);
    assert(!strncmp(name, "a", namelen));
    assert(!strncmp(val, "arg1", vallen));

    rv = get_query_arg(&hd, 1, &name, &namelen, &val, &vallen);
    assert(rv == 0);
    assert(!strncmp(name, "b", namelen));
    assert(!strncmp(val, "arg2", vallen));

    rv = get_query_arg(&hd, 2, &name, &namelen, &val, &vallen);
    assert(rv == 0);
    assert(!strncmp(name, "c", namelen));
    assert(!strncmp(val, "arg3", vallen));


    /**************************************************************************/
    rv = reset_http_header(&hd, 0, 0);
    assert(rv == 0);

    rv = add_known_header(&hd, MIME_HDR_X_NKN_QUERY, valid_str5, strlen(valid_str5));
    assert(rv == 0);

    rv = get_query_arg(&hd, 0, &name, &namelen, &val, &vallen);
    assert(rv == 0);
    assert(!strncmp(name, "[AQB]", namelen));
    assert(!strncmp(val, "", vallen));

    n = 0;
    while ((rv = get_query_arg(&hd, n, &name, &namelen, &val, &vallen)) == 0) {
        p1 = malloc(namelen+1);
	memcpy(p1, name, namelen);
	p1[namelen] = '\0';

        p2 = malloc(vallen+1);
	memcpy(p2, val, vallen);
	p2[vallen] = '\0';
        printf("[%d]: %s=%s\n", n, p1, p2);
	free(p1);
	free(p2);

	rv2 = get_query_arg_by_name(&hd, name, namelen, &val_tmp, &vallen_tmp);
	assert(vallen == vallen_tmp);
	assert(strncmp(val, val_tmp, vallen) == 0);
	n++;
    }

    /**************************************************************************/
    rv = reset_http_header(&hd, 0, 0);
    assert(rv == 0);

    rv = add_known_header(&hd, MIME_HDR_X_NKN_QUERY, break_qs, strlen(break_qs));
    assert(rv == 0);

    rv = get_query_arg_by_name(&hd, "fs", 2, &val_tmp, &vallen_tmp);
    assert(rv == 0);


    rv = shutdown_http_header(&hd);
    assert(rv == 0);

    return 0;
}

/** Basic header replace/append test */
int test_04(hdrname_t *hnames, int hnames_sz)
{
    int rv;
    mime_header_t hd;
    const char *name;
    int namelen;
    const char *data;
    int len;
    uint32_t attrs;
    int hcnt;
    int n;
    int iter = 0;

    rv = init_http_header(&hd, (char *)hnames, hnames_sz);
    assert(rv == 0);

start:

    /* known */
    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_known_header(&hd, http_known_headers[n].id, 
                        hnames[n].name, strlen(hnames[n].name));
	assert(rv == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_known_header(&hd, http_known_headers[n].id, 
                        hnames[(n+1) % MIME_HDR_MAX_DEFS].name, 
			strlen(hnames[(n+1) % MIME_HDR_MAX_DEFS].name));
	assert(rv == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_known_header(&hd, http_known_headers[n].id, &data, &len,
			&attrs, &hcnt);
	assert(rv == 0);
	if (http_known_headers[n].comma_hdr || (n == MIME_HDR_CONNECTION)) {
	    assert(strncmp(data, hnames[n].name, len) == 0);
	    assert(hcnt == 2);
    	    rv = get_nth_known_header(&hd, http_known_headers[n].id, 1, 
	    		&data, &len, &attrs);
	    assert(rv == 0);
	    assert(strncmp(data, hnames[(n+1) % MIME_HDR_MAX_DEFS].name, len) == 0);
	} else {
	    assert(strncmp(data, hnames[(n+1) % MIME_HDR_MAX_DEFS].name, len) == 0);
	    assert(hcnt == 1);
	}
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = delete_known_header(&hd, http_known_headers[n].id);
	assert(rv == 0);
    }
    //assert(hd.heap_free_index == hd.heap_frees);
    if (!iter) {
    	rv = reset_http_header(&hd, (char *)hnames, hnames_sz);
    } else {
    	rv = reset_http_header(&hd, 0, 0);
    }
    assert(rv == 0);

    /* unknown */
    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name),
                        hnames[n].name, strlen(hnames[n].name));
	assert(rv == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name),
                        hnames[(n+1) % MIME_HDR_MAX_DEFS].name, 
			strlen(hnames[(n+1) % MIME_HDR_MAX_DEFS].name));
	assert(rv == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name),
			&data, &len, &attrs);
	assert(rv == 0);
	assert(strncmp(data, hnames[(n+1) % MIME_HDR_MAX_DEFS].name, len) == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = delete_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name));
	assert(rv == 0);
    }
    //assert(hd.heap_free_index == hd.heap_frees);
    if (!iter) {
    	rv = reset_http_header(&hd, (char *)hnames, hnames_sz);
    } else {
    	rv = reset_http_header(&hd, 0, 0);
    }
    assert(rv == 0);

    /* namevalue */
    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_namevalue_header(&hd, hnames[n].name, strlen(hnames[n].name),
                        hnames[n].name, strlen(hnames[n].name),
			(u_int8_t)(n & 0xff));
	assert(rv == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_namevalue_header(&hd, hnames[n].name, strlen(hnames[n].name),
                        hnames[(n+1) % MIME_HDR_MAX_DEFS].name, 
			strlen(hnames[(n+1) % MIME_HDR_MAX_DEFS].name),
			(u_int8_t)(n & 0xff));
	assert(rv == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_namevalue_header(&hd, hnames[n].name, strlen(hnames[n].name),
			&data, &len, &attrs);
	assert(rv == 0);
	assert(ATTR2USERATTR(attrs) == (n & 0xff));
	assert(strncmp(data, hnames[(n+1) % MIME_HDR_MAX_DEFS].name, len) == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = delete_namevalue_header(&hd, hnames[n].name, 
				strlen(hnames[n].name));
	assert(rv == 0);
    }
    //assert(hd.heap_free_index == hd.heap_frees);
    assert(rv == 0);

    if (iter++ == 0) {
    	rv = reset_http_header(&hd, 0, 0);
    	assert(rv == 0);
    	goto start;
    } else {
    	rv = shutdown_http_header(&hd);
    	assert(rv == 0);
    }

    return 0;
}

/** Basic namevalue headers test */
int test_03(hdrname_t *hnames, int hnames_sz)
{
    int rv;
    mime_header_t hd;
    const char *name;
    int namelen;
    const char *data;
    int len;
    uint32_t attrs;
    int hcnt;
    int n;
    int iter = 0;

    rv = init_http_header(&hd, (char *)hnames, hnames_sz);
    assert(rv == 0);

start:

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_namevalue_header(&hd, hnames[n].name, strlen(hnames[n].name),
                        hnames[n].name, strlen(hnames[n].name),
			(u_int8_t)(n & 0xff));
	assert(rv == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_namevalue_header(&hd, hnames[n].name, strlen(hnames[n].name),
			&data, &len, &attrs);
	assert(rv == 0);
	assert(ATTR2USERATTR(attrs) == (n & 0xff));
	assert(strncmp(data, hnames[n].name, len) == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_nth_namevalue_header(&hd, n, &name, &namelen, &data, 
				&len, &attrs);
	assert(rv == 0);
	assert(ATTR2USERATTR(attrs) == (n & 0xff));
	assert(strncmp(name, hnames[n].name, namelen) == 0);
	assert(strncmp(data, hnames[n].name, len) == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = is_namevalue_header_present(&hd, hnames[n].name, 
				strlen(hnames[n].name));
	assert(rv == 1);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = delete_namevalue_header(&hd, hnames[n].name, 
				strlen(hnames[n].name));
	assert(rv == 0);
    	rv = delete_namevalue_header(&hd, hnames[n].name, 
				strlen(hnames[n].name));
	assert(rv);
    }
    assert(hd.heap_free_index == hd.heap_frees);

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_namevalue_header(&hd, hnames[n].name, strlen(hnames[n].name),
                        hnames[n].name, strlen(hnames[n].name),
			(u_int8_t)(n & 0xff));
	assert(rv == 0);
    }

    for (n = MIME_HDR_MAX_DEFS-1; n >= 0; n--) {
    	rv = delete_namevalue_header(&hd, hnames[n].name, 
			strlen(hnames[n].name));
	assert(rv == 0);
    	rv = delete_namevalue_header(&hd, hnames[n].name, 
			strlen(hnames[n].name));
	assert(rv);
    }
    //assert(hd.heap_free_index == hd.heap_frees);
    int cnt_known_headers, cnt_unknown_headers, cnt_namevalue_headers;
    int cnt_querystring_headers;
    rv = get_header_counts(&hd, &cnt_known_headers, &cnt_unknown_headers,
    		&cnt_namevalue_headers, &cnt_querystring_headers);
    assert(rv == 0);
    assert(cnt_known_headers == 0);
    assert(cnt_unknown_headers == 0);
    assert(cnt_namevalue_headers == 0);
    assert(cnt_querystring_headers == 0);

    if (iter++ == 0) {
    	rv = reset_http_header(&hd, 0, 0);
    	assert(rv == 0);
    	goto start;
    } else {
    	rv = shutdown_http_header(&hd);
    	assert(rv == 0);
    }
    return 0;
}

/** Basic unknown headers test */
int test_02(hdrname_t *hnames, int hnames_sz)
{
    int rv;
    mime_header_t hd;
    const char *name;
    int namelen;
    const char *data;
    int len;
    uint32_t attrs;
    int hcnt;
    int n;
    int iter = 0;

    rv = init_http_header(&hd, (char *)hnames, hnames_sz);
    assert(rv == 0);

start:

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name),
                        hnames[n].name, strlen(hnames[n].name));
	assert(rv == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name),
			&data, &len, &attrs);
	assert(rv == 0);
	assert(strncmp(data, hnames[n].name, len) == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_nth_unknown_header(&hd, n, &name, &namelen, &data, 
				&len, &attrs);
	assert(rv == 0);
	assert(strncmp(name, hnames[n].name, namelen) == 0);
	assert(strncmp(data, hnames[n].name, len) == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = is_unknown_header_present(&hd, hnames[n].name, 
				strlen(hnames[n].name));
	assert(rv == 1);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = delete_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name));
	assert(rv == 0);
    	rv = delete_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name));
	assert(rv);
    }
    assert(hd.heap_free_index == hd.heap_frees);

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = add_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name),
                        hnames[n].name, strlen(hnames[n].name));
	assert(rv == 0);
    }

    for (n = MIME_HDR_MAX_DEFS-1; n >= 0; n--) {
    	rv = delete_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name));
	assert(rv == 0);
    	rv = delete_unknown_header(&hd, hnames[n].name, strlen(hnames[n].name));
	assert(rv);
    }
    //assert(hd.heap_free_index == hd.heap_frees);

    if (iter++ == 0) {
    	rv = reset_http_header(&hd, 0, 0);
    	assert(rv == 0);
    	goto start;
    } else {
    	rv = shutdown_http_header(&hd);
    	assert(rv == 0);
    }
    return 0;
}

/** Basic known headers test */
int test_01(hdrname_t *hnames, int hnames_sz)
{
    int rv;
    mime_header_t hd;
    const char *data;
    int len;
    uint32_t attrs;
    int hcnt;
    int n;
    int data_enum;
    int iter = 0;

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = string_to_known_header_enum(hnames[n].name, 
					strlen(hnames[n].name), &data_enum);
	assert(rv == 0);
	assert((int)http_known_headers[n].id == data_enum);
    }

    rv = init_http_header(&hd, (char *)hnames, hnames_sz);
    assert(rv == 0);

start:

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = add_known_header(&hd, http_known_headers[n].id, 
			hnames[n].name, strlen(hnames[n].name));
	assert(rv == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_known_header(&hd, http_known_headers[n].id, &data, &len,
			&attrs, &hcnt);
	assert(rv == 0);
	assert(strncmp(data, hnames[n].name, len) == 0);
	assert(hcnt == 1);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_nth_known_header(&hd, http_known_headers[n].id, 0, &data, &len,
			&attrs);
	assert(rv == 0);
	assert(strncmp(data, hnames[n].name, len) == 0);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = is_known_header_present(&hd, http_known_headers[n].id);
	assert(rv);
    }

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = delete_known_header(&hd, http_known_headers[n].id);
	assert(rv == 0);
    	rv = delete_known_header(&hd, http_known_headers[n].id);
	assert(rv);
    }
    assert(hd.heap_free_index == hd.heap_frees);

    if (iter++ == 0) {
    	rv = reset_http_header(&hd, 0, 0);
    	assert(rv == 0);
    	goto start;
    } else {
    	rv = shutdown_http_header(&hd);
    	assert(rv == 0);
    }
    return 0;
}

int
main (int argc, char **argv)
{
    int rv;
    hdrname_t *hnames;
    int size;

    rv = mime_hdr_startup();
    assert(rv == 0);

    /*
     ***************************************************************************
     * test_01 -- Known headers
     ***************************************************************************
     */
    hnames = known_headers_to_buffer(0, &size); /* No map */
    assert(hnames);

    rv = test_01(hnames, size);
    assert(rv == 0);
    free(hnames);

    /**************************************************************************/
    hnames = known_headers_to_buffer(-1, &size); /* Lowercase */
    assert(hnames);

    rv = test_01(hnames, size);
    assert(rv == 0);
    free(hnames);

    /**************************************************************************/
    hnames = known_headers_to_buffer(1, &size); /* Uppercase */
    assert(hnames);

    rv = test_01(hnames, size);
    assert(rv == 0);
    free(hnames);

    /*
     ***************************************************************************
     * test_02 -- Unknown headers
     ***************************************************************************
     */
    hnames = known_headers_to_buffer(0, &size); /* No map */
    assert(hnames);

    rv = test_02(hnames, size);
    assert(rv == 0);
    free(hnames);

    /**************************************************************************/
    hnames = known_headers_to_buffer(-1, &size); /* Lowercase */
    assert(hnames);

    rv = test_02(hnames, size);
    assert(rv == 0);
    free(hnames);

    /**************************************************************************/
    hnames = known_headers_to_buffer(1, &size); /* Uppercase */
    assert(hnames);

    rv = test_02(hnames, size);
    assert(rv == 0);
    free(hnames);

    /*
     ***************************************************************************
     * test_03 -- Namevalue headers
     ***************************************************************************
     */
    hnames = known_headers_to_buffer(0, &size); /* No map */
    assert(hnames);

    rv = test_03(hnames, size);
    assert(rv == 0);
    free(hnames);

    /**************************************************************************/
    hnames = known_headers_to_buffer(-1, &size); /* Lowercase */
    assert(hnames);

    rv = test_03(hnames, size);
    assert(rv == 0);
    free(hnames);

    /**************************************************************************/
    hnames = known_headers_to_buffer(1, &size); /* Uppercase */
    assert(hnames);

    rv = test_03(hnames, size);
    assert(rv == 0);
    free(hnames);

    /*
     ***************************************************************************
     * test_04 -- Replace/append headers
     ***************************************************************************
     */
    hnames = known_headers_to_buffer(0, &size); /* No map */
    assert(hnames);

    rv = test_04(hnames, size);
    assert(rv == 0);
    free(hnames);

    /**************************************************************************/
    hnames = known_headers_to_buffer(-1, &size); /* Lowercase */
    assert(hnames);

    rv = test_04(hnames, size);
    assert(rv == 0);
    free(hnames);

    /**************************************************************************/
    hnames = known_headers_to_buffer(1, &size); /* Uppercase */
    assert(hnames);

    rv = test_04(hnames, size);
    assert(rv == 0);
    free(hnames);

    /*
     ***************************************************************************
     * test_05 -- Query string tests
     ***************************************************************************
     */
    rv = test_05();
    assert(rv == 0);

    /*
     ***************************************************************************
     * test_06 -- Cache control header
     ***************************************************************************
     */
    rv = test_06();
    assert(rv == 0);

    /*
     ***************************************************************************
     * test_07 -- Byte range headers
     ***************************************************************************
     */
    rv = test_07();
    assert(rv == 0);

    /*
     ***************************************************************************
     * test_08 -- Date header
     ***************************************************************************
     */
    rv = test_08();
    assert(rv == 0);

    /*
     ***************************************************************************
     * test_09 -- Serialize/Deserialize functions
     ***************************************************************************
     */
    hnames = known_headers_to_buffer(1, &size); /* Uppercase */
    assert(hnames);

    rv = test_09(hnames, size);
    assert(rv == 0);

    rv = test_09a(hnames, size);
    assert(rv == 0);

    free(hnames);

    /*
     ***************************************************************************
     * test_10 -- Canonicalize url
     ***************************************************************************
     */
    rv = test_10();
    assert(rv == 0);

    /*
     ***************************************************************************
     * test_10a -- Normalize url
     ***************************************************************************
     */
    rv = test_10a();
    assert(rv == 0);

    /*
     ***************************************************************************
     * test_11 -- Random tests for identified bugs
     ***************************************************************************
     */
    rv = test_11();
    assert(rv == 0);

    /*
     ***************************************************************************
     * test_12 -- Overflow heap test
     ***************************************************************************
     */
    rv = test_12();
    assert(rv == 0);

    /*
     ***************************************************************************
     * test_13 -- Serialize heap compress
     ***************************************************************************
     */
    rv = test_13();
    assert(rv == 0);

    rv = test_13a();
    assert(rv == 0);

    rv = test_13b();
    assert(rv == 0);

    /*
     ***************************************************************************
     * test_14 -- Overwrite known header
     ***************************************************************************
     */
    rv = test_14();
    assert(rv == 0);

    rv = shutdown_http_headers();
    assert(rv == 0);
    printf("\nSuccess...\n");

    return 0;
}

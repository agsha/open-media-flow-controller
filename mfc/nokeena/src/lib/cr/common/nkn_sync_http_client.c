#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>

#include "nkn_debug.h"
#include "nkn_memalloc.h"
#include "nkn_sync_http_client.h"
#include "nkn_vpe_parser_err.h"

static int32_t nkn_http_sync_strip_fqdn(const char *fqdn, char **h,
					char **u);  
static int32_t nkn_http_sync_build_head_req(nkn_http_reader_t *hr);
static int32_t nkn_http_sync_get_resp_status(nkn_http_reader_t *hr);
static int32_t nkn_http_sync_build_br_req(nkn_http_reader_t *hr, 
					  size_t start, 
					  size_t end);
static int32_t nkn_http_sync_get_version(const char *buf, 
					 int32_t buf_size,  
					 int32_t *version, 
					 int32_t *bytes_parsed);
static size_t nkn_http_sync_read_buf(void *hr, uint8_t *buf, 
				     size_t size);
static inline size_t nkn_http_sync_read_int(const char *buf, 
					    const int32_t s);
static inline int32_t nkn_http_sync_bytes_in_line(const char *buf, 
						  const int32_t size);
static inline int32_t nkn_http_sync_send_data(
					      const nkn_http_reader_t *hr); 
static inline int32_t nkn_http_sync_read_data(nkn_http_reader_t *hr);
static int32_t nkn_http_sync_read_bin_data(nkn_http_reader_t *hr, 
					   uint8_t *buf,
					   size_t size);
static inline int32_t nkn_http_sync_get_resp_len(
						 nkn_http_reader_t *hr);  
static struct sockaddr_in* nkn_http_init_sock_addr(char const* host);
static int32_t
nkn_http_timed_recv(int32_t sd, char**buf, uint32_t len);


void*
nkn_http_sync_open(char *fname, const char *mode, size_t size)
{
    nkn_http_reader_t *hr;
    struct sockaddr_in *addr = NULL;
    int32_t rv, bytes_sent, bytes_recv, pos, skip_bytes;
    hr = (nkn_http_reader_t*)\
	nkn_calloc_type(1, sizeof(nkn_http_reader_t),
			mod_vpe_http_reader_t);

    if (!hr) {
	goto http_open_error;
    }
  

    if((hr->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
	goto http_open_error;

    }

    rv = nkn_http_sync_strip_fqdn(fname, &hr->host, &hr->uri);

    if (rv) {
	goto http_open_error;
    }

    addr = nkn_http_init_sock_addr(hr->host);
    if (addr == NULL) {
 	goto http_open_error;
    }
    hr->sin = addr;

    if(connect(hr->sock, (struct sockaddr *)addr, sizeof(struct
							 sockaddr)) <
       0) {
	goto http_open_error;
    }

    if (!strcmp(mode, "f")) {
	hr->content_size = 64*1024;
	hr->full_file = 1;
	goto done;
    }   
	    
    hr->req_size = nkn_http_sync_build_head_req(hr);
    if (hr->req_size <= 0) {
	goto http_open_error;
    }

    bytes_sent = nkn_http_sync_send_data(hr);
    if (bytes_sent < 0) {
	goto http_open_error;
    }

    rv = nkn_http_sync_read_data(hr);
    if (rv < 0) {
	goto http_open_error;
    }
   
    rv = nkn_http_sync_get_resp_status(hr);
    if (rv != 200 && rv != 405) {
	goto http_open_error;

    }	
    if (rv == 405) {
	hr->content_size = 64*1024;
	hr->full_file = 1;
	goto done;
    }

    skip_bytes = 0;
    do {
	rv =\
	    nkn_http_sync_bytes_in_line(&hr->resp_buf[skip_bytes], 
					hr->resp_size - skip_bytes);
	if (!strncasecmp(&hr->resp_buf[skip_bytes],
			 "Content-Length:",
			 15)) {
	    hr->content_size = \
		nkn_http_sync_read_int(&hr->resp_buf[skip_bytes + 15],
				       rv - 15);
	}
	    
	skip_bytes += rv;
    }  while (skip_bytes < (int32_t)hr->resp_size);
	
 done:
    return hr;

 http_open_error:
	
    close(hr->sock);
    if(hr->uri != NULL)
	free(hr->uri);
    if(hr->sin != NULL)
	free(hr->sin);
    if(hr->host != NULL)
	free(hr->host);
    if(hr != NULL)
	free(hr);
    //    if(addr != NULL)
    //	free(addr);
    return NULL;
}

size_t
nkn_http_sync_read(void *buf, size_t n, size_t memb, void *desc)
{
    nkn_http_reader_t *hr;

    hr = (nkn_http_reader_t *)desc;
    return nkn_http_sync_read_buf(hr, buf, n * memb);
}

size_t
nkn_http_sync_seek(void *desc, size_t skip, size_t whence)
{
    int32_t rv;
    nkn_http_reader_t *hr;

    hr = (nkn_http_reader_t *)desc;
    rv = 0;
    switch (whence) {
	case SEEK_SET:
	    hr->pos = skip;
	    break;
	case SEEK_CUR:
	    hr->pos += skip;
	    break;
	case SEEK_END:
	    hr->pos = hr->content_size + skip;
	    break;
    }
    
    if (hr->pos > hr->content_size) {
	rv = 1;
	return rv;
    }

    return rv;
}

static size_t
nkn_http_sync_read_buf(void *desc, uint8_t *buf, size_t size)
{
    int32_t rv;
    nkn_http_reader_t *hr;
    size_t bytes_to_read;

    bytes_to_read = size;
    hr = (nkn_http_reader_t *)desc;
    if (hr->pos + size > hr->content_size) {
	bytes_to_read = hr->content_size - hr->pos;
    }

    rv = nkn_http_sync_build_br_req(hr, hr->pos, hr->pos + bytes_to_read);
    rv = nkn_http_sync_send_data(hr);
    if (rv < 0)
	return rv;
    
    rv = nkn_http_sync_read_bin_data(hr, buf, size);
    return rv;
}

size_t
nkn_http_sync_tell(void *desc)
{
    nkn_http_reader_t *hr;

    hr = (nkn_http_reader_t *)desc;
    return hr->pos;
}

void
nkn_http_sync_close(void *desc)
{
    nkn_http_reader_t *hr;

    hr = (nkn_http_reader_t *)desc;
    if (hr) {
	if (hr->sin) free(hr->sin);
	if (hr->host) free(hr->host);
	if (hr->uri) free(hr->uri);
	close(hr->sock);
	free(hr);
    }

    return;
}

static inline int32_t
nkn_http_sync_read_data(nkn_http_reader_t *hr)
{
    int32_t bytes_recv, rv, pos;
    bytes_recv = 0;

    while(1) {
	char* tmp_buf = &hr->resp_buf[bytes_recv];
	rv = nkn_http_timed_recv(hr->sock, &tmp_buf, 
				 VPE_SYNC_HTTP_READ_RESP_SIZE - bytes_recv);
	if (rv <= 0)
	    break;
	pos = 0;
	while (pos < rv) {
	    if (hr->resp_buf[bytes_recv + pos] == '\r' &&
		hr->resp_buf[bytes_recv + pos + 1] == '\n' &&
		hr->resp_buf[bytes_recv + pos + 2] == '\r' &&
		hr->resp_buf[bytes_recv + pos + 3] == '\n') {
		hr->resp_size = bytes_recv + pos;
		pos = 0;
		goto done;
	    } else {
		pos++;
	    }
	}
	bytes_recv += rv;
    }

 done:
    if (rv < 0) {
	return rv;
    } 

    return bytes_recv;
}


static int32_t
nkn_http_sync_read_bin_data(nkn_http_reader_t* hr, uint8_t* out_data,
			    size_t size) {

    uint8_t* buf = (uint8_t*)hr->resp_buf;
    int32_t sockfd = hr->sock;
    uint32_t buf_total_size = VPE_SYNC_HTTP_READ_RESP_SIZE;
    int32_t err = 0;
    uint32_t buf_read_pos = 0;
    uint32_t buf_write_pos = 0;
    int8_t resp_hdr_flag = 0;
    size_t ret = 0;
    int32_t rc = 0;

    while (1) {

	uint8_t* tmp_buf = buf + buf_write_pos;
	rc = nkn_http_timed_recv(sockfd, (char**)(&tmp_buf), 
				 buf_total_size - buf_write_pos); 
	if (rc <= 0) {
	    err = -1;
	    //assert(0);
	    break;
	}
	buf_write_pos += rc;
	uint32_t count = rc;
	while (count > 3) {

	    if ((buf[buf_read_pos] == '\r') &&
		(buf[buf_read_pos + 1] == '\n') &&
		(buf[buf_read_pos + 2] == '\r') &&
		(buf[buf_read_pos + 3] == '\n')) {
		buf_read_pos += 4;
		resp_hdr_flag = 1;
		hr->resp_size = buf_read_pos;
		break;
	    }
	    buf_read_pos++;
	    count--;
	}
	if (resp_hdr_flag > 0) {

	    nkn_http_sync_get_resp_len(hr);
	    uint32_t cont_len = hr->data_size;
	    uint32_t len_to_read = cont_len -
		(buf_write_pos - buf_read_pos); 
	    memcpy(out_data, buf + buf_read_pos, 
		   buf_write_pos - buf_read_pos);
	    uint32_t out_data_pos = buf_write_pos - buf_read_pos;
	    if (len_to_read == 0)
		break;
	    while (len_to_read > 0) {

		uint8_t* tmp_cont_buf = out_data + out_data_pos;
		rc = nkn_http_timed_recv(sockfd,
			 (char**)(&tmp_cont_buf), len_to_read);
		if (rc <= 0) {
		    err = -2;
		    //assert(0);
		    break;
		}
		len_to_read -= rc;
		out_data_pos += rc;
		buf_write_pos += rc;
	    }
	    break;
	} else {
	    buf_read_pos += count;
	    continue;
	}
    }
    if (err < 0)
	return err;
    if (buf_write_pos == size) {
	hr->pos += size;
	ret = size;
    } else {
	hr->pos += buf_write_pos;
	ret = buf_write_pos - hr->resp_size;
	if (ret < 1800) {
	    int uu = 0;
	}
    }

    return ret;

}

static inline int32_t
nkn_http_sync_get_resp_len(nkn_http_reader_t *hr)
{
    int32_t skip_bytes, rv;

    skip_bytes = 0;
    do {
	rv =\
	    nkn_http_sync_bytes_in_line(&hr->resp_buf[skip_bytes], 
					hr->resp_size - skip_bytes);
	if (!strncasecmp(&hr->resp_buf[skip_bytes],
			 "Content-Length:",
			 15)) {
	    hr->data_size = \
		nkn_http_sync_read_int(&hr->resp_buf[skip_bytes + 15],
				       rv - 15);
	}
	    
	skip_bytes += rv;
    }  while (skip_bytes < (int32_t)hr->resp_size);
    
    return hr->data_size;
}

static inline int32_t
nkn_http_sync_send_data(const nkn_http_reader_t *hr)
{
    int32_t bytes_sent, rv;

    bytes_sent = 0;
    while (bytes_sent < (int32_t)hr->req_size) {
	rv = send(hr->sock, &hr->req_buf[bytes_sent],
		  hr->req_size - bytes_sent, 0);
	if (rv < 0) {
	    return errno;
	}
	bytes_sent += rv;
    }    

    return bytes_sent;
}

static inline size_t 
nkn_http_sync_read_int(const char *buf, const int32_t size)
{
    char *p1, *p2, z[256];
    size_t s, len, rv;
    
    p1 = (char*)buf;
    p2 = p1;
    while (*p2 == ' ') {
	p2++;
    }

    p1 = p2;
    while (*p2 != '\r' || *p2 == ' ') {
	p2++;
    }
    len = p2 - p1;

    if (len + 1 > 256) {
	return -E_VPE_HTTP_SYNC_PARSE_ERR;
    }
    
    memcpy(z, p1, len);
    z[len] = '\0';
    //rv = atoi(z);
    rv = atol(z);
    
    return rv;
}
    
static inline int32_t
nkn_http_sync_bytes_in_line(const char *buf, const int32_t size)
{
    char *p;
    int32_t s;
    
    p = (char *)buf;
    s = (int32_t)size;
    
    while (s && !(*p == '\r' && *(p+1) == '\n')) {
	p++;
	s--;
    }

    return p-buf+2;
}

static int32_t
nkn_http_sync_get_resp_status(nkn_http_reader_t *hr)
{
    char *p1, *p2, status_str[5];
    int32_t size, pos, ver, status_len, status;
    p1 = p2 = hr->resp_buf;
    size = hr->resp_size;
    
    while ( !(*p2 == '\r' && *(p2 + 1) == '\n') ) {
	p2++;
    }

    nkn_http_sync_get_version(p1, p2 - p1, &ver, &pos);
    if (ver == -1) {
	return -E_VPE_HTTP_SYNC_VER_ERR;
    }
   
    p1 = p1 + pos;
    p2 = p1;
    while (*p2 == ' ') {
	p2++;
    }

    p1 = p2;
    while (*p2 != ' ') {
	p2++;
    }
    status_len = p2 - p1;
    if (status_len + 1 > 5) {
	return -E_VPE_HTTP_SYNC_PARSE_ERR;
    }

    memcpy(status_str, p1, status_len);
    status_str[status_len] = '\0';
    status = atoi(status_str);

    return status;
}

#define HTTP_V_11_STR "HTTP/1.1"
#define HTTP_V_11 1
#define HTTP_V_11_LEN 8

#define HTTP_V_10_STR "HTTP/1.0"
#define HTTP_V_10 1
#define HTTP_V_10_LEN 8

static int32_t
nkn_http_sync_get_version(const char *buf, int32_t buf_size, 
			  int32_t *version, int32_t *bytes_parsed)
{

    if (!strncasecmp(buf, HTTP_V_10_STR, HTTP_V_10_LEN)) {
	*version = HTTP_V_10;
	*bytes_parsed = HTTP_V_10_LEN;
    } else if(!strncasecmp(buf, HTTP_V_11_STR, HTTP_V_11_LEN)) {
	*version = HTTP_V_11;
	*bytes_parsed = HTTP_V_11_LEN;
    } else {
	*version = -1;
	*bytes_parsed = 0;
    }

    return 0;   
	
}

static int32_t
nkn_http_sync_build_head_req(nkn_http_reader_t *hr)
{
    int32_t len;
    const char *fmt = "HEAD %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: "
	"%s\r\nConnection: keep-alive\r\n\r\n"; 
    
    len  = snprintf(hr->req_buf, 4096, fmt, hr->uri, hr->host,
		    "MFP-HTTP-Client"); 
    hr->req_size = len;

    return len;
}

static int32_t
nkn_http_sync_build_br_req(nkn_http_reader_t *hr, size_t start, 
			   size_t end)
{
    int32_t len;
    const char *fmt_range = "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: " 
	"%s\r\nRange: bytes=%lu-%lu\r\nConnection: keep-alive\r\n\r\n";
    const char *fmt_full_file = "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: " 
	"%s\r\nConnection: keep-alive\r\n\r\n";
    char *fmt = (char *)fmt_range;
    
    if (hr->full_file) {
	fmt = (char *)fmt_full_file;
    }
    len  = snprintf(hr->req_buf, 4096, fmt, hr->uri, hr->host,
		    "MFP-HTTP-Client", start, end - 1, hr->content_size); 
    hr->req_size = len;

    return len;
}

static int32_t
nkn_http_sync_strip_fqdn(const char *fqdn, char **h, char **u)
{
    char *p, *p1, *host, *uri;
    int32_t len;
    
    len = 0;
    p = (char *)fqdn;
    
    p = strstr(fqdn, "http://");
    if (!p) {
	return -E_VPE_PARSER_NO_MEM;
    }

    p += 7;//move past the prot identifier

    p1 = strchr(p, '/');
    len = p1 - p + 1;
    
    host = (char*)\
	nkn_malloc_type(len * sizeof(char), mod_vpe_charbuf);

    if (!host) { 
	return -E_VPE_PARSER_NO_MEM;
    }

    memcpy(host, p, len - 1);//len includes null termination
    host[len - 1] = '\0';//len includes null termination
	
    /* skip trailing slashes */		
    while(*(p1+1) == '/') {
	p1++;
    }
    
    uri = strdup(p1);

    if (!uri) {
	return -E_VPE_PARSER_NO_MEM;
    }

    *h = host;
    *u = uri;

    return 0;   
}

static struct sockaddr_in*
nkn_http_init_sock_addr(char const* host) {

    uint16_t port = 80;
    char* port_str = strchr(host, ':');
    char* host_l3;
    uint32_t host_len = strlen(host);
    char* tail_ptr_dummy;
    if (port_str != NULL) {

	host_len = port_str - host;
	port_str++;

	char* tail_ptr = nkn_malloc_type((strlen(port_str) + 1) * sizeof(char),
					 mod_vpe_charbuf);
	tail_ptr_dummy = tail_ptr;
	if (tail_ptr == NULL)
	    return NULL;
	long int port_num = strtol(port_str, &tail_ptr, 0);
	if (tail_ptr[0] != '\0') {
	    perror("strtol: ");
	    free(tail_ptr_dummy);
	    return NULL;
	}
	free(tail_ptr_dummy);
	port = (uint16_t) port_num;
    }
    host_l3 = nkn_malloc_type((host_len + 1) * sizeof(char), mod_vpe_charbuf);
    if (host_l3 == NULL)
	return NULL;
    strncpy(host_l3, host, host_len); 
    host_l3[host_len] = '\0';

    struct sockaddr_in* addr = (struct sockaddr_in *)nkn_malloc_type(
								     sizeof(struct sockaddr_in), mod_vpe_charbuf);
    if (addr == NULL) {
	free(host_l3);
	return NULL;
    }
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if (inet_pton(AF_INET, host_l3, (void *)(&(addr->sin_addr.s_addr))) <= 0) {

	free(host_l3);
	free(addr);
	return NULL;
    }
    free(host_l3);
	
    return addr;
}


static int32_t
nkn_http_timed_recv(int32_t sd, char**buf, uint32_t len) {

    fd_set read_set;
    int32_t rc = 0;
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    FD_ZERO(&read_set);
    FD_SET(sd, &read_set);

    rc = select(sd+1, &read_set, NULL, NULL, &timeout);
    if (rc == 0) {
        DBG_MFPLOG ("HTTP_RECV", MSG, MOD_MFPFILE,
		    "Timeout: no data for 10 seconds");
	return -1;//timeout
    }
    if (rc < 0) {
	perror("nkn_http_timed_recv: select: ");
        DBG_MFPLOG ("HTTP_RECV", MSG, MOD_MFPFILE,
		    "Select() failed");
	return -2;
    }
    if (FD_ISSET(sd, &read_set)) {

	rc = recv(sd, *buf, len, 0);
	if (rc < 0) {
	    perror("nkn_http_timed_recv: recv: ");
	    DBG_MFPLOG ("HTTP_RECV", MSG, MOD_MFPFILE,
			"recv() failed");
	    return -3;
	}
    }

    return rc;
}

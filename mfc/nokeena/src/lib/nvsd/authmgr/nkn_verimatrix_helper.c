#include <stdio.h>

#include "nkn_authmgr.h"
#include "nkn_verimatrix_helper.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <glib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "nkn_debug.h"

#include "kms/mfp_kms_key_store.h"

/* VERIMATRIX HELPER - private methods */
/*static int32_t verimatrix_helper_create_keychain(kms_info_t *ki);
static int32_t verimatrix_helper_send_key_request(kms_info_t *ki,
						  kms_out_t *ko);
static int32_t find_crlf(const char *const buf, int32_t len);
static char* skip_till_crlf(const char *const buf, int32_t len);
static int32_t parse_key_response(const char *const buf, uint32_t resp_len,
				  uint32_t body_offset, const kms_out_t *out);
static int32_t get_resp_code(const char * const resp);
static int32_t mem_casecmp(const char * p1, const char * p2, int n);
static char *skip_till_space(const char *const buf);
*/

/******************************************************************
 *                  VERIMATRIX HELPER API
 *****************************************************************/
int32_t verimatrix_helper(auth_msg_t *data)
{
    data->errcode = 0;
    kms_client_key_store_t *kms_key_store =
		(kms_client_key_store_t*)data->authdata;
    kms_key_resp_t* key_resp_out;

    key_resp_out = (kms_key_resp_t*)nkn_task_get_private(TASK_TYPE_AUTH_MANAGER,
					  data->task_id);
    kms_key_resp_t* key_resp = kms_key_store->get_kms_key(kms_key_store,
							  data->seq_num);
	if (key_resp == NULL) {
		DBG_MFPLOG ("0", ERROR, MOD_MFPLIVE, "KMS Error retrieving key");
		return -1;
	}

    key_resp_out->key = key_resp->key;
    key_resp_out->key_len = key_resp->key_len;
    key_resp_out->key_uri = key_resp->key_uri;
    key_resp_out->is_new = key_resp->is_new;

    //    free(key_resp);
    return 200;
}

/******************************************************************
 *                PRIVATE METHODS
 ******************************************************************/
/*
static int32_t verimatrix_helper_send_key_request(kms_info_t *ki,
						  kms_out_t *ko)
{
    const char *fmt = "GET "
	"/CAB/keyfile?r=22&t=VOD&p=0 HTTP/1.1\r\n"
	"Host: %s\r\n\r\n";
    char req[1024], resp[1024];
    int32_t pos = 0, rv1 = 0, resp_size = 1024, send_bytes = 0, 
	err = 0; 
    int32_t sock = 0, rv = 0, off = 0;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
	sock = 0;
	err = -1;
	goto error;
    }

    addr.sin_family = AF_INET;
    rv = inet_pton(AF_INET, (char *)ki->kmsaddr, &addr.sin_addr.s_addr);
    if (rv <= 0) {
	err = -1;
	goto error;
    }
    addr.sin_port = htons(ki->kmsport);

    rv = connect(sock, (struct sockaddr *)&addr, 
		 sizeof(struct sockaddr));
    if (rv < 0) {
	err = -1;
	goto error;
    }

    send_bytes += snprintf(req, 1024, fmt, ki->kmsaddr);
    while (send_bytes) {
	rv1 = send(sock, &req[pos], send_bytes, 0);
	if (rv1 < 0) {
		break;
	}
	send_bytes -= rv1;
	pos += rv1;
    }
    
    pos = 0;
    while((rv1 = recv(sock, &resp[pos], resp_size - pos, 0))
	      > 0) {
	if ((off = find_crlf(&resp[pos], rv1)) >= 0) {
	    break;
	}
	pos += rv1;
    }

    err = parse_key_response(resp, pos, off, ko);
    close(sock);
    return err;

 error:
    if (sock) close(sock);
    return err;
    
}

static int32_t verimatrix_helper_create_keychain(kms_info_t *ki)
{
    const char *fmt = "POST "
	"/CAB/keyfile?r=22&t=VOD&c=100 HTTP/1.1\r\n"
	"Host: %s\r\n\r\n";
    char req[1024], resp[1024];
    int32_t pos = 0, rv1 = 0, resp_size = 1024, send_bytes = 0;
    int32_t sock = 0, err = 0, rv = 0, off = 0;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
	sock = 0;
	err = -1;
	goto error;
    }

    addr.sin_family = AF_INET;
    rv = inet_pton(AF_INET, (char *)ki->kmsaddr, &addr.sin_addr.s_addr);
    if (rv <= 0) {
	err = -1;
	goto error;
    }
    addr.sin_port = htons(ki->kmsport);

    rv = connect(sock, (struct sockaddr *)&addr, 
		 sizeof(struct sockaddr));
    if (rv < 0) {
	err = -1;
	goto error;
    }

    send_bytes += snprintf(req, 1024, fmt, ki->kmsaddr);
    while (send_bytes) {
	rv1 = send(sock, &req[pos], send_bytes, 0);
	if (rv1 < 0) {
		break;
	}
	send_bytes -= rv1;
	pos += rv1;
    }
    
    pos = 0;
    while((rv1 = recv(sock, &resp[pos], 
		      resp_size - pos, 
		      0)) > 0) {
	if ((off = find_crlf(&resp[pos], rv1)) >= 0) {
	    break;
	}
	pos += rv1;
    }

    err = get_resp_code(&resp[pos]);
    ki->flag &= (~VERIMATRIX_KEY_CHAIN_CREATE);

    close(sock);
    return err;

 error:
    if (sock) close(sock);
    return err;
    
}

static int32_t find_crlf(const char *const buf, int32_t len)
{
    int32_t i = 0, rv = -1;
    const char CRLF[4] = {'\r', '\n', '\r', '\n'};

    while (i <= len - 4) {
	if (!memcmp(&buf[i], CRLF, 4)) {
	    rv = i;
	    break;
	}
	i++;
    }
    
    return rv;
}

static char* skip_till_crlf(const char *const buf, int32_t len)
{
    int32_t i = 0;
    const char CRLF[2] = {'\r', '\n'};

    while (i <= len - 2) {
	if (!memcmp(&buf[i], CRLF, 2)) {
	    return (char*)(buf + i);
	}
	i++;
    }
   
    return (char*)(buf + len);
}

static int32_t parse_key_response(const char *const resp, uint32_t resp_len,
				  uint32_t body_offset, const kms_out_t *out)
{
    char b[256]={0};
    const char *p = resp, *p1, *p2, *p3;
    const char *const end = resp + body_offset;
    int32_t respcode, len, content_len, err = 0;
    const kms_out_t *ko;
    int fd;

    ko = out;
    resp_len = resp_len;
    respcode = err = get_resp_code(p);
    p1 = p;
    p1 = skip_till_crlf(p1, end - p1) + 2;//skip CRLF
    while (p1 < end) {
	p2 = skip_till_crlf(p1, end - p1) + 2; // skip CRLF
	switch(*(uint32_t *)p1 | 0x20202020) {
	    case 0x746e6f63: //content-length
		if (!mem_casecmp(p1, "content-length", 14)) {
		    if (*(p1+14) == ':') {
			p3 = skip_till_space(p1 + 14) + 1;
		    } else { 
			break;
		    }
		    len = p2 - p3 - 2//CRLF;
		    if (len > 127) {
			err = -1;
			goto error;
		    }
		    memcpy(b, p3, len);
		    content_len = atoi(b);
		    break;
		}
	    case 0x61636f6c: //location
		if (*(p1 + 8) == ':') {
		    p3 = skip_till_space(p1 + 8) + 1;
		} else {
		    break;
		}
		len = p2 - p3 - 2//CRLF;
		if (len > 255) {
		    err = -1;
		    goto error;
		}
		memcpy((void *)ko->key_location, p3, len);
		break;
	}
	p1 = p2;
    }
    
    if (err == 200) {
	if (content_len > 127) {
	    err = -1;
	    goto error;
	}
	memcpy((void *)ko->key, resp + body_offset + 4,
	       content_len); 
    }

    // get an initialization vector 
    fd = open("/dev/random", O_RDONLY);
#if 0
    if (fd == -1) {
	err = -1;
	goto error;
    }
    read(fd, (void*)out->iv, 16);
#endif
    close(fd);

    return err;

 error:
    return err;    
}

static int32_t get_resp_code(const char * const resp)
{
    char b[256]={0};
    const char *p1, *p = (const char*)resp;
    int32_t respcode_len, respcode, err;

    // skip the version string
    p += HTTP_VER_STR_LEN;
	
    // skip till next space 
    p1 = p;
    while (*p1 != ' ') 
	p1++;
	
    respcode_len = p1 - p;
    memcpy(b, p, respcode_len);
    err = respcode = atoi(b);
    printf("respcode=%d\n", respcode);
    
    return err;
}


static int32_t mem_casecmp(const char * p1, const char * p2, int n)
{
	// compare two string without case
	if(!p1 || !p2 || n==0) return 1;
	while(n) {
		if(*p1==0 || *p2==0) return 1;
		if((*p1|0x20) != (*p2|0x20)) return 1;
		p1++;
		p2++;
		n--;
	}
	return 0;
}


static char *skip_till_space(const char *const buf)
{
    const char *p = buf;

    while(*p != ' ') {
	p++;
    }

    return (char *)p;
}

*/

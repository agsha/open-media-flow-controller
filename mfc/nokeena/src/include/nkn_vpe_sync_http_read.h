#ifndef _VPE_HTTP_READ_
#define _VPE_HTTP_READ_

#include <stdio.h>
#include <inttypes.h>

#ifdef __cplusplus 
extern "C" {
#endif

#define VPE_SYNC_HTTP_READ_REQ_SIZE 4096
#define VPE_SYNC_HTTP_READ_RESP_SIZE 4096

    typedef struct tag_http_reader_t {
	size_t pos;
	int32_t sock;
	char *host;
	char *uri;
	struct sockaddr_in *sin;
	char req_buf[VPE_SYNC_HTTP_READ_REQ_SIZE];
	char resp_buf[VPE_SYNC_HTTP_READ_RESP_SIZE];
	size_t content_size;
	size_t resp_size;
	size_t req_size;
	size_t data_size;
    } vpe_http_reader_t;
    
    void* vpe_http_sync_open(char *fname, const char *mode, size_t size);
    size_t vpe_http_sync_seek(void *hr, size_t skip, 
			      size_t whence);
    size_t vpe_http_sync_read(void *buf, size_t n, size_t memb, 
			      void *desc); 
    size_t vpe_http_sync_tell(void *hr);
    void vpe_http_sync_close(void *hr);

#ifdef __cplusplus
}
#endif

#endif //_VPE_HTTP_READ_

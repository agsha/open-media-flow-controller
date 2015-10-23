#ifndef __NKN_VPEMGR_DEFS__H__
#define __NKN_VPEMGR_DEFS__H__

#include <assert.h>
#include <sys/queue.h>
#include "server.h"
#include "fqueue.h"

// Reserved field for vpe back end process identifiers
#define VPE_IDENTIFIER_MAX_LEN 		16
#define VPE_IDENTIFIER_SMOOTHFLOW         1
#define VPE_IDENTIFIER_RTSP               2
#define VPE_IDENTIFIER_GENERAL_TRANSCODE  3
#define VPE_IDENTIFIER_YOUTUBE_TRANSCODE  4

typedef struct vpemgr_request {
    fqueue_element_t fq_element;
    LIST_ENTRY(vpemgr_request) list;
} vpemgr_request_t;

LIST_HEAD(vpemgr_req_queue, vpemgr_request);
extern pthread_mutex_t vpemgr_req_queue_mutex;

/* Public vpemgr_api.c routines */
int VPEMGR_init(void);
int VPEMGR_shutdown (void);

int submit_vpemgr_sf_request(const char *hostData, int hostLen, const char *uriData, int uriLen, const char *stateQuery, int stateQueryLen);
int submit_vpemgr_rtsp_request(const char *hostData, int hostLen, const char *uriData, int uriLen);
int submit_vpemgr_generic_transcode_request(const char *hostData, int hostLen,
					    const char *uriData, int uriLen,
					    const char *containerType, int containerTypeLen,
					    const char *transRatioData, int transRatioLen,
          				    const char *transComplexData, int transComplexLen);
int submit_vpemgr_youtube_transcode_request(const char *hostData, int hostLen,
					    const char *video_id_uri_query, int video_id_uri_queryLen,
					    const char *format_tag,int format_tagLen,
					    const char *remapUriData, int remapUriLen,
					    const char *nsuriPrefix, int nsuriPrefixLen,
					    const char *transRatioData, int transRatioLen,
					    const char *transComplexData, int transComplexLen);

#endif // __NKN_VPEMGR_DEFS__H__

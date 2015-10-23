#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <netdb.h>
#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_stat.h"
#include "network.h"
#include "server.h"
#include "fqueue.h"
#include "nkn_vpemgr_defs.h"

#define DBG(_fmt, ...) DBG_LOG(MSG, MOD_OM, _fmt, __VA_ARGS__)
#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

extern unsigned long long glob_vpemgr_tot_reqs, glob_vpemgr_err_dropped;

pthread_mutex_t vpemgr_req_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t vpemgr_req_queue_cv;
extern struct vpemgr_req_queue vpemgr_req_head;
static int vpemgr_req_thread_wait = 1;
void *vpemgr_req_func(void *arg);


/*
 * submit_vpemgr_sf_request() -- Submit Smoothflow request to VPE Manager (VPEMGR)
 */
int submit_vpemgr_sf_request(const char *hostData, int hostLen, const char *uriData, int uriLen, const char *stateQuery, int stateQueryLen)
{
    fqueue_element_t *qe;
    vpemgr_request_t *vpemgr_q_req = 0;
    int rv = 0;
    char *mapBuf;

    // Build fqueue_element_t
    vpemgr_q_req = (vpemgr_request_t *)nkn_malloc_type(sizeof(vpemgr_request_t), mod_vpe_vpemgr_queue);

    mapBuf = (char *)alloca(VPE_IDENTIFIER_MAX_LEN);
    memset(mapBuf, 0, VPE_IDENTIFIER_MAX_LEN);

    if (vpemgr_q_req) {
        qe = &vpemgr_q_req->fq_element;

        if (uriData) {
            rv = init_queue_element(qe, 0, "", uriData);
            if (rv) {
		DBG_LOG(WARNING, MOD_VPEMGR, "init_queue_element() failed, rv=%d uri=%s", rv, uriData);
                rv = -1;
                goto exit;
            }

	    // Add Identifier attribute
	    snprintf(mapBuf, VPE_IDENTIFIER_MAX_LEN, "%d", VPE_IDENTIFIER_SMOOTHFLOW);
            rv = add_attr_queue_element_len(qe, "vpe_identifier", 14, mapBuf, strlen(mapBuf));
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR, "add_attr_queue_element(host_name: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }


            // Add additional attributes
            rv = add_attr_queue_element_len(qe, "host_name", 9,  hostData, hostLen);
            if (rv) {
		DBG_LOG(WARNING, MOD_VPEMGR, "add_attr_queue_element(host_name: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }

	    // Add additional attributes
            rv = add_attr_queue_element_len(qe, "uri_name", 8,  uriData, uriLen);
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR, "add_attr_queue_element(uri_name: vpe_process) failed,  rv=%d", rv);
                rv = -1;
                goto exit;
            }

	    // Add additional attributes
            rv = add_attr_queue_element_len(qe, "state_query_param", 17,  stateQuery, stateQueryLen);
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR, "add_attr_queue_element(state_query_param: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }


        } else {
	    DBG_LOG(WARNING, MOD_VPEMGR, "Null uri=%p", uriData);
            rv = -1;
            goto exit;
        }

	// Enqueue VPEMGR request to push thread
        pthread_mutex_lock(&vpemgr_req_queue_mutex);

        LIST_INSERT_HEAD(&vpemgr_req_head, vpemgr_q_req, list);

        if (vpemgr_req_thread_wait) {
            pthread_cond_signal(&vpemgr_req_queue_cv);
        }

        pthread_mutex_unlock(&vpemgr_req_queue_mutex);

    } else {
        DBG("malloc(%ld) failed", sizeof(vpemgr_request_t));
        rv = -1;
        goto exit;
    }

    return rv;

 exit:
    if (vpemgr_q_req) {
        free(vpemgr_q_req);
    }

    return rv;
}



/*
 * submit_vpemgr_rtsp_request() -- Submit RTSP request to VPE Manager (VPEMGR)
 */
int submit_vpemgr_rtsp_request(const char *hostData, int hostLen, const char *uriData, int uriLen)
{
    fqueue_element_t *qe;
    vpemgr_request_t *vpemgr_q_req = 0;
    int rv = 0;
    char *mapBuf;

    // Build fqueue_element_t
    vpemgr_q_req = (vpemgr_request_t *)nkn_malloc_type(sizeof(vpemgr_request_t), mod_vpe_vpemgr_queue);

    mapBuf = (char *)alloca(VPE_IDENTIFIER_MAX_LEN);
    memset(mapBuf, 0, VPE_IDENTIFIER_MAX_LEN);

    if (vpemgr_q_req) {
        qe = &vpemgr_q_req->fq_element;

        if (uriData) {
            rv = init_queue_element(qe, 0, "", uriData);
            if (rv) {
		DBG_LOG(WARNING, MOD_VPEMGR, "init_queue_element() failed, rv=%d uri=%s", rv, uriData);
                rv = -1;
                goto exit;
            }

	    // Add Identifier attribute
	    snprintf(mapBuf, VPE_IDENTIFIER_MAX_LEN, "%d", VPE_IDENTIFIER_RTSP);
            rv = add_attr_queue_element_len(qe, "vpe_identifier", 14, mapBuf, strlen(mapBuf));
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR, "add_attr_queue_element(host_name: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }


            // Add additional attributes
            rv = add_attr_queue_element_len(qe, "host_name", 9,  hostData, hostLen);
            if (rv) {
		DBG_LOG(WARNING, MOD_VPEMGR, "add_attr_queue_element(host_name: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }

	    // Add additional attributes
            rv = add_attr_queue_element_len(qe, "uri_name", 8,  uriData, uriLen);
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR, "add_attr_queue_element(uri_name: vpe_process) failed,  rv=%d", rv);
                rv = -1;
                goto exit;
            }

        } else {
	    DBG_LOG(WARNING, MOD_VPEMGR, "Null uri=%p", uriData);
            rv = -1;
            goto exit;
        }

	// Enqueue VPEMGR request to push thread
        pthread_mutex_lock(&vpemgr_req_queue_mutex);

        LIST_INSERT_HEAD(&vpemgr_req_head, vpemgr_q_req, list);

        if (vpemgr_req_thread_wait) {
            pthread_cond_signal(&vpemgr_req_queue_cv);
        }

        pthread_mutex_unlock(&vpemgr_req_queue_mutex);

    } else {
        DBG("malloc(%ld) failed", sizeof(vpemgr_request_t));
        rv = -1;
        goto exit;
    }

    return rv;

 exit:
    if (vpemgr_q_req) {
        free(vpemgr_q_req);
    }

    return rv;
}


/*
 * vpemgr_req_func() -- Thread handler for VPEMGR requests generated
 *     via submit_vpemgr_request()
 */
void *vpemgr_req_func(void *arg)
{
    UNUSED_ARGUMENT(arg);

    int rv;
    fhandle_t fh;
    vpemgr_request_t *vpemgr_req = 0;
    int retries;
    const char * vpemgr_queuefile = "/tmp/VPEMGR.queue";
    int vpemgr_queue_retries = 2;
    int vpemgr_queue_retry_delay = 1;

    // Naming VPEMGR Thread
    prctl(PR_SET_NAME, "nvsd-vpemgr", 0, 0, 0);

    // Open fqueue
    do {
        fh = open_queue_fqueue(vpemgr_queuefile);
        if (fh < 0) {
            DBG("Unable to open queuefile [%s]", vpemgr_queuefile);
            sleep(2);
        }
    } while (fh < 0);

    // Processing loop
    pthread_mutex_lock(&vpemgr_req_queue_mutex);
    while (1) {
        LIST_FOREACH(vpemgr_req, &vpemgr_req_head, list)
            {
                LIST_REMOVE(vpemgr_req, list);
                break;
            }
	if (vpemgr_req) {
            pthread_mutex_unlock(&vpemgr_req_queue_mutex);

            retries = vpemgr_queue_retries;
            while (retries--) {
                rv = enqueue_fqueue_fh_el(fh, &vpemgr_req->fq_element);
                if (!rv) {
		    glob_vpemgr_tot_reqs++;
                    break;
                } else if (rv == -1) {
                    if (retries) {
                        sleep(vpemgr_queue_retry_delay);
                    }
                } else {
                    glob_vpemgr_err_dropped++;
                    DBG("enqueue_fqueue_fh_el(fd=%d) failed rv=%d, Dropping fqueue_element_t", fh, rv);
                    break;
                }
            }
            free(vpemgr_req);
            pthread_mutex_lock(&vpemgr_req_queue_mutex);
        } else {
            vpemgr_req_thread_wait = 1;
            pthread_cond_wait(&vpemgr_req_queue_cv, &vpemgr_req_queue_mutex);
            vpemgr_req_thread_wait = 0;
        }
    }
    pthread_mutex_unlock(&vpemgr_req_queue_mutex);

    return NULL;
}

/*
 * submit_vpemgr_generic_transcode_request() -- Submit Transcoding(transcode) request to VPE Manager (VPEMGR)
 * this one is triggered in generalSSP
 *
 * transcodeData will provide the reduce bitrate transcode ratio,for example
 * "10%", "20%", or "30%", means the bitrate reduces 10%, 20%, or 30%
 * transComplex is the control of transcodding complexity, it will correspond to different transcoding setting
 * like one pass or two pass
 */
int
submit_vpemgr_generic_transcode_request(const char *hostData, int hostLen,
					    const char *uriData, int uriLen,
                                            const char *containerType, int containerTypeLen,
					    const char *transRatioData, int transRatioLen,
                                            const char *transComplexData, int transComplexLen)
{
    fqueue_element_t *qe;
    vpemgr_request_t *vpemgr_q_req = 0;
    int rv = 0;
    char *mapBuf;

    // Build fqueue_element_t
    vpemgr_q_req = (vpemgr_request_t *)nkn_malloc_type(sizeof(vpemgr_request_t), mod_vpe_vpemgr_queue);

    mapBuf = (char *)alloca(VPE_IDENTIFIER_MAX_LEN);
    memset(mapBuf, 0, VPE_IDENTIFIER_MAX_LEN);

    if (vpemgr_q_req) {
        qe = &vpemgr_q_req->fq_element;

        if (uriData) {
            rv = init_queue_element(qe, 0, "", uriData);
            if (rv) {
		DBG_LOG(WARNING, MOD_VPEMGR,
                        "init_queue_element() failed, rv=%d uri=%s", rv,uriData);
                rv = -1;
                goto exit;
            }

	    // Add Identifier attribute
	    snprintf(mapBuf, VPE_IDENTIFIER_MAX_LEN, "%d", VPE_IDENTIFIER_GENERAL_TRANSCODE);
            rv  = add_attr_queue_element_len(qe, "vpe_identifier", 14, mapBuf, strlen(mapBuf));
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR,
			"add_attr_queue_element(host_name: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }


            // Add additional attributes
            rv = add_attr_queue_element_len(qe, "host_name", 9,  hostData, hostLen);
            if (rv) {
		DBG_LOG(WARNING, MOD_VPEMGR,
			"add_attr_queue_element(host_name: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }

	    // Add additional attributes
            rv = add_attr_queue_element_len(qe, "uri_name", 8,  uriData, uriLen);
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR,
			"add_attr_queue_element(uri_name: vpe_process) failed,  rv=%d", rv);
                rv = -1;
                goto exit;
            }

            // Add additional attributes
            rv = add_attr_queue_element_len(qe, "container_type", 14, containerType, containerTypeLen);
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR,
			"add_attr_queue_element(transcode_ratio_param: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }

	    // Add additional attributes
            rv = add_attr_queue_element_len(qe, "transcode_ratio_param", 21, transRatioData, transRatioLen);
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR,
			"add_attr_queue_element(transcode_ratio_param: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }

            rv = add_attr_queue_element_len(qe, "transcode_complex_param", 23, transComplexData, transComplexLen);
            if (rv) {
		DBG_LOG(WARNING, MOD_VPEMGR,
			"add_attr_queue_element(transcode_complex_param: vpe_process) failed, rv=%d", rv);
		rv = -1;
		goto exit;
            }

        } else {
	    DBG_LOG(WARNING, MOD_VPEMGR, "Null uri=%p", uriData);
            rv = -1;
            goto exit;
        }

	// Enqueue VPEMGR request to push thread
        pthread_mutex_lock(&vpemgr_req_queue_mutex);

        LIST_INSERT_HEAD(&vpemgr_req_head, vpemgr_q_req, list);

        if (vpemgr_req_thread_wait) {
            pthread_cond_signal(&vpemgr_req_queue_cv);
        }

        pthread_mutex_unlock(&vpemgr_req_queue_mutex);

    } else {
        DBG("malloc(%ld) failed", sizeof(vpemgr_request_t));
        rv = -1;
        goto exit;
    }

    return rv;

 exit:
    if (vpemgr_q_req) {
        free(vpemgr_q_req);
    }

    return rv;
}

/*
 * submit_vpemgr_youtube_transcode_request() -- Submit transcode request to VPE Manager (VPEMGR)
 * this one is triggered in youtubeSSP
 *
 * transcodeData will provide the reduce bitrate transcode ratio,for example
 * "10%", "20%", or "30%", means the bitrate reduces 10%, 20%, or 30%
 * transComplex is the control of transcodding complexity,
 * it will correspond to different transcoding setting, like one pass or two pass
 */
int
submit_vpemgr_youtube_transcode_request(const char *hostData, int hostLen,
					const char *video_id_uri_query, int video_id_uri_queryLen,
					const char *format_tag,int format_tagLen,
					const char *remapUriData, int remapUriLen,
					const char *nsuriPrefix, int nsuriPrefixLen,
					const char *transRatioData, int transRatioLen,
					const char *transComplexData, int transComplexLen)
{
    fqueue_element_t *qe;
    vpemgr_request_t *vpemgr_q_req = 0;
    int rv = 0;
    char *mapBuf;

    // Build fqueue_element_t
    vpemgr_q_req = (vpemgr_request_t *)nkn_malloc_type(sizeof(vpemgr_request_t), mod_vpe_vpemgr_queue);

    mapBuf = (char *)alloca(VPE_IDENTIFIER_MAX_LEN);
    memset(mapBuf, 0, VPE_IDENTIFIER_MAX_LEN);

    if (vpemgr_q_req) {
        qe = &vpemgr_q_req->fq_element;

        if (remapUriData) {
            rv = init_queue_element(qe, 0, "", remapUriData);
            if (rv) {
		DBG_LOG(WARNING, MOD_VPEMGR, "init_queue_element() failed, rv=%d uri=%s", rv, remapUriData);
                rv = -1;
                goto exit;
            }

	    // Add Identifier attribute
	    snprintf(mapBuf, VPE_IDENTIFIER_MAX_LEN, "%d", VPE_IDENTIFIER_YOUTUBE_TRANSCODE);
            rv = add_attr_queue_element_len(qe, "vpe_identifier", 14, mapBuf, strlen(mapBuf));
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR, 
			"add_attr_queue_element(host_name: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }

            // Add additional attributes
            rv = add_attr_queue_element_len(qe, "host_name", 9, hostData, hostLen);
            if (rv) {
		DBG_LOG(WARNING, MOD_VPEMGR, 
			"add_attr_queue_element(host_name: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }

	    // Add additional attributes
            rv = add_attr_queue_element_len(qe, "video_id_uri_query", 18, video_id_uri_query, video_id_uri_queryLen);
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR, 
			"add_attr_queue_element(uri_name: vpe_process) failed,  rv=%d", rv);
                rv = -1;
                goto exit;
            }

	    // Add additional attributes
            rv = add_attr_queue_element_len(qe, "format_tag", 10, format_tag, format_tagLen);
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR, 
			"add_attr_queue_element(seek_name: vpe_process) failed,  rv=%d", rv);
                rv = -1;
                goto exit;
            }

	    // Add additional attributes
            rv = add_attr_queue_element_len(qe, "internal_cache_name", 19, remapUriData, remapUriLen);
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR, 
			"add_attr_queue_element(internal_cache_name: vpe_process) failed,  rv=%d", rv);
                rv = -1;
                goto exit;
            }

	    // Add additional attributes
            rv = add_attr_queue_element_len(qe, "ns_uri_prefix", 13, nsuriPrefix, nsuriPrefixLen);
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR, 
			"add_attr_queue_element(ns_uri_prefix: vpe_process) failed,  rv=%d", rv);
                rv = -1;
                goto exit;
            }

	    // Add additional attributes
            rv = add_attr_queue_element_len(qe, "transcode_ratio_param", 21, transRatioData, transRatioLen);
            if (rv) {
                DBG_LOG(WARNING, MOD_VPEMGR, 
			"add_attr_queue_element(transcode_ratio_param: vpe_process) failed, rv=%d", rv);
                rv = -1;
                goto exit;
            }

            rv = add_attr_queue_element_len(qe, "transcode_complex_param", 23, transComplexData, transComplexLen);
            if (rv) {
		DBG_LOG(WARNING, MOD_VPEMGR, 
			"add_attr_queue_element(transcode_complex_param: vpe_process) failed, rv=%d", rv);
		rv = -1;
		goto exit;
            }

        } else {
	    DBG_LOG(WARNING, MOD_VPEMGR, "Null uri=%p", remapUriData);
            rv = -1;
            goto exit;
        }

	// Enqueue VPEMGR request to push thread
        pthread_mutex_lock(&vpemgr_req_queue_mutex);

        LIST_INSERT_HEAD(&vpemgr_req_head, vpemgr_q_req, list);

        if (vpemgr_req_thread_wait) {
            pthread_cond_signal(&vpemgr_req_queue_cv);
        }

        pthread_mutex_unlock(&vpemgr_req_queue_mutex);

    } else {
        DBG("malloc(%ld) failed", sizeof(vpemgr_request_t));
        rv = -1;
        goto exit;
    }

    return rv;

 exit:
    if (vpemgr_q_req) {
        free(vpemgr_q_req);
    }

    return rv;
}

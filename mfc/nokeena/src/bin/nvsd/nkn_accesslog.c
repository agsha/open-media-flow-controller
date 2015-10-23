#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "nkn_am_api.h"
#include "server.h"
#include "network.h"
#include "nkn_http.h"
#include "nkn_debug.h"
#include "nknlog_pub.h"
//#include "../nknlogd/nknlogd.h"
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkn_accesslog.h"
#include "accesslog_pub.h"
#include "nkn_hash.h"

NKNCNT_DEF(al_tot_logged, uint64_t, "", "Num of transaction is logged")
NKNCNT_DEF(al_err_buf_overflow, uint64_t, "", "Num of transaction is not logged")

#define MAX_ACCESSLOG_PROFILE	(32)

al_info_t al_pm[MAX_ACCESSLOG_PROFILE];

#define ALI_INC_LOGGED()
#define ALI_INC_BUFF_OVERFLOW()
#define ALI_INC_DROPPED()


int al_remove_from_table(const char *name);

NHashTable *al_profile_tbl = NULL;
pthread_rwlock_t alp_tbl_lock;

uint32_t accesslog_ip = DEF_LOG_IP;
uint16_t accesslog_port = LOGD_PORT;
uint32_t accesslog_remote_if_ip = DEF_LOG_IP;

struct al_info {
	LIST_ENTRY(al_info) entries;
	int channelid;
	int num_al_list;
	char * al_list[32];
};
LIST_HEAD(al_info_queue, al_info);
//static struct al_info_queue g_al_info_head = { NULL };
//static int 	accfd = -1;
//static char   	al_buf[MAX_ACCESSLOG_BUFSIZE];
//static int32_t 	al_buf_start = 0;
//static int32_t 	al_buf_end = 0;
//static pthread_mutex_t al_mutex = PTHREAD_MUTEX_INITIALIZER;

//int al_init_socket(void);
void http_accesslog_write(con_t * con) ;
//static void al_info_freeall(void);

/*----------------------------------------------------------------------------
 * NEW APIs: Takes an additional context (profile) data
 *--------------------------------------------------------------------------*/
al_info_t *al_info_alloc(char *name);
static int al_send_data(al_info_t *ali);
static int al_handshake_accesslog(al_info_t *ali);
static void al_info_freeall(al_info_t *ali);
static int al_info_add(al_info_t *ali, NLP_accesslog_channel *plog, char *p);
static int __al_init_socket(char *name, al_info_t **pali);
static void al_info_init(char *name, al_info_t *ali);
static int __ali_free_on_err_ali(al_info_t *ali, int locked);

#if 0
static inline void __acquire_rd_lock(al_info_t *ali) {
    pthread_rwlock_rdlock(&ali->rd_lock);
}

static inline void __release_rd_lock(al_info_t *ali) {
    pthread_mutex_unlock(&ali->rd_lock);
}

static inline void __acquire_wr_lock(al_info_t *ali) {
    pthread_mutex_lock(&ali->wr_lock);
}

static inline void __release_wr_lock(al_info_t *ali) {
    pthread_mutex_unlock(&ali->wr_lock);
}
#endif

extern const char *nvsd_mgmt_acclog_profile_get(uint32_t idx);
extern uint32_t nvsd_mgmt_acclog_profile_length(void);

static int al_send_data(al_info_t *ali)
{
    int ret = 0;
    char *p = NULL;
    int len = 0;


    p = &ali->al_buf[ali->al_buf_start];
    len = ali->al_buf_end - ali->al_buf_start;

    while(len) {
	ret = send(ali->al_fd, p, len, 0);
	if (ret == -1) {
	    if (errno == EAGAIN) {
		NM_add_event_epollout(ali->al_fd);
		return 1;
	    }
	    return -1;
	} else if (ret == 0) {
	    /* socket is full. Try again later */
	    NM_add_event_epollout(ali->al_fd);
	    return 1;
	}

	p += ret;
	len -= ret;
	ali->al_buf_start += ret;
    }

    ali->al_buf_start = 0;
    ali->al_buf_end = 0;

    NM_add_event_epollin(ali->al_fd);

    return 1;
}

/***************************************************************************
 * handling socket in our network infrastructure
 ***************************************************************************/
#if 0
static int al_send_data(void);
static int al_send_data(void)
{
        int ret;
	char * p;
	int len;

	//printf("al_send_data: called\n");
	p = &al_buf[al_buf_start];
	len = al_buf_end - al_buf_start;
	while(len) {
	        ret = send(accfd, p, len, 0);
		if (ret == -1) {
			if (errno == EAGAIN) {
        			NM_add_event_epollout(accfd);
				return 1;
			}
			return -1;
		}
		else if (ret == 0) {
			/* socket is full. Try again later. */
        		NM_add_event_epollout(accfd);
			return 1;
		}

		p += ret;
		len -= ret;
		al_buf_start += ret;
	}
	al_buf_start = 0;
	al_buf_end = 0;

        NM_add_event_epollin(accfd);
        return 1;
}
#endif


static int __ali_free_on_err_ali(al_info_t *ali, int locked)
{
    if (locked == 0) {
	ALI_HASH_WLOCK();
    }
    NM_close_socket(ali->al_fd);

    /* PR 783847: remove from table before freeing ali context,
     * otherwise the keys are freed and we segault on an invalid
     * access
     */
    ALP_HASH_WLOCK();
    nhash_remove(al_profile_tbl, ali->keyhash, ali->key);
    ALP_HASH_WUNLOCK();
    al_info_freeall(ali);
    ALI_HASH_WUNLOCK();

    return TRUE;
}

static int al_epollin(int sockfd, void * private_data)
{
    char buf[256];
    al_info_t *ali = (al_info_t *)(private_data);;

    UNUSED_ARGUMENT(sockfd);

    // We should never receive any data from accesslog agent.
    // If we receive something, most likely, accesslog agent has closed the connection.
    ALI_HASH_WLOCK();
    if (ali->in_use && recv(ali->al_fd, buf, 256, 0)<=0) {
	return __ali_free_on_err_ali(ali, 1);
    }

    // Otherwise, we through away the data.
    ALI_HASH_WUNLOCK();
    return TRUE;
}


static int al_epollout(int sockfd, void * private_data)
{
    al_info_t *ali = (al_info_t *)(private_data);;
    UNUSED_ARGUMENT(sockfd);

    ALI_HASH_WLOCK();

    if (ali->in_use && al_send_data(ali) == -1) {
	return __ali_free_on_err_ali(ali, 1);
    }

    ALI_HASH_WUNLOCK();
    return TRUE;
}

static int al_epollerr(int sockfd, void * private_data)
{
    al_info_t *ali = (al_info_t *)(private_data);;

    UNUSED_ARGUMENT(sockfd);

    return __ali_free_on_err_ali(ali, 0);
}

static int al_epollhup(int sockfd, void * private_data)
{
        // Same as al_epollerr
        return al_epollerr(sockfd, private_data);
}


/***************************************************************************
 * Make the accesslog socket
 ***************************************************************************/
static char * get_parseCookie(char * cookiename, const char * cookieline, int cookieline_len);
static char * get_parseCookie(char * cookiename, const char * cookieline, int cookieline_len)
{
    static char buf[128];
    int len;
    char * p, * pend;

    if (cookieline_len > 127) cookieline_len = 127;
    memcpy(buf, cookieline, cookieline_len);
    buf[cookieline_len] = 0;
    len = strlen(cookiename);
    // Find the exact match
    if (strncasecmp(buf, cookiename, len)) return NULL;
    p = &buf[len];
    while (*p != 0 && *p != '=') {
	if (*p != ' ' && *p != '\t') return NULL;
	p++;
    }
    if (*p == 0) return NULL;
    p++;
    while (*p != 0) {
	// Skip the leading spaces
	while (*p ==  ' ' || *p == '\t') p++;
	pend = p;
	while (*pend != 0) {
	    if (*pend == ';') break;
	    pend++;
	}
	*pend = 0;
	return p;
    }

    return NULL;
}

/*
 * This function is called by HTTP module.
 */
void http_accesslog_write(con_t * con) 
{
	al_info_t *pinfo = NULL;
	NLP_data *pdata = NULL;
	struct accesslog_item *item = NULL;
	char *p = NULL;
	char *ps = NULL;
	char *pname = NULL;
	const char *pvalue = NULL;
	int32_t pvalue_len = 0;
	int32_t list_len = 0;
	int32_t buf_len = 0;
	int32_t i = 0;
	u_int32_t attr = 0;
	int hdrcnt = 0;
	int token = 0;
	char buf[512];
	al_info_t *ali = NULL;
	int rv = 0 ;
	int nth = 0 ;


	if (!con || !is_known_header_present(&con->http.hdr, MIME_HDR_X_NKN_URI)) {
	    return;
	}

	if (CHECK_HTTP_FLAG(&con->http, HRF_MFC_PROBE_REQ)) {
	    return;
	}

	if (con->http.nsconf && con->http.nsconf->acclog_config->name) {
	    // Lookup AccLog Info
	    ALP_HASH_RLOCK();
	    ali = nhash_lookup(al_profile_tbl,
		    con->http.nsconf->acclog_config->keyhash,
		    con->http.nsconf->acclog_config->key);
	    ALP_HASH_RUNLOCK();

	    if (ali == NULL)
		return;
	} else {

	    /* this is expected to be rare case.. when no namespace match is seen. */
	    /* TODO:
	     *  We should just pre-create these on init, instead of having to
	     *  create these for every request.
	     */
	    uint64_t keyhash = n_str_hash("uds-acclog-default");
	    char *def = alloca(32);
	    snprintf(def, 22, "uds-acclog-default"); // safe = 22 < 32!
	    ALP_HASH_RLOCK();
	    ali = nhash_lookup(al_profile_tbl, keyhash, def);
	    ALP_HASH_RUNLOCK();
	    if (!ali) {
	    	/* Tried my best to get a profile.. dont have one, bail */
	    	return;
	    }
	}


	glob_al_tot_logged++;

	ALI_HASH_WLOCK();
	if (ali->in_use == 0) {
	    ALI_HASH_WUNLOCK();
	    return;
	}

	buf_len = MAX_ACCESSLOG_BUFSIZE - ali->al_buf_end;

	if (buf_len < (int) (sizeof(NLP_data) + sizeof(struct accesslog_item))) {
	    glob_al_err_buf_overflow++;
	    goto skip_logit;
	}

	pinfo = ali;
	pdata = (NLP_data *) &(ali->al_buf[ali->al_buf_end]);
	ps = (char *)(pdata) + sizeof(NLP_data);
	pdata->channelid = pinfo->channelid;
	buf_len -= (sizeof(NLP_data) + sizeof(struct accesslog_item));
	item = (struct accesslog_item *) ps;


	item->start_ts.tv_sec =	    con->http.start_ts.tv_sec;
	item->start_ts.tv_usec =    con->http.start_ts.tv_usec;
	item->end_ts.tv_sec =	    con->http.end_ts.tv_sec;
	item->end_ts.tv_usec =	    con->http.end_ts.tv_usec;
	item->ttfb_ts.tv_sec =	    con->http.ttfb_ts.tv_sec;
	item->ttfb_ts.tv_usec =	    con->http.ttfb_ts.tv_usec;
	item->in_bytes =	    con->http.cb_offsetlen;
	item->out_bytes =	    con->sent_len;
	item->resp_hdr_size =	    con->http.res_hdlen;
	item->status =		    con->http.respcode;
	item->subcode =		    con->http.subcode;
	item->dst_port =	    con->http.remote_port;
	memcpy(&(item->dst_ipv4v6), &(con->http.remote_ipv4v6), sizeof(ip_addr_t));
	memcpy(&(item->src_ipv4v6), &(con->src_ipv4v6), sizeof(ip_addr_t));
	item->flag =		    0;

	if( CHECK_HTTP_FLAG(&con->http, HRF_METHOD_GET) ) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_GET);
	} else if ( CHECK_HTTP_FLAG(&con->http, HRF_METHOD_HEAD) ) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_HEAD);
	} else if ( CHECK_HTTP_FLAG(&con->http, HRF_METHOD_POST) ) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_POST);
	} else if ( CHECK_HTTP_FLAG(&con->http, HRF_METHOD_CONNECT) ) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_CONNECT);
	} else if ( CHECK_HTTP_FLAG(&con->http, HRF_METHOD_OPTIONS) ) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_OPTIONS);
	} else if ( CHECK_HTTP_FLAG(&con->http, HRF_METHOD_PUT) ) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_PUT);
	} else if ( CHECK_HTTP_FLAG(&con->http, HRF_METHOD_DELETE) ) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_DELETE);
	} else if ( CHECK_HTTP_FLAG(&con->http, HRF_METHOD_TRACE) ) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_TRACE);
	}

	if ( CHECK_HTTP_FLAG(&con->http, HRF_RETURN_REVAL_CACHE) ) {
	    SET_ALITEM_FLAG(item, ALF_RETURN_REVAL_CACHE);
	}

	if( CHECK_HTTP_FLAG(&con->http, HRF_HTTP_10) ) {
	    SET_ALITEM_FLAG(item, ALF_HTTP_10);
	} else if (CHECK_HTTP_FLAG(&con->http, HRF_HTTP_11)) {
	    SET_ALITEM_FLAG(item, ALF_HTTP_11);
	} else {
	    SET_ALITEM_FLAG(item, ALF_HTTP_09);
	}

	if( CHECK_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE) ) {
	    SET_ALITEM_FLAG(item, ALF_CONNECTION_KEEP_ALIVE);
	} else {
	    SET_ALITEM_FLAG(item, ALF_CONNECTION_CLOSE);
	}

	if( CHECK_HTTP_FLAG(&con->http, HRF_HTTPS_REQ) ) {
	    SET_ALITEM_FLAG(item, ALF_SECURED_CONNECTION);
	}

        if (con->http.attr && (con->http.attr->na_flags & NKN_OBJ_COMPRESSED) &&
		    (con->http.attr->encoding_type == HRF_ENCODING_GZIP)) {
	    SET_ALITEM_FLAG(item, ALF_RETURN_OBJ_GZIP);
	} else if(con->http.attr && (con->http.attr->na_flags & NKN_OBJ_COMPRESSED) &&
		    (con->http.attr->encoding_type == HRF_ENCODING_DEFLATE)) {	
	    SET_ALITEM_FLAG(item, ALF_RETURN_OBJ_DEFLATE);
	}

	if (CHECK_HTTP_FLAG2(&con->http, HRF2_EXPIRED_OBJ_ENT)) {
	    SET_ALITEM_FLAG(item, ALF_EXP_OBJ_DEL);
	    CLEAR_HTTP_FLAG2(&con->http, HRF2_EXPIRED_OBJ_ENT); 
	}

	p = ps + sizeof(struct accesslog_item);
	for (i=0; i < pinfo->num_al_list; i++) {
	    pvalue = NULL;
	    list_len = pinfo->al_listlen[i];

	    switch (*(unsigned int *)(&(pinfo->al_list[i][0]))) {
	    case 0x6972752e:
		if( CHECK_HTTP_FLAG(&con->http, HRF_METHOD_CONNECT)) {
		    /* It is CONNECT method */
		    pvalue = NULL;
		    pvalue_len = 0;
		} else {
		    /* It is NOT CONNECT method */
		    get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI,
			    &pvalue, &pvalue_len, &attr, &hdrcnt);
		}
		break;
	    case 0x6c71722e:    /* ".rql" */
		get_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_LINE,
			&pvalue, &pvalue_len, &attr, &hdrcnt);
		break;
	    case 0x746f682e:	/* ".hot" */
	    {
		char hotval[64];  // %d_%d won't cross 64 bytes
		int32_t hval, htime;
		if (con->http.object_hotval != 0xaaaaaaaa) {
		    hval = am_decode_hotness(&(con->http.object_hotval));
		    htime = am_decode_update_time(&(con->http.object_hotval));
		    pvalue_len = snprintf(hotval, 64, "%u_%d", htime, hval);
		    pvalue = hotval;
		}
		break;
	    }
	    //provide namespace info, only name at this point
	    case 0x70736e2e:	/* ".nsp" */
	    {
		if(con->http.nsconf != NULL) {
		    pvalue = con->http.nsconf->namespace;
		    pvalue_len = con->http.nsconf->namespace_strlen;
		}
		break;
	    }
	    //find MFC name or servername
	    case 0x6d6e732e:	/* ".snm" */
	    {
		char server[256];
		int ret = gethostname(server, sizeof(server));
		if(ret != -1){
		    pvalue_len = strlen(server);
		    pvalue = server;
		}
		break;
	    }
	    //find filename
	    case 0x6d6e662e:	/* ".fnm" */
	    {
		pvalue = NULL;
		pvalue_len = 0;
		if( !CHECK_HTTP_FLAG(&con->http, HRF_METHOD_CONNECT)) {
		    const char *temp;
		    int temp_len, j;
		    char c;
		    get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI,
			    &temp, &temp_len, &attr, &hdrcnt);
		    if (temp_len > 1) {
			for(j = temp_len - 1; j >= 0; j--) {
			    c = temp[j];
			    if(c == '/')
				break;
			}
			pvalue_len = temp_len - (j+1); //Don't print /
			pvalue = temp + j + 1 ;
		    }
		}
		break;
	    }
	    /* Record provide name */
	    case 0x7469682e:	/* ".hit" */
	    {
		switch (con->http.first_provider) {
		case OriginMgr_provider:
		    pvalue = "Origin"; pvalue_len = 6; break;
		case SolidStateMgr_provider:
		    pvalue = "SSD"; pvalue_len = 3; break;
		case BufferCache_provider:
		    pvalue = "Buffer"; pvalue_len = 6; break;
		case SAS2DiskMgr_provider:
		    pvalue = "SAS"; pvalue_len = 3; break;
		case SATADiskMgr_provider:
		    pvalue = "SATA"; pvalue_len = 4; break;
		case NFSMgr_provider:
		    pvalue = "NFS"; pvalue_len = 3; break;
		case Tunnel_provider: 
		    pvalue = buf;
		    pvalue_len = snprintf(buf, sizeof(buf),
				"Tunnel_%d", con->http.tunnel_reason_code);
		    break;
		default:
		    pvalue="internal"; pvalue_len=8; break;
		}
		break;
	    }
	    case 0x67726f2e:	/* ".org" */
		pvalue = buf;
		pvalue_len = snprintf(buf, sizeof(buf),
			"%ld", con->http.size_from_origin);
		break;
	    case 0x7468632e:	/* ".cht" */
		if (con->http.hit_history) {
		    pvalue = con->http.hit_history;
		    pvalue_len = con->http.hit_history_used;
		}
		break;
	    /* 2. search HTTP request Cookie header. */
	    case 0x6b6f632e:	/* ".cok" */
		pname = &pinfo->al_list[i][4];
		/* it is cookie string, use .cok for performance reason */
		get_known_header(&con->http.hdr, MIME_HDR_COOKIE,
			&pvalue, &pvalue_len, &attr, &hdrcnt);
	  	for(nth = 0; nth < hdrcnt; nth++) {
		    rv = get_nth_known_header(&con->http.hdr, MIME_HDR_COOKIE, nth, &pvalue, &pvalue_len, &attr);
		    if (!rv && pvalue) {
		        pvalue = get_parseCookie(pname, pvalue, pvalue_len);
			    if(pvalue) {
			        pvalue_len = strlen(pvalue);
				goto logit;
			    }
		    }
		}
		break;
           /* Include Origin Server names details */
           case 0x6e736f2e:    /* ".osn" */
               {
               char ips[4*INET6_ADDRSTRLEN];
               char ip[INET6_ADDRSTRLEN]; // INET_ADDRSTRLEN > INET6_ADDRSTRLEN , using INET6_ADDRSTRLEN
	    
               pname = &pinfo->al_list[i][4];
               pvalue = NULL;
               pvalue_len = 0;
	       ips[0] = 0;

               for(nth = 0 ; nth < con->origin_servers.count; nth++) {
                       if(con->origin_servers.ipv4v6[nth].family == AF_INET) {
                               if(inet_ntop(AF_INET, &IPv4(con->origin_servers.ipv4v6[nth]), 
							&ip[0], INET6_ADDRSTRLEN) == NULL) 
					break;
		
                       } else if (con->origin_servers.ipv4v6[nth].family == AF_INET6) {
                               if(inet_ntop(AF_INET6, &IPv6(con->origin_servers.ipv4v6[nth]), 
							&ip[0], INET6_ADDRSTRLEN) == NULL) 
                               break;
                       } else {
                               break;
                       }

                       strcat(ips, ip);
                       strcat(ips, ",");
               }

               if(con->origin_servers.count) {
                       pvalue = ips;
                       pvalue_len = strlen(pvalue);

                       goto logit;
               } else if(con->http.first_provider == OriginMgr_provider){
			mime_header_t response_hdr;
			uint32_t attrs;
			const char *host;
			int hostlen;
			int ret = 0;
                        if(con->http.attr) {
                        init_http_header(&response_hdr, 0, 0);
                        ret = nkn_attr2http_header(con->http.attr, 0, &response_hdr);
                        if (!ret) {
                          if(is_known_header_present(&response_hdr, MIME_HDR_X_NKN_ORIGIN_IP)) {
                              ret = get_known_header(&response_hdr,
                                            MIME_HDR_X_NKN_ORIGIN_IP,
                                            &host, &hostlen,
                                            &attrs, &hdrcnt);
                              if(!ret) {
                                 pvalue = host;
				 pvalue_len = strlen(pvalue);
				 shutdown_http_header(&response_hdr);
				 goto logit;
		              }

                          }

                          shutdown_http_header(&response_hdr);
                        }

                        }

		}

               break;
               }

	    case 0x7165722e:	/* ".req" */
		pname = &pinfo->al_list[i][4];
		list_len -= 4;
		token = pinfo->al_token[i];
		if (token >= 0) {
		    /*
		    * some tokens have been deleted from get_known_header.
		    * But they are still stored in namevalue_header.
		    */
		    switch (token) {
		    case MIME_HDR_IF_MODIFIED_SINCE:
		    case MIME_HDR_IF_NONE_MATCH:
			break;
		    default:
			if (get_known_header(&con->http.hdr, token,
			    &pvalue, &pvalue_len, &attr, &hdrcnt) == 0) {
			    goto logit;
			}
			break;
		    }
		}
		/* 2. search HTTP request unknown header. */
		if (get_unknown_header(&con->http.hdr,
			pname, list_len,
			&pvalue, &pvalue_len, &attr) == 0) {
		    goto logit;
		}
		/* 3. search HTTP request header. */
		if (get_namevalue_header(&con->http.hdr,
			pname,
			list_len,
			&pvalue, &pvalue_len, &attr) == 0) {
		    goto logit;
		}
		break;
	    /* 5. Now it is time to search from response known header. */
	    case 0x7365722e:	/* ".res" */
		pname = &pinfo->al_list[i][4];
		list_len -= 4;

		if (con->http.p_resp_hdr == NULL) {
		    /* impossible. we should assert here */
		    goto logit;
		}

		token = pinfo->al_token[i];
		if ((token >= 0) &&
		    (get_known_header(con->http.p_resp_hdr, token,
			&pvalue, &pvalue_len, &attr, &hdrcnt) == 0)) {
		    goto logit;
		}

		/* 2. search HTTP request unknown header. */
		if (get_unknown_header(con->http.p_resp_hdr,
			pname, list_len,
			&pvalue, &pvalue_len, &attr) == 0) {
		    goto logit;
		}

		/* 3. search other HTTP response header. */
		if (get_namevalue_header(con->http.p_resp_hdr,
			pname, list_len,
			&pvalue, &pvalue_len, &attr) == 0) {
		    goto logit;
		}
		break;
	    }
	    /*
	    * if yes we found pvalue, copy it.
	    * if not, we return "-"
	    */
logit:
	    if (pvalue) {
		if(buf_len < pvalue_len+1) {
		    glob_al_err_buf_overflow++;
		    goto skip_logit;
		}
		buf_len -= pvalue_len+1;
		memcpy(p, pvalue, pvalue_len);
		p[pvalue_len] = '\0';
		p += pvalue_len + 1;
	    } else {
		if(buf_len < 2) {
		    glob_al_err_buf_overflow++;
		    goto skip_logit;
		}
		buf_len -= 2;
		*p++ = '-';
		*p++ = 0;
	    }
	}

	pdata->len = p - ps;
	ali->al_buf_end += (pdata->len + sizeof(NLP_data));

skip_logit:
	if (ali->al_buf_end > (MAX_ACCESSLOG_BUFSIZE / 4)) {
	    NM_add_event_epollout(ali->al_fd);
	}

	ALI_HASH_WUNLOCK();
	return;
}


/***************************************************************************
 * Make the accesslog socket
 ***************************************************************************/

static int al_info_add(al_info_t *ali, NLP_accesslog_channel *plog, char *p)
{
    int i = 0, list_len, token;
    char *pname = NULL;

    //printf("------- adding (%s: %s) ----\n", __FUNCTION__, p);
    if (plog->num_of_hdr_fields >= 32) {
	return FALSE;
    }

    ali->channelid = plog->channelid;
    ali->num_al_list = plog->num_of_hdr_fields;
    ali->al_resp_header_configured = 0;
    ali->al_record_cache_history = 0;

    for (i = 0; i < plog->num_of_hdr_fields; i++) {

	ali->al_token[i] = token = -1;
	ali->al_listlen[i] = 0;
	ali->al_list[i] = nkn_strdup_type(p, mod_log_al_info);

	if(ali->al_list[i] == NULL){
	    return FALSE;
	}
	ali->al_listlen[i] = strlen(ali->al_list[i]);

	p += (strlen(p) + 1);

	switch (*(int *)(&ali->al_list[i][0]))
	{
        case 0x7165722e:    /* ".req" */
	    pname = &ali->al_list[i][4];
	    list_len = ali->al_listlen[i] - 4;
	    string_to_known_header_enum(pname, list_len, &token);
	    ali->al_token[i] = token;
	    break;
	case 0x7365722e: /* ".res" */
	    pname = &ali->al_list[i][4];
	    list_len = ali->al_listlen[i] - 4;
	    string_to_known_header_enum(pname, list_len, &token);
	    ali->al_token[i] = token;
	    ali->al_resp_header_configured = 1;
	    break;
	case 0x67726f2e: /* ".org" */
	case 0x7468632e: /* ".cht" */
	    ali->al_record_cache_history = 1;
	    break;
	}
    }

    return TRUE;
}

#if 0
static int al_info_add(NLP_accesslog_channel * plog, char * p);
static int al_info_add(NLP_accesslog_channel * plog, char * p)
{
	struct al_info * pinfo;
	int i;

	if(plog->num_of_hdr_fields >= 32) {
		return FALSE;
	}

	// Save all the information as socket
	pinfo = (struct al_info *)nkn_malloc_type(sizeof(struct al_info),
						  mod_log_al_info);
	if(!pinfo) {
		return FALSE;
	}
	pinfo->channelid = plog->channelid;
	pinfo->num_al_list = plog->num_of_hdr_fields;
	//printf("al_info_add: plog->num_of_hdr_fields=%d\n", plog->num_of_hdr_fields);
	al_resp_header_configured = 0;
	al_record_cache_history = 0;
	for(i=0; i<(int)plog->num_of_hdr_fields; i++) {
		pinfo->al_list[i] = nkn_strdup_type(p, mod_log_al_info);
		p+=strlen(p)+1;
		//printf("al_info_add: i=%d string=%s\n", i, pinfo->al_list[i]);
		switch (*(int *)&pinfo->al_list[i][0]) {
		case 0x7365722e : /* ".res" */
			al_resp_header_configured = 1;
			break;
		case 0x67726f2e : /* ".org" */
			al_record_cache_history = 1;
			break;
		case 0x7468632e : /* ".cht" */
			al_record_cache_history = 1;
			break;
		}
	}

	pthread_mutex_lock(&al_mutex);
	LIST_INSERT_HEAD(&g_al_info_head, pinfo, entries);
	pthread_mutex_unlock(&al_mutex);

	return TRUE;
}
#endif

/* caller should get the al_mutex locker */
static void al_info_freeall(al_info_t *ali)
{
    int i;
    //char *ali_name = alloca(256);
    /*
     * TODO: 
     *	a. claenup routines go here.
     */
    if (ali->in_use == 0) return;
    /* Unregister stats */
#if 0
    snprintf(ali_name, 255, "acclog.%s", ali->name);
    nkn_mon_delete("tot_logged", ali_name);
    nkn_mon_delete("err_buf_overflow", ali_name);
    nkn_mon_delete("tot_dropped", ali_name);
#endif
    for (i = 0; i < ali->num_al_list; i++) {
	if (ali->al_list[i])
	    free(ali->al_list[i]);
    }

    free(ali->name);
    free(ali->key);
    free(ali->al_buf);
    ali->in_use = 0;
    //free(ali);

    return;
}

#if 0
static void al_info_freeall(void)
{
	int i;
	struct al_info * pinfo;

	while( !LIST_EMPTY(&g_al_info_head) ) {
		pinfo = g_al_info_head.lh_first;
		LIST_REMOVE(pinfo, entries);
		for(i=0; i<pinfo->num_al_list; i++) {
			free(pinfo->al_list[i]);
		}
		free(pinfo);
	}
	al_buf_start = 0;
	al_buf_end = 0;
	return ;
}
#endif


static int al_handshake_accesslog(al_info_t *ali)
{
    NLP_client_hello *req = NULL;
    NLP_server_hello *resp = NULL;

    int ret = 0, len = 0, i = 0;
    NLP_accesslog_channel *plog;
    char *p = NULL;

    /* Send request to server */
    req = (NLP_client_hello *)(ali->al_buf);
    strcpy(req->sig, SIG_SEND_ACCESSLOG);
    if (send(ali->al_fd, ali->al_buf, sizeof(NLP_client_hello), 0) <= 0) {
	return errno;
    }

    /* Read response from server */
    ret = recv(ali->al_fd, ali->al_buf, sizeof(NLP_server_hello), 0);
    if (ret < (int) sizeof(NLP_server_hello)) {
	return errno;
    }

    resp = (NLP_server_hello *) ali->al_buf;
    len = resp->lenofdata;

    if (len != 0) {
	ret = recv(ali->al_fd, &ali->al_buf[sizeof(NLP_server_hello)], len, 0);
	if (ret < len) {
	    return errno;
	}

	p = &ali->al_buf[sizeof(NLP_server_hello)];
	for (i = 0; i < resp->numoflog; i++) {
	    plog = (NLP_accesslog_channel *) p;

	    p += sizeof(NLP_accesslog_channel);

	    if (al_info_add(ali, plog, p) == FALSE) {
		return -1;
	    }

	    p += plog->len;
	}
    }

    NM_set_socket_nonblock(ali->al_fd);

    if (register_NM_socket(ali->al_fd, 
		(void *) ali, 
		al_epollin,
		al_epollout,
		al_epollerr,
		al_epollhup,
		NULL, 
		NULL,
		0,
		USE_LICENSE_FALSE,
		FALSE) == FALSE) {

	return -1;
    }

    NM_add_event_epollin(ali->al_fd);

    return 0;
}


#if 0
static int al_handshake_accesslog(void);
static int al_handshake_accesslog(void)
{
        NLP_client_hello * req;
        NLP_server_hello * res;
        int ret, len, i;
	NLP_accesslog_channel * plog;
	char * p;

        /*
         * Send request to nvsd server.
         */
        req = (NLP_client_hello *)al_buf;
        strcpy(req->sig, SIG_SEND_ACCESSLOG);
        if(send(accfd, al_buf, sizeof(NLP_client_hello), 0) <=0 ) {
                return -1;
        }
        //printf("Sends out errorlog request to nvsd ...\n");

        /*
         * Read response from nvsd server.
         */
        ret = recv(accfd, al_buf, sizeof(NLP_server_hello), 0);
        if(ret<(int)sizeof(NLP_server_hello)) {
                perror("Socket recv.");
                return -1;
        }
        res = (NLP_server_hello *)al_buf;

	len = res->lenofdata;

	if (len != 0)
	{
        	ret = recv(accfd, &al_buf[sizeof(NLP_server_hello)], len, 0);
        	if(ret<len) {
                	perror("Socket recv.");
                	return -1;
        	}
        //printf("len=%d ret=%d\n", len, ret);
		p = &al_buf[sizeof(NLP_server_hello)];
		for(i=0; i<res->numoflog; i++) {
			plog = (NLP_accesslog_channel *)p;
			//printf("level=%d\n", plog->len);
			//printf("channelid=%d\n", plog->channelid);

			p += sizeof(NLP_accesslog_channel);
			if(al_info_add(plog, p) == FALSE) {
				return -1;
			}
			p += plog->len;
		}
	}
        //printf("Debuglog socket is established.\n");

	NM_set_socket_nonblock(accfd);

        // We need to unlock this socket because when the locker
        if(register_NM_socket(accfd,
			NULL,
                        al_epollin,
                        al_epollout,
                        al_epollerr,
                        al_epollhup,
			NULL,
                        NULL,
                        0,
                        USE_LICENSE_FALSE,
			FALSE) == FALSE)
        {
		return -1;
        }
        NM_add_event_epollin(accfd);

        return 1;
}
#endif


al_info_t *al_info_alloc(char *name)
{
    //char *ali_name = alloca(256);
    al_info_t *ali = nkn_calloc_type(1, sizeof(al_info_t), mod_log_al_info);

    if (ali == NULL)
	return NULL;
    
    ali->name = nkn_strdup_type(name, mod_log_al_info);
    ali->key = nkn_strdup_type(name, mod_log_al_info);

    pthread_mutex_init(&(ali->lock), NULL);
    ali->al_fd = -1;
    ali->channelid = -1;
    ali->num_al_list = -1;
    ali->al_resp_header_configured = 0;
    ali->al_record_cache_history = 0;
    ali->al_buf_start = 0;
    ali->al_buf_end = 0;

    /* Register stats */
#if 0
    snprintf(ali_name, 255, "acclog.%s", name);
    nkn_mon_add("tot_logged", ali_name, &(ali->stats.tot_logged),
	    sizeof(ali->stats.tot_logged));
    nkn_mon_add("err_buf_overflow", ali_name, &(ali->stats.err_buf_overflow),
	    sizeof(ali->stats.err_buf_overflow));
    nkn_mon_add("tot_dropped", ali_name, &(ali->stats.tot_dropped),
	    sizeof(ali->stats.tot_dropped));
#endif
    return ali;
}

static void al_info_init(char *name, al_info_t *ali)
{
    //char *ali_name = alloca(256);
    ali->name = nkn_strdup_type(name, mod_log_al_info);
    ali->key = nkn_strdup_type(name, mod_log_al_info);
    ali->al_buf = nkn_calloc_type(1, MAX_ACCESSLOG_BUFSIZE, mod_log_al_info);

    ali->al_fd = -1;
    ali->channelid = -1;
    ali->num_al_list = -1;
    ali->al_resp_header_configured = 0;
    ali->al_record_cache_history = 0;
    ali->al_buf_start = 0;
    ali->al_buf_end = 0;
    ali->in_use = 1;

    memset(&(ali->stats), 0, sizeof(ali->stats));

    /* Register stats */
#if 0
    snprintf(ali_name, 255, "acclog.%s", name);
    nkn_mon_add("tot_logged", ali_name, &(ali->stats.tot_logged),
	    sizeof(ali->stats.tot_logged));
    nkn_mon_add("err_buf_overflow", ali_name, &(ali->stats.err_buf_overflow),
	    sizeof(ali->stats.err_buf_overflow));
    nkn_mon_add("tot_dropped", ali_name, &(ali->stats.tot_dropped),
	    sizeof(ali->stats.tot_dropped));
#endif

    return;
}


int al_remove_from_table(const char *name)
{
    al_info_t *ali = NULL;
    char *pname = (char *) name; //alloca(64);
    uint64_t keyhash = 0;

    /* PR: 783129
     * possible race causes this API to be called from nvsd_mgmt,
     * even before the table is created. this causes a segfault
     * when a lookup in the table is attempted.
     */
    if (al_profile_tbl == NULL) {
	return 0;
    }

    keyhash = n_str_hash(pname);
    ALP_HASH_WLOCK();

    ali = nhash_lookup(al_profile_tbl, keyhash, (void *) pname);

    nhash_remove(al_profile_tbl, keyhash, pname);
    ALP_HASH_WUNLOCK();

    if (ali && ali->al_fd) {
        ALI_HASH_WLOCK();
        NM_close_socket(ali->al_fd);
        al_info_freeall(ali);
        ALI_HASH_WUNLOCK();
    }

    return 0;
}

static int __al_init_socket(char *name, al_info_t **pali)
{
    struct sockaddr_un su;
    int ret = 0;
    int addrlen = 0;
    int i;
    int empty_slot = 0;
    uint64_t keyhash;

    al_info_t *ali = NULL;

    if (name == NULL)
	assert(!"profile name can't be NULL");

    /* PR: 783129
     */
    if (al_profile_tbl == NULL) {
	return 0;
    }

    keyhash = n_str_hash(name);
    ALP_HASH_RLOCK();
    ali = nhash_lookup(al_profile_tbl, keyhash, (void*) name);
    ALP_HASH_RUNLOCK();

    if (ali == NULL) {

	for (i = 0; i < MAX_ACCESSLOG_PROFILE; i++) {
	    ali = &al_pm[i];
	    ALI_HASH_WLOCK();
	    if (al_pm[i].in_use == 0) {
		empty_slot = 1;
		break;
	    }
	    ALI_HASH_WUNLOCK();
	}
	if (empty_slot == 0) {
		return ENOMEM;
	}
	al_info_init(name, ali);

	ali->al_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ali->al_fd < 0) {
	    ali->al_fd = -1;
	    al_info_freeall(ali);
	    ALI_HASH_WUNLOCK();
	    return errno;
	}

	nkn_mark_fd(ali->al_fd, NETWORK_FD);

	su.sun_family = AF_UNIX;
	addrlen = snprintf(su.sun_path, sizeof(su.sun_path), "%s/%s", LOG_UDS_PATH, name);
	addrlen = (addrlen + sizeof(su.sun_family));

	ret = connect(ali->al_fd, (struct sockaddr *)(&su), addrlen);
	if (ret < 0) {
	    ret = errno;
	    nkn_close_fd(ali->al_fd, NETWORK_FD);
	    al_info_freeall(ali);
	    ALI_HASH_WUNLOCK();
	    return ret;
	}

	if ((ret = al_handshake_accesslog(ali)) != 0) {
	    nkn_close_fd(ali->al_fd, NETWORK_FD);
	    al_info_freeall(ali);
	    ALI_HASH_WUNLOCK();
	    return ret;
	}
	ALI_HASH_WUNLOCK();
	ALP_HASH_WLOCK();
	// Add this log context to hash table
        ali->keyhash = n_str_hash(ali->key);
        //printf("insert: %p, %s, %lu\n", ali, (char*)ali->key, ali->keyhash);
	nhash_insert(al_profile_tbl, ali->keyhash, ali->key, ali);
        ALP_HASH_WUNLOCK();
    }

    /* At this point, config is done and we can insert into hashtable */
    *pali = ali;

    return 0;
}

/* called eevry 5 seconds */
void al_cleanup(void);
void al_cleanup(void)
{
    uint32_t l = nvsd_mgmt_acclog_profile_length();
    uint32_t i;
    al_info_t *ali = NULL;
    char *a_name = NULL;

    for (i = 0; i < l; i++) {
	// Bug 10331
	// Possible race here. length tells it is N, profile is deleted
	// and then we iterate over N elements, but in reality
	// there are N-1 names returned.
	// The Nth entry will be returned as NULL, which will trigger
	// assert at __al_init_socket
	a_name = NULL;
	a_name =  (char *) nvsd_mgmt_acclog_profile_get(i);

	if (a_name && __al_init_socket(a_name, &ali) == 0) {
	    if (ali && (ali->al_fd > 0)) {
		NM_del_event_epoll_inout(ali->al_fd);
		NM_add_event_epollout(ali->al_fd);
	    }
	}
    }
	//if(accfd == -1) {
		//al_init_socket();
		//return;
	//}

    /* Force adding epollout event 
     * There is no lock in this function which is called from timer thread to add epollout event. 
     * By the same time , it is also possible for the accesslog thread to call 
     * NM_add_event_epollin() function which may lead to race condition in setting epoll events.
     * Instead of adding lock in timer thread ,calling function to delete epoll_inout event 
     * before adding epollout event. 
     * */
        //NM_del_event_epoll_inout(accfd);
        //NM_add_event_epollout(accfd);
}

int al_init_socket(void);
int al_init_socket(void)
{
    int i;

    if (al_profile_tbl == NULL) {
	for (i = 0; i < MAX_ACCESSLOG_PROFILE; i++) {
	    pthread_mutex_init(&(al_pm[i].lock), NULL);
	}
	pthread_rwlock_init(&alp_tbl_lock, NULL);
	al_profile_tbl = nhash_table_new(n_str_hash, n_str_equal, 4096, mod_acclog_profile);
	if (al_profile_tbl == NULL)  return -1;
    }
    al_cleanup();
    return TRUE;
}


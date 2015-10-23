#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

#include "server.h"
#include "network.h"
#include "nkn_debug.h"
#include "nknlog_pub.h"
//#include "../nknlogd/nknlogd.h"
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "rtsp_server.h"
#include "rtsp_session.h"
#include "rtsp_header.h"
#include "accesslog_pub.h"
#include "nkn_cfg_params.h"

#define INFO_BUF 4096 //4 * 1024

NKNCNT_DEF(sl_err_buf_overflow, uint64_t, "", "Num of RTSP transaction is overflowed")
NKNCNT_DEF(tot_streaminglog, uint64_t, "", "Num of RTSP transaction is logged")

uint32_t streamlog_ip = DEF_LOG_IP;
uint16_t streamlog_port = LOGD_PORT;


#define MAX_STREAMLOG_BUFSIZE   (1*MiBYTES)
struct sl_info {
        LIST_ENTRY(sl_info) entries;
        int channelid;
        int num_sl_list;
	int  sl_num[32];
};

LIST_HEAD(sl_info_queue, sl_info);

static struct sl_info_queue g_sl_info_head = { NULL };
static int      stmfd = -1;
static char     sl_buf[MAX_STREAMLOG_BUFSIZE];
static int32_t  sl_buf_start = 0;
static int32_t  sl_buf_end = 0;
static pthread_mutex_t sl_mutex = PTHREAD_MUTEX_INITIALIZER;

enum{
                FORMAT_X_TIMESTAMP,       //x-localtime
                FORMAT_S_IP,            //s-ip
                FORMAT_R_IP,            //r-ip Origin server ipaddress, (dst_ip)
                FORMAT_C_IP,            //c-ip Client ip address, (src_ip)
                //FORMAT_BYTES_DELIVERED,       //Total bytes delivered, (out_bytes)
                FORMAT_TRANSACTION_CODE,//transaction or reply code, (status)
                FORMAT_CS_URI,          //Streaming URL access, (urlSuffix)
                FORMAT_CS_URI_STEM,     //Streaming URL stem only, (urlPreSuffix)
                FORMAT_CLIENT_ID,       //unique client side streaming id
                FORMAT_STREAM_ID,       //unique server side streaming id, (fOurSessionId)

                FORMAT_EMD_TIME,        //GMT time when transaction ended, (end_ts)
                FORMAT_CLIP_START,      //#of milisec into the stream, when client started receiving stream, (start_ts)
                FORMAT_CLIP_END,        //#of milisec into the stream, when the client stop receiving stream, (end_ts)
                FORMAT_PACKETS,         //Total number of packets delivered to the client
                FORMAT_CS_STATUS,       //HTTP responce code returned to the client
                FORMAT_CON_PROTOCOL,    //Protocol used during connection, RTSP
                FORMAT_TRA_PROTOCOL,    //Transport protocol used during the connection, TCP
                FORMAT_ACTION,          //Action performed, SPLIT, PROXY, CACHE, CONNECTING
                FORMAT_DROPPED_PACKETS, //#pf packets dropped
                FORMAT_PLAY_TIME,       //duration of time the client received the content

                FORMAT_PRODUCT,         //Streaming product used to create and stream content
                FORMAT_RECENT_PACKET,   //packets resent to the client
                FORMAT_C_PLAYERVERSION, //client media player version
                FORMAT_C_OS,            //client os
                FORMAT_C_OSVERSION,     //client os version
                FORMAT_C_CPU,           //Client CPU type
                FORMAT_FILELENGTH,      //length of the sream in secs
                FORMAT_AVG_BANDWIDTH,   //Avg bandwidth in bytes per sec
                FORMAT_CS_USER_AGENT,   //player info used in the header
                FORMAT_E_TIMESTAMP,     //number of sec since Jan 1, 1970, when transaction ended

                FORMAT_S_BYTES_IN,      //bytes received from client
                FORMAT_S_BYTES_OUT,     //bytes sent to client
                FORMAT_S_NAMESPACE_NAME,//Namespae name

                FORMAT_S_CACHEHIT,
                FORMAT_S_SERVER_PORT,

                FORMAT_S_PERCENT,
                FORMAT_S_UNSET,
                FORMAT_S_STRING,        // Special format, defined by configure
        };

int sl_init_socket(void);
//void http_accesslog_write(con_t * con) ;
//void rtsp_streaminglog_write(struct rtsp_cb * prtsp);
static void sl_info_freeall(void);


/***************************************************************************
 * handling socket in our network infrastructure
 ***************************************************************************/
static int sl_send_data(void);
static int sl_send_data(void)
{
        int ret;
        char * p;
        int len;

        //printf("al_send_data: called\n");
        p = &sl_buf[sl_buf_start];
        len = sl_buf_end - sl_buf_start;
        while(len) {
                ret = send(stmfd, p, len, 0);
                if (ret == -1) {
                        if (errno == EAGAIN) {
                                NM_add_event_epollout(stmfd);
                                return 1;
                        }
                        return -1;
                }
                else if (ret == 0) {
                        /* socket is full. Try again later. */
                        NM_add_event_epollout(stmfd);
                        return 1;
                }

                p += ret;
                len -= ret;
                sl_buf_start += ret;
        }
        sl_buf_start = 0;
        sl_buf_end = 0;

        NM_add_event_epollin(stmfd);
        return 1;
}

static int sl_epollin(int sockfd, void * private_data)
{
        char buf[256];

        UNUSED_ARGUMENT(private_data);

        // We should never receive any data from streamlog agent.
        // If we receive something, most likely, streamlog agent has closed the connection.
        pthread_mutex_lock(&sl_mutex);
        if (recv(sockfd, buf, 256, 0)<=0) {
                sl_info_freeall();
                stmfd = -1;
                pthread_mutex_unlock(&sl_mutex);
                return FALSE;
        }
        pthread_mutex_unlock(&sl_mutex);

        // Otherwise, we through away the data.
        return TRUE;
}


static int sl_epollout(int sockfd, void * private_data)
{
        UNUSED_ARGUMENT(sockfd);
        UNUSED_ARGUMENT(private_data);

        pthread_mutex_lock(&sl_mutex);
        if( sl_send_data() == -1) {
                sl_info_freeall();
                stmfd = -1;
                pthread_mutex_unlock(&sl_mutex);
                return FALSE;
        }
        pthread_mutex_unlock(&sl_mutex);
        return TRUE;
}

static int sl_epollerr(int sockfd, void * private_data)
{
        UNUSED_ARGUMENT(sockfd);
        UNUSED_ARGUMENT(private_data);

        pthread_mutex_lock(&sl_mutex);
        sl_info_freeall();
        stmfd = -1;
        pthread_mutex_unlock(&sl_mutex);
        return FALSE;
}

static int sl_epollhup(int sockfd, void * private_data)
{
        // Same as al_epollerr
        return sl_epollerr(sockfd, private_data);
}






static int find_starttime_value(char * buf, int buflen, char *data, int databuflen);
static int find_starttime_value(char * buf, int buflen, char *data, int databuflen)
{
	char * p = NULL; char * pp = NULL;
        int j = 0, k = 0, spos = 0, epos = 0, len = 0;
        char c;

        //Look for range:npt= in the buffer
        if ((buflen == 0) || (databuflen == 0))
                return 0;
        p = buf;
	for(j = 0; j < (buflen - 10); j++){
                if( (p[j] == 'r') && (p[j+1] == 'a') && (p[j+2] == 'n') && (p[j+3] == 'g') && (p[j+4] == 'e') &&
		    (p[j+5] == ':') && (p[j+6] == 'n') && (p[j+7] == 'p') && (p[j+8] == 't') && (p[j+9] == '=')){
                        //We have found range:npt=
                        j += 10;
                        while(j < buflen && (p[j] == ' ' || p[j] =='\t')) ++j;
			//look for - sign at the end of 0.000 stream
			for(; j < buflen; j++){
				c = p[j]; 
				if (c == '-')// we found the - sign
					break;
			}
			//remove the trailing spaces
			while(j < buflen && (p[j] == ' ' || p[j] =='\t')) ++j;
			spos = j;
			pp = buf + j;
                        for(k = j; k < buflen; k++, j++){
                                c = p[k];
                                if(c == '\r' || c == '\n')
                                        break;
                        }
                        epos = j;
                        len = epos - spos;
                        if(len > databuflen) {//data is more than what we need
                                strncpy(data, pp, databuflen);
                                len = databuflen;
                        }
                        else{
                                strncpy(data, pp, len);
                        }
                }//end if
        }//for
        return len;
}

static int find_Product_value(char * buf, int buflen, char *data, int databuflen);
static int find_Product_value(char * buf, int buflen, char *data, int databuflen)
{
	char * p = NULL; char * pp = NULL;
        int j = 0, k = 0, spos = 0, epos = 0;
        int len = 0;
        char c;

        if ((buflen == 0) || (databuflen == 0))
                return 0;
        p = buf;
	//find tool: in buf
        for(j = 0; j < (buflen - 5); j++){
                if( (p[j] == 't') && (p[j+1] == 'o') && (p[j+2] == 'o') && (p[j+3] == 'l') && (p[j+4] == ':')){
                        //We have found tool:
                        j += 5;
                        while(j < buflen && (p[j] == ' ' || p[j] =='\t')) ++j;
                        spos = j;
                        pp = buf + j;
                        for(k = j; k < buflen; k++, j++){
                                c = p[k];
                                if(c == '\r' || c == '\n')
                                        break;
                        }
                        epos = j;
                        len = epos - spos;
                        if(len > databuflen) {//data is more than what we need
                                strncpy(data, pp, databuflen);
                                len = databuflen;
                        }
                        else{
                                strncpy(data, pp, len);
                        }
                }//end if
        }//for
        return len;
}



/*
 * This function is called by RTSP module.
 */
void rtsp_streaminglog_write(struct rtsp_cb * prtsp) 
{
	struct sl_info   *pinfo;
        NLP_data * pdata;
	streamlog_item_t * item;
	char * p, * ps;
        int i;
        char *pvalue;
        int pvalue_len; // list_len;
        int32_t buf_len;


	unsigned int hdr_attr = 0;
	int hdr_cnt = 0;
	const char *hdr_str = NULL;
	int hdr_len = 0;
	char data[INFO_BUF];

	int j = 0;
	char * pp = NULL;
	char c;

	pthread_mutex_lock(&sl_mutex);

	if( LIST_EMPTY(&g_sl_info_head) || !prtsp ){
		pthread_mutex_unlock(&sl_mutex);
		return;
	}
	//glob_sl_tot_logged++;
	
	//Find User Agent
	char uagnt[INFO_BUF]; //User Agent data
        int ulen = 0;
	char os_str[INFO_BUF];  // Client OS info
        int oslen = 0;
        char c_player[INFO_BUF]; //Client player
        int cplen = 0;

	//Use the new api mime_hdr_get_known 
	if ((0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_USER_AGENT, &hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))){
		if((hdr_len > INFO_BUF)&& (hdr_str != NULL)){//Hdr_len seems to send wrong value if header is too long
			hdr_len = strlen(hdr_str);
			if (hdr_len > INFO_BUF) hdr_len = INFO_BUF;
		}
		memcpy(uagnt, hdr_str, hdr_len); uagnt[hdr_len] = 0; ulen = hdr_len;
	}

        //Find OS from User Agent Data
        if (ulen > 3){
                if ((strstr(uagnt, "win") != NULL) || (strstr(uagnt, "Win") != NULL)){
                        strcpy(os_str, "Windows");
                        oslen = strlen(os_str);
                }
                else if ((strstr(uagnt, "linux") != NULL) || (strstr(uagnt, "Linux") != NULL)){
                        strcpy(os_str, "Linux");
                        oslen = strlen(os_str);
                }
        }
	//Find Player
	if (ulen > 0){ 
		pp = uagnt;
		for(j = 0; j < ulen;  j++){
        		c = pp[j];
        	        if(c == '/' || c == '('){
        	        	break;
        	 	}
		}
		if (j > 0){
			cplen = j;
			strncpy(c_player, uagnt, cplen);
		}
	}


	//x-start-time
	char stimestr[INFO_BUF];
	int stimelen = 0;
	//We shouldn't get cb_resplen count more than the allocated buffer size bug#3366
	if(prtsp->cb_resplen < RTSP_BUFFER_SIZE){
		stimelen = find_starttime_value(prtsp->cb_respbuf, prtsp->cb_resplen, stimestr, INFO_BUF);
	}
	//x-product
	char prd[INFO_BUF];
	int prdlen = 0;
	//We shouldn't get cb_resplen  count more than the allocated buffer size bug#3366
	if(prtsp->cb_resplen < RTSP_BUFFER_SIZE){
		prdlen = find_Product_value(prtsp->cb_respbuf, prtsp->cb_resplen, prd, INFO_BUF);
	}

	for(pinfo = g_sl_info_head.lh_first; pinfo != NULL; pinfo = pinfo->entries.le_next) {
		buf_len = MAX_STREAMLOG_BUFSIZE - sl_buf_end;
		/*
                 * Setup NLP protocol
                 */ 
		if(buf_len < (int)sizeof(NLP_data)){
		//	//glob_sl_err_buf_overflow++
			goto skip_logit;
		}
	
		pdata = (NLP_data *)&sl_buf[sl_buf_end];
		pdata->channelid = pinfo->channelid;
		buf_len -= sizeof(NLP_data);

		/*
                 * Setup general information
                 */
		if(buf_len < (int)streamlog_item_s) {
                        //glob_sl_err_buf_overflow++;
                        goto skip_logit;
                }
		ps = (char *)pdata + sizeof(NLP_data);
		buf_len -= streamlog_item_s; // adjust avaliable space.
		item = (streamlog_item_t *)ps;
		item->start_ts.tv_sec = prtsp->start_ts.tv_sec;
		item->start_ts.tv_usec = prtsp->start_ts.tv_usec;
		item->end_ts.tv_sec = prtsp->end_ts.tv_sec;
		item->end_ts.tv_usec = prtsp->end_ts.tv_usec;
		item->in_bytes = prtsp->in_bytes;
		item->out_bytes = prtsp->out_bytes;
		item->resp_hdr_size = prtsp->cb_resplen;
		item->status = prtsp->status;
		item->subcode = 0; // no subcode available
		item->dst_port = 554; // always 554
		IPv4(item->dst_ipv4v6) = prtsp->dst_ip;
		item->dst_ipv4v6.family = AF_INET;
		IPv4(item->src_ipv4v6) = prtsp->src_ip;
		item->src_ipv4v6.family = AF_INET;
		item->flag = 0;

		if ( prtsp->method == METHOD_BAD){
			SET_ALITEM_FLAG(item, SLF_METHOD_BAD);
		} else if( prtsp->method == METHOD_OPTIONS ) {
                        SET_ALITEM_FLAG(item, SLF_METHOD_OPTIONS);
                } else if( prtsp->method == METHOD_DESCRIBE ) {
                        SET_ALITEM_FLAG(item, SLF_METHOD_DESCRIBE);
                } else if( prtsp->method == METHOD_SETUP ) {
                        SET_ALITEM_FLAG(item, SLF_METHOD_SETUP);
                } else if( prtsp->method == METHOD_PLAY ) {
                        SET_ALITEM_FLAG(item, SLF_METHOD_PLAY);
                } else if( prtsp->method == METHOD_PAUSE ) {
                        SET_ALITEM_FLAG(item, SLF_METHOD_PAUSE);
                } else if( prtsp->method == METHOD_GET_PARAMETER ) {
                        SET_ALITEM_FLAG(item, SLF_METHOD_GET_PARAMETER);
                } else if( prtsp->method == METHOD_TEARDOWN ) {
                        SET_ALITEM_FLAG(item, SLF_METHOD_TEARDOWN);
		} else if( prtsp->method == METHOD_BROADCAST_PAUSE ) {
			SET_ALITEM_FLAG(item, SLF_METHOD_BROADCAST_PAUSE);
		} else if( prtsp->method == METHOD_ANNOUNCE){
			SET_ALITEM_FLAG(item, SLF_METHOD_ANNOUNCE);
		} else if( prtsp->method == METHOD_RECORD){
                        SET_ALITEM_FLAG(item, SLF_METHOD_RECORD);
		} else if( prtsp->method == METHOD_REDIRECT){
                        SET_ALITEM_FLAG(item, SLF_METHOD_REDIRECT);
		} else if( prtsp->method == METHOD_SET_PARAM){
                        SET_ALITEM_FLAG(item, SLF_METHOD_SET_PARAM);
#if 0
		} else if( prtsp->method == METHOD_SENT_DESCRIBE_RESP){
                        SET_ALITEM_FLAG(item, SLF_METHOD_SENT_DESCRIBE_RESP);
		} else if( prtsp->method == METHOD_SENT_SETUP_RESP){
                        SET_ALITEM_FLAG(item, SLF_METHOD_SENT_SETUP_RESP);
		} else if( prtsp->method == METHOD_SENT_PLAY_RESP){
                        SET_ALITEM_FLAG(item, SLF_METHOD_SENT_PLAY_RESP);
		} else if( prtsp->method == METHOD_SENT_TEARDOWN_RESP){
                        SET_ALITEM_FLAG(item, SLF_METHOD_SENT_TEARDOWN_RESP);
#endif // 0
		}
	
		
		switch(prtsp->streamingMode) {
		case RTP_UDP: SET_ALITEM_FLAG(item, SLF_SMODE_RTP_UDP); break;
		case RTP_TCP: SET_ALITEM_FLAG(item, SLF_SMODE_RTP_TCP); break;
		case RAW_UDP: SET_ALITEM_FLAG(item, SLF_SMODE_RAW_UDP); break;
		case RAW_TCP: SET_ALITEM_FLAG(item, SLF_SMODE_RAW_TCP); break;
		} 

		/*
		 * Copy each field depends on configuration
		 */
		p = ps + streamlog_item_s;
		const namespace_config_t *nsconf = NULL;
		origin_server_HTTP_t *oshttp = NULL;
		origin_server_NFS_t *osnfs = NULL;
		origin_server_select_method_t select_method;
		char o_ip[256];
		char ns_name[256];
		int oiplen = 0; 
		int nslen = 0;

		nsconf = namespace_to_config(prtsp->nstoken);
                if(nsconf != NULL) {
			strcpy(ns_name, nsconf->namespace);
			nslen = strlen(ns_name);
			select_method =  nsconf->http_config->origin.select_method;
			switch ((int)select_method){
			case OSS_HTTP_CONFIG:
				oshttp = nsconf->http_config->origin.u.http.server[0];
				strcpy(o_ip, nsconf->http_config->origin.u.http.server[0]->hostname);
				oiplen = strlen(o_ip);
				break;
			case OSS_NFS_CONFIG:
				osnfs = nsconf->http_config->origin.u.nfs.server[0];
				strcpy(o_ip,nsconf->http_config->origin.u.nfs.server[0]->hostname_path);
				oiplen = strlen(o_ip);
				break;
			default:
				break;
			}
                        release_namespace_token_t(prtsp->nstoken);
                }//end if		

	
		for(i=0; i<pinfo->num_sl_list; i++) 
		{
			pvalue = NULL;

			switch( pinfo->sl_num[i]){
			case FORMAT_R_IP:
				if(oiplen > 0){
					pvalue = o_ip;
					pvalue_len = oiplen;
				}
				break;
			case FORMAT_CS_URI_STEM:
			case FORMAT_CS_URI:
				if ((0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_X_URL, &hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))){
					//hdr_len doesn't return the correct length bug #4234
					if ((hdr_len > INFO_BUF) && (hdr_str != NULL)){
						hdr_len = strlen(hdr_str);
						if (hdr_len > INFO_BUF) hdr_len = INFO_BUF;
					}
					memcpy(data, hdr_str, hdr_len); pvalue = data; pvalue_len = hdr_len;
				}
				break;
			case FORMAT_CLIENT_ID:
				if((0 == mime_hdr_get_unknown(&prtsp->hdr, "ClientId", 
					sizeof("ClientId")-1, &hdr_str, &hdr_len, &hdr_attr))){
					memcpy(data,hdr_str, hdr_len); pvalue = data; pvalue_len = hdr_len;
				}
				break;
			case FORMAT_STREAM_ID:
				if ((0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_CSEQ, &hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))){
					if ((hdr_len > INFO_BUF) && (hdr_str != NULL)){
                                                hdr_len = strlen(hdr_str);
						if (hdr_len > INFO_BUF) hdr_len = INFO_BUF;
                                        }
					memcpy(data, hdr_str, hdr_len); pvalue = data; pvalue_len = hdr_len; 
				}
				break;
			case FORMAT_CLIP_START:
				if(stimelen > 0){
					pvalue = stimestr;
					pvalue_len = stimelen;
				} 
				break;
			case FORMAT_CLIP_END:
			case FORMAT_PACKETS:
			case FORMAT_DROPPED_PACKETS:
                                break;
			case FORMAT_PRODUCT:
				if(prdlen > 0){
					pvalue = prd;
					pvalue_len = prdlen;
				}
				break;
			case FORMAT_RECENT_PACKET:
				break;
			case FORMAT_C_PLAYERVERSION:
				if(cplen > 0){
					pvalue = c_player;
					pvalue_len = cplen;
				}
                                break;
			case FORMAT_C_OS:
				if(oslen > 0){
					pvalue = os_str;
					pvalue_len = strlen(os_str);
				}
				break;
			case FORMAT_C_OSVERSION:
				break;
			case FORMAT_C_CPU:
                                break;
			case FORMAT_CS_USER_AGENT:
				//Get it from req buf/res buf
				if(ulen > 0){
					pvalue = uagnt;
					pvalue_len = strlen(uagnt);
				}
				break;
			case FORMAT_S_NAMESPACE_NAME:
				if(nslen > 0){
					pvalue = ns_name;
					pvalue_len = nslen;
				}
				break;
			default:
				break;

			}//end switch(pinfo->sl_num[i]
			/*
			 * if yes we found pvalue, copy it.
			 * if not, we return "-"
			 */
//logit:
			if (pvalue) {
				if(buf_len < pvalue_len+1) { 
					glob_sl_err_buf_overflow++; 
					goto skip_logit; 
				}
				buf_len -= pvalue_len+1;
				memcpy(p, pvalue, pvalue_len);
				p[pvalue_len] = '\0';
				p += pvalue_len + 1;
			} else {
				if(buf_len < 2) { 
					glob_sl_err_buf_overflow++; 
					goto skip_logit; 
				}
				buf_len -= 2;
				*p++ = '-'; 
				*p++ = 0; 
                        }
		}//end of for

		pdata->len = p-ps;
		sl_buf_end += pdata->len + sizeof(NLP_data);

skip_logit:
		if(sl_buf_end > MAX_STREAMLOG_BUFSIZE/4){
 			NM_add_event_epollout(stmfd);	
		}
	
	}

	pthread_mutex_unlock(&sl_mutex);
	return;
}



/***************************************************************************
 * Make the streamlog socket
 ***************************************************************************/

static int sl_info_add(NLP_streamlog_channel * plog, char * p);
static int sl_info_add(NLP_streamlog_channel * plog, char * p)
{
        struct sl_info * pinfo;
        int i;
	
	//printf("sl_info_add p = %s \n", p);
        if(plog->num_of_hdr_fields >= 32) {
                return FALSE;
        }
	
	//printf("sl_info_add: plog->len = %d\n", plog->len);
        // Save all the information as socket
        pinfo = (struct sl_info *)nkn_malloc_type(sizeof(struct sl_info),
						  mod_log_sl_info);
        if(!pinfo) {
                return FALSE;
        }
        pinfo->channelid = plog->channelid;
        pinfo->num_sl_list = plog->num_of_hdr_fields;
        //printf("sl_info_add: plog->num_of_hdr_fields=%d\n", plog->num_of_hdr_fields);
        for(i=0; i<(int)plog->num_of_hdr_fields; i++) {
		pinfo->sl_num[i] = atoi(p);
                p+=strlen(p)+1;
        }

        pthread_mutex_lock(&sl_mutex);
        LIST_INSERT_HEAD(&g_sl_info_head, pinfo, entries);
        pthread_mutex_unlock(&sl_mutex);

        return TRUE;
}

/* caller should get the al_mutex locker */
static void sl_info_freeall(void)
{
        //int i;
        struct sl_info * pinfo;

        while( !LIST_EMPTY(&g_sl_info_head) ) {
                pinfo = g_sl_info_head.lh_first;
                LIST_REMOVE(pinfo, entries);
                free(pinfo);
        }
        sl_buf_start = 0;
        sl_buf_end = 0;
        return ;
}


static int sl_handshake_streamlog(void);
static int sl_handshake_streamlog(void)
{
        NLP_client_hello * req;
        NLP_server_hello * res;
        int ret, len, i;
        NLP_streamlog_channel * plog;
        char * p;

        /*
         * Send request to nvsd server.
         */
        req = (NLP_client_hello *)sl_buf;
        strcpy(req->sig, SIG_SEND_STREAMLOG);
        if (send(stmfd, sl_buf, sizeof(NLP_client_hello), 0) <=0) {
                return -1;
        }
        //printf("Sends out errorlog request to nvsd ...\n");

        /*
         * Read response from nvsd server.
         */
        ret = recv(stmfd, sl_buf, sizeof(NLP_server_hello), 0);
        if (ret<(int)sizeof(NLP_server_hello)) {
                perror("Socket recv.");
                return -1;
        }
        res = (NLP_server_hello *)sl_buf;

        len = res->lenofdata;

	if (len != 0)
	{
        	ret = recv(stmfd, &sl_buf[sizeof(NLP_server_hello)], len, 0);
        	if (ret<len) {
                	perror("Socket recv.");
                	return -1;
        	}
        	//printf("len=%d ret=%d\n", len, ret);
        	p = &sl_buf[sizeof(NLP_server_hello)];
        	for(i=0; i<res->numoflog; i++) {
                	plog = (NLP_streamlog_channel *)p;

                	p += sizeof(NLP_streamlog_channel);
                	if(sl_info_add(plog, p) == FALSE) {
                        	return -1;
                	}
                	p += plog->len;
        	}
	}
        //printf("Debuglog socket is established.\n");

        NM_set_socket_nonblock(stmfd);

        // We need to unlock this socket because when the locker
        if(register_NM_socket(stmfd,
                        NULL,
                        sl_epollin,
                        sl_epollout,
                        sl_epollerr,
                        sl_epollhup,
                        NULL,
                        NULL,
                        0,
                        USE_LICENSE_FALSE,
			FALSE) == FALSE)
        {
                return -1;
        }
        NM_add_event_epollin(stmfd);

        return 1;
}


int sl_init_socket(void)
{
        struct sockaddr_in srv;
        int ret;

	if (rtsp_enable == 0) return -1;

        pthread_mutex_lock(&sl_mutex);

        // Create a socket for making a connection.
        // here we implemneted non-blocking connection
        if ((stmfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                //printf("%s: stmfd failed", __FUNCTION__);
                stmfd = -1;
                pthread_mutex_unlock(&sl_mutex);
                return -1;
        }
	nkn_mark_fd(stmfd, NETWORK_FD);

        // Now time to make a socket connection.
        bzero(&srv, sizeof(srv));
        srv.sin_family = AF_INET;
        //srv.sin_addr.s_addr = 0x0100007F;
        //srv.sin_port = htons(LOGD_PORT);
	srv.sin_addr.s_addr = streamlog_ip;
	srv.sin_port = htons(streamlog_port);

        //log_set_socket_nonblock(stmfd);

        ret = connect(stmfd, (struct sockaddr *)&srv, sizeof(struct sockaddr));
        if (ret < 0) {
                //printf("Failed to connect to nknlogd server\n");
                nkn_close_fd(stmfd, NETWORK_FD);
                stmfd = -1;
                pthread_mutex_unlock(&sl_mutex);
                return -1;
        }
        //printf("Successfully connect to nknlogd server\n");
        pthread_mutex_unlock(&sl_mutex);

        if (sl_handshake_streamlog() < 0) {
                sl_info_freeall();
                nkn_close_fd(stmfd, NETWORK_FD);
                stmfd = -1;
                return -1;
        }

        return 1;
}


/* called every 5 seconds */
void sl_cleanup(void);
void sl_cleanup(void)
{
        if(stmfd == -1) {
                sl_init_socket();
                return;
        }

	/* Force adding epollout event 
         * There is no lock in this function which is called from timer thread to add epollout event. 
         * By the same time , it is also possible for the accesslog thread to call 
         * NM_add_event_epollin() function which may lead to race condition in setting epoll events. 
         * Instead of adding lock in timer thread ,calling function to delete epoll_inout event 
         * before adding epollout event. 
         * */ 

	if (stmfd != -1) {
		NM_del_event_epoll_inout(stmfd);
	}
	if (stmfd != -1) {
	        NM_add_event_epollout(stmfd);
	}
}


#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "log_accesslog_pri.h"
#include "nknlogd.h"
#include "accesslog_pub.h"
#include "log_streamlog_pri.h"

extern pthread_mutex_t log_lock;

static void fix_format_field(logd_file * plf, streamlog_item_t * item);
static void fix_format_field(logd_file * plf, streamlog_item_t * item)
{
	int j;
	char * p;

	// Server will return the string in the same order that we sent out.
	// If it is none, server will fill in "-" for this field
	p=(char *)item + streamlog_item_s;
	for(j=0; j<plf->sl_used; j++) {
		if(plf->sl_ptr[j].string[0] == 0) continue;
		if(plf->sl_ptr[j].field == FORMAT_S_STRING) continue;
		plf->sl_ptr[j].pdata = p;
		p+=strlen(p)+1;
	}
}

static void rtsp_streamlog_write(logd_file * plf, char * psrc, int len);
static void rtsp_streamlog_write(logd_file * plf, char * psrc, int len)
{
	int j;
	char * p;
	const char * value;
	char logbuf[MAX_OTHERLOG_BUFSIZE + 400];
	struct in_addr in;
	streamlog_item_t * item;

	
	item = (streamlog_item_t *)psrc;
	fix_format_field(plf, item);

	p = &logbuf[0];
	//Calculate average bandwidth
	double avg_bw = 0.0;
	double time_diff = (double)(item->end_ts.tv_sec -  item->start_ts.tv_sec);
	if(time_diff > 0){
		avg_bw = (double)item->out_bytes / time_diff;
	}
	else {
		avg_bw = (double)item->out_bytes;
	}

	for (j = 0; j < plf->sl_used; j++) {
		value=NULL;

		switch(plf->sl_ptr[j].field) {

		/*
		 * Whatever configured, copy as is.
		 */
		case FORMAT_S_STRING:
			strcpy(p, plf->sl_ptr[j].string);
			p += strlen(p);
		break;

		/*
		 * Time stamp
		 */
		case FORMAT_X_TIMESTAMP:
		{
			/* cache the generated timestamp */
			//[02/Oct/2008:18:54:52 -0700] 
			struct tm tm;
			localtime_r(&(item->end_ts.tv_sec), &tm);
			strftime(p, 255, "[%d/%b/%Y:%H:%M:%S %z]", &tm);
			p += strlen(p);
		}
		break;
		case FORMAT_EMD_TIME:
		{
			/* cache the generated timestamp */
			// in the format 10:32:02
			struct tm tm;
                        localtime_r(&(item->end_ts.tv_sec), &tm);
                        strftime(p, 255, "%H:%M:%S", &tm);
                        p += strlen(p);
		}
		break;
		case FORMAT_S_IP:
			in.s_addr=IPv4(item->dst_ipv4v6);
                        sprintf(p, "%s", inet_ntoa(in));
                        p += strlen(p);
		break;

		case FORMAT_C_IP:
			in.s_addr=IPv4(item->src_ipv4v6);
			sprintf(p, "%s", inet_ntoa(in));
			p += strlen(p);
                break;

#if 0
		//case FORMAT_BYTES_DELIVERED:
		case FORMAT_TRANSACTION_CODE:
			assert(plf->sl_ptr[j].pdata[0] != 0);
			sprintf(p, "%s", plf->sl_ptr[j].pdata);
			p += strlen(p);
		break;
#endif // 0

		case FORMAT_CS_URI:
		case FORMAT_CS_URI_STEM:
			if(plf->sl_ptr[j].pdata[0] == 0){
				strcpy(p, "-"); p += 1;
			}
                        /*if (CHECK_ALITEM_FLAG(item, SLF_METHOD_DESCRIBE)) {
                                strcpy(p, "DESCRIBE"); p+=8;
                        } else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_SETUP)) {
                                strcpy(p, "SETUP"); p+=5;
                        } else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_PLAY)) {
                                strcpy(p, "PLAY"); p+=4;
                        } else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_PAUSE)) {
                                strcpy(p, "PAUSE"); p+=5;
                        } else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_TEARDOWN)) {
                                strcpy(p, "TEARDOWN"); p+=8;
                        } else {
                                strcpy(p, "UNKNOWN"); p+=7;
                        }
                        sprintf(p, " %s RTSP/1.1",
                                plf->sl_ptr[j].pdata);
			*/
			else{
				sprintf(p, "%s", plf->sl_ptr[j].pdata);
                        	p += strlen(p);
			}
		break;

		case FORMAT_TRANSACTION_CODE:
		case FORMAT_ACTION:
                        if (CHECK_ALITEM_FLAG(item, SLF_METHOD_OPTIONS)) {
                                strcpy(p, "OPTIONS"); p+=7;
                        } else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_DESCRIBE)) {
                                strcpy(p, "DESCRIBE"); p+=8;
                        } else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_SETUP)) {
                                strcpy(p, "SETUP"); p+=5;
                        } else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_PLAY)) {
                                strcpy(p, "PLAY"); p+=4;
                        } else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_PAUSE)) {
                                strcpy(p, "PAUSE"); p+=5;
                        } else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_GET_PARAMETER)) {
                                strcpy(p, "GET_PARAMETER"); p+=13;
                        } else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_TEARDOWN)) {
                                strcpy(p, "TEARDOWN"); p+=8;
                        } else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_BROADCAST_PAUSE)) {
			 	strcpy(p, "BROADCAST_PAUSE"); p+=15;
			} else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_BAD)) {
				 if(item->status == 400){
                                        // Bug # 4150, we don't want to show BAD method in case of a 400 response code
                                        strcpy(p, "-"); p+=1;
                                }
                                else{
                                	strcpy(p, "BAD"); p+=3;
   				}
			} else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_ANNOUNCE)) {
                                strcpy(p, "ANNOUNCE"); p+=8;
			} else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_RECORD)) {
                                strcpy(p, "RECORD"); p+=6;
			} else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_REDIRECT)) {
                                strcpy(p, "REDIRECT"); p+=8;
			} else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_SET_PARAM)) {
                                strcpy(p, "SET_PARAMETER"); p+=13;
			} else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_SENT_DESCRIBE_RESP)) {
                                strcpy(p, "DESCRIBE_RESP"); p+=13;
			} else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_SENT_SETUP_RESP)) {
                                strcpy(p, "SETUP_RESP"); p+=10;
			} else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_SENT_PLAY_RESP)) {
                                strcpy(p, "PLAY_RESP"); p+=9;
			} else if (CHECK_ALITEM_FLAG(item, SLF_METHOD_SENT_TEARDOWN_RESP)) {
                                strcpy(p, "TEARDOWN_RESP"); p+=13;
			} else {
                                strcpy(p, "UNKNOWN"); p+=7;
                        }
                        //p += strlen(p);
		break;

		case FORMAT_PLAY_TIME:
			sprintf(p, "%ld",(long)(item->end_ts.tv_sec - item->start_ts.tv_sec));
                        p += strlen(p);
		break;

		case FORMAT_CON_PROTOCOL:
			strcpy(p, "RTSP"); p+=4;
		break;
		case FORMAT_TRA_PROTOCOL:
			if (CHECK_ALITEM_FLAG(item, SLF_SMODE_RTP_UDP)) {
				strcpy(p, "RTP_UDP"); p+=7;
			} else if (CHECK_ALITEM_FLAG(item, SLF_SMODE_RTP_TCP)) {
				strcpy(p, "RTP_TCP"); p+=7;
			} else if (CHECK_ALITEM_FLAG(item, SLF_SMODE_RAW_UDP)) {
				strcpy(p, "RAW_UDP"); p+=7;
			} else if (CHECK_ALITEM_FLAG(item, SLF_SMODE_RAW_TCP)) {
				strcpy(p, "RAW_TCP"); p+=7;
			} else {
				strcpy(p, "UNKNOWN"); p+=7;
			}
		break;
		case FORMAT_FILELENGTH:
			sprintf(p, "%ld",(long)(item->end_ts.tv_sec - item->start_ts.tv_sec));
			p += strlen(p);
		break;
		case FORMAT_E_TIMESTAMP:
			sprintf(p, "%ld", (long)item->end_ts.tv_sec);
			p += strlen(p);
		break;
		case FORMAT_AVG_BANDWIDTH:
			sprintf(p, "%.2f", avg_bw);
			p += strlen(p);	
		break;
		case FORMAT_R_IP:
		case FORMAT_CLIENT_ID:
		case FORMAT_STREAM_ID:
		case FORMAT_CLIP_START:
		case FORMAT_CLIP_END:
		case FORMAT_PACKETS:
			if (plf->sl_ptr[j].pdata[0] == 0){
				strcpy(p, "-"); p += 1;
			}
			else{
				sprintf(p, "%s", plf->sl_ptr[j].pdata);
                        	p += strlen(p);
			}
		break;
		case FORMAT_CS_STATUS:
			sprintf(p, "%d", item->status);
			p += strlen(p);
		break;
		
		case FORMAT_DROPPED_PACKETS:
		case FORMAT_PRODUCT:
		case FORMAT_RECENT_PACKET:
		case FORMAT_S_NAMESPACE_NAME:
			if (plf->sl_ptr[j].pdata[0] == 0){
				strcpy(p, "-"); p += 1;
			}
			else{
                        	sprintf(p, "%s", plf->sl_ptr[j].pdata);
                        	p += strlen(p);
			}
                break;

		case FORMAT_C_PLAYERVERSION:
		case FORMAT_C_OS:
		case FORMAT_C_OSVERSION:
		case FORMAT_C_CPU:
			if( plf->sl_ptr[j].pdata[0] == 0){
				strcpy(p, "-"); p += 1;
			}
			else{
                        	sprintf(p, "%s", plf->sl_ptr[j].pdata);
                        	p += strlen(p);
			}
                break;
	 
		case FORMAT_CS_USER_AGENT:
			if (plf->sl_ptr[j].pdata[0] == 0){
				strcpy(p, "-"); p += 1;
			}
			else{
                        	sprintf(p, "%s", plf->sl_ptr[j].pdata);
                        	p += strlen(p);
			}
                break;
		//case FORMAT_STR_CACHEHIT:
		//case FORMAT_STR_SERVER_PORT:
		case FORMAT_S_BYTES_IN:
			sprintf(p, "%ld", item->in_bytes);
			p += strlen(p);
		break;

		case FORMAT_S_BYTES_OUT:
			sprintf(p, "%ld", item->out_bytes);
			p += strlen(p);
		break;

		default:
			*p = '-';
			p++;
		break;
		}

		if(value) {
			strcpy(p, value);
			p += strlen(p);
		}
	}

	*p='\n';
	p++;

	log_write(plf, logbuf, p-logbuf);
}


/***************************************************************************
 * Log server socket handling
 ***************************************************************************/
static int streamlog_epollin(log_network_mgr_t *pnm)
{
    struct log_recv_t * pstmlog = (struct log_recv_t *)pnm->private_data;
    logd_file * plf;
    NLP_data * pdata;
    int len, see_channelid;

    len = recv(pnm->fd, &pstmlog->buf[pstmlog->offsetlen+pstmlog->totlen], 
	    MAX_OTHERLOG_BUFSIZE - pstmlog->offsetlen - pstmlog->totlen, 0);
    if (len <= 0) {
	if (len < 0) {
	    complain_error_errno(1, "recv failed");
	}
	printf("Streamlog socket %d closed\n", pnm->fd);
	unregister_free_close_sock(pnm);
	return FALSE;
    }
    pstmlog->totlen += len;

    while(1) {
	if(pstmlog->totlen == 0) {
	    pstmlog->offsetlen = 0;
	    return TRUE;
	}

	pdata = (NLP_data *)&pstmlog->buf[pstmlog->offsetlen];
	//printf("channelid=%d len=%d totlen=%d offsetlen=%d\n", 
	//		pdata->channelid, pdata->len, pacclog->totlen, pacclog->offsetlen);
	if(pstmlog->totlen < pdata->len + (uint32_t)sizeof(NLP_data)) {
	    // need more data
	    if(pstmlog->offsetlen > MAX_OTHERLOG_BUFSIZE/2) {
		memcpy(&pstmlog->buf[0], 
			&pstmlog->buf[pstmlog->offsetlen],
			pstmlog->totlen);
		pstmlog->offsetlen = 0;
	    }
	    return TRUE;
	}

	see_channelid = 0;
	pthread_mutex_lock(&log_lock);
	for (plf = g_lf_head.lh_first; plf != NULL; plf = plf->entries.le_next) {
	    if(plf->type != TYPE_STREAMLOG) continue;
	    if(plf->channelid == pdata->channelid) {
		see_channelid = 1; //We see the channel id.
		rtsp_streamlog_write(plf, 
			&pstmlog->buf[pstmlog->offsetlen + sizeof(NLP_data)], 
			pdata->len);
		break;
	    }
	}
	pthread_mutex_unlock(&log_lock);
	pstmlog->offsetlen += sizeof(NLP_data) + pdata->len;
	pstmlog->totlen -= sizeof(NLP_data) + pdata->len;

	if(see_channelid == 0) {
	    // configuration has been changed.
	    // close this socket.
	    //LIST_REMOVE(pstmlog, entries);
	    unregister_free_close_sock(pnm);
	    return TRUE;	
	}
    }
}


static int streamlog_epollout(log_network_mgr_t *pnm)
{
	assert(0);
        return FALSE;
}

static int streamlog_epollerr(log_network_mgr_t *pnm)
{
	//struct log_recv_t * pstmlog = (struct log_recv_t *)pnm->private_data;
	//LIST_REMOVE(pstmlog, entries);
	unregister_free_close_sock(pnm);
        return TRUE;
}

static int streamlog_epollhup(log_network_mgr_t *pnm)
{
	//struct log_recv_t * pstmlog = (struct log_recv_t *)pnm->private_data;
	//LIST_REMOVE(pstmlog, entries);
	unregister_free_close_sock(pnm);
        return TRUE;
}

void sl_handle_new_send_socket(log_network_mgr_t *pnm);
void sl_handle_new_send_socket(log_network_mgr_t *pnm)
{
	char buf[1024] = {0};
        logd_file * plf;
	NLP_streamlog_channel * plog = NULL;
	NLP_server_hello * res = NULL;
	int numoflog = 0;
	struct log_recv_t * pstmlog = NULL;
	char * p = NULL;
	int j = 0, ret = 0;

	pstmlog = (struct log_recv_t *)malloc(sizeof(struct log_recv_t));
	if(!pstmlog) {
		close(pnm->fd);
		exit(1);
	}
	memset((char *)pstmlog, 0, sizeof(struct log_recv_t));
	pstmlog->fd = pnm->fd;
	//LIST_INSERT_HEAD(&g_log_recv_head, pstmlog, entries);

	res = (NLP_server_hello *)buf;
	p = (char *)&buf[sizeof(NLP_server_hello)];


        if(register_log_socket(pnm->fd,
                        pstmlog,
                        streamlog_epollin,
                        streamlog_epollout,
                        streamlog_epollerr,
                        streamlog_epollhup,
                        NULL,
			&pnm) == FALSE)
        {
                // will this case ever happen at all?
                close(pnm->fd);
                exit(1);
        }

	pthread_mutex_lock(&log_lock);
        for (plf = g_lf_head.lh_first; plf != NULL; plf = plf->entries.le_next) {
		if(plf->type != TYPE_STREAMLOG) continue;
		plf->pnm = pnm;

		if(plf->active == FALSE) continue;

		plog = (NLP_streamlog_channel *)p;
		plog->channelid = plf->channelid;
		plog->num_of_hdr_fields = 0;

		p += sizeof(NLP_streamlog_channel);
	        for(j=0; j<plf->sl_used; j++) {
			if(plf->sl_ptr[j].string[0] == 0) continue;
	                if(plf->sl_ptr[j].field == FORMAT_S_STRING) continue;
	                strcpy(p, plf->sl_ptr[j].string);
	                p += strlen(p)+1;
			plog->num_of_hdr_fields ++;
        	}
		plog->len = p - (char *)plog - sizeof(NLP_streamlog_channel); // exclude header size
		numoflog++;
        }
	pthread_mutex_unlock(&log_lock);
	res->numoflog = numoflog;
	res->lenofdata = p - (char *)&buf[sizeof(NLP_server_hello)];


	log_add_event_epollin(pnm);

	ret = send(pnm->fd, buf, p - (char *)buf, 0);
	if(ret < 0)
	{
	    complain_error_errno(1, "send failed");
	}


	/* setting this to avoid freeing of pnm from management thread 
           when management thread tries to use this pnm 
        */
        LOG_SET_FLAG(pnm, LOGD_NW_MGR_MAGIC_ALIVE);

}

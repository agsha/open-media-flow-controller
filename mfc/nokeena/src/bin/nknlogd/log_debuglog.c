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
#include <syslog.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <unistd.h>
#include <pthread.h>
#include "log_accesslog_pri.h"
#include "nknlogd.h"



extern pthread_mutex_t log_lock;

/***************************************************************************
 * Log server socket handling
 ***************************************************************************/
static int dbglog_epollin(log_network_mgr_t *pnm)
{
    struct log_recv_t * pdbglog = (struct log_recv_t *)pnm->private_data;
    logd_file * plf;
    NLP_data * pdata;
    int see_channelid;
    int len;

    len = recv(pnm->fd, &pdbglog->buf[pdbglog->offsetlen+pdbglog->totlen], 
	    MAX_OTHERLOG_BUFSIZE - pdbglog->offsetlen - pdbglog->totlen, 0);
    if (len <= 0) {
	//perror("debuglog socket closed: ");
	//printf("Debuglog socket %d closed\n", pnm->fd);
	if (len < 0) {
	    complain_error_errno(1, "recv failed");
	}
	unregister_free_close_sock(pnm);
	return FALSE;
    }
    pdbglog->totlen += len;

    while(1) {
	if(pdbglog->totlen == 0) {
	    pdbglog->offsetlen = 0;
	    return TRUE;
	}

	pdata = (NLP_data *)&pdbglog->buf[pdbglog->offsetlen];
	if(pdbglog->totlen < pdata->len + (uint32_t)sizeof(NLP_data)) {
	    // need more data
	    if(pdbglog->offsetlen > MAX_OTHERLOG_BUFSIZE/2) {
		memcpy(&pdbglog->buf[0], 
			&pdbglog->buf[pdbglog->offsetlen],
			pdbglog->totlen);
		pdbglog->offsetlen = 0;
	    }
	    return TRUE;
	}

	see_channelid = 0;
	pthread_mutex_lock ( &log_lock);
	for (plf = g_lf_head.lh_first; plf != NULL; plf = plf->entries.le_next) {
	    if ((plf->type  == TYPE_ACCESSLOG) || (plf->type  == TYPE_STREAMLOG))continue;
	    if(plf->channelid == pdata->channelid) {
		see_channelid = 1;
		if(plf->active){
		    log_write(plf, 
			    &pdbglog->buf[pdbglog->offsetlen + sizeof(NLP_data)], 
			    pdata->len);
		}
		break;
	    }
	}
	pthread_mutex_unlock ( &log_lock);
	pdbglog->offsetlen += sizeof(NLP_data) + pdata->len;
	pdbglog->totlen -= sizeof(NLP_data) + pdata->len;

	if(see_channelid == 0) {
	    unregister_free_close_sock(pnm);
	    return TRUE;
	}
    }
}


static int dbglog_epollout(log_network_mgr_t *pnm)
{
	donotreachme("dbglog_epollout");
        return FALSE;
}

static int dbglog_epollerr(log_network_mgr_t *pnm)
{
	unregister_free_close_sock(pnm);
        return FALSE;
}

static int dbglog_epollhup(log_network_mgr_t *pnm)
{
	unregister_free_close_sock(pnm);
        return FALSE;
}

void el_handle_new_send_socket(log_network_mgr_t *pnm);
void el_handle_new_send_socket(log_network_mgr_t *pnm)
{
	char buf[1024] = {0};
        logd_file * plf = NULL;
	NLP_debuglog_channel * plog = NULL;
	NLP_server_hello * res = NULL;
	int numoflog = 0;
	struct log_recv_t * pdbglog = NULL;
	int ret = 0;

	pdbglog = (struct log_recv_t *)malloc(sizeof(struct log_recv_t));
	if(!pdbglog) {
		close(pnm->fd);
		exit(1);
	}
	memset((char *)pdbglog, 0, sizeof(struct log_recv_t));
	pdbglog->fd = pnm->fd;
	res = (NLP_server_hello *)buf;
	plog = (NLP_debuglog_channel *)&buf[sizeof(NLP_server_hello)];

        if(register_log_socket(pnm->fd,
                        pdbglog,
                        dbglog_epollin,
                        dbglog_epollout,
                        dbglog_epollerr,
                        dbglog_epollhup,
                        NULL,
			&pnm) == FALSE)
        {
                // will this case ever happen at all?
                close(pnm->fd);
                exit(1);
        }


	pthread_mutex_lock ( &log_lock);
        for (plf = g_lf_head.lh_first; plf != NULL; plf = plf->entries.le_next) {
		if(plf->type == TYPE_ACCESSLOG || plf->type == TYPE_STREAMLOG) // only error based logs 
			continue;
		plf->pnm = pnm;
		if(plf->active == FALSE) 
			continue;

		plog->channelid = plf->channelid;
		plog->level = plf->level;
		plog->mod = plf->mod;
		plog->type = plf->type;
		plog++;
		numoflog++;
        }
	pthread_mutex_unlock ( &log_lock);
	res->numoflog = numoflog;
	res->lenofdata = (char *)plog - (char *)&buf[sizeof(NLP_server_hello)];


	log_add_event_epollin(pnm);

	ret = send(pnm->fd, buf, (char *)plog - (char *)buf, 0);
	if(ret <= 0)
	{
	    complain_error_errno(1, "send failed");

	}
	/* setting this to avoid freeing of pnm from management thread 
           when management thread tries to use this pnm 
        */
        LOG_SET_FLAG(pnm, LOGD_NW_MGR_MAGIC_ALIVE);
}


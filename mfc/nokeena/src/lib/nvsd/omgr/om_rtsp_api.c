#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "nvsd_mgmt.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "server.h"
#include "nkn_util.h"
#include "nkn_stat.h"
#include "nkn_cod.h"
#include "nkn_mediamgr_api.h"
#include "rtsp_session.h"
#include "rtsp_func.h"
#include "rtsp_server.h"
#include "rtsp_live.h"
#include "atomic_ops.h"
#include "nkn_lockstat.h"

extern orp_fetch_uri_t *orp_look_up_uri(char * uri);
extern orp_fetch_uri_t *orp_look_up_physid(char * physid);

NKNCNT_DEF(rtspom_tot_stat_called, AO_t, "", "Total RTSP OM stat called")
NKNCNT_DEF(rtspom_open_get_task, AO_t, "", "Total open RTSP OM task")
NKNCNT_DEF(rtspom_tot_get_called, AO_t, "", "Total RTSP OM get called")
NKNCNT_DEF(rtspom_tot_attrbufs_alloced, uint32_t, "", "total attribute buffers allocated")
NKNCNT_DEF(rtspom_err_get_task, AO_t, "", "RTSP OM Get error")
NKNCNT_DEF(rtspom_err_stat_inv_offset, AO_t, "", "RTSP OM invalid offset");
//NKNCNT_DEF(rtspom_err_get_inv_offset, AO_t, "", "RTSP OM invalid offset");

#define GET_PROVIDER_ALLOC_ATTRBUF	0x00000001

static void rtsp_om_alloc_attrbuf(MM_get_resp_t * p_mm_task)
{
	p_mm_task->in_attr = nkn_buffer_alloc(NKN_BUFFER_ATTR, 0, 0);
	if (p_mm_task->in_attr) {
		p_mm_task->provider_flags |= GET_PROVIDER_ALLOC_ATTRBUF;
		glob_rtspom_tot_attrbufs_alloced++;
	}
}

static void rtsp_om_free_attrbuf(MM_get_resp_t * p_mm_task)
{
	if (p_mm_task->provider_flags & GET_PROVIDER_ALLOC_ATTRBUF) {
		p_mm_task->provider_flags &= ~GET_PROVIDER_ALLOC_ATTRBUF;
		nkn_buffer_release(p_mm_task->in_attr);
		p_mm_task->in_attr = 0;
		glob_rtspom_tot_attrbufs_alloced--;
	}
}

/* adding stubs for all the RTSP OM handlers
 */

static int RTSP_OM_stat(nkn_uol_t uol, MM_stat_resp_t *in_ptr_resp);
static int RTSP_OM_get(MM_get_resp_t *p_mm_task);
static int RTSP_OM_validate (MM_validate_t *pvalidate);

static int RTSP_OM_stat(nkn_uol_t uol, MM_stat_resp_t *in_ptr_resp)
{
	//static uint64_t rtsp_om_physid = 0;
	//struct stat buf;
	orp_fetch_uri_t *porp = NULL;
	orp_fetch_om_uri_t *pom;
	int ret = NKN_OM_RTSP_ERR_UNKNOWN;

	UNUSED_ARGUMENT(uol);

	if (!VALID_NS_TOKEN(in_ptr_resp->ns_token)) {
		// No namespace associated with request
		return NKN_OM_NO_NAMESPACE;
	}

	if (in_ptr_resp->in_client_data.proto != NKN_PROTO_RTSP) {
		return NKN_OM_RTSP_ERR_UNKNOWN;
	}
	
	//snprintf(&in_ptr_resp->physid[0], NKN_MAX_FILE_NAME_SZ,
	//	"RTSP_OM_%08ld", rtsp_om_physid);
	//rtsp_om_physid++;
	
	in_ptr_resp->media_blk_len = MAX_CM_IOVECS*CM_MEM_PAGE_SIZE;
	in_ptr_resp->content_len = OM_STAT_SIG_TOT_CONTENT_LEN;
	in_ptr_resp->tot_content_len = OM_STAT_SIG_TOT_CONTENT_LEN;

	pom = (orp_fetch_om_uri_t *) in_ptr_resp->in_proto_data;
	if (pom) {
		porp = orp_look_up_physid(pom->physid);
	}
	if (porp && CHECK_PORP_FLAG(porp, PORPF_ACTIVE)) {
		in_ptr_resp->physid_len = 
			snprintf(&in_ptr_resp->physid[0], NKN_PHYSID_MAXLEN,
				 "%s", porp->physid);
		in_ptr_resp->physid_len = MIN(in_ptr_resp->physid_len, NKN_PHYSID_MAXLEN - 1);
		pthread_mutex_lock(porp->pmutex); 
		if (porp->pplayer) {
			porp->pplayer->uol.cod = nkn_cod_dup(uol.cod, NKN_COD_ORIGIN_MGR);
			porp->pplayer->uol.offset = uol.offset;
			porp->pplayer->uol.length = uol.length;
			if (porp->pplayer->ob_tot_written != (uint64_t) uol.offset) {
				ret = NKN_OM_INV_PARTIAL_OBJ;
				AO_fetch_and_add1(&glob_rtspom_err_stat_inv_offset);
				DBG_LOG(MSG, MOD_RTSP, "Inv offset Physid: %s uri=%s offset=%ld len=%ld, OM wr=%ld",
					pom->physid, nkn_cod_get_cnp(uol.cod),
					uol.offset, uol.length, porp->pplayer->ob_tot_written);
			}
			else {
				DBG_LOG(MSG, MOD_RTSP, "Physid: %s uri=%s offset=%ld len=%ld, OM wr=%ld",
					pom->physid, nkn_cod_get_cnp(uol.cod),
					uol.offset, uol.length, porp->pplayer->ob_tot_written);
				ret = 0;
			}
		}
		pthread_mutex_unlock(porp->pmutex);
	}
	//else {
	//	return NKN_OM_RTSP_ERR_UNKNOWN;
	//}
	AO_fetch_and_add1(&glob_rtspom_tot_stat_called);
	return ret;
}

/* Need to cache video/audio stream data and passthrough the rest
 */
static int RTSP_OM_get(MM_get_resp_t *p_mm_task)
{
	//off_t rlen;
	//FILE * fd;
	//char buf[4096];
	orp_fetch_uri_t *porp = NULL;
	orp_fetch_om_uri_t *pom;
	//nkn_uol_t attruol;

	AO_fetch_and_add1(&glob_rtspom_open_get_task);
	glob_rtspom_tot_get_called++;

	assert( p_mm_task != NULL );
	if ((p_mm_task->in_flags & MM_FLAG_NEW_REQUEST) && !p_mm_task->in_attr) {
		rtsp_om_alloc_attrbuf(p_mm_task);
	}

	DBG_LOG(MSG, MOD_RTSP, "Id=%ld Physid: %s uri=%s offset=%ld len=%ld",
		p_mm_task->get_task_id,
		p_mm_task->physid, nkn_cod_get_cnp(p_mm_task->in_uol.cod),
		p_mm_task->in_uol.offset, p_mm_task->in_uol.length);

	pom = (orp_fetch_om_uri_t *) p_mm_task->in_proto_data;
	if (pom) {
		porp = orp_look_up_physid(pom->physid);
	}
	if (porp && CHECK_PORP_FLAG(porp, PORPF_ACTIVE)) {
		pthread_mutex_lock(porp->pmutex);
		if (porp->pplayer) {
			porp->pplayer->p_mm_task = p_mm_task;
			//SET_ORP_FLAG(porp, ORPF_SEND_BUFFER);
			SET_ORP_FLAG(porp, ORPF_START_PLAY);
			SET_PLAYER_FLAG(porp->pplayer, PLAYERF_SEND_BUFFER);
		}
		pthread_mutex_unlock(porp->pmutex);
	}
	else {
		p_mm_task->err_code = NKN_OM_RTSP_ERR_UNKNOWN;
		nkn_task_set_action_and_state(p_mm_task->get_task_id, TASK_ACTION_OUTPUT,
						TASK_STATE_RUNNABLE);
	}
	return 0;
}

static int RTSP_OM_validate (MM_validate_t *pvalidate)
{
	UNUSED_ARGUMENT(pvalidate);
	return 0;
}

#define MAX_RTSP_OM_PENDING_REQUESTS         (8 * 1024)

void RTSP_OM_init( void );
void RTSP_OM_init( void )
{
	/*
	 * Disable RTSP OM for now.
	 */
	//return;

	/*
	 * register a MM provider
	 */
	MM_register_provider(RTSPMgr_provider,
			0,
			NULL,
			RTSP_OM_stat,
			RTSP_OM_get,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			RTSP_OM_validate,
			NULL,
			NULL,
			20 * 2 / 10, /* # of PUT threads */
			20 * 8 / 10, /* # of GET threads */
			MAX_RTSP_OM_PENDING_REQUESTS,
			0,
			MM_THREAD_ACTION_IMMEDIATE);

	return;
}

int RTSP_OM_get_ret_error_to_task(MM_get_resp_t *p_mm_task, rtsp_cb_t *prtsp)
{
	UNUSED_ARGUMENT(prtsp);

	glob_rtspom_err_get_task++;

	if (p_mm_task) {
		AO_fetch_and_sub1(&glob_rtspom_open_get_task);
		rtsp_om_free_attrbuf(p_mm_task);
		p_mm_task->err_code = NKN_OM_RTSP_ERR_UNKNOWN;
		nkn_task_set_action_and_state(p_mm_task->get_task_id, TASK_ACTION_OUTPUT,
						TASK_STATE_RUNNABLE);
	}
	return TRUE;
}

int RTSP_OM_get_ret_task(MM_get_resp_t *p_mm_task, char * buf, int recv_data_size, uint64_t *tot_len)
{
	int32_t i;
	int copylen;
	nkn_uol_t uol;
	nkn_uol_t attruol;
	void *ptr_vobj;
	nkn_buffer_t *attrbuf = NULL;
	nkn_attr_t *pnknattr;
	int len;
	int offset;
	int sent_len = 0;

	AO_fetch_and_sub1(&glob_rtspom_open_get_task);

	if (p_mm_task->in_attr) {
		attrbuf = p_mm_task->in_attr;
	} 
	if (attrbuf) {
		pnknattr = (nkn_attr_t *) nkn_buffer_getcontent(attrbuf);
		nkn_attr_set_streaming(pnknattr);
	}

	len = MIN(recv_data_size, p_mm_task->in_uol.length);
	p_mm_task->out_num_content_objects = len / (CM_MEM_PAGE_SIZE);
	if (len % CM_MEM_PAGE_SIZE != 0) {
		p_mm_task->out_num_content_objects++;
	}
	if (p_mm_task->in_num_content_objects < p_mm_task->out_num_content_objects)
		p_mm_task->out_num_content_objects = p_mm_task->in_num_content_objects;

	offset = p_mm_task->in_uol.offset;

	//printf( "Req off=%ld len=%ld size=%d\n",
	//	p_mm_task->in_uol.offset, p_mm_task->in_uol.length, 
	//	recv_data_size );

	for (i = 0; i < (int32_t)p_mm_task->out_num_content_objects;i++) {
		ptr_vobj = p_mm_task->in_content_array[i];

		//setup the uol
		uol.cod = p_mm_task->in_uol.cod;
		uol.offset = offset;

		/* get raw buffers from a nkn_buf_t page
		 * copying the data back.
		 */
		copylen = MIN(len, CM_MEM_PAGE_SIZE);
		uol.length = copylen;
		memcpy(nkn_buffer_getcontent(ptr_vobj),
			buf,
			copylen);
		buf += copylen;
		len -= copylen;
		offset += copylen;
		sent_len += copylen;
		nkn_buffer_setid(ptr_vobj, &uol, RTSPMgr_provider, 0);
		
	}

	/* Do setid for attrib only if setid for data buffer was done
	 */
	if (p_mm_task->in_attr && p_mm_task->out_num_content_objects) {
		attruol.cod = p_mm_task->in_uol.cod;
		attruol.offset = 0;
		attruol.length = 0;
		nkn_buffer_setid(p_mm_task->in_attr, &attruol, RTSPMgr_provider, 0);
	}
	if (recv_data_size == 0) {
		p_mm_task->err_code = NKN_OM_INV_PARTIAL_OBJ;
	}
	else {
		*tot_len += sent_len;
		p_mm_task->err_code = 0;
	}
	nkn_task_set_action_and_state(p_mm_task->get_task_id, TASK_ACTION_OUTPUT,
					TASK_STATE_RUNNABLE);
	return recv_data_size;
}


#include "mfp_live_event_handler.h"
#include "mfp_mgmt_sess_id_map.h"

extern uint64_t glob_mfp_live_num_active_sessions;
extern uint64_t  glob_mfp_live_num_pmfs_cntr;
uint32_t glob_mfp_live_stream_timeout = 30;
extern uint32_t glob_enable_stream_ha;

#include <sys/time.h>


static int32_t diffTimevalToMs(struct timeval const* from, 
	struct timeval const * val); 

#define TS_PKT_SIZE 188


int8_t mfpLiveEpollinHandler(entity_context_t* ctx) 
{

    sess_stream_id_t* id = (sess_stream_id_t*)ctx->context_data; 
    mfp_publ_t* pub_ctx = 
	live_state_cont->get_ctx(live_state_cont,
		id->sess_id);  
    int8_t* buff = NULL;
    uint32_t len_avl = 0;
    uint32_t stream_id = id->stream_id;
    pub_ctx->accum_intf[stream_id]->get_data_buf(&buff, &len_avl, id); 
    if (len_avl == 0) {
	pub_ctx->stream_parm[stream_id].stream_state = STRM_DEAD;
	ctx->disp_mngr->self_del_all_event(ctx);
	printf("Stream state dead FD : %d\n", ctx->fd);
	return -1;
    }
    if(len_avl<1316)
	assert(0);

    int32_t rc = 0;
    struct sockaddr_in from_addr;
    uint32_t addr_len = sizeof(struct sockaddr_in);
    rc = recvfrom(ctx->fd, buff, len_avl, 0,(struct sockaddr*)&from_addr,
			&addr_len);
    //	printf("received %d bytes from fd = %d\n", rc, ctx->fd);
    if (rc < 0) {
	if (errno != EINTR) {
	    ctx->disp_mngr->self_del_all_event(ctx);
	    pub_ctx->stream_parm[stream_id].stream_state = STRM_DEAD;
	    DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE, 
		       "Receive error : setting state dead FD : %d", ctx->fd);
	    return -1;
	}
	} else {
		struct in_addr* src_addr =
			&pub_ctx->stream_parm[stream_id].media_src.live_src.source_if;
		if ((src_addr->s_addr == 0) ||
				(src_addr->s_addr == from_addr.sin_addr.s_addr)) {
			//log the activity : note the event timestamp
			id->last_seen_at.tv_sec = ctx->event_time.tv_sec;
			id->last_seen_at.tv_usec = ctx->event_time.tv_usec;
			struct timeval now1, now2;
			gettimeofday(&now1, NULL);
			//printf("--\r");
			if (pub_ctx->accum_intf[stream_id]->data_in_handler(buff,
						rc, id) < 0) {
				disp_mngr->self_unset_read(ctx);
				pub_ctx->stream_parm[stream_id].stream_state = STRM_DEAD;
				DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE, 
						"Stream state dead FD : %d", ctx->fd);
				return -1;
			}
			gettimeofday(&now2, NULL);
			int32_t tmp_pd_time = diffTimevalToMs(&now2, &now1); 
			//if(tmp_pd_time > 10)
			//printf("sess %u strm %u processdata %d\n", id->sess_id, id->stream_id, tmp_pd_time);
			
		}
	}
    return 1;
}


int8_t mfpLiveEpolloutHandler(entity_context_t* ctx) 
{

    // Not handled
    return 1;
}


int8_t mfpLiveEpollerrHandler(entity_context_t* ctx) 
{

    sess_stream_id_t* id = (sess_stream_id_t*)ctx->context_data;
    disp_mngr->self_unregister(ctx);
    mfp_publ_t* pub_ctx = 
	live_state_cont->get_ctx(live_state_cont, id->sess_id); 
    ctx->disp_mngr->self_del_all_event(ctx);
    pub_ctx->stream_parm[id->stream_id].stream_state = STRM_DEAD;
    return 1;
}


int8_t mfpLiveEpollhupHandler(entity_context_t* ctx) 
{

    sess_stream_id_t* id = (sess_stream_id_t*)ctx->context_data;
    disp_mngr->self_unregister(ctx);
    mfp_publ_t* pub_ctx = 
	live_state_cont->get_ctx(live_state_cont, id->sess_id); 
    ctx->disp_mngr->self_del_all_event(ctx);
    pub_ctx->stream_parm[id->stream_id].stream_state = STRM_DEAD;
    return 1;
}


int8_t mfpLiveTimeoutHandler(entity_context_t* ctx) 
{

    //printf("Timeout Handler : %d\n", ctx->fd);
    sess_stream_id_t* id = (sess_stream_id_t*)ctx->context_data;
    uint32_t stream_id = id->stream_id;
    uint32_t tmp_sess_id = id->sess_id;
    mfp_publ_t* pub_ctx = 
	live_state_cont->acquire_ctx(live_state_cont, id->sess_id); 

    if (live_state_cont->get_flag(live_state_cont, id->sess_id) == -1) {
	pub_ctx->stream_parm[stream_id].stream_state = STRM_DEAD;
    } else { // calculate stream idle time
	struct timeval now;
	gettimeofday(&now, NULL);
	int32_t idle_time_ms = diffTimevalToMs(&now, &(id->last_seen_at));
	if (idle_time_ms > 0) {
	    uint32_t idle_time_s = ((uint32_t)(idle_time_ms))/1000;
	    if ((idle_time_s >= glob_mfp_live_stream_timeout)&& pub_ctx->stream_parm[stream_id].stream_state == STRM_ACTIVE) {
		// TIMEOUT : Mark as DEAD 
		pub_ctx->stream_parm[stream_id].stream_state = STRM_DEAD;
		DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE, 
			   "sess %u strm %u FD %d:  STRM TIMEOUT", 
			   id->sess_id, id->stream_id ,ctx->fd); 
		if(glob_enable_stream_ha){
			int8_t* buff = NULL;
			int32_t rc = 0;
			pub_ctx->accum_intf[stream_id]->data_in_handler(buff, rc, id);
		}
	    } else {
		// calculate next timeout to be scheduled
		ctx->to_be_scheduled.tv_sec = now.tv_sec +
		    (glob_mfp_live_stream_timeout - idle_time_s);
		ctx->to_be_scheduled.tv_usec = now.tv_usec;
	    }
	} else {
	    // calculate next timeout to be scheduled
	    ctx->to_be_scheduled.tv_sec = now.tv_sec +
		glob_mfp_live_stream_timeout;
	    ctx->to_be_scheduled.tv_usec = now.tv_usec;
	}
	/* Check if streams are being received and calculate the 
	   next timeout period */
    }
    if (pub_ctx->stream_parm[stream_id].stream_state == STRM_DEAD) {
	DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE, 
		   "Unregister FD : %d", ctx->fd);
	int32_t fd = ctx->fd, err = 0;

	disp_mngr->self_unregister(ctx);
	err = close(fd);
	if (err == -1) {
	    DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE,
		       "error when closing fd, errno %d\n", errno);
	}
	pub_ctx->active_fd_count--;
	if (pub_ctx->active_fd_count == 0) {
	    // last fd to unregister will have this true
	    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
		    "Session cleaned up fd(%d)", fd);
	    live_state_cont->mark_inactive_ctx(live_state_cont, tmp_sess_id);
	    if(pub_ctx->ms_parm.status == FMTR_ON){
	    AO_fetch_and_sub1(&glob_mfp_live_num_pmfs_cntr);
	    }
	    mfp_mgmt_sess_unlink((char*)pub_ctx->name, 
				 PUB_SESS_STATE_REMOVE,
				 -E_MFP_SESS_TIMEOUT);
	    live_state_cont->remove(live_state_cont,
		    tmp_sess_id);
	    glob_mfp_live_num_active_sessions--;
	}
    } else
	disp_mngr->self_schedule_timeout(ctx);
    live_state_cont->release_ctx(live_state_cont, tmp_sess_id); 

    return 1;
}


static int32_t diffTimevalToMs(struct timeval const* from, 
	struct timeval const * val) {
    //if -ve, return 0
    double d_from = from->tv_sec + ((double)from->tv_usec)/1000000;
    double d_val = val->tv_sec + ((double)val->tv_usec)/1000000;
    double diff = d_from - d_val;
    if (diff < 0)
	return 0;
    return (int32_t)(diff * 1000);
}


/*

static int32_t checkTS_StreamContinuity(int8_t const *recv_buff,
	int32_t rc, stream_param_t *stream_parm, int8_t const* sess_name);

int32_t checkTS_StreamContinuity(int8_t const *data,
	int32_t len, stream_param_t *stream_parm, int8_t const * sess_name) {

    if ((len % 188) != 0)
	return -1;
    uint32_t pos = 0;
    int32_t err = 0;

    stream_parm->apid = AUDIO_PID;
    stream_parm->vpid = VIDEO_PID;

    while (len > 0) {

	uint8_t adap_fld_ind = data[pos+3];
	adap_fld_ind = (adap_fld_ind 0x30) >> 4;
	uint8_t cont_ctr = data[pos+3];
	cont_ctr= 0x0F;
	uint16_t pid = ((data[pos+1] 0x1F) << 8) + data[pos + 2];
	//   printf("%02X %02X\n", data[pos+1], data[pos+2]);
	//   printf("pid: %u stream_parm apid: %d vpid: %d\n",
	//   pid, stream_parm->apid,stream_parm->vpid);
	if ((adap_fld_ind == 1) || (adap_fld_ind == 3)) {

	    uint16_t* last_cont_ctr = NULL;
	    if (pid == stream_parm->apid)
		last_cont_ctr =stream_parm->apid_cont_ctr;
	    else if (pid == stream_parm->vpid)
		last_cont_ctr =stream_parm->vpid_cont_ctr;
	    //else : Non Audio/Video PID

	    if (last_cont_ctr != NULL) {
		if (cont_ctr == (*last_cont_ctr + 1)%16) {
		    *last_cont_ctr = cont_ctr;
		    //printf("Current Cont Ctr: %d\n", cont_ctr);
		} else {
		    DBG_MFPLOG (sess_name, MSG, MOD_MFPLIVE,
			    "Discontinuity in TS Pkt Sequence: %d %d %d",
			    adap_fld_ind, cont_ctr, pid);
		    *last_cont_ctr = cont_ctr;
		    err = -1;
		}
	    }
	} //else //printf("Adaptation only TS Pkt: %d\n", adap_fld_ind);
	len -= 188;
	pos += 188;
    }
    return err;
}
*/



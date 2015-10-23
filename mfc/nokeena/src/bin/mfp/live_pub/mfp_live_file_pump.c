#include "mfp_live_file_pump.h"
#include "nkn_vpe_mfp_ts2mfu.h"
#include "mfp_mgmt_sess_id_map.h"

extern uint64_t glob_mfp_live_num_active_sessions;
extern uint64_t  glob_mfp_live_num_pmfs_cntr;
uint32_t glob_file_pump_delay_exec = 1000;
extern uint32_t glob_enable_stream_ha;
//global variable
pthread_t file_pump_thread;

static void register_value_db(
	uint32_t pkt_time, 
	uint32_t pid,
	pid_ppty_map_t *pid_map,
	uint32_t array_depth){

    uint32_t i;
    for(i = 0; i < array_depth; i++){
    	if((pid_map->entry_valid == 0) ||
	   (pid_map->entry_valid && pid_map->pid == pid)){
		pid_map->pid = pid;
		if(pid_map->entry_valid == 0)
 		   pid_map->min_pkt_time = pkt_time;	
		if(pkt_time < pid_map->min_pkt_time)
		   pid_map->min_pkt_time = pkt_time;	
	 	if(pkt_time > pid_map->max_pkt_time)
		   pid_map->max_pkt_time = pkt_time;
		if(pid_map->max_pkt_time){
		   pid_map->max_duration = pid_map->max_pkt_time -
				   pid_map->min_pkt_time;
		}
		pid_map->entry_valid = 1;
		break;
	}
	pid_map++;
    }
    return;
}

//-------------------------------------------------------
//calc_bitrate
//statically, estimate the bitrate of the given file
//-------------------------------------------------------
static uint32_t calc_bitrate(
	int32_t fd, 
	uint32_t inp_bitrate,
	FILE_PUMP_MODE_E fp_mode){
	
    uint32_t bitrate_est = 0;
    uint32_t file_size;
    int32_t start_pos, end_pos;

    pid_ppty_map_t *pid_map = NULL;

    uint32_t pkt_time;
    uint16_t pid;
    int32_t ret = 0;
    uint32_t bytes_read = 0;
    uint32_t duration;

    uint8_t *init_data = NULL;
    uint8_t *final_data = NULL;
    uint32_t i;
    uint32_t dts, pts;

    pid_map = (pid_ppty_map_t *)nkn_calloc_type(MAX_PID_EST,
    		sizeof(pid_ppty_map_t), mod_mfp_file_pump);
    if(pid_map == NULL){
    	//unable to allocate mem
    	ret = -1;
	goto calc_bitrate_ret;
    }
    init_data = (uint8_t *) nkn_malloc_type(EST_PKT_DEPTH * sizeof(uint8_t), 
    	mod_mfp_file_pump); 
    if(init_data == NULL){
	    //unable to allocate mem
	    ret = -1;
	    goto calc_bitrate_ret;
    }
    final_data= (uint8_t *) nkn_malloc_type(EST_PKT_DEPTH * sizeof(uint8_t), 
    	mod_mfp_file_pump); 
    if(final_data == NULL){
	    //unable to allocate mem
	    ret = -1;
	    goto calc_bitrate_ret;
    }

    //memset(pid_map, 0, MAX_PID_EST*sizeof(pid_ppty_map_t));

    start_pos = lseek(fd, 0, SEEK_CUR);

    end_pos = lseek(fd, 0, SEEK_END);
    
    if(fp_mode == TS_INPUT){
    	lseek(fd, -EST_PKT_DEPTH, SEEK_END);
	bytes_read = read(fd, final_data, EST_PKT_DEPTH);
    } else if(fp_mode == UPCP_INPUT){
    	lseek(fd, -(EST_PKT_DEPTH + NUM_UPCP_PKTS_IN_105TSPKTS*UPCP_HEADER), SEEK_END);
	for(i = 0; i < NUM_NETWORK_PKTS; i++){
		lseek(fd, UPCP_HEADER, SEEK_CUR);
		bytes_read += read(fd, 
			final_data + i * NUM_PKTS_IN_NETWORK_CHUNK * TS_PKT_SIZE,
			NUM_PKTS_IN_NETWORK_CHUNK * TS_PKT_SIZE);
	}
    }


    if(bytes_read !=EST_PKT_DEPTH){
    	//failed to read enough packets
    	ret = -1;
	goto calc_bitrate_ret;
    }
    //calc file size
    file_size = end_pos - start_pos;
    bytes_read = 0;
    //calc duration
    lseek(fd, 0, SEEK_SET);
    while(1){
    	   
	   if(fp_mode == TS_INPUT){
	   	bytes_read = read(fd, init_data, EST_PKT_DEPTH);
	   } else if(fp_mode == UPCP_INPUT){
		   for(i = 0; i < NUM_NETWORK_PKTS; i++){
			   lseek(fd, UPCP_HEADER, SEEK_CUR);
			   bytes_read += read(fd, 
				   init_data + i * NUM_PKTS_IN_NETWORK_CHUNK * TS_PKT_SIZE,
				   NUM_PKTS_IN_NETWORK_CHUNK * TS_PKT_SIZE);
		   }

	   }

	    if(bytes_read !=EST_PKT_DEPTH){
	    	//failed to read enough packets
	    	ret = -1;
		break;
		//goto calc_bitrate_ret;
	    }
	    for (i = 0; i < MAX_PKTS_REQD_FOR_ESTIMATE; i++){
	    	pts = 0;
		dts = 0;
	    	mfu_converter_get_timestamp(
			(uint8_t *)(init_data+(i * BYTES_IN_PACKET)),NULL,&pid, 
			&pts, &dts, NULL);
		pkt_time = dts;
		if(!dts)
			pkt_time = pts;
		if(pkt_time){
			register_value_db(pkt_time, (uint32_t)pid, pid_map, MAX_PID_EST);
		}
	    }
	    if(pid_map->entry_valid && (pid_map + 1)->entry_valid)
	    	break;
    }
    if(ret == -1){
    	goto calc_bitrate_ret;
    }
    uint32_t offset = 0;
    for (i = 0; i < MAX_PKTS_REQD_FOR_ESTIMATE; i++){
	pts = 0;
	dts = 0;
	uint32_t tmp;
	while(1){
		tmp = *(final_data+(i * BYTES_IN_PACKET)+offset);
		if( tmp == 0x47)
			break;
		offset += 1;
	}
    	mfu_converter_get_timestamp(
		(uint8_t *)(final_data+(i * BYTES_IN_PACKET)+offset),NULL,&pid,
		&pts, &dts, NULL);
	pkt_time = dts;
	if(!dts)
		pkt_time = pts;	
	if(pkt_time){
		register_value_db(pkt_time, (uint32_t)pid, pid_map, MAX_PID_EST);
	}
    }
    duration = 0;
    
    //calc the max duration among the diff PIDs
    pid_ppty_map_t *l_pid_map = pid_map;
    for(i = 0; i < MAX_PID_EST; i++){
    	if(l_pid_map->entry_valid){
		if(l_pid_map->max_duration > duration)
			duration = l_pid_map->max_duration;
    	}
	l_pid_map++;
    }
    if(!duration){
    	//unable to find duration
    	ret = -1;
	goto calc_bitrate_ret;    	
    }
    
    //duration is in milli seconds; divided by 90 to move to ms
    duration = duration / 90;

    //bitrate is in kbps    
    bitrate_est = (uint32_t) 
    	(((uint64_t)((uint64_t)file_size * NUM_BITS_IN_BYTES * MS_IN_SEC))/ (uint64_t)(duration * BITS_PER_Kb));

calc_bitrate_ret:
    if(ret == -1){
    	//on failure, assign the user input bitrate
    	 bitrate_est = inp_bitrate;	 
    } 
    if(pid_map != NULL){
    	free(pid_map);
	pid_map = NULL;
    }
    if(init_data != NULL){
    	free(init_data);
	init_data = NULL;	
    }
    if(final_data != NULL){
    	free(final_data);
	final_data = NULL;	
    }
    
    //reset the fd to start of file
    lseek(fd, 0, SEEK_SET);
    return (bitrate_est);

}



//-------------------------------------------------------
//delete sched entry
//rearrange the linked list
//-------------------------------------------------------
static sched_entry_t * delete_sched_entry(
	sched_entry_t *ctxt,
	file_pump_ctxt_t *fp_ctxt){

    sched_entry_t *prev, *ct, *ret;
    ct = ctxt;
    ret = ctxt->next;
    prev = NULL;
    DBG_MFPLOG(ctxt->pub_ctx->name, MSG, MOD_MFPLIVE, "delete filepump sched entry %p", ctxt);
    //this list is in circular loop, hence possible to finite the previous node to 
    //the given node
    while(1){
	if(ct->next == ctxt){
	    prev = ct;
	    break;
	}
	ct = ct->next;
    }

    //remove the node
    close(ctxt->fd);
    prev->next = ctxt->next; 
    fp_ctxt->rrsched_entry = ctxt->next;
    fp_ctxt->rrsched_entry = ctxt->next;
    if(ctxt == fp_ctxt->origin_rrsched_entry){
    	fp_ctxt->origin_rrsched_entry = prev;
	if(prev == ctxt){
	    prev->fd = -1;
	    ret = NULL;
	}
    }
    if(ctxt->next == ctxt){
    	//sleep(20);
    	//clearing the session
    	sleep(5);
	DBG_MFPLOG (ctxt->pub_ctx->name, MSG, MOD_MFPLIVE,
		    "Session cleaned up fd(%d)", ctxt->fd);
	if(ctxt->pub_ctx->ms_parm.status == FMTR_ON){
	    AO_fetch_and_sub1(&glob_mfp_live_num_pmfs_cntr);
	}
	mfp_mgmt_sess_unlink((char*)ctxt->pub_ctx->name, 
		 PUB_SESS_STATE_REMOVE,
		 -E_MFP_SESS_TIMEOUT);
	live_state_cont->remove(live_state_cont,
	    ctxt->id->sess_id);
	glob_mfp_live_num_active_sessions--;
    }
    
    if(ret != NULL)
    free(ctxt);
    
    return ret;

}

//-------------------------------------------------------
//file read handler 
//input sched entry context
//get fd
//get pub_ctxt
//get_data
//process_data
// return eSuccess
//-------------------------------------------------------
static file_pump_err_t file_read_handler(
	sched_entry_t *ctxt){

    int32_t fd = ctxt->fd;
    mfp_publ_t *pub_ctx = ctxt->pub_ctx;
    sess_stream_id_t* id = ctxt->id;
    uint32_t num_bytes_to_read = NUM_PKTS_IN_NETWORK_CHUNK * BYTES_IN_PACKET;
    //if(ctxt->num_pkts_processed % 100 < 7)
    //DBG_MFPLOG("FilePump", MSG, MOD_MFPLIVE, "file_read_handler sched ctxt %x, fd %x, pkts proc %d", ctxt, fd, ctxt->num_pkts_processed);

    int8_t recv_buff[1500];
    int8_t* buff = NULL;
    uint32_t len_avl = 0;
    uint32_t stream_id = id->stream_id;
    pub_ctx->accum_intf[stream_id]->get_data_buf(&buff, &len_avl, id); 
    if (len_avl == 0) {
	pub_ctx->stream_parm[stream_id].stream_state = STRM_DEAD;
	DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE,"Stream state dead FD : %d", fd);
	return eUnknownError;	
    }
    int32_t rc = 0, ret = 0;
    
    if(ctxt->fp_mode == TS_INPUT){
	    rc = read(fd, buff, num_bytes_to_read);
    } else if(ctxt->fp_mode == UPCP_INPUT){
    	    ret = read(fd, recv_buff, UPCP_HEADER);
    	    rc = read(fd, buff, num_bytes_to_read);
    }
	

    if (rc == 0) {
	DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE, "EOF fd %x, pkts proc %d", fd, ctxt->num_pkts_processed);
	//pub_ctx->stream_parm[stream_id].stream_state = STRM_DEAD;
	//DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,"Stream state dead FD : %d", ctxt->fd);
	//if(glob_enable_stream_ha){
	//pub_ctx->accum_intf[stream_id]->data_in_handler(buff, rc, id);
	//}
	return eEOF;

    } else {

        ctxt->num_pkts_processed += NUM_PKTS_IN_NETWORK_CHUNK;
	
	if (pub_ctx->accum_intf[stream_id]->data_in_handler(buff, rc, id) < 0) {
	    pub_ctx->stream_parm[stream_id].stream_state = STRM_DEAD;
	    DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE,"Stream state dead FD : %d", ctxt->fd);
	    return eUnknownError;
	}
    }
    return eSuccess;
}


//-------------------------------------------------------
//register with file pump
//input fd and pub_ctxt
//pthread_cond_signal 
//int pthread_cond_signal(pthread_cond_t *cond);
//the condition variable
//-------------------------------------------------------
static file_pump_err_t register_with_file_pump(
	file_pump_ctxt_t *fp_ctxt,
	int32_t fd,
	uint32_t bit_rate,
	FILE_PUMP_MODE_E fp_mode,
	mfp_publ_t *pub_ctx,
	sess_stream_id_t* id){

    DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE, "register_with_file_pump; fd %x", fd);

    if(fp_ctxt->rrsched_entry->fd != -1){
	sched_entry_t *new_sched_entry = (sched_entry_t *)nkn_calloc_type
	    (1, sizeof(sched_entry_t), mod_mfp_file_pump);
	if(new_sched_entry == NULL){
	   DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE,"failed to create round robin scheduler entry");
	    return eNoMem;
	}
	new_sched_entry->fd = fd;
	new_sched_entry->pub_ctx = pub_ctx;
	new_sched_entry->id = id;
	bit_rate = calc_bitrate(fd, bit_rate, fp_mode);
	new_sched_entry->bitrate = bit_rate;
	new_sched_entry->fp_mode = fp_mode;
	new_sched_entry->num_pkts_per_second = 
		(uint32_t) (bit_rate * BITS_PER_Kb)/(BYTES_IN_PACKET * NUM_BITS_IN_BYTES);
	new_sched_entry->sched_weight = 
		(uint32_t)new_sched_entry->num_pkts_per_second/NUM_PKTS_IN_NETWORK_CHUNK;
	new_sched_entry->next = fp_ctxt->rrsched_entry->next;	
	//update ct entry
	fp_ctxt->rrsched_entry->next = new_sched_entry;	
	//move to next entry
	fp_ctxt->rrsched_entry = fp_ctxt->rrsched_entry->next;

    } else {

	fp_ctxt->rrsched_entry->fd = fd;
	fp_ctxt->rrsched_entry->pub_ctx = pub_ctx;
	fp_ctxt->rrsched_entry->id = id;
	bit_rate = calc_bitrate(fd, bit_rate, fp_mode);
	fp_ctxt->rrsched_entry->bitrate = bit_rate;
	fp_ctxt->rrsched_entry->fp_mode = fp_mode;
	fp_ctxt->rrsched_entry->num_pkts_per_second = 
		(uint32_t) (bit_rate * BITS_PER_Kb)/(BYTES_IN_PACKET * NUM_BITS_IN_BYTES);
	fp_ctxt->rrsched_entry->sched_weight = 
		(uint32_t)fp_ctxt->rrsched_entry->num_pkts_per_second/NUM_PKTS_IN_NETWORK_CHUNK;	
	//point it back to origin
	fp_ctxt->rrsched_entry->next = fp_ctxt->origin_rrsched_entry;
    }
    DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,"wakeup filepump");
    pthread_mutex_lock(&fp_ctxt->file_pump_mutex);
    pthread_cond_signal(&fp_ctxt->file_pump_cond_var);
    pthread_mutex_unlock(&fp_ctxt->file_pump_mutex);

    return eSuccess;

}



//-------------------------------------------------------
//file pump on mode
//while (1) {
//round robin of available fds
//call file read handler
//sleep(n)
//if eof all fds are reached, then go back to sig_wait
//}
//-------------------------------------------------------
static file_pump_err_t file_dispatcher(
	file_pump_ctxt_t *fp_ctxt){

    file_pump_err_t ret = eSuccess; 
    sched_entry_t *ctxt = fp_ctxt->origin_rrsched_entry;
    uint32_t sched_weight = 0;
    
    while(1){
	ret = file_read_handler(ctxt);
	sched_weight++;
	usleep(glob_file_pump_delay_exec);
	if(ret == eEOF || ret == eUnknownError){
	    DBG_MFPLOG(ctxt->pub_ctx->name, MSG, MOD_MFPLIVE,"delete_sched_entry ctxt %p", ctxt);		
	    ctxt = delete_sched_entry(ctxt, fp_ctxt);
	    sched_weight = 0;
	    if(ctxt == NULL){
		DBG_MFPLOG("FilePump", MSG, MOD_MFPLIVE,"no more fds, sched goes to wait");
	    	break;
	    }
	} else {
	    if(sched_weight > ctxt->sched_weight){
	    	//switch to next sched entry only when 
	    	// designated sched weightage is reached
	    	ctxt = ctxt->next;
		sched_weight = 0;
	    }
	}
    }
    return ret;
}

//-------------------------------------------------------
//delete file pump
//input file pump context
//-------------------------------------------------------
static file_pump_err_t delete_file_pump(file_pump_ctxt_t *fp_ctxt){

    DBG_MFPLOG("FilePump", MSG, MOD_MFPLIVE,"delete_file_pump %p", fp_ctxt);	    
    pthread_cond_destroy(&fp_ctxt->file_pump_cond_var);
    pthread_condattr_destroy(&fp_ctxt->file_pump_cont_attr);
    pthread_mutex_destroy(&fp_ctxt->file_pump_mutex);
    pthread_mutexattr_destroy(&fp_ctxt->file_pump_mutex_attr);
    free(fp_ctxt);
    return eSuccess;
}


//-------------------------------------------------------
//start file pump
//if no fds, pthread_cond_wait 
//gets processing signal if a fd is registered ,call file_dispatcher
//-------------------------------------------------------
static void *start_file_pump(void *args){
    file_pump_ctxt_t *fp_ctxt = (file_pump_ctxt_t *)args;
    while(1){
	pthread_mutex_lock(&fp_ctxt->file_pump_mutex);
	if(fp_ctxt->num_fds == 0){
	    DBG_MFPLOG("FilePump", MSG, MOD_MFPLIVE,"File Pump waiting for fd");
	    pthread_cond_wait(&fp_ctxt->file_pump_cond_var, &fp_ctxt->file_pump_mutex);
	}
	pthread_mutex_unlock(&fp_ctxt->file_pump_mutex);
	DBG_MFPLOG("FilePump", MSG, MOD_MFPLIVE,"got fd, going to dispatcher");	
	file_dispatcher(fp_ctxt);
    }
    if(delete_file_pump(fp_ctxt) != eSuccess){
	DBG_MFPLOG("FilePump", ERROR, MOD_MFPLIVE,"error deleting file pump thread");
	return NULL;
    }
    pthread_exit(NULL);
    return NULL;
}


//-------------------------------------------------------
//new file pump 
//create and return context
//return -1 if error
//-------------------------------------------------------
file_pump_ctxt_t *new_file_pump_context(void){
    file_pump_ctxt_t *fp_ctxt;
    fp_ctxt = (file_pump_ctxt_t *)nkn_calloc_type(1, sizeof(file_pump_ctxt_t), mod_mfp_file_pump);
    if(fp_ctxt == NULL){
	DBG_MFPLOG("FilePump", ERROR, MOD_MFPLIVE,"failed to create file pump context");
	return NULL;
    }
    fp_ctxt->num_fds = 0;
    fp_ctxt->start = &start_file_pump;
    fp_ctxt->insert_entry = &register_with_file_pump;

    fp_ctxt->rrsched_entry = (sched_entry_t *)nkn_calloc_type(1, sizeof(sched_entry_t), mod_mfp_file_pump);
    if(fp_ctxt->rrsched_entry ==NULL){
	DBG_MFPLOG("FilePump", ERROR, MOD_MFPLIVE,"failed to create round robin scheduler entry");
	return NULL;
    }
    fp_ctxt->rrsched_entry->fd = -1;
    fp_ctxt->origin_rrsched_entry = fp_ctxt->rrsched_entry; //save the origin

    pthread_mutexattr_init(&fp_ctxt->file_pump_mutex_attr);
    pthread_mutex_init(&fp_ctxt->file_pump_mutex, &fp_ctxt->file_pump_mutex_attr);
    pthread_condattr_init(&fp_ctxt->file_pump_cont_attr);
    pthread_cond_init(&fp_ctxt->file_pump_cond_var, &fp_ctxt->file_pump_cont_attr);

    return (fp_ctxt);
}



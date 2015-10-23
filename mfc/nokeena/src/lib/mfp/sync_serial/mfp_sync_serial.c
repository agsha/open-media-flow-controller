#include "mfp_sync_serial.h"

#define MFP_TASK_MICROSEC 15000

#define LACK_SYNC_NO_SEQ_INCR

static void serialIn(void *task);

static void synchronize(void* task);

static void serialOut(void *task);

static void initSyncSerializer(sync_serial_t* sync_ser); 

static void errorHandler(sync_serial_t* sync_ser);

static void stream_high_availability_error_hdlr(
	sync_serial_t* sync_ser,
	uint32_t group_id);


static void destroySyncSerializer(sync_serial_t* sync_ser);

static void* memAlloc(uint32_t count, uint32_t size);

static void clearSyncSerQueue(sync_serial_t* sync_ser); 

sync_serial_t* createSyncSerializer(uint32_t sync_member_count,
		uint32_t queue_size, schedule_hdlr_fptr schedule_hdlr,
		uint32_t ser_sched_out_flag, mem_alloc_hdlr_fptr mem_alloc_hdlr,
		task_hdlr_fptr error_cb_hdlr, task_hdlr_fptr resync_cb_hdlr,
		void* cb_ctx, int8_t* name) {

	//printf("createSyncSerializer : %u %u\n",sync_member_count, queue_size);
	sync_serial_t* sync_ser = NULL;
	if (mem_alloc_hdlr == NULL)
		mem_alloc_hdlr = memAlloc; 
	sync_ser = mem_alloc_hdlr(1, sizeof(sync_serial_t));
	if (sync_ser == NULL)
		return NULL;
	if ((sync_member_count == 0) || (queue_size == 0))
		goto clean_return;
	initSyncSerializer(sync_ser);
	sync_ser->sync_member_count = sync_member_count;
	sync_ser->ser_sched_out_flag = ser_sched_out_flag;
	sync_ser->total_queue_size = sync_member_count * queue_size;
	sync_ser->ser_in_resched_ctr = 0;
	sync_ser->ser_out_resched_ctr = 0;
	sync_ser->schedule_hdlr = schedule_hdlr;
	sync_ser->mem_alloc_hdlr = mem_alloc_hdlr;
	sync_ser->seq_num = mem_alloc_hdlr(sync_ser->total_queue_size,
			sizeof(uint32_t));
	if (sync_ser->seq_num == NULL)
		goto clean_return;
	sync_ser->seq_ctx = mem_alloc_hdlr(sync_ser->total_queue_size,
			sizeof(void*));
	if (sync_ser->seq_ctx == NULL)
		goto clean_return;
	sync_ser->seq_ctx_hdlr =
		mem_alloc_hdlr(sync_ser->total_queue_size, sizeof(task_hdlr_fptr));
	if (sync_ser->seq_ctx_hdlr == NULL)
		goto clean_return;
	sync_ser->seq_ctx_delete_hdlr =
		mem_alloc_hdlr(sync_ser->total_queue_size, sizeof(task_hdlr_fptr));
	if (sync_ser->seq_ctx_delete_hdlr == NULL)
		goto clean_return;

	sync_ser->error_cb_hdlr = error_cb_hdlr;
	sync_ser->resync_cb_hdlr = resync_cb_hdlr;
	sync_ser->cb_ctx = cb_ctx;

	if (pthread_mutex_init(&sync_ser->lock, NULL) != 0)
		goto clean_return;

	sync_ser->serial_in_hdlr = serialIn;
	sync_ser->sync_hdlr = synchronize;
	sync_ser->serial_out_hdlr = serialOut;
	sync_ser->error_hdlr = errorHandler;
	sync_ser->delt = destroySyncSerializer;
	sync_ser->streamha_error_hdlr = stream_high_availability_error_hdlr;

	sync_ser->strm_state = mem_alloc_hdlr(sync_ser->sync_member_count,
			sizeof(uint32_t));
	if (sync_ser->strm_state == NULL)
		goto clean_return;
	for(uint32_t strm_id = 0; strm_id < sync_ser->sync_member_count; strm_id++){
		sync_ser->strm_state[strm_id] = STRM_ACTIVE;
	}
		
	sync_ser->name = name;
	return sync_ser;
clean_return:
	sync_ser->delt(sync_ser);
	return NULL;
}


static void initSyncSerializer(sync_serial_t* sync_ser) {

	sync_ser->seq_num = NULL;
	sync_ser->seq_ctx = NULL;
	sync_ser->seq_ctx_hdlr = NULL;
	sync_ser->seq_ctx_delete_hdlr = NULL;
	sync_ser->mem_alloc_hdlr = memAlloc;
	sync_ser->schedule_hdlr = NULL;

	sync_ser->sync_member_count = 0;
	sync_ser->total_queue_size = 0;

	sync_ser->ser_in_exp_seq_num = 0;

	sync_ser->sync_seq_num_base = 0;
	sync_ser->sync_exp_seq_num = 0;
	sync_ser->sync_curr_seq_num = 0;

        sync_ser->ser_sched_out_flag = 0;
	sync_ser->ser_out_exp_seq_num = 0;
	sync_ser->ser_out_curr_seq_num = 0;

	sync_ser->err_flag = 0;
}


static void serialIn(void* task) {

	ser_task_t* ser_task = (ser_task_t*)task;
	sync_serial_t* sync_ser = ser_task->sync_ser;
	pthread_mutex_lock(&sync_ser->lock);
	if (sync_ser->err_flag) {
	    pthread_mutex_unlock(&sync_ser->lock);
		return;
	}
#if 0	
	DBG_MFPLOG (sync_ser->name, MSG, MOD_MFPLIVE, "serialIn Exp %u Got %u",
		sync_ser->ser_in_exp_seq_num,
		ser_task->seq_num);
#endif
	if (sync_ser->ser_in_exp_seq_num == ser_task->seq_num) {
		//printf("Serial IN\n");
		ser_task->context_hdlr(ser_task->ctx);
		sync_ser->ser_in_exp_seq_num++;
		free(ser_task);
	} else {
#if 0//def SYNCSER_LOG_ENABLE
		DBG_MFPLOG (sync_ser->name, ERROR, MOD_MFPLIVE,
		"Serial IN : Reschedule: %lu sync_ser->ser_in_exp_seq_num: %u \
		ser_task->seq_num: %u\n", sync_ser->ser_in_resched_ctr,
		sync_ser->ser_in_exp_seq_num, ser_task->seq_num);
#endif
		sync_ser->ser_in_resched_ctr++;
		sync_ser->schedule_hdlr(MFP_TASK_MICROSEC, sync_ser->serial_in_hdlr,
				(void*)ser_task);
	}
	pthread_mutex_unlock(&sync_ser->lock);
}


static void synchronize(void* task) {

	sync_task_t* sync_task = (sync_task_t*)task;
	sync_serial_t* sync_ser = sync_task->sync_ser;
	uint32_t group_id = sync_task->group_id;
	uint32_t seq_num = sync_task->seq_num;
	if(sync_ser->strm_state[group_id] == STRM_DEAD)
		seq_num = sync_ser->sync_seq_num_base + group_id;
	/*
	if (sync_task->ctx == NULL)
		printf("seqnum: %u groupid: %u NULL\n", seq_num, group_id);
	else
		printf("seqnum: %u groupid: %u\n", seq_num, group_id);*/

	
	if (seq_num < sync_ser->sync_seq_num_base) {
		/*
		printf("Receiving slot idx outside (<) slot_idx %u : %u\n",
				seq_num, sync_ser->sync_seq_num_base);*/
		DBG_MFPLOG (sync_ser->name, ERROR, MOD_MFPLIVE,
				"Receiving slot idx outside (<) slot_idx %u : %u\n",
				seq_num, sync_ser->sync_seq_num_base);
		sync_ser->err_flag = 1;
		clearSyncSerQueue(sync_ser);
		sync_task->context_delete_hdlr(sync_task->ctx);
		free(sync_task);
		sync_ser->error_cb_hdlr(sync_ser->cb_ctx);
		return;
	}
	uint32_t count = (seq_num * sync_ser->sync_member_count) + group_id;
	uint32_t slot_idx = count - (sync_ser->sync_seq_num_base *
			sync_ser->sync_member_count);
	if (slot_idx >= sync_ser->total_queue_size) {
		/*
		   printf("Receiving slot idx outside (>) slot_idx %u : %u\n",
		   seq_num, sync_ser->sync_seq_num_base);*/
		DBG_MFPLOG (sync_ser->name, ERROR, MOD_MFPLIVE,
				"Receiving slot idx outside (>) slot_idx %u : %u\n",
				seq_num, sync_ser->sync_seq_num_base);
		sync_ser->err_flag = 1;
		clearSyncSerQueue(sync_ser);
		sync_ser->error_cb_hdlr(sync_ser->cb_ctx);
		goto clean_return;
	}
	sync_ser->seq_num[slot_idx] = 1;
	sync_ser->seq_ctx[slot_idx] = sync_task->ctx;
	sync_ser->seq_ctx_hdlr[slot_idx] = sync_task->context_hdlr;
	sync_ser->seq_ctx_delete_hdlr[slot_idx] = sync_task->context_delete_hdlr;
	while (1) {

	    uint32_t i = 0, members_ready = 0, sync_lost_flag = 0, dead_members_ready=0;
	    for (; i < sync_ser->sync_member_count; i++) {
		if (sync_ser->seq_num[i] == 1) {
		    if (sync_ser->seq_ctx[i] == NULL) {
			sync_lost_flag =1;
			break;
		    } else
			members_ready++;
		}else if(sync_ser->strm_state[i] == STRM_DEAD
			 /*&& sync_ser->sync_member_count > 1*/){
		    members_ready++;
		    dead_members_ready++;
		} 
	    }
	    if(dead_members_ready ==  sync_ser->sync_member_count)
		break;
#if 0		
		DBG_MFPLOG (sync_ser->name, MSG, MOD_MFPLIVE,
			"Members ready : %u Sync Lost Flag: %u idx: %u\n",
				members_ready, sync_lost_flag, i);
#endif
		if (members_ready == sync_ser->sync_member_count) {
			for (i = 0; i < sync_ser->total_queue_size; i++) {
				if (i < sync_ser->sync_member_count) {
				    if(sync_ser->seq_num[i] == 1){
					DBG_MFPLOG (sync_ser->name, MSG, MOD_MFPLIVE,
						    "syncser processing stream %u seqnum %u", 
						    i, sync_ser->sync_seq_num_base);
					
					if (sync_ser->ser_sched_out_flag == 1) {
					    ser_task_t* ser_task = createSerialTask(sync_ser, 
										    sync_ser->ser_out_curr_seq_num,
										    sync_ser->seq_ctx[i],
										    sync_ser->seq_ctx_hdlr[i]);
					    sync_ser->ser_out_curr_seq_num++;
					    sync_ser->schedule_hdlr(MFP_TASK_MICROSEC,
								    sync_ser->serial_out_hdlr, ser_task);
					} else {
					    //Inline
					    sync_ser->seq_ctx_hdlr[i](sync_ser->seq_ctx[i]); 
					    sync_ser->sync_curr_seq_num++;
					}
				    }
				} else {
				    sync_ser->seq_num[i - sync_ser->sync_member_count] = 
					sync_ser->seq_num[i];
				    sync_ser->seq_ctx[i - sync_ser->sync_member_count] = 
					sync_ser->seq_ctx[i];
				    sync_ser->seq_ctx_hdlr[i - sync_ser->sync_member_count]
						= sync_ser->seq_ctx_hdlr[i];
				    sync_ser->seq_ctx_delete_hdlr[i - sync_ser->
								  sync_member_count]=sync_ser->seq_ctx_delete_hdlr[i];
				}
				sync_ser->seq_num[i] = 0;
				sync_ser->seq_ctx[i] = NULL;
				sync_ser->seq_ctx_hdlr[i] = NULL;
				sync_ser->seq_ctx_delete_hdlr[i] = NULL;
			}
			sync_ser->sync_seq_num_base++;
		} else if (sync_lost_flag == 1) {
			for (i = 0; i < sync_ser->total_queue_size; i++) {
				sync_ser->seq_num[i] = 0;
				if (sync_ser->seq_ctx[i] != NULL)
					sync_ser->seq_ctx_delete_hdlr[i](sync_ser->seq_ctx[i]);
				sync_ser->seq_ctx[i] = NULL;
				sync_ser->seq_ctx_delete_hdlr[i] = NULL;
			}
#ifndef LACK_SYNC_NO_SEQ_INCR
			sync_ser->sync_seq_num_base++;
#endif
			if(sync_ser->resync_cb_hdlr!=NULL)
			    sync_ser->resync_cb_hdlr(sync_ser->cb_ctx);
			break;
		} else {
			break;
		}
	}
clean_return:
	free(sync_task);
}


static void serialOut(void* task) {

	ser_task_t* ser_task = (ser_task_t*)task;
	sync_serial_t* sync_ser = ser_task->sync_ser;
	pthread_mutex_lock(&sync_ser->lock);
	if (sync_ser->ser_out_exp_seq_num == ser_task->seq_num) {
		/*printf("Serial OUT\n");
		printf("Task Seq Num: %u sync_ser->ser_out_curr_seq_num: %u \
				sync_ser->ser_out_exp_seq_num: %u\n",
				ser_task->seq_num,sync_ser->ser_out_curr_seq_num,
				sync_ser->ser_out_exp_seq_num);*/
		ser_task->context_hdlr(ser_task->ctx);
		sync_ser->ser_out_exp_seq_num++;
		free(ser_task);
	} else {
#if 1 //def SYNCSER_LOG_ENABLE
		DBG_MFPLOG (sync_ser->name, ERROR, MOD_MFPLIVE,
				"Serial OUT : Reschedule: %lu Task Seq Num: %u \
				sync_ser->ser_out_curr_seq_num: %u \
				sync_ser->ser_out_exp_seq_num: %u\n",
				sync_ser->ser_out_resched_ctr,
				ser_task->seq_num, sync_ser->ser_out_curr_seq_num,
				sync_ser->ser_out_exp_seq_num);
#endif
		sync_ser->ser_out_resched_ctr++;
		sync_ser->schedule_hdlr(MFP_TASK_MICROSEC, sync_ser->serial_out_hdlr,
				(void*)ser_task);
	}
	pthread_mutex_unlock(&sync_ser->lock);
}


static void errorHandler(sync_serial_t* sync_ser) {

	pthread_mutex_lock(&sync_ser->lock);
	sync_ser->err_flag = 1;
	clearSyncSerQueue(sync_ser);
	pthread_mutex_unlock(&sync_ser->lock);
}

static void stream_high_availability_error_hdlr(
	sync_serial_t* sync_ser,
	uint32_t group_id){
	DBG_MFPLOG (sync_ser->name, ERROR, MOD_MFPLIVE,
				"stream_ha_error_hdlr for stream id %u\n",
				group_id);
	sync_ser->strm_state[group_id] = STRM_DEAD;
	return;
}


static void destroySyncSerializer(sync_serial_t* sync_ser) {

	if (sync_ser->seq_num != NULL)
		free(sync_ser->seq_num);
	if (sync_ser->seq_ctx != NULL) {
		uint32_t i = 0;
		for (; i < sync_ser->total_queue_size; i++)
		if (sync_ser->seq_ctx[i] != NULL) {
			if (sync_ser->seq_ctx_delete_hdlr != NULL)
				sync_ser->seq_ctx_delete_hdlr[i](sync_ser->seq_ctx[i]);
			else
				free(sync_ser->seq_ctx[i]);
		}
		free(sync_ser->seq_ctx);
	}
	if (sync_ser->seq_ctx_hdlr != NULL)
		free(sync_ser->seq_ctx_hdlr);
	if (sync_ser->seq_ctx_delete_hdlr != NULL)
		free(sync_ser->seq_ctx_delete_hdlr);
	if(sync_ser->strm_state != NULL){
		free(sync_ser->strm_state);
		sync_ser->strm_state = NULL;
        }
	pthread_mutex_destroy(&sync_ser->lock);
	free(sync_ser);
}


static void clearSyncSerQueue(sync_serial_t* sync_ser) {

	uint32_t i = 0;
	for (; i < sync_ser->total_queue_size; i++)
		if (sync_ser->seq_ctx[i] != NULL) {
			if (sync_ser->seq_ctx_delete_hdlr != NULL)
				sync_ser->seq_ctx_delete_hdlr[i](sync_ser->seq_ctx[i]);
			else
				free(sync_ser->seq_ctx[i]);
			sync_ser->seq_ctx[i] = NULL;
		}
}


ser_task_t* createSerialTask(sync_serial_t* sync_ser, uint32_t seq_num,
		        void* ctx, context_hdlr_fptr context_hdlr) {

	ser_task_t* task = sync_ser->mem_alloc_hdlr(1, sizeof(ser_task_t));
	if (task == NULL)
		return NULL;
	task->sync_ser = sync_ser;
	task->seq_num = seq_num;
	task->ctx = ctx;
	task->context_hdlr = context_hdlr;
	return task;
}


sync_task_t* createSyncTask(sync_serial_t* sync_ser, uint32_t group_id,
		        uint32_t seq_num, void* ctx, context_hdlr_fptr context_hdlr,
				        context_delete_hdlr_fptr context_delete_hdlr) {

	sync_task_t* task = sync_ser->mem_alloc_hdlr(1, sizeof(sync_task_t));
	if (task == NULL)
		return NULL;
	task->sync_ser = sync_ser;
	task->group_id = group_id;
	task->seq_num = seq_num;
	task->ctx = ctx;
	task->context_hdlr = context_hdlr;
	task->context_delete_hdlr = context_delete_hdlr;
	return task;
}


static void* memAlloc(uint32_t count, uint32_t size) {

	return calloc(count, size);
}


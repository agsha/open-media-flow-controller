#ifndef MFP_SYNC_SERIAL_H
#define MFP_SYNC_SERIAL_H

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include "nkn_vpe_ha_state.h"

#define SYNCSER_LOG_ENABLE

#ifdef SYNCSER_LOG_ENABLE
#define SYNCSER_MOD_NAME "SYNC_SER"
#include "nkn_debug.h"
#endif


#define SYNCSER_CUST_ASSERT
#ifdef SYNCSER_CUST_ASSERT
#include "nkn_assert.h"
#endif

typedef void (*task_hdlr_fptr)(void* clientData);
typedef uint64_t (*schedule_hdlr_fptr)(int64_t, task_hdlr_fptr, void*);
typedef void (*context_hdlr_fptr)(void*);
typedef void (*context_delete_hdlr_fptr)(void*);

typedef void* (*mem_alloc_hdlr_fptr)(uint32_t count, uint32_t size);

struct sync_serial;
typedef int32_t (*process_fptr)(struct sync_serial*, uint32_t, uint32_t, void*);
typedef void (*error_hdlr_fptr)(struct sync_serial*);
typedef void (*delete_sync_serial_fptr)(struct sync_serial*);
typedef void (*streamha_error_hdlr_fptr)(struct sync_serial*,uint32_t group_id);


typedef struct sync_serial {

	uint32_t* seq_num;
	void** seq_ctx;
	task_hdlr_fptr* seq_ctx_hdlr;
	task_hdlr_fptr* seq_ctx_delete_hdlr;

	uint32_t sync_member_count;
	uint32_t total_queue_size;

	uint32_t sync_seq_num_base;
	uint32_t sync_exp_seq_num;
	uint32_t sync_curr_seq_num;

	uint32_t ser_in_exp_seq_num;

	uint32_t ser_sched_out_flag;
	uint32_t ser_out_exp_seq_num;
	uint32_t ser_out_curr_seq_num;

	int32_t err_flag;
	void* cb_ctx;

	// debug and perf measure parms
	uint64_t ser_in_resched_ctr;
	uint64_t ser_out_resched_ctr;

	schedule_hdlr_fptr schedule_hdlr;

	mem_alloc_hdlr_fptr mem_alloc_hdlr;

	pthread_mutex_t lock;

	task_hdlr_fptr serial_in_hdlr;
	task_hdlr_fptr sync_hdlr;
	task_hdlr_fptr serial_out_hdlr;
	task_hdlr_fptr error_cb_hdlr;
    	error_hdlr_fptr error_hdlr;
    	task_hdlr_fptr resync_cb_hdlr;
	delete_sync_serial_fptr delt;
	streamha_error_hdlr_fptr streamha_error_hdlr;
	
	stream_state_e  *strm_state;

	int8_t* name;
} sync_serial_t;


sync_serial_t* createSyncSerializer(uint32_t sync_member_count,
		uint32_t queue_size, schedule_hdlr_fptr schedule_hdlr,
		uint32_t ser_sched_out_flag, mem_alloc_hdlr_fptr mem_alloc_hdlr,
		task_hdlr_fptr error_cb_hdlr, task_hdlr_fptr resync_cb_hdlr,
		void* cb_ctx, int8_t *name);


typedef struct serial_task {

	sync_serial_t* sync_ser;
	uint32_t seq_num;
	void* ctx;
	context_hdlr_fptr context_hdlr;
} ser_task_t;


typedef struct sync_task {

	sync_serial_t* sync_ser;
	uint32_t group_id;
	uint32_t seq_num;
	void* ctx;
	context_hdlr_fptr context_hdlr;
	context_delete_hdlr_fptr context_delete_hdlr;
} sync_task_t;


ser_task_t* createSerialTask(sync_serial_t* sync_ser, uint32_t seq_num,
		void* ctx, context_hdlr_fptr context_hdlr);


sync_task_t* createSyncTask(sync_serial_t* sync_ser, uint32_t group_id,
		uint32_t seq_num, void* ctx, context_hdlr_fptr context_hdlr,
		context_delete_hdlr_fptr context_delete_hdlr);


#endif


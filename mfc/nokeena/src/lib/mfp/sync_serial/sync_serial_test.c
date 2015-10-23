#include "mfp_sync_serial.h"

static ser_task_t* create_ser_sync_fmtr_task(sync_serial_t* sync_ser,
		uint32_t sess_seq_num, uint32_t group_id,
		uint32_t group_seq_num, task_hdlr_fptr task_hdlr, void* ctx);
static void contextDeleteHdlr(void* ctx); 
static void contextHdlr(void* ctx);

static uint64_t simScheduler(int64_t sec, task_hdlr_fptr task_hdlr, 
		void* ctx); 

static void test1(void);
static void test2(void);

int main() {

	test1();
	printf("\n");
	test2();
	return 0;
}


static void test1(void) {

	printf("############################\n");
	printf("TEST 1 \n");
	// vanilla case
	uint32_t sync_member_count = 5;
	uint32_t queue_size = 2;
	schedule_hdlr_fptr schedule_hdlr = simScheduler;
	uint32_t ser_sched_out_flag = 1;
	int8_t *name = "syncser-test";
	sync_serial_t* sync_ser = createSyncSerializer(sync_member_count,
			queue_size, schedule_hdlr, ser_sched_out_flag, name);
	if (sync_ser == NULL) {
		printf("createSyncSerializer failed\n");
		return;
	}

	uint32_t task_count = 5;
	uint32_t group_count = 5;
	uint32_t sess_seq_num = 0;
	ser_task_t* ser_tasks[group_count][task_count];
	uint32_t i = 0, j = 0;
	for (j = 0; j < task_count; j++) {
		for (i = 0; i < group_count; i++) {
			uint32_t* ctx = malloc(4);
			*ctx = sess_seq_num; 
			ser_tasks[j][i] = create_ser_sync_fmtr_task(sync_ser, sess_seq_num++, 
					i, j, contextHdlr, ctx);
			schedule_hdlr(0, sync_ser->serial_in_hdlr, ser_tasks[j][i]);
		}
	}
	sync_ser->delt(sync_ser);
	printf("############################\n");
}


static void test2(void) {

	printf("############################\n");
	printf("TEST 2 \n");
	uint32_t sync_member_count = 3;
	uint32_t queue_size = 2;
	schedule_hdlr_fptr schedule_hdlr = simScheduler;
	uint32_t ser_sched_out_flag = 1;
	int8_t *name = "syncser-test";
	sync_serial_t* sync_ser = createSyncSerializer(sync_member_count,
			queue_size, schedule_hdlr, ser_sched_out_flag, name);
	if (sync_ser == NULL) {
		printf("createSyncSerializer failed\n");
		return;
	}

	// TEST-2 : Simulating out of sync
	uint32_t task_count = 3;
	uint32_t group_count = 3;
	uint32_t sess_seq_num = 0;
	ser_task_t* ser_tasks[group_count][task_count];
	uint32_t* ctx = malloc(4);
	*ctx = sess_seq_num; 
	ser_tasks[0][0] = create_ser_sync_fmtr_task(sync_ser, sess_seq_num++, 
			0, 0, contextHdlr, ctx);

	ctx = malloc(4);
	*ctx = sess_seq_num; 
	ser_tasks[0][1] = create_ser_sync_fmtr_task(sync_ser, sess_seq_num++, 
			1, 0, contextHdlr, ctx);

	ctx = malloc(4);
	*ctx = sess_seq_num; 
	ser_tasks[0][2] = create_ser_sync_fmtr_task(sync_ser, sess_seq_num++, 
			2, 0, contextHdlr, ctx);

	ctx = malloc(4);
	*ctx = sess_seq_num; 
	ser_tasks[1][0] = create_ser_sync_fmtr_task(sync_ser, sess_seq_num++, 
			0, 1, contextHdlr, ctx);

	ctx = malloc(4);
	*ctx = sess_seq_num; 
	ser_tasks[1][1] = create_ser_sync_fmtr_task(sync_ser, sess_seq_num++, 
			1, 1, contextHdlr, ctx);

	ctx = NULL;
	ser_tasks[1][2] = create_ser_sync_fmtr_task(sync_ser, sess_seq_num++, 
			2, 1, contextHdlr, ctx);

	ctx = malloc(4);
	*ctx = sess_seq_num; 
	ser_tasks[2][0] = create_ser_sync_fmtr_task(sync_ser, sess_seq_num++, 
			0, 1, contextHdlr, ctx);

	ctx = malloc(4);
	*ctx = sess_seq_num; 
	ser_tasks[2][1] = create_ser_sync_fmtr_task(sync_ser, sess_seq_num++, 
			1, 1, contextHdlr, ctx);

	ctx = malloc(4);
	*ctx = sess_seq_num; 
	ser_tasks[2][2] = create_ser_sync_fmtr_task(sync_ser, sess_seq_num++, 
			2, 1, contextHdlr, ctx);

	// total 9 tasks
	schedule_hdlr(0, sync_ser->serial_in_hdlr, ser_tasks[0][0]);
	schedule_hdlr(0, sync_ser->serial_in_hdlr, ser_tasks[0][1]);
	schedule_hdlr(0, sync_ser->serial_in_hdlr, ser_tasks[0][2]);

	schedule_hdlr(0, sync_ser->serial_in_hdlr, ser_tasks[1][0]);
	schedule_hdlr(0, sync_ser->serial_in_hdlr, ser_tasks[1][1]);
	schedule_hdlr(0, sync_ser->serial_in_hdlr, ser_tasks[1][2]);

	schedule_hdlr(0, sync_ser->serial_in_hdlr, ser_tasks[2][0]);
	schedule_hdlr(0, sync_ser->serial_in_hdlr, ser_tasks[2][1]);
	schedule_hdlr(0, sync_ser->serial_in_hdlr, ser_tasks[2][2]);

	sync_ser->delt(sync_ser);
	printf("############################\n");
}

static void contextHdlr(void* ctx) {

	uint32_t* val = (uint32_t*)ctx;
	printf("Handling context: %u\n", *val);
	contextDeleteHdlr(ctx);
}


static void contextDeleteHdlr(void* ctx) {

	uint32_t* val = (uint32_t*)ctx;
	printf("Deleting context: %u\n", *val);
	free(val);
}

static uint64_t simScheduler(int64_t sec, task_hdlr_fptr task_hdlr, 
		void* ctx) {

	task_hdlr(ctx);
	return 0;
}


static ser_task_t* create_ser_sync_fmtr_task(sync_serial_t* sync_ser,
		uint32_t sess_seq_num, uint32_t group_id,
		uint32_t group_seq_num, task_hdlr_fptr task_hdlr, void* ctx) {

	sync_task_t* sync_task = createSyncTask(sync_ser, group_id, group_seq_num,
			ctx, task_hdlr, contextDeleteHdlr);
	if (sync_task == NULL)
		return NULL;
	ser_task_t* ser_task = createSerialTask(sync_ser, sess_seq_num,
			(void*)sync_task, sync_ser->sync_hdlr);
	if (ser_task == NULL) {
		free(sync_task);
		return NULL;
	}
	return ser_task;
}


#include "dispatcher_manager.h"

//nkn includes
#include "nkn_defs.h"
#include "nkn_mem_counter_def.h"
#include "nkn_sched_api.h"
#include "nkn_rtsched_api.h"
#include "nkn_hash_map.h"
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "nkn_lockstat.h"
#include "../../../../lib/nvsd/rtsched/nkn_rtsched_task.h"
#include "nkn_slotapi.h"

// Sched related var and func
GThread** nkn_sched_threads;
GThread** nkn_rtsched_threads;
uint32_t sched_num_core_threads = 1;

typedef void (*TaskFunc)(void* clientData);
void mfp_proc(void* data);
static nkn_task_id_t nkn_rtscheduleDelayedTask(int64_t microseconds,
                                TaskFunc proc, void* data);
int nkn_mfp_rtscheduler_init(GThread** gthreads );
int nkn_rtsched_core_thread(void *message);

// Dispatcher related var and func
dispatcher_mngr_t* disp_mngr = NULL;
int ctr = 0;
entity_context_t *ent_context1, *ent_context2;
pthread_t disp_mngr_thrd;

int8_t printStr(entity_context_t* ent_context);
int8_t timeout(entity_context_t* ent_context);
int32_t run_event_dispatcher_ut(int argc, char *argv[]);

static void exitClean(int signal_num);
static void exitClean(int signal_num) { disp_mngr->end(disp_mngr); }

void* disp_malloc_custom(int32_t size) { return malloc(size); }
void* disp_calloc_custom(int32_t num, int32_t size) { return calloc(num, size); }


int nkn_assert_enable = 1;
int rtsched_enable = 1;

// Used in sched init: Declared to get it compiled
void media_mgr_cleanup(nkn_task_id_t id); 
void media_mgr_cleanup(nkn_task_id_t id) { }
void media_mgr_input(nkn_task_id_t id);
void media_mgr_input(nkn_task_id_t id) { }
void media_mgr_output(nkn_task_id_t id);
void media_mgr_output(nkn_task_id_t id) { }

void move_mgr_input(nkn_task_id_t id); 
void move_mgr_input(nkn_task_id_t id) { }
void move_mgr_output(nkn_task_id_t id);
void move_mgr_output(nkn_task_id_t id) { }
void move_mgr_cleanup(nkn_task_id_t id);
void move_mgr_cleanup(nkn_task_id_t id) { }

void chunk_mgr_input(nkn_task_id_t id) { }
void chunk_mgr_output(nkn_task_id_t id) { }
void chunk_mgr_cleanup(nkn_task_id_t id) { }

void http_mgr_input(nkn_task_id_t id) { }
void http_mgr_output(nkn_task_id_t id) { }
void http_mgr_cleanup(nkn_task_id_t id) { }

void om_mgr_input(nkn_task_id_t id) { }
void om_mgr_output(nkn_task_id_t id) { }
void om_mgr_cleanup(nkn_task_id_t id) { }


int main(int argc, char *argv[]) {
    
	g_thread_init(NULL);
    	nkn_rtsched_threads = nkn_calloc_type(glob_rtsched_num_core_threads,
					  sizeof(*nkn_rtsched_threads),
					  mod_nkn_sched_threads);
    
    	nkn_mfp_rtscheduler_init(nkn_rtsched_threads);
    	TaskFunc handler = mfp_proc;
    	nkn_rtscheduleDelayedTask(15000, handler, NULL);
    	run_event_dispatcher_ut(0, NULL);
}

int nkn_mfp_rtscheduler_init(GThread** gthreads ) {

    int i;
    GError           *err1 = NULL ;

    nkn_rttask_init();
    //c_rtsleeper_q_tbl_init();
    nkn_rttask_q_init();
    nkn_rtsched_tmout_init();

    for(i = 0; i < glob_rtsched_num_core_threads; i++) {
	if( (gthreads[i] = g_thread_create_full(
				(GThreadFunc)nkn_rtsched_core_thread,
				(void *)(long)i, (128*1024), TRUE, FALSE,
				G_THREAD_PRIORITY_URGENT, &err1)) == NULL) {
	    g_error_free ( err1 ) ;
	    exit(0);
	}
    }
    return 0;
}


int run_event_dispatcher_ut(int argc, char *argv[]) {

	struct sigaction action_cleanup;
	memset(&action_cleanup, 0, sizeof(struct sigaction));
	action_cleanup.sa_handler = exitClean; 
	action_cleanup.sa_flags = 0;
	sigaction(SIGINT, &action_cleanup, NULL);
	sigaction(SIGTERM, &action_cleanup, NULL);


	int fd1 = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in addr1;
	addr1.sin_family = AF_INET;
	addr1.sin_addr.s_addr = INADDR_ANY;
	addr1.sin_port = htons(1234);
	if (bind(fd1, (struct sockaddr *) &addr1, sizeof(addr1)) == -1) {
		perror("bind");
		exit(1);
	}

        int fd2 = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in addr2;
        addr2.sin_family = AF_INET;
        addr2.sin_addr.s_addr = INADDR_ANY;
        addr2.sin_port = htons(1235);
        if (bind(fd2, (struct sockaddr *) &addr2, sizeof(addr2)) == -1) {
                perror("bind");
                exit(1);
        }

	disp_mngr = newDispManager(1, 100);

	handleEvent print_str = printStr;
	handleEvent timeout_check = timeout;
	ent_context1 = newEntityContext(fd1, NULL, NULL, 
			print_str, NULL,
			NULL, NULL, timeout_check, disp_mngr);


	ent_context2 = newEntityContext(fd2, NULL, NULL, 
			print_str, NULL,
			NULL, NULL, timeout_check, disp_mngr);

	ent_context1->init_event_flags |= EPOLLIN;
	ent_context2->init_event_flags |= EPOLLIN;

	printf("ent_context->fd1: %d\n", ent_context1->fd);
	printf("ent_context->fd2: %d\n", ent_context2->fd);

	disp_mngr->register_entity(ent_context1);
	disp_mngr->register_entity(ent_context2);

	gettimeofday(&(ent_context1->to_be_scheduled), NULL);
	gettimeofday(&(ent_context2->to_be_scheduled), NULL);
	ent_context1->to_be_scheduled.tv_sec += 30;
	ent_context2->to_be_scheduled.tv_sec += 30;
	disp_mngr->self_schedule_timeout(ent_context1);
	disp_mngr->self_schedule_timeout(ent_context2);

	pthread_create(&(disp_mngr_thrd), NULL,
                                disp_mngr->start,
                                disp_mngr);
	pthread_join(disp_mngr_thrd, NULL);
	return 0;
}

int8_t printStr(entity_context_t* ent_context) {

	char buf[5000];
	struct sockaddr addr;
	socklen_t len = 0;
	memset(&buf[0], 0, 5000);
	memset(&addr, 0, sizeof(addr));
	int32_t ret;
	ret = recvfrom(ent_context->fd, &buf[0], 5000, 0, &addr, &len);
	if (ret >=0)
		printf("Bytes Received: %d\n", ret);
	ctr++;/*
	if (ctr == 5) {
		ent_context->disp_mngr->end(ent_context->disp_mngr);
		//ent_context1->disp_mngr->self_unregister(ent_context1);
		//ent_context2->disp_mngr->unregister_entity(ent_context2);
	}
	*/
	return 1;
}

int8_t timeout(entity_context_t* ent_context) {

	//printf("timeout\n");
	gettimeofday(&(ent_context->to_be_scheduled), NULL);
	ent_context->to_be_scheduled.tv_sec += 30;
	ent_context->disp_mngr->self_schedule_timeout(ent_context);
	return 1;
}

void mfp_proc(void* data) {
	printf("In Proc.\n");
    	TaskFunc handler = mfp_proc;
	nkn_rtscheduleDelayedTask(1000000, mfp_proc, NULL);
}

static nkn_task_id_t nkn_rtscheduleDelayedTask(int64_t microseconds,
                                TaskFunc proc,
                                void* clientData) {

    uint64_t tid;
    uint32_t next_avail;
    tid = nkn_rttask_delayed_exec(microseconds / 1000,
				  (microseconds / 1000) + NKN_DEF_MAX_MSEC,
				  proc,
				  clientData,
				  &next_avail);
    return ((nkn_task_id_t)tid);
}


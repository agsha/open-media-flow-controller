/*
 * nkn_cmm.cc -- Cluster Membership Manager (CMM).
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#include "curl/curl.h"

#include "nkn_common_config.h"

#include "nkn_mgmt_agent.h"
#include "nkn_cfg_params.h"
#include "nkn_mem_counter_def.h"
#include "nkn_cmm_shm.h"

#include "cmm_defs.h"
#include "cmm_input_fqueue.h"
#include "cmm_output_fqueue.h"
#include "cmm_timer.h"
#include "cmm_misc.h"
#include "CMMNewDelete.h"
#include "CMMConnectionSM.h"
#include "CMMSharedMemoryMgr.h"
#include "CMMNodeStatus.h"

/*
 * nkn logging controls
 */
extern uint64_t console_log_mod;
extern int console_log_level;

/*
 * Statistics
 */
long g_add_req_new;
long g_add_req_new_sched_fail;
long g_add_req_updt;
long g_add_req_updt_sched;
long g_add_req_updt_sched_fail;
long g_add_req_updt_setcfg_fail;

long g_cancel_req;
long g_cancel_req_sched_fail;
long g_cancel_req_inv_hdl;

long g_active_SM;
long g_canceled_SM;
long g_stale_SM;
long g_op_other;
long g_op_success;
long g_op_timeout;
long g_op_unexpected;
long g_op_online;
long g_op_offline;
long g_initiate_req_failed;
long g_last_initiate_fail_status;
long g_invalid_sm_incarn;

long g_nsc_req_waits;
long g_nsc_hits;
long g_nsc_stale_misses;
long g_nsc_misses;
long g_nsc_entries_flushed;

long g_getshm_data_failed;
long g_getshm_data_retry_failed;
long g_getshm_data_retry_success;

long g_getshm_lm_failed;
long g_getshm_lm_retry_failed;
long g_getshm_lm_retry_success;

long g_input_memq_stalls;
long g_input_memq_msg_limit;
long g_output_memq_stalls;

long g_output_fq_retry_drops;
long g_output_fq_err_drops;

/*
 * Global configuration data
 *
 *   cmm_debug_output -	switch
 *   cmm_debug_output_file - FILE * for debug output stream
 *   nodeonline_interval_msecs - Online interval before declaring node online
 *   stale_sm_timeout_msecs - Delete SM if update interval exceeded
 *   send_config_interval_secs - Periodic send config (CMM->NVSD) interval
 */
int cmm_debug_output = 0;
FILE *cmm_debug_output_file = NULL;
long nodeonline_interval_msecs = (15 * MILLI_SECS_PER_SEC);
long stale_sm_timeout_msecs = (120 * MILLI_SECS_PER_SEC);
int send_config_interval_secs = 60;
int enable_connectionpool = 0;
int input_memq_limit = 8192;
int output_memq_limit = 8192;
int use_fastmem_alloc = 1;

/* Global runtime data */
int (*add_SM)(CURLM* mch, const cmm_node_config_t* config, 
	      const char *ns_name, int ns_name_strlen);
int (*cancel_SM)(const char *node_handle, int node_handle_strlen);
int (*schedule_curl_requests)(CURLM *mch, struct timespec *next_deadline);
int (*handle_curl_completions)(CURLM *mch);

CMMSharedMemoryMgr* CMMNodeInfo = NULL;
CMMSharedMemoryMgr* CMMNodeLoadMetric = NULL;

CMMNodeStatusCache* CMMNodeCache = NULL;

static int do_not_create_daemon = 0;

/* Memory based logging */

#define MEMLOG_SIZE_BYTES (10 * 1024 * 1024)
int cmm_debug_memlog = 0;
int cmm_memlog_size_bytes = MEMLOG_SIZE_BYTES;
int cmm_memlog_bytes_used = 0;
char cmm_memlog_buffer[MEMLOG_SIZE_BYTES + 1024];
char *cmm_memlog = cmm_memlog_buffer;
pthread_mutex_t cmm_memlog_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cmm_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
/*
 ******************************************************************************
 * print_memlog() - GDB interface
 ******************************************************************************
 */
 void print_memlog()
 {
     printf("%s", cmm_memlog);
 }

/*
 ******************************************************************************
 * dump_stats() - Dump internal counters
 ******************************************************************************
 */
void dump_stats()
{
    static struct timespec last_dumptime;
    struct timespec now;
    int64_t msecs;

    if (!DBG_ON && !cmm_debug_memlog) {
    	return;
    }
    clock_gettime(CLOCK_MONOTONIC, &now);
    msecs = timespec_diff_msecs(&last_dumptime, &now);
    if (msecs < MILLI_SECS_PER_SEC) {
    	return;
    }
    last_dumptime = now;

    DBG("g_add_req_new=%ld", g_add_req_new);
    DBG("g_add_req_new_sched_fail=%ld", g_add_req_new_sched_fail);
    DBG("g_add_req_updt=%ld", g_add_req_updt);
    DBG("g_add_req_updt_sched=%ld", g_add_req_updt_sched);
    DBG("g_add_req_updt_sched_fail=%ld", g_add_req_updt_sched_fail);
    DBG("g_add_req_updt_setcfg_fail=%ld", g_add_req_updt_setcfg_fail);
    DBG("g_cancel_req=%ld", g_cancel_req);
    DBG("g_cancel_req_sched_fail=%ld", g_cancel_req_sched_fail);
    DBG("g_cancel_req_inv_hdl=%ld", g_cancel_req_inv_hdl);

    DBG("g_active_SM=%ld", g_active_SM);
    DBG("g_canceled_SM=%ld", g_canceled_SM);
    DBG("g_stale_SM=%ld", g_stale_SM);
    DBG("g_op_other=%ld", g_op_other);
    DBG("g_op_success=%ld", g_op_success);
    DBG("g_op_timeout=%ld", g_op_timeout);
    DBG("g_op_unexpected=%ld", g_op_unexpected);
    DBG("g_op_online=%ld", g_op_online);
    DBG("g_op_offline=%ld", g_op_offline);
    DBG("g_initiate_req_failed=%ld", g_initiate_req_failed);
    DBG("g_last_initiate_fail_status=%ld", g_last_initiate_fail_status);
    DBG("g_invalid_sm_incarn=%ld", g_invalid_sm_incarn);

    DBG("g_nsc_req_waits=%ld", g_nsc_req_waits);
    DBG("g_nsc_hits=%ld", g_nsc_hits);
    DBG("g_nsc_stale_misses=%ld", g_nsc_stale_misses);
    DBG("g_nsc_misses=%ld", g_nsc_misses);
    DBG("g_nsc_entries_flushed=%ld", g_nsc_entries_flushed);

    DBG("g_getshm_data_failed=%ld", g_getshm_data_failed);
    DBG("g_getshm_data_retry_failed=%ld", g_getshm_data_retry_failed);
    DBG("g_getshm_data_retry_success=%ld", g_getshm_data_retry_success);

    DBG("g_getshm_lm_failed=%ld", g_getshm_lm_failed);
    DBG("g_getshm_lm_retry_failed=%ld", g_getshm_lm_retry_failed);
    DBG("g_getshm_lm_retry_success=%ld", g_getshm_lm_retry_success);

    DBG("g_input_memq_stalls=%ld", g_input_memq_stalls);
    DBG("g_input_memq_msg_limit=%ld", g_input_memq_msg_limit);
    DBG("g_output_memq_stalls=%ld", g_output_memq_stalls);
    DBG("g_output_fq_retry_drops=%ld", g_output_fq_retry_drops);
    DBG("g_output_fq_err_drops=%ld", g_output_fq_err_drops);
}

/*
 ******************************************************************************
 * pid_file_exist() - Check for presence of pid file
 *	Returns: True => 1 else 0
 ******************************************************************************
 */
int pid_file_exist(void)
{
    FILE *fpPidFile;
    fpPidFile = fopen(CMM_PID_FILE, "r");
    if (fpPidFile != NULL) {
    	fclose(fpPidFile);
    	return 1;
    } else {
    	return 0;
    }
}

/*
 *******************************************************************************
 * create_pid_file() - Create the pid file to track the process
 *	Returns: 0 => Success
 *******************************************************************************
 */
int create_pid_file(void)
{
    FILE *fpPidFile;
    pid_t pidSelf;

    /* Get the pid */
    pidSelf = getpid();

    /* Write this to the pid file */
    fpPidFile = fopen(CMM_PID_FILE, "w");
    if (fpPidFile != NULL) {
    	fprintf(fpPidFile, "%d", pidSelf);
    	fclose(fpPidFile);
    } else {
	DBG("Failed to create the pid file <%s>", CMM_PID_FILE);
	return 1;
    }
    return 0;
}

/*
 ******************************************************************************
 * exit_cmm() - Process exit cleanup handler
 ******************************************************************************
 */
void exit_cmm(void)
{
    /* Deallocate shared memory segment */
    if (CMMNodeInfo) {
    	CMMNodeInfo->CMMShmShutdown();
    	delete CMMNodeInfo;
    	CMMNodeInfo = NULL;
    }
    if (CMMNodeLoadMetric) {
    	CMMNodeLoadMetric->CMMShmShutdown();
    	delete CMMNodeLoadMetric;
    	CMMNodeLoadMetric = NULL;
    }
    /* unlink the pid file */
    unlink(CMM_PID_FILE);
    return;
}

/*
 *******************************************************************************
 * termination_handler() - Signal handler for SIGTERM, SIGINT and SIGHUP
 *******************************************************************************
 */
void termination_handler(int nSignum)
{
    if (nSignum == SIGUSR1) {
	cmm_debug_output = cmm_debug_output ? 0 : 1;
	return;
    } else if (nSignum == SIGUSR2) {
    	/* Force coredump via SEGV */
	DBG("Creating coredump via SEGV, sig=%d", nSignum);
	*((char *)1) = 1;
    }
    /* Call the exit routine */
    exit_cmm(); 
    exit(100);
}

/*
 *******************************************************************************
 * print_usage() - Print usage options
 *******************************************************************************
 */
void print_usage(char *cpProgName)
{
    fprintf(stdout, "usage : %s [-C-D-I:-O:-a:-b:-i:-r:-d:-n:-p:-s:-v-x-h]\n", 
    	    cpProgName);
    fprintf(stdout, "\t -C : Enable connection pool\n");
    fprintf(stdout, "\t -D : Do not create daemon process \n");
    fprintf(stdout, "\t -I : Input queue filename (default: %s)\n", 
    	input_queue_filename);
    fprintf(stdout, "\t -O : Output queue filename (default: %s)\n",
    	output_queue_filename);
    fprintf(stdout, "\t -a : Input memory queue limit (default: %d)\n", 
    	input_memq_limit);
    fprintf(stdout, "\t -b : Output memory queue limit (default: %d)\n", 
    	output_memq_limit);
    fprintf(stdout, "\t -i : Input queue maxsize (default: %d)\n", 
    	input_queuefile_maxsize);
    fprintf(stdout, "\t -r : Output queue retries (default: %d)\n", 
    	enqueue_retries);
    fprintf(stdout, "\t -d : Output queue retry delay in secs (default: %d)\n", 
    	enqueue_retry_interval_secs);
    fprintf(stdout, "\t -m : Enable memory based logging\n");
    fprintf(stdout, "\t -n : Node online interval in msecs (default: %ld)\n", 
    	nodeonline_interval_msecs);
    fprintf(stdout, "\t -p : Send configuration interval in secs "
	    "(default: %d)\n", send_config_interval_secs);
    fprintf(stdout, "\t -s : Stale SM timeout in msecs (default: %ld)\n", 
    	stale_sm_timeout_msecs);
    fprintf(stdout, "\t -v : Verbose output (default: no)\n");
    fprintf(stdout, "\t -x : Disable fast memory allocator\n");
    fprintf(stdout, "\t -h : Help\n");
    return;
}

/*
 *******************************************************************************
 * getoptions() - Parse command line arguments
 *	Returns: 0 => Success
 *******************************************************************************
 */
int getoptions(int argc, char *argv[])
{
    char chTemp;

    input_queue_filename = CMM_QUEUE_FILE;
    output_queue_filename = NODESTATUS_QUEUE_FILE;

    /* Using getopt get the parameters */
    while ((chTemp = getopt(argc, argv, "CDI:O:a:b:i:r:d:mn:p:s:vxh")) != -1) {
    	switch (chTemp) {
	case 'a':
	    input_memq_limit = atoi(optarg);
	    break;
	case 'b':
	    output_memq_limit = atoi(optarg);
	    break;
	case 'C':
	    enable_connectionpool = 1;
	    break;
	case 'D':
	    do_not_create_daemon = 1;
	    break;
	case 'I':
	    input_queue_filename = optarg;
	    break;
	case 'O':
	    output_queue_filename = optarg;
	    break;
	case 'i':
	    input_queuefile_maxsize = atoi(optarg);
	    break;
	case 'r':
	    enqueue_retries = atoi(optarg);
	    break;
	case 'd':
	    enqueue_retry_interval_secs = atoi(optarg);
	    break;
	case 'm':
	    cmm_debug_memlog = 1;
	    break;
	case 'n':
	    nodeonline_interval_msecs = atoi(optarg);
	    break;
	case 'p':
	    send_config_interval_secs = atoi(optarg);
	    break;
	case 's':
	    stale_sm_timeout_msecs = atoi(optarg);
	    break;
	case 'v':
	    cmm_debug_output = 1;
	    break;
	case 'x':
	    use_fastmem_alloc = 0;
	    break;
	case 'h':
	case '?':
	    print_usage(argv[0]) ;
	    return 1;
	    break;
	default:
	    return 2;
	    break;
         }
    }
    return 0;
}

/*
 ******************************************************************************
 * daemonize() - Create daemon process
 ******************************************************************************
 */
void daemonize(void)
{
    if (fork() != 0) {
    	/* Terminate parent */
    	exit(0);
    }
    if (setsid() == -1) {
    	exit(0);
    }
    signal(SIGHUP, SIG_IGN);
    if (fork() != 0) {
    	/* Terminate parent */
    	exit(0);
    }

#if 0
    /*
     * Do not chdir when this process is managed by PM
     */
    if (chdir("/") != 0) exit(0);
#endif
}

/*
 *******************************************************************************
 * main_event_loop() - Dispatch loop for input fqueue and CURL requests
 *******************************************************************************
 */
int main_event_loop(void)
{
    int rv;
    CURLMcode mret;
    CURLM *mch;
    int running_handles;
    struct timeval timeout;
    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
    int maxfd;
    long timeout_msecs;
    struct timespec now;
    struct timespec sleeptime;
    int scheduled_requests;
    int pending_request;
    struct timespec pending_request_deadline = CMMConnectionSM::NULLtimespec;
    long curl_multi_param;
#ifdef VALGRIND_SUPPORT
    int terminating = 0;
#endif

    mch = curl_multi_init();
    if (!mch) {
    	DBG("curl_multi_init() failed, mch=%p", mch);
	return 1;
    }

    curl_multi_param = 256;
    mret = curl_multi_setopt(mch, CURLMOPT_MAXCONNECTS, curl_multi_param); 
    if (mret != CURLM_OK) {
	DBG("curl_multi_setopt(CURLMOPT_MAXCONNECTS) failed, rv=%d", mret);
    }

    ////////////////////////////////////////////////////////////////////////////
    while (1) {
    ////////////////////////////////////////////////////////////////////////////

#ifdef VALGRIND_SUPPORT
    {
    	struct stat sbuf;
	int sret;
	sret = stat("/tmp/stop-cmm", &sbuf);
	if (!sret) {
	    terminating = 1;
	}

	sret = stat("/tmp/terminate-cmm", &sbuf);
	if (!sret) {
	    exit(0);
	}
    }
    if (!terminating) {
    	handle_fqueue_requests(mch, 1, 128);
    } else {
    	handle_fqueue_requests(mch, 1, 0);
    }
#else
    handle_fqueue_requests(mch, 1, 128);
#endif
    scheduled_requests = CMMConnectionSM::ScheduleCurlRequests(mch, 
    						pending_request, 
						pending_request_deadline);
    if (scheduled_requests) {
	unlock_input_fqueue();
    } else {
    	// Nothing to do, compute sleep time
    	clock_gettime(CLOCK_MONOTONIC, &now);

	// Flush any stale entries in Node Status Cache
    	g_nsc_entries_flushed += CMMNodeCache->Flush(&now);

    	if (pending_request) {
	    if (timespec_cmp(&now, &pending_request_deadline) < 0) {
	    	sleeptime = pending_request_deadline;
	    } else {
	    	sleeptime = now;
	    }
    	} else {
	    sleeptime = now;
	    timespec_add_msecs(&sleeptime, 100); // 100 msec sleep
    	}
    	cond_timedwait_input_fqueue(&sleeptime);
	continue;
    }
    while (curl_multi_perform(mch, &running_handles) == 
    		CURLM_CALL_MULTI_PERFORM)
	;

    while (running_handles) {
    	FD_ZERO(&fdread);
    	FD_ZERO(&fdwrite);
    	FD_ZERO(&fdexcep);

	mret = curl_multi_fdset(mch, &fdread, &fdwrite, &fdexcep, &maxfd);
	if (mret != CURLM_OK) {
	    DBG("curl_multi_fdset() failed, rv=%d", mret);
	}
	if (maxfd == -1) {
	    break;
	}

	mret = curl_multi_timeout(mch, &timeout_msecs);
	if (mret != CURLM_OK) {
	    DBG("curl_multi_timeout() failed, rv=%d", mret);
	}
	if (timeout_msecs <= 0) {
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 0;
	} else {
	    timeout.tv_sec = timeout_msecs / 1000;
	    timeout.tv_usec = (timeout_msecs % 1000) * 1000;
	}

	rv = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
	switch(rv) {
	case -1:
	    // Error
	    DBG("select() error rv=%d\n", rv);
	    break;
	case 0:
	    // Timeout
	    // Fall through
	default:
	    // Events pending
	    while (curl_multi_perform(mch, &running_handles) == 
    			CURLM_CALL_MULTI_PERFORM)
		;
	    break;
	}
    }
    CMMConnectionSM::HandleCurlCompletions(mch);
    ::dump_stats();

    ////////////////////////////////////////////////////////////////////////////
    } // End of while
    ////////////////////////////////////////////////////////////////////////////

    mret = curl_multi_cleanup(mch);
    if (mret != CURLM_OK) {
    	DBG("curl_multi_cleanup() failed, rv=%d", mret);
    }
}

/*
 *******************************************************************************
 * main()
 *******************************************************************************
 */
int main(int argc, char *argv[])
{
    int rv;
    int exit_status = 0;
    CURLcode eret;
    int stacksize;
    pthread_attr_t attr;

    ////////////////////////////////////////////////////////////////////////////
    while(1) {
    ////////////////////////////////////////////////////////////////////////////

    // Set nkn logging to minimal output level
    console_log_mod = 0;
    console_log_level = 1;

    if (getoptions(argc, argv)) {
    	exit_status = 1;
	break;
    }

    // Initialize fast memory allocator
    CMMAllocInit(use_fastmem_alloc);

#if 0
    if (pid_file_exist()) {
    	DBG("[%s] already running ...", argv[0]);
	exit_status = 2;
	break;
    }
#endif

    // Install signal handler
    if (signal(SIGINT, termination_handler) == SIG_IGN)
    	signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, termination_handler) == SIG_IGN)
    	signal(SIGHUP, SIG_IGN);
    if (signal(SIGTERM, termination_handler) == SIG_IGN)
    	signal(SIGTERM, SIG_IGN);
    if (signal(SIGQUIT, termination_handler) == SIG_IGN)
    	signal(SIGQUIT, SIG_IGN);
    signal(SIGUSR1, termination_handler);
    signal(SIGUSR2, termination_handler);

    if (!do_not_create_daemon) {
    	daemonize();
    }

    // Set the atexit function
    rv = atexit(exit_cmm);
    if (rv) {
    	DBG("atexit() failed, rv=%d", rv);
    	exit_status = 3;
	break;
    }

    // Create the pid file
    rv = create_pid_file();
    if (rv) {
    	DBG("create_pid_file() failed, rv=%d", rv);
	exit_status = 4;
	break;
    }

    // Initialize CMMConnectionSM

    rv = CMMConnectionSM::CMMConnectionSMinit();
    if (rv) {
    	DBG("CMMConnectionSMinit() failed, rv=%d", rv);
	exit_status = 5;
	break;
    }

    // Assert fundamental shared memory assumptions

    assert((sizeof(cmm_shm_hdr_t) +
	    (roundup(CMM_SEGMENTS, NBBY)/NBBY)-1) <= CMM_MAX_HDR_SIZE);
    assert(sizeof(cmm_loadmetric_entry_t) == CMM_LD_SEGMENT_SIZE);

    // Initialize shared memory segments

    CMMNodeInfo = new CMMSharedMemoryMgr();
    rv = CMMNodeInfo->CMMShmInit(CMM_SHM_KEY, CMM_SHM_SIZE, CMM_MAX_HDR_SIZE, 
    				 CMM_SHM_MAGICNO, CMM_SHM_VERSION, 
				 CMM_SEGMENT_SIZE, CMM_SEGMENTS);
    if (rv) {
    	DBG("CMMNodeInfo CMMShmInit() failed, rv=%d, "
	    "disable use of shared memory", rv);
    }

    CMMNodeLoadMetric = new CMMSharedMemoryMgr();
    rv = CMMNodeLoadMetric->CMMShmInit(CMM_LD_SHM_KEY, CMM_LD_SHM_SIZE, 
    				 CMM_LD_MAX_HDR_SIZE, CMM_LD_SHM_MAGICNO, 
				 CMM_LD_SHM_VERSION, CMM_LD_SEGMENT_SIZE, 
				 CMM_LD_SEGMENTS);
    if (rv) {
    	DBG("CMMNodeLoadMetric CMMShmInit() failed, rv=%d, "
	    "disable use of shared memory", rv);
    }

    // Initialize the Node Status Cache
    CMMNodeCache = new CMMNodeStatusCache();
    if (!CMMNodeCache) {
    	DBG("new CMMNodeStatusCache() failed, CMMNodeCache=%p", CMMNodeCache);
	exit_status = 51;
	break;
    }

    // C to C++ interface initialization
    add_SM = &CMMConnectionSM::AddCMMConnectionSM;
    cancel_SM = &CMMConnectionSM::CancelCMMConnectionSMptrLen;

    // Initialize CURL library
    eret = curl_global_init(CURL_GLOBAL_ALL);
    if (eret != CURLE_OK) {
    	DBG("curl_global_init() failed, eret=%d", eret);
	exit_status = 6;
	break;
    	
    }

    // Initialize output FQueue
    init_output_fqueue();

    // Create FQueue output thread
    rv = pthread_attr_init(&attr);
    if (rv) {
	DBG("pthread_attr_init() failed, rv=%d", rv);
	exit_status = 7;
	break;
    }

    stacksize = 128 * 1024;
    rv = pthread_attr_setstacksize(&attr, stacksize);
    if (rv) {
	DBG("pthread_attr_setstacksize() failed, rv=%d", rv);
	exit_status = 8;
	break;
    }

    if ((rv = pthread_create(&fqueue_output_thread_id, &attr, 
			     fqueue_output_request_handler, NULL))) {
	DBG("pthread_create() failed, rv=%d", rv);
	exit_status = 9;
	break;
    }

    // Wait until output fqueue thread initialization is complete
    while (!fqueue_output_init_complete) {
    	sleep(1);
    }

    // Initialize input FQueue
    init_input_fqueue();

    // Create FQueue input thread
    rv = pthread_attr_init(&attr);
    if (rv) {
	DBG("pthread_attr_init() failed, rv=%d", rv);
	exit_status = 10;
	break;
    }

    stacksize = 128 * 1024;
    rv = pthread_attr_setstacksize(&attr, stacksize);
    if (rv) {
	DBG("pthread_attr_setstacksize() failed, rv=%d", rv);
	exit_status = 11;
	break;
    }

    if ((rv = pthread_create(&fqueue_input_thread_id, &attr, 
			     fqueue_input_request_handler, NULL))) {
	DBG("pthread_create() failed, rv=%d", rv);
	exit_status = 12;
	break;
    }

    // Wait until the input fqueue thread initialization is complete
    while (!fqueue_input_init_complete) {
    	sleep(1);
    }

    stacksize = 128 * 1024;
    rv = pthread_attr_setstacksize(&attr, stacksize);
    if (rv) {
	DBG("pthread_attr_setstacksize() failed, rv=%d", rv);
	exit_status = 13;
	break;
    }

    if ((rv = pthread_create(&timer_thread_id, &attr, 
			     timer_handler, NULL))) {
	DBG("pthread_create() failed, rv=%d", rv);
	exit_status = 14;
	break;
    }

    rv = main_event_loop();
    if (rv) {
	DBG("main_event_loop() failed, rv=%d", rv);
    	exit_status = 15;
	break;
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End of while
    ////////////////////////////////////////////////////////////////////////////

    exit(exit_status);
}

/*
 * End of nkn_cmm.cc
 */

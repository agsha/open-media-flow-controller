#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/queue.h>
#include <syslog.h>
#include <signal.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <libxml/xmlschemastypes.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <aio.h>


// mfp includes
#include "mfp_publ_context.h"
#include "mfp_pmf_processor.h"
#include "mfp_file_sess_mgr_api.h"
#include "mfp_pmf_parser_api.h"
#include "mfp_pmf_tag_defs.h"
#include "mfp_live_sess_id.h"
#include "mfp_mgmt_sess_id_map.h"
#include "thread_pool/mfp_thread_pool.h"
#include "mfu2iphone.h"
#include "mfp_read_config.h"

#include "nkn_defs.h"
#include "nkn_sched_api.h"
#include "nkn_rtsched_api.h"
#include "nkn_hash_map.h"
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "nkn_lockstat.h"
#include "../../../lib/nvsd/rtsched/nkn_rtsched_task.h"
#include "nkn_slotapi.h"
#include "nkn_authmgr.h"
#include "nkn_encmgr.h"
#include "mfp_read_config.h"

/***************************************************************
 *                GLOBAL VARIABLES
 **************************************************************/

int8_t* xml_schema_data;
char *cfg_file_name = NULL;
uint32_t xsd_len;

/* FD's for directory watching */
int32_t dir_watch_fd, dir_watch_wd;
/* pmf processor thread */
pthread_t pmf_process_thread;
/* thread pools for read and write tasks */
mfp_thread_pool_t* read_thread_pool = NULL;
mfp_thread_pool_t* write_thread_pool = NULL;
/* session id and session context index */
file_id_t *file_state_cont;
file_id_t *live_state_cont = NULL;


static void* tp_malloc_custom(uint32_t size);
static void* tp_calloc_custom(uint32_t num, uint32_t size);

// rt sched conditional timed wait : To set it as 1ms slot interval
// Also required for slow systems
extern int32_t glob_rtsched_mfp_enable;
extern int32_t nkn_enc_mgr_enable;
extern mfp_cfg_param_val_t filed_cfg[];
extern uint32_t glob_apple_fmtr_num_task_low_threshold;
extern uint32_t glob_apple_fmtr_num_task_high_threshold;
extern uint32_t glob_use_async_io_apple_fmtr;
extern uint32_t glob_use_async_io_sl_fmtr;
extern int nkn_max_domain_ips;
extern int http_cfg_fo_use_dns;
// Sched related var and func
GThread** nkn_rtsched_threads = NULL;
GThread** nkn_sched_threads = NULL;
pmf_proc_mgr_t *g_pmgr;

/**************************************************************
 *                   CONFIG VARIABLES
 **************************************************************/
/* number of thrads in the read thread pool */
uint32_t glob_mfp_file_num_read_thr = 2;
/* number of threads in the write thread pool */
uint32_t glob_mfp_file_num_write_thr = 4;
/* total number of tasks supported per thread pool */
uint32_t glob_mfp_file_max_tasks = 1000;
/* total number of sessions to be supported */
uint32_t glob_mfp_max_sess_supported = 1000;
/* will be set if thread pool is used for file conversion */
uint32_t glob_mfp_fconv_tpool_enable = 0;
/* run each in synchronous mode (only one job at a time). useful for
 * running unit tests sequentially */
uint32_t glob_mfp_fconv_sync_mode = 1;
mfp_thread_pool_t* apple_fmtr_task_processor = NULL;
uint32_t glob_enable_stream_ha = 0;
uint32_t glob_mfp_max_fds = 100000;
uint32_t glob_mfp_num_aio_threads = 100;

/* NKN assert enable */
int nkn_assert_enable = 0;
/* enable RT scheduler */
int rtsched_enable = 1;
/* global time measured every 5 seconds */
time_t nkn_cur_ts;

mfp_cfg_param_val_t filed_cfg [] = {
    {{"file.converter.tpoolmode.enable", MFP_INT_TYPE},
     &glob_mfp_fconv_tpool_enable},
    {{"file.global.max_concurrent_sessions", MFP_INT_TYPE},
     &glob_mfp_max_sess_supported},
    {{"file.converter.sequential_mode.enable", MFP_INT_TYPE},
     &glob_mfp_fconv_sync_mode},
    {{"file.converter.num_worker_threads", MFP_INT_TYPE},
     &glob_mfp_file_num_read_thr},
    {{"file.MobileStreaming.task_low_threshold", MFP_INT_TYPE},
     &glob_apple_fmtr_num_task_low_threshold},
    {{"file.MobileStreaming.task_high_threshold", MFP_INT_TYPE},
     &glob_apple_fmtr_num_task_high_threshold},
    {{"file.global.use_async_io_apple_fmtr", MFP_INT_TYPE},
     &glob_use_async_io_apple_fmtr},
    {{"file.global.use_async_io_sl_fmtr", MFP_INT_TYPE},
    &glob_use_async_io_sl_fmtr},       
    {{"file.global.libc_assert_enable", MFP_INT_TYPE},
     &nkn_assert_enable},
    {{"file.global.rtsched_num_core_threads", MFP_INT_TYPE},
     &glob_rtsched_num_core_threads},
    {{"file.global.max_fds", MFP_INT_TYPE},
     &glob_mfp_max_fds},
    {{"file.global.num_aio_worker_threads", MFP_INT_TYPE},
     &glob_mfp_num_aio_threads},
    {{NULL, MFP_INT_TYPE}, NULL}
};


/***************************************************************
 *              FUNCTION PROTOTYPES
 **************************************************************/
static void file_listener_exit(int sig_num);
static void check_existing_instance(void);
static void init_signal_handling(void);
static char* remove_trailing_slash(char *str);
static void daemonize(void);
static void print_usage(void);
extern void config_and_run_counter_update(void);
static int scheduler_init(GThread** gthreads);

/***************************************************************
 * NKN SCHED DEPENDS
 **************************************************************/
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

void nfs_tunnel_mgr_input(nkn_task_id_t id);
void nfs_tunnel_mgr_input(nkn_task_id_t id) { }
void nfs_tunnel_mgr_output(nkn_task_id_t id);
void nfs_tunnel_mgr_output(nkn_task_id_t id) { }
void nfs_tunnel_mgr_cleanup(nkn_task_id_t id);
void nfs_tunnel_mgr_cleanup(nkn_task_id_t id) { }

void auth_helper_input(nkn_task_id_t id);
void auth_helper_input(nkn_task_id_t id) {}
void auth_helper_output(nkn_task_id_t id);
void auth_helper_output(nkn_task_id_t id) {}
void auth_helper_cleanup(nkn_task_id_t id);
void auth_helper_cleanup(nkn_task_id_t id) {}

void pe_geo_mgr_input(nkn_task_id_t id);
void pe_geo_mgr_input(nkn_task_id_t id) {}
void pe_geo_mgr_output(nkn_task_id_t id);
void pe_geo_mgr_output(nkn_task_id_t id) {}
void pe_geo_mgr_cleanup(nkn_task_id_t id);
void pe_geo_mgr_cleanup(nkn_task_id_t id) {}

void pe_helper_input(nkn_task_id_t id);
void pe_helper_input(nkn_task_id_t id) {}
void pe_helper_output(nkn_task_id_t id);
void pe_helper_output(nkn_task_id_t id) {}
void pe_helper_cleanup(nkn_task_id_t id);
void pe_helper_cleanup(nkn_task_id_t id) {}

void pe_ucflt_mgr_input(nkn_task_id_t id);
void pe_ucflt_mgr_input(nkn_task_id_t id) {}
void pe_ucflt_mgr_output(nkn_task_id_t id);
void pe_ucflt_mgr_output(nkn_task_id_t id) {}
void pe_ucflt_mgr_cleanup(nkn_task_id_t id);
void pe_ucflt_mgr_cleanup(nkn_task_id_t id) {}


int nkn_rtsched_core_thread(void *message);

//void cache_update_debug_data(struct nkn_task *ntask);
void cache_update_debug_data(struct nkn_task *ntask) {}
void dns_hash_and_insert(char *domain, ip_addr_t *ip, int32_t *ttl, int res, int num);
void dns_hash_and_insert(char *domain, ip_addr_t *ip, int32_t *ttl, int res, int num) {}
int is_loopback(ip_addr_t *ip_ptr);
int is_loopback(ip_addr_t *ip_ptr) {return 0;}
void mm_async_thread_hdl(void);
void mm_async_thread_hdl(void) {}

static void initIO_Conf(void);

static void check_existing_instance(void)
{
    DIR * dir=opendir("/proc");
    struct dirent * ent;
    pid_t pid, mypid;
    int len;
    int fd;
    char buffer[4096];

    mypid=getpid();

    while ((ent = readdir(dir)) != NULL)
        {
	    if (!isdigit(ent->d_name[0]))
		continue;

	    pid=atoi(ent->d_name);
	    if(pid == mypid) continue;

	    snprintf(buffer, 4096, "/proc/%d/cmdline", pid);
	    if ((fd = open(buffer, O_RDONLY)) != -1)
                {
		    // read command line data
		    if ((len = read(fd, buffer, 4096)) > 1)
                        {
			    buffer[len] = '\0';
			    if(strstr(buffer, "file_mfpd") != 0) {
				printf("\nThere is another running file_mfp process (pid=%d)\n", pid);
				printf("Please exit it before continue.\n\n");
				exit(2);
			    }
                        }
                }

	    close(fd);
        }

    closedir(dir);

    return;
}

/**
 * prints the usage for the live listener
 */
static void print_usage(void)
{
    printf("Media Flow Publisher File Listener Daemon\n");
    printf("-D - run as a daemon\n");
    printf("-w - directory to watch for publish events \n");
    printf("-x - Path to the XML schema file \n");
    printf("-F - path to runtime config parameter file for file mfpd\n");
    printf("-h - print help\n");
}

/**
 * run the live listener as a daemon 
 */
static void daemonize(void) 
{

    if (0 != fork()) exit(0);

    if (-1 == setsid()) exit(0);

    if (0 != fork()) exit(0);

    /* Do not chdir when this processis managed by the PM
     * if (0 != chdir("/")) exit(0);
     */
}

/**
 * remove trailing slash(es) in a string in place
 * @param - string in which the slash(es) need to be removed
 * @return - returns the modified string with all trailing slash(es)
 * removed 
 */
static char* remove_trailing_slash(char *str)
{
    int len,i;

    i=0;
    len = 0;

    if(str==NULL){
	printf("string cannot be NULL\n");
	return NULL;
    }

    i = len = strlen(str);

    if(len == 0){
	printf("string cannot be a blank\n");
	return NULL;
    }

    while( i && str[--i]=='/'){

    }

    str[i+1]='\0';

    return str;

}

/**
 * create signal handlers to handle user inputs to the daemon
 */
static void init_signal_handling(void)
{

	struct sigaction action_cleanup;
	memset(&action_cleanup, 0, sizeof(struct sigaction));
	action_cleanup.sa_handler = file_listener_exit;
	action_cleanup.sa_flags = 0;
	sigaction(SIGINT, &action_cleanup, NULL);
	sigaction(SIGTERM, &action_cleanup, NULL);

	struct sigaction signal_action;
	memset(&signal_action, 0, sizeof(struct sigaction));
	signal_action.sa_handler = SIG_IGN;
	signal_action.sa_flags = 0;
	if (sigaction(SIGPIPE, &signal_action, NULL) == -1) {
		perror("sigpipe action set failed: ");
		exit(-1);
	}

}

/**
 * exit handler called when user inputs Ctrl+c
 */
static void file_listener_exit(int sig_num)
{
    /* cleanup all global resources */
	free(xml_schema_data);
    //read_thread_pool->delete_thread_pool(read_thread_pool);
    //write_thread_pool->delete_thread_pool(read_thread_pool);
    exit(0);
}

static int scheduler_init(GThread** gthreads ) 
{
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
	    DBG_MFPLOG ("INIT", ERROR, MOD_MFPLIVE, "Error Initializing"
			" Real Time Scheduler");

	    g_error_free ( err1 ) ;
	    exit(0);
	}
    }

    if( 0 != nkn_scheduler_init( nkn_sched_threads, (~NKN_SCHED_START_MMTHREADS))) {
	DBG_MFPLOG ("INIT", ERROR, MOD_MFPLIVE, "Error Initializing"
		    " Scheduler");
	exit(1);
    }

    nkn_task_register_task_type(TASK_TYPE_MFP_LIVE,
				&mfp_live_input,
				&mfp_live_output,
				&mfp_live_cleanup);

    return 0;
}

#ifndef UNIT_TEST
int32_t main(int argc, char *argv[])
{
    int32_t dont_daemonize, run_multiple_instance_flag;
    char ret, *watch_dir;
    dir_watch_th_obj_t *dwto = NULL;
    pthread_t dir_watch_thread;
    /* Initialize */
    dont_daemonize = 1;
    run_multiple_instance_flag = 0;
    glob_rtsched_num_core_threads = 1;

    init_signal_handling();

    /* process command line arguments */
    if (argc <= 1) {
	print_usage();
	exit(-1);
    }
    while ((ret = getopt(argc, argv, "w:x:F:Dhm")) != -1) {
	switch (ret) {
	    case 'D': //daemonize flag
		dont_daemonize = 0;
		break;
	    case 'w': //watch directory 
		watch_dir = optarg;
		watch_dir = remove_trailing_slash(watch_dir);
		break;
	    case 'x': //xml schema file 
		readFileIntoMemory((int8_t const*)optarg,
				   &xml_schema_data, &xsd_len);
		if (xml_schema_data == NULL) {
		    printf("XML Schema load failed.\n");
			DBG_MFPLOG ("GETOPT", ERROR, MOD_MFPFILE,
					"XML schema load failed");
		    exit(-1);
		}
		break;
	    case 'F':
		cfg_file_name = strdup(optarg);
		mfp_read_cfg_file(cfg_file_name, &filed_cfg[0]);
		break;
	    case 'm':
		run_multiple_instance_flag = 1;
		break;
	    case 'h': //print help
		print_usage();
		exit(-1);
	    case '?': // Invalid arguments
		print_usage();
		exit(-1);
	}
    }

	initThreadPoolLib(tp_malloc_custom, tp_calloc_custom, NULL);


    /* check if there is another instance of the process running */
    if (!run_multiple_instance_flag) {
	check_existing_instance();
    }
	initIO_Conf();

    if (!dont_daemonize) {
	daemonize();
    }

    //init the mgmt <-> mfp sess id map
    mfp_mgmt_sess_tbl_init();

    // Publishing Log start
    log_thread_start();

    // start the counter monitor
    config_and_run_counter_update();

    // init scheduler (RT and normal sched)
    g_thread_init(NULL);
    time(&nkn_cur_ts);

    glob_sched_num_core_threads = 1;
    nkn_rtsched_threads = nkn_calloc_type(glob_rtsched_num_core_threads,
					  sizeof(*nkn_rtsched_threads),
					  mod_nkn_sched_threads);
    nkn_sched_threads = nkn_calloc_type(glob_sched_num_core_threads,
					sizeof(*nkn_sched_threads),
					mod_nkn_sched_threads);
    scheduler_init(nkn_rtsched_threads);

    // init thread pool
    apple_fmtr_task_processor = newMfpThreadPool(1, 10000);

    // init the tag names for the PMF parser
    pmfLoadTagNames();

    // start auth mgr
    auth_mgr_init(AUTH_MGR_INIT_START_WORKER_TH);
    
    // start encryption manager
    nkn_enc_mgr_enable = 1;
    enc_mgr_init();

    // create the sesssion id generator
    file_state_cont = newLiveIdState(glob_mfp_max_sess_supported);

    // craete the pmf processor manager instance
    g_pmgr =\
	create_pmf_processor_mgr(mfp_file_start_listener_session); 
    if (!g_pmgr) {
	printf("Fatal error: cannot continue, unable to "
	       "create PMF processor manager for FILE listener\n");
	exit(1);
    }

    // create the thread pools
    read_thread_pool = newMfpThreadPool(glob_mfp_file_num_read_thr,
					glob_mfp_file_max_tasks);
    write_thread_pool = newMfpThreadPool(glob_mfp_file_num_write_thr,
					 glob_mfp_file_max_tasks);

    // start pmf processor
    pthread_create(&pmf_process_thread, NULL, mfp_process_pmf,
		   (void*)g_pmgr); 

    // add the watch folder 
    ret = create_dir_watch_obj(watch_dir, g_pmgr, &dwto);
    if (ret) {
	printf("exiting daemon, unable to create directory watch obj"
	       ", errcode: %d\n", ret);
    }
    dir_watch_thread = start_dir_watch(dwto);

    // wait for pmf processor thread to exit
    pthread_join(pmf_process_thread, NULL);

    // wait for disp thread to exit

    return 0;
}
#endif


static void initIO_Conf(void) {

	// update max open fd limit
	struct rlimit open_fd_limit;
	open_fd_limit.rlim_cur = glob_mfp_max_fds;//2048;
	open_fd_limit.rlim_max = 2 * glob_mfp_max_fds;//4096;
	if (setrlimit(RLIMIT_NOFILE, &open_fd_limit) < 0) {
		perror("setrlimit RLIMIT_NOFILE: ");
		DBG_MFPLOG ("INIT", ERROR, MOD_MFPLIVE, "Error setting max \
				open fd limit");
	}
	getrlimit(RLIMIT_NOFILE, &open_fd_limit);
	// AIO related conf
	struct aioinit aio_conf;
	memset(&aio_conf, 0, sizeof(struct aioinit));
	aio_conf.aio_threads = glob_mfp_num_aio_threads;
	aio_conf.aio_num = 10000;
	aio_conf.aio_idle_time = 300;

	aio_init(&aio_conf);
}


static void* tp_malloc_custom(uint32_t size) {

	    return nkn_malloc_type(size, mod_vpe_thread_pool_t);
}


static void* tp_calloc_custom(uint32_t num, uint32_t size) {

	    return nkn_calloc_type(num, size, mod_vpe_thread_pool_t);
}



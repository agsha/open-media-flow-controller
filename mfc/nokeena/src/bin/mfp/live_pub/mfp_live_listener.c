#include <sys/inotify.h>
#include <sys/queue.h>
#include <syslog.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <ares.h>


#include <netinet/in.h>
//#include <linux/if.h>
#include <net/if.h> //if_nameindex()
#include <sys/socket.h>
#include <arpa/inet.h> //inet_ntop()
#include <sys/types.h>
#include <sys/ioctl.h>
#include <aio.h>

// event dispatcher includes
#include "disp/dispatcher_manager.h"
#include "disp/displib_conf.h"
#include "disp/entity_context.h"

// thread pool include
#include "thread_pool/mfp_thread_pool.h"

// event timer include
#include "event_timer/mfp_event_timer.h"

// IO handler include
#include "io_hdlr/mfp_io_hdlr.h"

// KMS include
#include "kms/mfp_kms_lib_cmn.h"

// mfp includes
#include "mfp_limits.h"
#include "mfp_live_file_pump.h"
#include "mfp_live_sess_id.h"
#include "mfp_publ_context.h"
#include "mfp_pmf_processor.h"
#include "mfp_live_conf.h"
#include "mfp_live_sess_mgr_api.h"
#include "mfp_pmf_parser_api.h"
#include "mfp_pmf_tag_defs.h"
#include "mfp_mgmt_sess_id_map.h"
#include "mfu2iphone.h"

#include "mfp_cpu_affinity.h"

//nkn include
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

#define MFP_MAX_IP_INTERFACES 10

/***************************************************************
 *                GLOBAL VARIABLES
 **************************************************************/
/* total number of sessions that will be supported */
uint64_t glob_mfp_max_sess_supported = MAX_MFP_SESSIONS_SUPPORTED;
/* maximum streams that will be supported per session */
uint32_t glob_mfp_max_streams_per_sess = MAX_STREAM_PER_ENCAP;
/* instance of the network event dispatcher manager*/
dispatcher_mngr_t* disp_mnr;

file_pump_ctxt_t *file_pump_ctxt;

/* container context for live events */
live_id_t* live_state_cont;
file_id_t* file_state_cont = NULL; /* to keep the compiler happy */

uint32_t mfp_live_thrd_count = 0;
pthread_mutex_t live_mfp_lock;

int8_t *xml_schema_data = NULL;
uint32_t xsd_len = 0;
pmf_proc_mgr_t *g_pmgr;

uint32_t glob_num_aio_threads = 100;
extern int http_cfg_fo_use_dns;
extern uint32_t glob_sync_time_calc_offset;
extern uint32_t glob_num_idrs_sync;

extern uint32_t glob_mfp_buff_size_scale;
extern uint32_t glob_mfp_max_av_lag;


//extern uint32_t glob_mfp_max_sess_supported;
extern uint64_t glob_output_ts_strms_base_time;
extern uint32_t glob_apple_fmtr_num_task_low_threshold;
extern uint32_t glob_apple_fmtr_num_task_high_threshold;
extern uint32_t glob_dump_input_stream_before_sync;
extern uint32_t glob_dump_input_stream_after_sync;
extern uint32_t glob_enable_mfu_sanity_checker;
extern uint32_t glob_use_async_io_apple_fmtr;
extern uint32_t glob_use_async_io_sl_fmtr;
extern uint32_t glob_file_pump_delay_exec;
extern uint32_t glob_print_aligner_output;
extern uint32_t glob_dump_cc_input;
extern uint32_t glob_sync_table_debug_print;
extern int32_t glob_print_avsync_log;
extern uint32_t glob_init_av_bias_flag;
extern uint32_t glob_max_audio_bitrate;
extern uint32_t glob_enable_stream_ha;
extern uint32_t glob_enable_session_ha;
extern uint32_t glob_mfp_live_stream_timeout;
extern uint32_t glob_max_sl_queue_items;
extern uint32_t glob_anomaly_sample_duration_correction;
extern uint32_t glob_slow_strm_HA_enable_flag;
extern uint32_t glob_ignore_audio_pid;
extern uint32_t glob_latm_audio_encapsulation_enabled;

#ifdef MFP_LIVE_ACCUMV2
extern uint32_t glob_mfp_audio_buff_time ;
extern uint32_t glob_max_kfi_tolerance ;
#endif
mfp_cfg_param_val_t filed_cfg [] = {
    {{"live.global.num_idrs_sync", MFP_INT_TYPE},
     &glob_num_idrs_sync},
    {{"live.global.sync_time_calc_offset", MFP_INT_TYPE},
     &glob_sync_time_calc_offset},
    {{"live.global.max_concurrent_sessions", MFP_LONG_TYPE},
     &glob_mfp_max_sess_supported},
    {{"live.MobileStreaming.base_timestamp", MFP_LONG_TYPE},
     &glob_output_ts_strms_base_time},
    {{"live.MobileStreaming.task_low_threshold", MFP_INT_TYPE},
     &glob_apple_fmtr_num_task_low_threshold},
    {{"live.MobileStreaming.task_high_threshold", MFP_INT_TYPE},
     &glob_apple_fmtr_num_task_high_threshold},
    {{"live.global.dump_input_stream_before_sync", MFP_INT_TYPE},
    &glob_dump_input_stream_before_sync},
    {{"live.global.dump_input_stream_after_sync", MFP_INT_TYPE},
    &glob_dump_input_stream_after_sync},
    {{"live.global.enable_mfu_sanity_checker", MFP_INT_TYPE},
    &glob_enable_mfu_sanity_checker},
    {{"live.global.use_async_io_apple_fmtr", MFP_INT_TYPE},
    &glob_use_async_io_apple_fmtr},    
    {{"live.global.use_async_io_sl_fmtr", MFP_INT_TYPE},
    &glob_use_async_io_sl_fmtr},    
    {{"live.global.async_io_threads", MFP_INT_TYPE},
     &glob_num_aio_threads},
    {{"live.global.file_pump_delay_exec", MFP_INT_TYPE},
     &glob_file_pump_delay_exec},    
    {{"live.global.print_aligner_output", MFP_INT_TYPE},
     &glob_print_aligner_output}, 
    {{"live.global.dump_callconverter_input", MFP_INT_TYPE},
     &glob_dump_cc_input},
    {{"live.global.print_sync_table", MFP_INT_TYPE},
     &glob_sync_table_debug_print},    
    {{"live.global.print_avsync_log", MFP_INT_TYPE},
     &glob_print_avsync_log}, 
    {{"live.global.max_bitrate_tolerance", MFP_INT_TYPE},
     &glob_mfp_buff_size_scale},
    {{"live.global.max_av_lag_in_secs", MFP_INT_TYPE},
     &glob_mfp_max_av_lag},
    {{"live.global.init_av_bias_flag", MFP_INT_TYPE},
     &glob_init_av_bias_flag},
    {{"live.global.rtsched_num_core_threads", MFP_INT_TYPE},
     &glob_rtsched_num_core_threads},  
#ifdef MFP_LIVE_ACCUMV2
    {{"live.global.audio_buff_time", MFP_INT_TYPE},
     &glob_mfp_audio_buff_time},
    {{"live.global.max_kfi_tolerance", MFP_INT_TYPE},
     &glob_max_kfi_tolerance},
#endif
    {{"live.global.max_sl_queue_items", MFP_INT_TYPE},
     &glob_max_sl_queue_items},
    {{"live.global.max_audio_bitrate", MFP_INT_TYPE},
     &glob_max_audio_bitrate},
    {{"live.global.libc_assert_enable", MFP_INT_TYPE},
     &nkn_assert_enable},
    {{"live.global.enable_stream_ha", MFP_INT_TYPE},
     &glob_enable_stream_ha},     
    {{"live.global.enable_session_ha", MFP_INT_TYPE},
     &glob_enable_session_ha},     
    {{"live.global.stream_timeout", MFP_INT_TYPE},
     &glob_mfp_live_stream_timeout},
    {{"live.global.anomaly_sample_duration_correction", MFP_INT_TYPE},
     &glob_anomaly_sample_duration_correction},
    {{"live.global.slow_strm_HA_enable_flag", MFP_INT_TYPE},
     &glob_slow_strm_HA_enable_flag},
    {{"live.global.ignore_audio_track", MFP_INT_TYPE},
     &glob_ignore_audio_pid},
    {{"live.global.enable_latm_audio", MFP_INT_TYPE},
     &glob_latm_audio_encapsulation_enabled},
    {{NULL, MFP_INT_TYPE}, NULL}
};

struct sockaddr_in** glob_net_ip_addr = NULL;

/* threads to run directory watch events  */ 
pthread_t pmf_process_thread;
pthread_t dir_watch_thread;

/* NKN assert enable */
int nkn_assert_enable = 0;
/* enable RT scheduler */
int rtsched_enable = 1;
/* global time measured every 5 seconds */
time_t nkn_cur_ts;

// rt sched conditional timed wait : To set it as 1ms slot interval
// Also required for slow systems
extern int32_t glob_rtsched_mfp_enable;
extern int32_t nkn_enc_mgr_enable; 

// Sched related var and func
GThread** nkn_rtsched_threads = NULL;
GThread** nkn_sched_threads = NULL;

typedef void (*TaskFunc)(void* clientData);
static void mfp_proc(void* data);
static nkn_task_id_t rtscheduleDelayedTask(int64_t microseconds,
					   TaskFunc proc, void* data);
static int scheduler_init(GThread** gthreads );
int nkn_rtsched_core_thread(void *message);

/** nkn task handler **/
static void mfpLiveHandlerCleanup(nkn_task_id_t id);
static void mfpLiveHandlerInput(nkn_task_id_t id); 
static void mfpLiveHandlerOutput(nkn_task_id_t id);
void mm_async_thread_hdl(void);

/***************************************************************
 *                FUNCTION PROTOTYPES
 ***************************************************************/
static int8_t mfp_init(void); 
static void check_existing_instance(void);
static void init_signal_handling(void);
static void exitClean(int signal_num);
static mfp_publ_t* build_live_request(void);
static void daemonize(void);
static void print_usage(void);
static void freeRegisteredCtx(entity_context_t* entity_ctx);
extern void config_and_run_counter_update(void);

// function to get IP addresses of all the local networks interfaces
static struct sockaddr_in** getIPInterfaces(void); 

static void initIO_Conf(void);
/***************************************************************
 *              SHARED VARIABLE - EXTERNS
 **************************************************************/
extern pthread_mutex_t pmf_process_queue_lock;
extern TAILQ_HEAD(, tag_pmf_process_queue_item) pmf_process_queue_head;

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

void nfs_tunnel_mgr_input(nkn_task_id_t id);
void nfs_tunnel_mgr_input(nkn_task_id_t id) { }
void nfs_tunnel_mgr_output(nkn_task_id_t id);
void nfs_tunnel_mgr_output(nkn_task_id_t id) { }
void nfs_tunnel_mgr_cleanup(nkn_task_id_t id);
void nfs_tunnel_mgr_cleanup(nkn_task_id_t id) { }

//void enc_mgr_input(nkn_task_id_t id); 
//void enc_mgr_input(nkn_task_id_t id) { }
//void enc_mgr_output(nkn_task_id_t id);
//void enc_mgr_output(nkn_task_id_t id) { }
//void enc_mgr_cleanup(nkn_task_id_t id);
//void enc_mgr_cleanup(nkn_task_id_t id) { }

//void auth_mgr_input(nkn_task_id_t id);
//void auth_mgr_input(nkn_task_id_t id) {}
//void auth_mgr_output(nkn_task_id_t id);
//void auth_mgr_output(nkn_task_id_t id) {}
//void auth_mgr_cleanup(nkn_task_id_t id);
//void auth_mgr_cleanup(nkn_task_id_t id) {}

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


//void cache_update_debug_data(struct nkn_task *ntask);
void cache_update_debug_data(struct nkn_task *ntask) {}
void dns_hash_and_insert(char *domain, ip_addr_t *ip, int32_t *ttl, int res, int num);
void dns_hash_and_insert(char *domain, ip_addr_t *ip, int32_t *ttl, int res, int num) {}
int is_loopback(ip_addr_t *ip_ptr);
int is_loopback(ip_addr_t *ip_ptr) {return 0;}

// Network event dispatcher;
dispatcher_mngr_t* disp_mngr;
pthread_t disp_thread;
static void* disp_malloc_custom(uint32_t size);
static void* disp_calloc_custom(uint32_t num, uint32_t size); 
static void disp_thread_conf(uint32_t thr_id);

// Thread pool
mfp_thread_pool_t* task_processor = NULL;
mfp_thread_pool_t* apple_fmtr_task_processor = NULL;

static void* tp_malloc_custom(uint32_t size); 
static void* tp_calloc_custom(uint32_t num, uint32_t size);
static void tp_thread_conf(uint32_t thr_id);

// Event timer
event_timer_t* ev_timer = NULL;
pthread_t ev_thread;
static void* event_timer_calloc_custom(uint32_t num, uint32_t size); 
static void et_thread_conf(uint32_t thr_id);

static void* IO_Hdlr_custom_calloc(uint32_t num, uint32_t size); 

static void* KMS_CustomMalloc(uint32_t size); 
static void* KMS_CustomCalloc(uint32_t num, uint32_t size); 

/**
 * checks for existing instance of the binary
 */
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
			    if(strstr(buffer, "live_mfpd") != 0) {
				printf("\nThere is another running live_mfpd process (pid=%d)\n", pid);
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
    printf("Media Flow Publisher Listener Daemon\n");
    printf("-D - run as a daemon\n");
    printf("-w - directory to watch for publish events \n");
    printf("-x - Path to the XML schema file \n");
    printf("-F - path to runtime config parameter file for live mfpd\n");
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
static char* remove_trailing_slash(char *str);
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

#ifndef UNIT_TEST
int32_t main(int argc, char *argv[]) 
{
    int32_t dont_daemonize, run_multiple_instance_flag;
    char *watch_dir, ret;
    char *cfg_file_name = NULL;
    dir_watch_th_obj_t *dwto = NULL;

    xmlInitParser();
    dont_daemonize = 1;
    run_multiple_instance_flag = 0;
    glob_rtsched_mfp_enable = 1;
    glob_rtsched_num_core_threads = 1;

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
		    exit(-1);
		}
		break;
	    case 'h': //print help
		print_usage();
		exit(-1);
	    case 'm':
		run_multiple_instance_flag = 1;
		break;
	    case 'F':
		cfg_file_name = strdup(optarg);
		mfp_read_cfg_file(cfg_file_name, &filed_cfg[0]);
		break;		
	    case '?': // Invalid arguments
		print_usage();
		exit(-1);
	}
    }

    /* check if there is another instance of the process running */
    if (!run_multiple_instance_flag) {
	check_existing_instance();
    }

    initPhysicalProcessorCount();

	initIO_Conf();

    // init global variables
    mfp_init();
    
    // pmf processor init
    init_mfp_pmf_processor();

    //init the mgmt <-> mfp sess id map
    mfp_mgmt_sess_tbl_init();

    // Publishing Log start
    log_thread_start();

    // start the counter monitor
    config_and_run_counter_update();

    // init the tag names for the PMF parser
    pmfLoadTagNames();

    // init scheduler (RT and normal sched)
    g_thread_init(NULL);
    time(&nkn_cur_ts);
    scheduler_init(nkn_rtsched_threads);

    // init thread pool
    task_processor = newMfpThreadPool(1, 10000);
    
	// init thread pool
	apple_fmtr_task_processor = newMfpThreadPool(1, 10000);

    // init event timer
    ev_timer = createEventTimer(event_timer_calloc_custom, et_thread_conf);
    pthread_create(&ev_thread, NULL, ev_timer->start_event_timer, ev_timer);
    
    // set signal handlers
    init_signal_handling();	

    glob_net_ip_addr = getIPInterfaces();
    
    /* Daemonize if necessary */
    if (!dont_daemonize) {
	daemonize();
    }

    initIO_Handlers(IO_Hdlr_custom_calloc);
    initKMS_Lib(KMS_CustomMalloc, KMS_CustomCalloc);

    /*** CHECKING SCHED INTEGRATION */
    // scheduling a RT task : For Test Purpose	
    TaskFunc handler = mfp_proc;
    rtscheduleDelayedTask(15000, handler, NULL);

    // start disp
    pthread_create(&disp_thread, NULL, 
		   disp_mngr->start,
		   disp_mngr);  

    //start file pump
    pthread_create(&file_pump_thread, NULL,
    		    file_pump_ctxt->start, file_pump_ctxt);

    // craete the pmf processor manager instance
    g_pmgr =\
	create_pmf_processor_mgr(mfp_live_schedule_live_session);
    if (!g_pmgr) {
	printf("Fatal error: cannot continue, unable to "
	       "create PMF processor manager for LIVE listener\n");
	exit(1);
    }
    
    // start auth mgr
    auth_mgr_init(AUTH_MGR_INIT_START_WORKER_TH);
    
    // start encryption manager
    nkn_enc_mgr_enable = 1;
    enc_mgr_init();

    // start pmf processor
    pthread_create(&pmf_process_thread, NULL, mfp_process_pmf,
		   (void*)g_pmgr); 

    // add the watch folder 
    ret = create_dir_watch_obj(watch_dir, g_pmgr, &dwto);
    if (ret) {
	printf("exiting daemon, unable to create directory watch obj"
	       ", errcode: %d\n", ret);
	exit(1);
    }
    
    dir_watch_thread = start_dir_watch(dwto);
    mfp_publ_t* publ_ctx = build_live_request();

    // wait for pmf processor thread to exit
    pthread_join(pmf_process_thread, NULL);

    // wait for disp thread to exit
    pthread_join(disp_thread, NULL);

    //wait for file pump thread to exit
    pthread_join(file_pump_thread, NULL);

    //wait for directory watch thread
    pthread_join(dir_watch_thread, NULL);

    free(glob_net_ip_addr);

    return 0;
}
#endif

void mm_async_thread_hdl()
{
}

static int8_t mfp_init(void) 
{
    pthread_mutex_init(&live_mfp_lock, NULL);
    glob_sched_num_core_threads = 1;

    // INIT Disp Lib : Configuring alloc to use and thread affinity callback
    initDispLib(disp_malloc_custom, disp_calloc_custom, disp_thread_conf,
			log_debug_msg, MOD_MFPLIVE);
    
    // INIT ThreadPool Lib : Configuring alloc to use and thread aff. callback
    initThreadPoolLib(tp_malloc_custom, tp_calloc_custom, tp_thread_conf);
    
    // Initialize the global variables
    disp_mngr = newDispManager(2, 
			       glob_mfp_max_streams_per_sess * glob_mfp_max_sess_supported);
    
    file_pump_ctxt = new_file_pump_context();
    
    live_state_cont = newLiveIdState(glob_mfp_max_sess_supported);

    nkn_rtsched_threads = nkn_calloc_type(glob_rtsched_num_core_threads,
					  sizeof(*nkn_rtsched_threads),
					  mod_nkn_sched_threads);

    nkn_sched_threads = nkn_calloc_type(glob_sched_num_core_threads,
					sizeof(*nkn_sched_threads),
					mod_nkn_sched_threads);

    return 0;
}


static struct sockaddr_in** getIPInterfaces(void) {

	struct ifreq net_if_data;
	int32_t sock = 0, i = 0;
	struct if_nameindex *net_if_list = NULL;
	struct sockaddr_in **res_addr = nkn_calloc_type(MFP_MAX_IP_INTERFACES + 1,
			sizeof(struct sockaddr_in*), mod_mfp_pmf_data);
	if (res_addr == NULL) {
	    DBG_MFPLOG ("INIT", ERROR, MOD_MFPLIVE, "getIPInterfaces : No memory");
		printf("memory alloc failed\n");
		return NULL;
	}

	if( (sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
	    DBG_MFPLOG ("INIT", ERROR, MOD_MFPLIVE,
				"getIPInterfaces :socket error");
		perror("socket");
		goto error_return;
		return NULL;
	}

	net_if_list = if_nameindex();
	struct if_nameindex* net_if_list_ft = net_if_list;

	int32_t ip_if_count = 0;
	for(; net_if_list->if_index != 0; net_if_list++) {

		strncpy(net_if_data.ifr_name, net_if_list->if_name, IF_NAMESIZE);

		if(ioctl(sock, SIOCGIFADDR, &net_if_data) != 0){
			if (errno != EADDRNOTAVAIL) {
				DBG_MFPLOG ("INIT", WARNING, MOD_MFPLIVE,
						"getIPInterfaces :ioctl: %s", net_if_list->if_name);
				perror("ioctl");
			}
			continue;
		}
		res_addr[ip_if_count] = nkn_calloc_type(1, sizeof(struct sockaddr_in),
				mod_mfp_pmf_data);
		if (res_addr[ip_if_count] == NULL)
			goto error_return;
		memcpy(res_addr[ip_if_count], &(net_if_data.ifr_addr),
				sizeof(struct sockaddr_in));
		// debug purpose 
		char ip_addr[INET_ADDRSTRLEN];
		if (inet_ntop(AF_INET, &(res_addr[ip_if_count]->sin_addr.s_addr), 
					&ip_addr[0], INET_ADDRSTRLEN) == NULL) {
			perror("inet_ntop");
		} else
				DBG_MFPLOG ("INIT", MSG, MOD_MFPLIVE, "IF IP ADDR: %s", 
						&(ip_addr[0]));
		// debug end
		ip_if_count++;
		if (ip_if_count == MFP_MAX_IP_INTERFACES)
			break;
	}
	res_addr[ip_if_count] = NULL;
	if_freenameindex(net_if_list_ft);
	close(sock);
	return res_addr;

error_return:
	if (res_addr != NULL) {
		while(res_addr[i] != NULL)
			free(res_addr[i++]);
		free(res_addr);
	}
	return NULL;	
}


static void initIO_Conf(void) {

	// update max open fd limit
	struct rlimit open_fd_limit;
	open_fd_limit.rlim_cur = 2048;
	open_fd_limit.rlim_max = 4096;
	if (setrlimit(RLIMIT_NOFILE, &open_fd_limit) < 0) {
		perror("setrlimit RLIMIT_NOFILE: ");
	    DBG_MFPLOG ("INIT", ERROR, MOD_MFPLIVE, "Error setting max \
				open fd limit");
	}
	getrlimit(RLIMIT_NOFILE, &open_fd_limit);
	// AIO related conf
	struct aioinit aio_conf;
	memset(&aio_conf, 0, sizeof(struct aioinit));
	aio_conf.aio_threads = glob_num_aio_threads;
	aio_conf.aio_num = 10000;
	aio_conf.aio_idle_time = 300;

	aio_init(&aio_conf);
}


static void init_signal_handling(void)
{
    struct sigaction action_cleanup;
    memset(&action_cleanup, 0, sizeof(struct sigaction));
    action_cleanup.sa_handler = exitClean;
    action_cleanup.sa_flags = 0;
    sigaction(SIGINT, &action_cleanup, NULL);
    sigaction(SIGTERM, &action_cleanup, NULL);
}

static void exitClean(int signal_num) 
{
    printf("SIGNAL: %d received Exiting MFP Live Listener\n",
	   signal_num);

    xmlCleanupParser();
    free(xml_schema_data);
    /* remove the directory watch */
    //    inotify_rm_watch(dir_watch_fd, dir_watch_wd);

    /* peform cleanup for all init modules */
    exit(0);
}


static mfp_publ_t* build_live_request(void)
{

    //	mfp_publ_t* publ_ctx = newPublishContext(2, LIVE_SRC);

    return 0;

}


static void* disp_malloc_custom(uint32_t size) {

    return nkn_malloc_type(size, mod_mfp_disp);
}


static void* disp_calloc_custom(uint32_t num, uint32_t size) {

    return nkn_calloc_type(num, size, mod_mfp_disp);
}


static void disp_thread_conf(uint32_t thr_id) {

	pthread_mutex_lock(&live_mfp_lock);
	chooseProcessor(mfp_live_thrd_count);
	mfp_live_thrd_count++;
	char buf[50];
	snprintf(buf, 50, "disp%u", thr_id);
	nkn_mem_create_thread_pool(buf);
	pthread_mutex_unlock(&live_mfp_lock);
}


static void* tp_malloc_custom(uint32_t size) {

    return nkn_malloc_type(size, mod_mfp_live_thread_pool);
}


static void* tp_calloc_custom(uint32_t num, uint32_t size) {

    return nkn_calloc_type(num, size, mod_mfp_live_thread_pool);
}


static void tp_thread_conf(uint32_t thr_id) {

	pthread_mutex_lock(&live_mfp_lock);
	chooseProcessor(mfp_live_thrd_count);
	mfp_live_thrd_count++;
	char buf[50];
	snprintf(buf, 50, "tp%u", thr_id);
	nkn_mem_create_thread_pool(buf);
	pthread_mutex_unlock(&live_mfp_lock);
}


static void et_thread_conf(uint32_t thr_id) {

	pthread_mutex_lock(&live_mfp_lock);
	chooseProcessor(mfp_live_thrd_count);
	mfp_live_thrd_count++;
	char buf[50];
	snprintf(buf, 50, "et%u", thr_id);
	nkn_mem_create_thread_pool(buf);
	pthread_mutex_unlock(&live_mfp_lock);
}


void* mfp_live_malloc_custom(uint32_t size) {

    return nkn_malloc_type(size, mod_mfp_pmf_data);
}

void* mfp_live_calloc_custom(uint32_t num, uint32_t size) {

    return nkn_calloc_type(num, size, mod_mfp_pmf_data);
}

static void* event_timer_calloc_custom(uint32_t num, uint32_t size) {

    return nkn_calloc_type(num, size, mod_mfp_event_timer);
}


static void* IO_Hdlr_custom_calloc(uint32_t num, uint32_t size) {

    return nkn_calloc_type(num, size, mod_mfp_io_hdlr);
}


static void* KMS_CustomMalloc(uint32_t size) {

    return nkn_malloc_type(size, mod_mfp_kms);
}


static void* KMS_CustomCalloc(uint32_t num, uint32_t size) {

    return nkn_calloc_type(num, size, mod_mfp_kms);
}

static int scheduler_init(GThread** gthreads ) {

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

static void mfp_proc(void* data) {
    printf("In Proc.\n");
    TaskFunc handler = mfp_proc;
    //rtscheduleDelayedTask(1000000, mfp_proc, NULL);
}

static nkn_task_id_t rtscheduleDelayedTask(int64_t microseconds,
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


static void mfpLiveHandlerCleanup(nkn_task_id_t id) {

    return;
}


static void mfpLiveHandlerInput(nkn_task_id_t id) {

    //	printf("In Sched Test\n");
    return;
}


static void mfpLiveHandlerOutput(nkn_task_id_t id) {

    return;
}


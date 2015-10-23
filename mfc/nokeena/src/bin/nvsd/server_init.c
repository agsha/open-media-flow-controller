#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <glib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/resource.h>


#include "nvsd_mgmt.h"
#include "nkn_debug.h"
#include "server.h"
#include "network.h"
#include "nkn_mgmt_agent.h"
#include "nkn_cfg_params.h"
#include "nkn_log_strings.h"
#include "nkn_trace.h"
#include "file_manager.h"
#include "nkn_am_api.h"
#include "ssp.h"
#include "nkn_namespace.h"
#include "nkn_vpemgr_defs.h"
#include "nkn_cl.h"
#include "get_manager.h"
#include "nkn_mediamgr_api.h"
#include "nkn_rtsched_api.h"
#include "network_hres_timer.h"
#include "nkn_encmgr.h"
#include "nkn_cim.h"
#include "nkn_dpi_netfilter.h"
#include "nkn_dpi.h"
#include "nkn_pfbridge.h"


extern int glob_rtsched_num_sched_loops;
GThread** nkn_sched_threads = NULL;
GThread** nkn_rtsched_threads = NULL;
extern int valgrind_mode;
extern volatile sig_atomic_t srv_shutdown;
pthread_key_t namespace_key;


extern int al_init_socket(void);
extern int sl_init_socket(void);
extern void server_timer_init(void);
extern void OM_init(void);
extern void RTSP_OM_init(void);
extern void cp_init(void);
extern void fp_init(void);
extern void cpumgr_init(void);
extern void urimgr_init(void);
extern void dns_init(void);
extern void http_accesslog_init(void);
//extern void mgrsvr_init(void);
extern void httpsvr_threaded_init(void);
extern void init_elf(void);
extern void nkn_load_netstack(void);
extern void cache_manager_threaded_init(void);
extern int nvsd_mgmt_thread_init(void);
extern int nvsd_disk_mgmt_thread_init(void);
extern void nkn_start_driver(void);
extern void nkn_init_vfs(void);
extern void RTSP_player_init(void);
extern void nkn_rtsp_init(void);
extern void nkn_fuse_init(void);
//extern int VPEMGR_init(void);
extern void server_timer_1sec(void);
extern int auth_mgr_init(uint32_t init_flags);
extern int compress_mgr_init(uint32_t init_flags);
extern int pe_geo_client_init(void);
extern int pe_ucflt_client_init(void);
extern void virt_cache_server_init(void);

extern int nvsd_system_tp_set_nvsd_state(int state);

void check_nknvd(void);
void daemonize(void);
int server_init(void);
int nkn_system_inited = 0;
int glob_cachemgr_init_done = 0;
extern int nkn_http_service_inited;
unsigned long glob_nvsd_uptime_since = 0;

extern int kernel_space_l3_pxy;

/*
 * ==================> main functions <======================
 * initialization supporting functions
 */
void check_nknvd(void)
{
    DIR *dir = opendir("/proc");
    struct dirent *ent;
    pid_t pid, mypid;
    int len;
    int fd;
    char buffer[4096];

    mypid = getpid();

    while ((ent = readdir(dir)) != NULL) {
	if (!isdigit(ent->d_name[0]))
	    continue;

	pid = atoi(ent->d_name);
	if (pid == mypid)
	    continue;

	snprintf(buffer, 4096, "/proc/%d/cmdline", pid);
	if ((fd = open(buffer, O_RDONLY)) != -1) {
	    // read command line data
	    if ((len = read(fd, buffer, 4096)) > 1) {
		buffer[len] = '\0';
		if (strstr(buffer, "nvsd") != 0) {
		    printf("\nThere is another running nvsd process (pid=%d)\n",
			   pid);
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

void
catcher(int sig)
{
    switch (sig) {
	case SIGHUP: exit(1); break;
	case SIGINT:
	case SIGTERM:
	    srv_shutdown = 1;
	    glob_rtsched_num_sched_loops= 1;
	    break;
	case SIGKILL: exit(1); break;
    }
}


void
daemonize(void)
{
    if (0 != fork())
	exit(0);

    if (-1 == setsid())
	exit(0);

#if 0
    signal(SIGHUP, SIG_IGN);
#endif /* 0 */

    if (0 != fork())
	exit(0);

    /* Do not chdir when this processis managed by the PM
     * if (0 != chdir("/")) exit(0);
     */
}

int
server_init(void)
{
    int i_am_root;
    struct rlimit rlim;
    FILE *fp;
    int err = 0;

    // mon add system init/ http service init
    nkn_mon_add("nvsd.global.system_init_done", NULL,
		(void *)&nkn_system_inited, sizeof(nkn_system_inited));

    nkn_mon_add("nvsd.global.http_service_init_done", NULL,
		(void *)&nkn_http_service_inited, sizeof(nkn_http_service_inited)); 
    /*
     * Catalog 1: all modules depend on this initialization
     */
    system("/opt/nkn/bin/nvsd_init.sh");
    log_thread_start();
    server_timer_1sec();
    NknLogInit();
    DBG_LOG(MSG, MOD_SYSTEM, "nvsd started initialization.");

    pthread_key_create(&namespace_key, NULL);

#if 0
    // Catch all signals
    signal(SIGPIPE,SIG_IGN);
    signal(SIGHUP,catcher);
    signal(SIGKILL,catcher);
    signal(SIGTERM,catcher);
    signal(SIGINT,catcher);

#endif /* 0  */
    i_am_root = (getuid() == 0);
    if (i_am_root) {

	// Set RLIMIT_NOFILE only
	if (0 != getrlimit(RLIMIT_NOFILE, &rlim)) {
	    DBG_LOG(MSG, MOD_SYSTEM,
		    "failed to get rlimit errno=%s", strerror(errno));
	    return -1;
	}

	if (!valgrind_mode) {
	    rlim.rlim_cur = MAX_GNM; // Just set to 64K
	    rlim.rlim_max = MAX_GNM; // Just set to 64K

	    if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
		DBG_LOG(MSG, MOD_SYSTEM, "failed to set rlimit errno=%s",
			strerror(errno));
		return -1;
	    }
	    if (max_virtual_memory != 0) {
		rlim.rlim_cur = max_virtual_memory;
		rlim.rlim_max = max_virtual_memory;
		if (0 != setrlimit(RLIMIT_AS, &rlim)) {
		    DBG_LOG(MSG, MOD_SYSTEM,
			"failed to set rlimit errno=%s",
			strerror(errno));
		    return -1;
		}
	    }
	}
	DBG_LOG(MSG, MOD_SYSTEM,
		"File descriptor rlimit: rlim_cur=%ld rlim_max=%ld",
		rlim.rlim_cur, rlim.rlim_max);
    } else {
	if (0 != getrlimit(RLIMIT_NOFILE, &rlim)) {
	    DBG_LOG(MSG, MOD_SYSTEM,
		    "failed to get rlimit errno=%s", strerror(errno));
	    return -1;
	}
	if ((int)rlim.rlim_max < nkn_max_client) {
	    nkn_max_client = rlim.rlim_cur;
	    DBG_LOG(MSG, MOD_SYSTEM,"reduced rlimit to %d", nkn_max_client);
	    return -1;
	}
	if (max_virtual_memory != 0) {
	    if (0 != getrlimit(RLIMIT_AS, &rlim)) {
		DBG_LOG(MSG, MOD_SYSTEM,
			"failed to get rlimit errno=%s", strerror(errno));
		return -1;
	    }
	    if (rlim.rlim_max < max_virtual_memory) {
		max_virtual_memory = rlim.rlim_cur;
		DBG_LOG(MSG, MOD_SYSTEM,
			"reduced rlimit to %ld", max_virtual_memory);
	    }
	}
    }

    /*
     * Set the core filter to avoid dumping anonymous shared pages
     * This allows us to avoid dumping large regions (e.g. buffer pages)
     * that are not needed for debug.
     */
    fp = fopen("/proc/self/coredump_filter", "w+");
    if (fp == NULL) {
	DBG_LOG(WARNING, MOD_SYSTEM,
		"Unable to open coredump_filter: %s", strerror(errno));
    } else {
	int ret;

	ret = fprintf(fp, "1");
	if (ret < 1)
	    DBG_LOG(WARNING, MOD_SYSTEM,
		    "Unable to set coredump_filter %d", ret);
	fclose(fp);
    }

    /* Get PF dest mac address */
    nkn_pf_get_dst_mac();

    /*
     * Catalog 2: these modules depends on no other modules
     */
    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Libraries");
    mime_hdr_startup();	// Initialize mime header library
    init_elf();             // Initialize ELF structure
    //init_mem_counters();    // Initialize memory counters
    //urimgr_init();		// Initialize URI manager
    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Debug");
    cpumgr_init();		// Initialize CPU
    nkn_trace_init();	// Initialize debug trace tool
    SSP_init();		// Initialize SSP customer types
    g_thread_init(NULL); 	// Initialize Nokeena librarddies 
    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize DB");
    err = nvsd_mgmt_thread_init();// Initialize TM mgmt thread
    if(err) {
	    DBG_LOG(WARNING, MOD_SYSTEM,
			    "Unable to init mgmt_thrd %d", err);
	    exit(1);
    }

    err = nvsd_disk_mgmt_thread_init();// Initialize NVSD Disk mgmt thread
    if(err) {
	    DBG_LOG(WARNING, MOD_SYSTEM,
			    "Unable to init disk_mgmt_thrd %d", err);
	    exit(1);
    }

    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Resolver1");
    if (auth_mgr_init(0/*no init flags*/) == -1) {	// Initialize ADNS
		DBG_LOG(SEVERE, MOD_AUTHHLP, "unable to init auth_mgr()");
		exit(1);
    }
    if (compress_mgr_init(0/*no init flags*/) == -1) {	// Initialize Compress Manager
		DBG_LOG(SEVERE, MOD_SYSTEM, "unable to init compress_mgr()");
		exit(1);
    }
    /*
     * Catalog 3: These modules depend on some other modules.
     * 	      These modules should be initialized at right order.
     */
    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Scheduler");
    nkn_sched_threads = nkn_calloc_type(glob_sched_num_core_threads,
					sizeof(*nkn_sched_threads),
					mod_nkn_sched_threads);
    if (0 != nkn_scheduler_init( nkn_sched_threads, NKN_SCHED_START_MMTHREADS)) {
	DBG_LOG(SEVERE, MOD_SCHED, "scheduler return error. %s", "");
	exit(1);
    }			// Initialize Scheduler thread

    nkn_rtsched_threads = nkn_calloc_type(glob_rtsched_num_core_threads,
					  sizeof(*nkn_rtsched_threads),
					  mod_nkn_sched_threads);
    if (0 != nkn_rtscheduler_init( nkn_rtsched_threads)) {
	DBG_LOG(SEVERE, MOD_SCHED, "scheduler return error. %s", "");
	exit(1);
    }                       // Initialize Scheduler thread

    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Resolver2")
    dns_init();		// Initializes the DNS hash table

    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Analytics");
    AM_init();		// Initializes the analytics manager
    MM_init();              // Media Manager initializes all providers

    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Memory in parallel");
    cache_manager_threaded_init();   // Initialize BM, including COD


    if (l4proxy_enabled && (nkn_init_health_info() == -1)) { // Initialize health info of nvsd
       DBG_LOG(SEVERE, MOD_NETWORK, "unable to init health monitor for proxyd");
       exit(1);
    }

    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Timer");
    //   which includes DiskManager
    NM_init();		// Initialize network manager
    network_hres_timer_init(); // Initialize high res network timer
    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize CL");
    CL_init();		// Initialize Cluster subsystem
    load_cluster_map_init();	// Load cluster map component initialization

    /*
     * Catalog 4: Modules to be initialized at last
     *	      Within this module, order might be important too.
     */
    if (net_use_nkn_stack)
	nkn_load_netstack();	// Initialize the user space net stack
    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Disk");
    DM2_init();		// Initialize DM2: depends on BM, namespace
    //mgrsvr_init();		// Initialize manager socket servers
    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Logger");
    al_init_socket();	// Initialize the Accesslog agent: ==> NM
    sl_init_socket();	// Initialize the StreamLog agent: ==> NM

    DBG_LOG(SEVERE,MOD_SYSTEM, "Initialize Network");
    cp_init();		// Initailize Connection Pool: ==>NM
    fp_init();
    OM_init();		// Initailize OM: ==> NM, BM, COD
    RTSP_OM_init();         // Initialize RTSP OM
    server_timer_init();	// Initialize timer thread
    nkn_init_vfs();         // Initialize virtual file system
    VPEMGR_init();		// Initialize VPEMGR
    //get_manager_init();	// Initialize get manager thread

    /*
     * Catalog 5: External services
     */
    DBG_LOG(SEVERE,MOD_SYSTEM, "Initialize External Services");
    nkn_fuse_init();		// Initialize fuse module thread
    nkn_rtsp_init();		// Initialize rtsp module
    RTSP_player_init();		// Initialize rtsp player module
    file_manager_init();	// Initialize file manager thread
    pe_geo_client_init();	// Initialize PE external server client
    pe_ucflt_client_init();	// Initialize PE external server client
    enc_mgr_init();                // Initialize Encryption Manager
    //virt_cache_server_init();	// Initialize virt_cache manager thread


    DBG_LOG(SEVERE,MOD_SYSTEM, "Initialize Http Service");
    httpsvr_threaded_init(); 	// Initialize Socket : ==> every modules


    
    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Namespace Service");
    nkn_init_namespace(is_local_ip);	// Initialize namespace manager

    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize Cache Interface Service");
    cim_init();

    DBG_LOG(SEVERE, MOD_SYSTEM, "Initialize DPI Netfilter Service");
#ifdef NFQUEUE_URI_FILTER
    nkn_nfqueue_init();
#endif
    if (nkn_cfg_dpi_uri_mode) {
	nkn_dpi_init();
	nkn_pfbridge_init();
    }

    if(kernel_space_l3_pxy) {
        // Notify TPROXY module
        nvsd_system_tp_set_nvsd_state(1 /* NVSD is UP */);
    }

    /*
     * Give some indication to user
     */
    DBG_LOG(ALARM, MOD_SYSTEM,
	    "-- nvsd initialization is done --");
    nkn_system_inited = 1;
    if (net_use_nkn_stack) {
	sleep(1);
	nkn_start_driver();	// Initialize the user space net stack
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    glob_nvsd_uptime_since = tv.tv_sec;

    return 1;
}

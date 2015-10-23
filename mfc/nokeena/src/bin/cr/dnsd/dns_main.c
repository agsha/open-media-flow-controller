/**
 * @file   dns_main.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Mon Feb 19 15:57:00 2012
 * 
 * @brief  entry point for the DNS server daemon.Initializes the
 * modules, loads defaults, starts the DNS server
 * 
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <netinet/in.h>
#include <net/if.h> //if_nameindex()
#include <sys/socket.h>
#include <arpa/inet.h> //inet_ntop()
#include <sys/types.h>
#include <sys/ioctl.h>
#include <aio.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <sys/un.h>

// nkn common defs
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_mem_counter_def.h"
#include "nkn_sched_api.h"
#include "nkn_slotapi.h"
#include "nkn_authmgr.h"
#include "nkn_encmgr.h"
#include "../../../lib/nvsd/rtsched/nkn_rtsched_task.h"
#include "common/nkn_free_list.h"

#include "event_dispatcher.h"
#include "entity_data.h"
#include "mfp_thread_pool.h"
#include "mfp_event_timer.h"
#include "cont_pool.h"
#include "common/cr_utils.h"
#include "common/cr_cfg_file_api.h"
#include "dns_udp_handler.h"
#include "dns_common_defs.h"
#include "cr_common_intf.h"
#include "dns_proto_handler.h"
#include "dns_con.h"
#include "dns_server.h"
#include "dns_mgmt.h"

/* globals */
mfp_thread_pool_t* gl_th_pool = NULL;
event_timer_t* gl_ev_timer = NULL;
pthread_t gl_ev_thread;
dns_disp_mgr_t gl_disp_mgr;
pthread_t gl_disp_thread, gl_mgmt_thread;
cont_pool_t* gl_cp = NULL;
GThread** nkn_sched_threads = NULL;
time_t nkn_cur_ts; /* global time measured every 5 seconds */
int nkn_assert_enable = 0;
int rtsched_enable = 1;
obj_list_ctx_t *dns_con_ctx_list = NULL;

/* counters */
AO_t glob_dns_tot_transactions = 0;
AO_t glob_dns_max_con_limit_err = 0;

/* config defaults */
cr_dns_cfg_t g_cfg;

/* config from file */
cr_cfg_param_val_t cr_dns_cfg [] = {
    {{"global.listen_port", CR_INT_TYPE}, 
     &g_cfg.dns_listen_port},
    {{"global.max_disp_desc", CR_INT_TYPE},
     &g_cfg.max_disp_descs},
    {{"global.max_th_tasks", CR_INT_TYPE},
     &g_cfg.max_th_pool_tasks},
    {{"global.default_dns_ttl", CR_INT_TYPE},
     &g_cfg.def_dns_ttl},
    {{NULL, CR_INT_TYPE}, NULL}
};

/* static definitions */
static void init_signal_handling(void); 
static void exitClean(int signal_num);
static int scheduler_init(GThread** gthreads );
static void setup_cfg_defaults(void);

static int32_t initStoreConnections(void); 

/* extern definitions */
extern void config_and_run_counter_update(void); //API as an extern?!

int32_t main(int32_t argc, char *argv[]) {

    int32_t err = 0, ret = 0;
    const char *cfg_file_name_def = 
	"/config/nkn/cr_dns.conf.default";
    char *cfg_file_name = (char *)cfg_file_name_def;
    uint32_t tot_disp, i = 0;

    /* basic inits: config, GLIB, signal handlers */
    pthread_mutex_init(&g_cfg.lock, NULL);
    init_signal_handling();
    g_thread_init(NULL);
    setup_cfg_defaults();

    /* parse command line parameters */
    while ((ret = getopt(argc, argv, "F:")) != -1) {
	switch(ret) {
	    case 'F':
		cfg_file_name = strdup(optarg);
		break;
	}
    }

    /* start nknlog thread */
    log_thread_start();
    sleep(2);

    /* load config from file */
    err = cr_read_cfg_file(cfg_file_name,
	    cr_dns_cfg);
    if (err) {
	DBG_LOG(WARNING, MOD_NETWORK, "error loading config file "
		"[err = %d], reverting to runtime defaults", err);
    }

    /* start mgmt plane */
    pthread_create(&gl_mgmt_thread, NULL, dns_mgmt_init, &g_cfg);

    /* start the counter shared memory copy infra */
    config_and_run_counter_update();

    /* create a thread pool */
    gl_th_pool = newMfpThreadPool(1, g_cfg.max_th_pool_tasks);

    /* create an timed event scehduler */
    gl_ev_timer = createEventTimer(NULL, NULL);
    pthread_create(&gl_ev_thread, NULL, 
	    gl_ev_timer->start_event_timer, gl_ev_timer);

    /* enumerate the interface list */
    g_cfg.ip_if_addr = getIPInterfaces(g_cfg.max_ip_interfaces);
    if (!g_cfg.ip_if_addr) {
	DBG_LOG(SEVERE, MOD_NETWORK, "error enumerating devices; "
		"exiting now");
	exit(-1);
    }

    /* create connection list */
    err = obj_list_create(100000, dns_con_create, NULL,
	    &dns_con_ctx_list);
    if (err) {
	DBG_LOG(SEVERE, MOD_NETWORK, "error creating connection list "
		"err code: %d, exiting now", err);
	exit (-1);
    }

    if (initStoreConnections() < 0) {

	printf("Init store connections failed\n");
	DBG_LOG(SEVERE, MOD_NETWORK, "error creating store connections");
	if (gl_cp != NULL) {
	    gl_cp->delete_hdlr(gl_cp);
	    gl_cp = NULL;
	}
    }

    /* create network event dispatcher and start the UDP and TCP server
     * on the dispatcher
     */
    HOLD_CFG_OBJ(&g_cfg);
    memset(&gl_disp_mgr, sizeof(dns_disp_mgr_t), 0);
    if (g_cfg.rx_threads == 0) {
	g_cfg.rx_threads = 1;
    } 
    if (g_cfg.tx_threads == 0) {
	g_cfg.tx_threads = 1;
    }	    
    tot_disp = g_cfg.rx_threads +  g_cfg.tx_threads;
    if (tot_disp > DNS_MAX_DISP) {
	DBG_LOG(SEVERE, MOD_NETWORK, "number of rx threads %d "
		"and tx threads %d exceed the total number "
		"of available dispatchers %d, reverting to "
		"defaults", g_cfg.rx_threads, g_cfg.tx_threads,
		DNS_MAX_DISP);
	g_cfg.tx_threads = 1;
	g_cfg.rx_threads = 1;
	tot_disp = 2;
    }
    gl_disp_mgr.num_disp = tot_disp;
    REL_CFG_OBJ(&g_cfg);
    for (i = 0; i < tot_disp; i++) {
	gl_disp_mgr.disp[i] = 
	    newEventDispatcher(0, g_cfg.max_disp_descs, 1);
	pthread_create(&gl_disp_mgr.disp_thread[i], NULL, 
	    gl_disp_mgr.disp[i]->start_hdlr, gl_disp_mgr.disp[i]);
	err = dns_start_udp_server(g_cfg.ip_if_addr, 
				   gl_disp_mgr.disp[i]);
	if (err) {
	    DBG_LOG(SEVERE, MOD_NETWORK, "error starting the DNS UDP " 
		    " server with err code = %d, exiting now", err);
	    /* any error here is FATAL to service and the service
	     * cannot procced
	     */
	    exit(-1);
	}
    }
    err = dns_start_tcp_server(g_cfg.ip_if_addr, gl_disp_mgr.disp[0], 
	    dns_con_ctx_list);
    if (err) {
	DBG_LOG(SEVERE, MOD_NETWORK, "error starting the DNS TCP " 
		" server with err code = %d, exiting now", err);
	/* any error here is FATAL to service and the service
	 * cannot procced
	 */
	exit(-1);
    }

    /* wait for the dispatcher(s) to complete;
     * SHOULD NEVER HAPPEN under normal circumstances
     * CAN HAPPEN only if a SIGKILL/QUIT is explicitly sent
     */
    pthread_join(gl_disp_mgr.disp_thread[0], NULL);

    return 0;
}


static void init_signal_handling(void) {

    struct sigaction action_cleanup;
    memset(&action_cleanup, 0, sizeof(struct sigaction));
    action_cleanup.sa_handler = exitClean;
    action_cleanup.sa_flags = 0;
    sigaction(SIGINT, &action_cleanup, NULL);
    sigaction(SIGTERM, &action_cleanup, NULL);
    action_cleanup.sa_handler = SIG_IGN;
    action_cleanup.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &action_cleanup, NULL);
}

static void exitClean(int signal_num) {

    printf("SIGNAL: %d received\n", signal_num);
    //TODO : termination cleanup 
    exit(-1);
}

static int scheduler_init(GThread** gthreads ) {

    int i;
    GError           *err1 = NULL ;

    nkn_rttask_init();
    //c_rtsleeper_q_tbl_init();
    nkn_rttask_q_init();
    nkn_rtsched_tmout_init();

    if( 0 != nkn_scheduler_init( nkn_sched_threads, (~NKN_SCHED_START_MMTHREADS))) {
	DBG_LOG (SEVERE, MOD_MFPLIVE, "Error Initializing Scheduler "
		":exiting now");
	exit(1);
    }

#if 0
    nkn_task_register_task_type(TASK_TYPE_CR_DNS_STORE_LOOKUP1,
	    &dns_domain_lookup_input,
	    &dns_domain_lookup_output,
	    &dns_domain_lookup_cleanup);
#endif

    return 0;
}

#if 0
void dns_domain_lookup_input(nkn_task_id_t tid)
{
}

void dns_domain_lookup_output(nkn_task_id_t tid)
{
}

void dns_domain_lookup_cleanup(nkn_task_id_t tid)
{
}
#endif 

/** 
 * set up the config defaults
 * 
 */
static void setup_cfg_defaults(void) {
    g_cfg.max_disp_descs = 30000;
    g_cfg.max_th_pool_tasks = 100000;
    g_cfg.max_ip_interfaces = 10;
    g_cfg.dns_listen_port = 53;
    g_cfg.mtu = 1400;
    g_cfg.dns_udp_max_pkt_size = 152;
    g_cfg.dns_qr_buf_size = 2048;
    g_cfg.def_dns_ttl = 0;
}


static int32_t initStoreConnections(void) {


    gl_cp = createContainerPool(1, 100);
    if (gl_cp == NULL)
	return -1;
    uint32_t i = 0;
    for (; i < 100; i++) {

	int32_t* fd = malloc(sizeof(int32_t));
	if (fd == NULL)
	    return -1;
	*fd = makeUnixConnect(STORE_SOCK_FILE);
	/*
	if (*fd < 0)
	    return -1;
	    */
	gl_cp->put_cont_hdlr(gl_cp, fd);
    }
    return 0;
}


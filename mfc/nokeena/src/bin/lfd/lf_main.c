#include <stdio.h>
#include <pthread.h>
#include <syslog.h>
#include <netinet/in.h>
#include <net/if.h> //if_nameindex()
#include <sys/socket.h>
#include <arpa/inet.h> //inet_ntop()
#include <sys/types.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>

// libxml
#include <libxml/xmlwriter.h>

#include "nkn_defs.h"
#include "interface.h"
#include "dispatcher_manager.h"
#include "lf_uint_ctx_cont.h"
#include "lf_nw_event_handler.h"

#include "nkn_mem_counter_def.h"
#include "lf_common_defs.h"
#include "lf_config_file_read_api.h"
#include "lf_metrics_monitor.h"
#include "lf_err.h"
#include "lf_dbg.h"

#define LFD_SERVER_PORT 2010

dispatcher_mngr_t* disp_mngr;
pthread_t disp_thread;
uint_ctx_cont_t* uint_ctx_cont;
lf_app_list_t na_list = {1, {"/opt/nkn/sbin/nvsd"}};
lf_app_list_t vma_list = {1, {"wms"}};
lf_app_metrics_intf_t *g_app_intf[LF_MAX_SECTIONS * LF_MAX_APPS];
extern lf_app_metrics_intf_t lf_na_http_metrics_cb;
extern lf_app_metrics_intf_t lf_vma_metrics_cb;
extern lf_app_metrics_intf_t lf_sys_glob_metrics_cb;
extern void config_and_run_counter_update(void);
lf_metrics_ctx_t *lfm = NULL;

uint32_t g_lfd_listen_port = LFD_SERVER_PORT;
uint32_t g_lfd_processor_count = 0xffffffff;
lf_sys_glob_max_t g_sys_max_cfg = {0,0,0,0,0};
uint32_t g_lfd_sampling_interval = 500;
uint32_t g_lfd_window_length = 5000;
uint32_t g_lfd_ext_window_length = 60000;
uint32_t g_lfd_log_level = 3;
char* g_lfd_root_disk_label = NULL;
char* g_lfd_xml_ns_str = NULL;
uint32_t g_na_http_show_vip_stats = 0;

static void init_signal_handling(void);
static void exitClean(int signal_num);
static void initPhysicalProcessorCount(void);
static int32_t lf_validate_cfg_params(void);
// function to get IP addresses of all the local networks interfaces
static struct sockaddr_in** getIPInterfaces(void);
struct sockaddr_in** glob_net_ip_addr = NULL;

#define MFP_MAX_IP_INTERFACES (MAX_NKN_ALIAS+MAX_NKN_INTERFACE)

lf_cfg_param_val_t lfd_cfg[] = {
    {{"lfd.listen_port", LF_INT_TYPE}, &g_lfd_listen_port},
    {{"lfd.sys.glob.tcp_conn_max", LF_INT_TYPE},
     &g_sys_max_cfg.tcp_conn_rate_max},
    {{"lfd.sys.glob.disk_io_util_max", LF_INT_TYPE},
     &g_sys_max_cfg.blk_io_rate_max},
    {{"lfd.sys.glob.if_bw_max", LF_INT_TYPE},
     &g_sys_max_cfg.if_bw_max},
    {{"lfd.sys.glob.http_tps_max", LF_INT_TYPE},
     &g_sys_max_cfg.http_tps_max},
    {{"lfd.sys.glob.tcp_active_conn_max", LF_INT_TYPE},
     &g_sys_max_cfg.tcp_active_conn_max},
    {{"lfd.sampling_interval", LF_INT_TYPE},
     &g_lfd_sampling_interval},
    {{"lfd.window_length", LF_INT_TYPE},
     &g_lfd_window_length},
    {{"lfd.log_level", LF_INT_TYPE},
     &g_lfd_log_level},
    {{"lfd.root_disk_label", LF_STR_VAL_TYPE},
     &g_lfd_root_disk_label},
    {{"lfd.xmlns", LF_STR_VAL_TYPE},
     &g_lfd_xml_ns_str},
    {{"lfd.na.http.show_vip_stats", LF_INT_TYPE}, 
     &g_na_http_show_vip_stats},
    {{NULL, LF_INT_TYPE}, NULL}
};

int main(int argc, char *argv[]) {

        int32_t err = 0, ret = 0;
	char *cfg_file_name = NULL;
	pthread_attr_t attr;
	size_t stacksize;

	/* init GLIB */
	g_thread_init(NULL);

	/* init XML */
	xmlInitParser();

	/* parse command line parameters */
	while ((ret = getopt(argc, argv, "F:")) != -1) {
	    switch(ret) {
		case 'F':
		    cfg_file_name = strdup(optarg);
		    break;
	    }
	}
    
	/* update config parameters from file */
	err = lf_read_cfg_file(cfg_file_name, lfd_cfg);
	if (err == -E_LF_INVAL) {
	    printf("no config file, loading defaults\n");
	}
	err = lf_validate_cfg_params();

	if (err) {
		printf("config parameter validation"
		   " failed with errcode %d, restart with valid "
		   "configuration", err);
	    exit (-1);
	}

	/* initialize logging to syslog */
	printf("g_lfd_log_level : %u\n", g_lfd_log_level);
	setlogmask(LOG_UPTO (g_lfd_log_level));
	openlog ("lfd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

	// start the counter monitor
	config_and_run_counter_update();

	/* detect proc count */
	initPhysicalProcessorCount();
	if (g_lfd_processor_count == 0xffffffff) {
	    LF_LOG(LOG_ERR, LFD_MAIN, "unable to detect processor"
		   " count, exitng daemon");
	    return -1;
	}

	/* initialize dispatcher manager */
	disp_mngr = newDispManager(LFD_NUM_DISP_THREAD, LFD_MAX_FD_LIMIT);
	if (disp_mngr == NULL)
		exit(-1);

	pthread_attr_init(&attr);
	pthread_attr_getstacksize (&attr, &stacksize);
	stacksize = 512*1024;
	pthread_attr_setstacksize (&attr, stacksize);
	pthread_create(&disp_thread, &attr,
			disp_mngr->start,
			disp_mngr);

	uint_ctx_cont = createUintCtxCont(LFD_MAX_FD_LIMIT);
	if (uint_ctx_cont == NULL)
		exit(-1);

	glob_net_ip_addr = getIPInterfaces();
	if (glob_net_ip_addr == NULL) {

        LF_LOG(LOG_ERR, LFD_MAIN, "unable to get All IP interfaces\n"); 
		exit(-1);
	}

	/* initialize metrics monitor */
	err = lf_app_metrics_intf_register(g_app_intf, LF_NA_SECTION,
					   LF_NA_HTTP, &lf_na_http_metrics_cb);
	if (err) {
	    LF_LOG(LOG_ERR, LFD_MAIN, "error registering HTTP metrics interface\n");
	    return -1;
	}
	err = lf_app_metrics_intf_register(g_app_intf, LF_VM_SECTION,
					   LF_VMA_WMS, &lf_vma_metrics_cb);
	if (err) {
	    LF_LOG(LOG_ERR, LFD_MAIN, "error registering WMS metrics interface\n");
	    return -1;
	}
	err = lf_app_metrics_intf_register(g_app_intf, LF_SYSTEM_SECTION,
					   LF_SYS_GLOB, &lf_sys_glob_metrics_cb);
	if (err) {
	    LF_LOG(LOG_ERR, LFD_MAIN, "error registering global system metrics interface\n");
	    return -1;
	}
	
	err = lf_metrics_create_ctx(&na_list, &vma_list, 
				    g_lfd_sampling_interval,
				    g_lfd_window_length, 
				    g_lfd_ext_window_length,
				    5, &lfm);
	if (err) {
	    LF_LOG(LOG_ERR, LFD_MAIN, "error init metrics context; errcode = %d\n",
		   err);
	    return -1;
	}
	
	err = lf_metrics_start_monitor(lfm);
	if (err) {
	    LF_LOG(LOG_ERR, LFD_MAIN, "error starting metrics monitor; errcode = %d\n",
		   err);
	    return -1;
	}

	sleep(5);// to allow time for metrics monitor to startup

	int32_t net_idx = 0;
	while (glob_net_ip_addr[net_idx] != NULL) {

		/* initialize server */
		int32_t sr_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (sr_fd < 0) {
			perror("server socket failed: ");
			exit(-1);
		}

		int32_t val = 1;
		if (setsockopt(sr_fd, SOL_SOCKET, SO_REUSEADDR,&val,sizeof(val)) < 0) {
			perror("setsockopt failed: ");
			exit(-1);
		}

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(g_lfd_listen_port);
		addr.sin_addr.s_addr = glob_net_ip_addr[net_idx++]->sin_addr.s_addr; 
		if (bind(sr_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			perror("bind: ");
			exit(-1);
		}

		if (listen(sr_fd, 100) < 0) {
			perror("listen: ");
			exit(-1);
		}

		/* tie entity context and dispatcher */
		entity_context_t* entity_ctx = newEntityContext(sr_fd,
				NULL, NULL,lfdListenHandler, NULL, NULL, NULL, NULL, disp_mngr);
		if (entity_ctx == NULL)
			exit(-1);
		entity_ctx->event_flags |= EPOLLIN;
		disp_mngr->register_entity(entity_ctx);
	}


	LF_LOG(LOG_NOTICE, LFD_MAIN, "LFD initialized and listening on"
	       " port %d", g_lfd_listen_port);
	pthread_join(disp_thread, NULL);

	return 0;
}

void initPhysicalProcessorCount(void) 
{

	FILE* fp;
	char buf[1024];

	fp = fopen("/proc/cpuinfo", "r");
	if (fp == NULL)
		return;
	uint32_t phy_id[32] = {0};
	uint32_t phy_id_cpu_count[32] = {0};
	int32_t err = 0;
	int64_t id_val = -1;
	while (!feof(fp)) {

		if (fgets(buf, 1024, fp) == NULL)
			break;
		if (strncmp(buf, "physical id", 11) == 0) {
			char* id_val_str = strchr(buf, ':');
			if (id_val_str == NULL) {
				err = -1;
				break;
			}
			char* tmp_buf = NULL;
			id_val = strtol(id_val_str + 1, &tmp_buf, 0);
			if (strcmp(id_val_str, tmp_buf) == 0) {
				err = -2;
				break;
			}
			phy_id[id_val]++;
		} else if (strncmp(buf, "cpu cores", 9) == 0) {
			char* cc_val_str = strchr(buf, ':');
			if (cc_val_str == NULL) {
				err = -3;
				break;
			}
			char* tmp_buf = NULL;
			int64_t cc_val = strtol(cc_val_str + 1, &tmp_buf, 0);
			if (strcmp(cc_val_str, tmp_buf) == 0) {
				err = -4;
				break;
			}
			if (id_val == -1) {
				// "cpu cores" found before "physical id"
				err =-5;
				break;
			}
			if (phy_id_cpu_count[id_val] == 0)
				phy_id_cpu_count[id_val] = cc_val;
			id_val = -1;
		}
	}
	uint32_t i, phy_proc_count = 0, log_proc_count = 0;
	for (i = 0; i < 32; i++) {
		phy_proc_count += phy_id_cpu_count[i];
		log_proc_count += phy_id[i];
	}
	printf("Count of physical processors: %u\n", phy_proc_count);
	printf("Count of logical processors: %u\n", log_proc_count);

	g_lfd_processor_count = log_proc_count;

	fclose(fp);
	return;
}


static void init_signal_handling(void) {

	struct sigaction action_cleanup;
	memset(&action_cleanup, 0, sizeof(struct sigaction));
	action_cleanup.sa_handler = exitClean;
	action_cleanup.sa_flags = 0;
	sigaction(SIGINT, &action_cleanup, NULL);
	sigaction(SIGTERM, &action_cleanup, NULL);
}


static void exitClean(int signal_num) {

	printf("SIGNAL: %d received Exiting LFD\n", signal_num);
	exit(0);
}


static struct sockaddr_in** getIPInterfaces(void) {

	struct ifreq net_if_data;
	int32_t sock = 0, i = 0;
	//struct if_nameindex *net_if_list = NULL;
	struct sockaddr_in **res_addr = nkn_calloc_type(MFP_MAX_IP_INTERFACES + 1,
			sizeof(struct sockaddr_in*), mod_mfp_pmf_data);
	if (res_addr == NULL) {
		LF_LOG(LOG_ERR, LFD_MAIN, "getIPInterfaces : No memory\n");
		return NULL;
	}

	if( (sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		LF_LOG(LOG_ERR, LFD_MAIN, "getIPInterfaces : socket error\n");
		perror("socket");
		goto error_return;
		return NULL;
	}

	//net_if_list = if_nameindex();
	//struct if_nameindex* net_if_list_ft = net_if_list;
	struct ifaddrs *ifap = NULL, *net_if_list = NULL;
	if (getifaddrs(&ifap)) {
	    LF_LOG(LOG_ERR, LFD_MAIN, "getIPInterfaces : IF list "
		   "enumeration failed: [err=-%d]", errno);
	    perror("IF enumearation failed:");
	    return NULL;
	}

	int32_t ip_if_count = 0, max_if = MFP_MAX_IP_INTERFACES;
	net_if_list = ifap;
	for(; net_if_list; net_if_list=net_if_list->ifa_next) {
    	        if (!net_if_list->ifa_addr) {
		    continue;
		}
		if (net_if_list->ifa_addr->sa_family != AF_INET) {
		    continue;
		}
		strncpy(net_if_data.ifr_name, net_if_list->ifa_name, IF_NAMESIZE);
		//		printf("IF name %s\n", net_if_list->ifa_name);
		if(ioctl(sock, SIOCGIFADDR, &net_if_data) != 0){
			if (errno != EADDRNOTAVAIL) {
				LF_LOG(LOG_INFO, LFD_MAIN, "getIPInterfaces :ioctl: %s",
						net_if_list->ifa_name); 
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
			LF_LOG(LOG_INFO, LFD_MAIN, "IF IP ADDR: %s", &(ip_addr[0]));
		// debug end
		ip_if_count++;
		
		if (ip_if_count == max_if)
			break;
	}
	res_addr[ip_if_count] = NULL;
	//if_freenameindex(net_if_list_ft);
	freeifaddrs(ifap);
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

static int32_t 
lf_validate_cfg_params(void)
{
    int32_t err = 0;

    if (g_lfd_listen_port >=4096) {
	err = -E_LF_INVAL_LISTEN_PORT;
	LF_LOG(LOG_ERR, LFD_MAIN, "listen port %d should be lesser"
	       " than 4095 - errcode %d", g_lfd_listen_port, err);
	goto error;
    }

    if (!g_lfd_sampling_interval ||
	g_lfd_sampling_interval <  LF_MIN_SAMPLING_RES) {
	err = -E_LF_INVAL_SAMP_RATE;
	LF_LOG(LOG_ERR, LFD_MAIN, "sampling rate %d, should not be lesser "
	       "than %d, errcode %d", g_lfd_sampling_interval,
	       LF_MIN_SAMPLING_RES, err);
	goto error;
    }

    if (!g_lfd_window_length ||
	g_lfd_window_length < LF_MIN_WINDOW_LEN) {
	err = -E_LF_INVAL_WND_LEN;
        LF_LOG(LOG_ERR, LFD_MAIN, "window length %d, should not be lesser"
               " than %d, errcode %d", g_lfd_window_length,
	       LF_MIN_WINDOW_LEN, err);
	goto error;
    }

    if ( (g_lfd_sampling_interval > g_lfd_window_length) ||
	 ((g_lfd_window_length % g_lfd_sampling_interval) != 0) ) {
	err = -E_LF_SAMP_RATE_WND_LEN_INCOMPAT;
	LF_LOG(LOG_ERR, LFD_MAIN, "the sampling interval should be"
	       " lesser than the window length and the window"
	       " length should be a integral multiple of the "
	       "sampling interval - errcode %d", err);
	goto error;
    }

	if (g_lfd_root_disk_label == NULL) {
		g_lfd_root_disk_label = malloc(sizeof(char) * 4);
		strcpy(g_lfd_root_disk_label, "sda");
	}

 error:
    return err;
}

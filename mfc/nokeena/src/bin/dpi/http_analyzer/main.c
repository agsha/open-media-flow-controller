/*
 * File: main.c
 *   
 * Author: Sivakumar Gurmurthy & Ramanand Narayanan
 *
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef _STUB_
int
main (int argc, const char **argv)
{
}

#else /* !_STUB_ */

/* Include Files */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sched.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>

/* Include DPI/DPDK headers */
#include <rte_config.h>
#include <rte_ring.h>

#include "event_handler.h"
#include "thread.h"
#include "nkn_shm.h"

#include "atomic_ops.h"
#include "nkn_stat.h"
#include "nkn_debug.h"


#define HWA_SOFT_QUEUE_SIZE 1024 * 64

/* Global Variables */
volatile int application_not_stopped = 1;
unsigned long long packets_count = 0;
unsigned long long packets_dropped = 0;
unsigned long long packets_queued = 0;

/* Function Prototypes */
static int dpi_performance_tuning(void);
static int dpi_init(void);
void run_threads(void);

/* Extern Prototypes */
extern int dpdk_init(int argc, const char **argv);
extern int dpdk_main();

extern void config_and_run_counter_update(void );

/*
 * Array of list of packets for all threads
 */
struct smp_thread smp_thread[NUM_THREADS];

DECLARE_DPI_ENGINE_DATA(deng_data);


/********************************************************************************
 * function: on_expire_session()
 * 
 ********************************************************************************/
static void
on_expire_session(DPI_ENGINE_SESSIONCB_PARAM(u, dev))
{
    void *user_handle = DPI_ENGINE_SESSIONCB_HANDLE(u);
    int thread_id;

    /* Get the thread id to reference into the global array */
    thread_id = afc_get_cpuid();

   // printf ("[%d]@on_expire_session (%p) \n", thread_id, user_handle);
    if(user_handle) {
	datalist *httplist = (datalist *)user_handle;
	datalist_free(&httplist);
	user_handle = NULL;
    }
} /* end of on_expire_session() */


/********************************************************************************
 * function: ctrlc_handler()
 * 
 ********************************************************************************/
static void
ctrlc_handler(int sig __attribute ((unused)))
{
	application_not_stopped = 0;
} /* end of ctrlc_handler() */


/********************************************************************************
 * init_signals()
 * Capture signals for Ctrl-C
 ********************************************************************************/
static void
init_signals(void) 
{
    struct sigaction action;

    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = ctrlc_handler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT,  &action, NULL);
} /* end of init_signals () */

/********************************************************************************
 * This callback will be called by DPI for exception.
 ********************************************************************************/
static void
exception_handler(ctb_uint32 exception_type, void *args, void *params) 
{
    (void)args;
    (void)params;
    if (exception_type == CTB_EXCEPTION_NO_MEMORY){
        printf("ixengine exception: no memory!\n");
    }
} /* end of exception_handler() */

/********************************************************************************
 * ixEngine performance tuning
 ********************************************************************************/
static int
dpi_performance_tuning(void)
{
	//Performance tuning.
	clep_proto_t proto;

	PROTO_CLEAR(&proto);

	proto.unique_id = Q_PROTO_HTTP;
	//Instruct the DPI engine to ignore the http content
	if (ctl_proto_set_proto_tune(&proto, "server_content_bypass", 1) < 0)
		DBG_LOG(ERROR, MOD_DPI_TPROXY,"Not able to tune the http protocol");
	if (ctl_proto_set_proto_tune(&proto, "disable_fast_cases", 1) < 0)
		DBG_LOG(ERROR, MOD_DPI_TPROXY,"Not able to tune the http protocol");

	/* Return */
	return 0;

} /* end of dpi_performance_tuning() */


/********************************************************************************
 * DPI engine intialization 
 ********************************************************************************/
static int
dpi_init(void)
{
        struct clep_config_params cp;
	clep_proto_t _proto;
	int err;


	/* Init the config parameters */
        config_params_init(&cp);

        //cp.td_maxcnx = 5000;

        if (uafc_init_params(&cp) < 0) {
            DBG_LOG(ERROR, MOD_DPI_TPROXY,"DPI init error");
                return -1;
        }
        if (ctl_proto_enable_all() < 0) { // enable all protocols ix DPI
                DBG_LOG(ERROR, MOD_DPI_TPROXY,"error enabling protocols");
                return -1;
        }

	//Enable only the http protocol
	ctl_proto_disable_all();
	PROTO_CLEAR(&_proto);
	strcpy(_proto.name, "http");
	err = ctl_proto_enable(&_proto);
	if (err < 0)
	{
	    DBG_LOG(ERROR, MOD_DPI_TPROXY,"error enabling http protocol");
	    return -1;
	}
	
        afc_set_callback_expire_session(on_expire_session);
        afc_set_callback_exception(exception_handler, NULL);

        event_handler_init(); // initialize our event handler
	ALLOC_DPI_ENGINE_DATA_ENGINE(&deng_data, NUM_THREADS);
        dpi_engine_init_nr(&deng_data, NUM_THREADS);

	return 0;

} /* end of dpi_init() */

/********************************************************************************
 * Function to create and run threads
 ********************************************************************************/
void
run_threads(void)
{
	int i;
	cpu_set_t cpu_set;


	for (i = 0; i < NUM_THREADS; i++) 
	{
		char name[] = "QUEUE-0";

		/* Change the queue number in the string */
		name[sizeof(name) - 2] = i + '0';

		/* Initialize thread datastructure */
		init_list_head((struct list_entry *)&smp_thread[i].pkt_head);
		pthread_spin_init(&smp_thread[i].lock, 0);
		smp_thread[i].thread_number = i;
		smp_thread[i].core_id = i + 2; // CPU affinity starting with 2nd
		smp_thread[i].queue_id = i; 
		smp_thread[i].queue = rte_ring_create(name, HWA_SOFT_QUEUE_SIZE,
					0, RING_F_SP_ENQ | RING_F_SC_DEQ);
		DBG_LOG(MSG, MOD_DPI_TPROXY,"Creating thread %d", i);

		/* Start the thread */
		pthread_create(&smp_thread[i].handle, NULL, smp_thread_routine,
							&smp_thread[i]);

		/* Set the CPU affinity */
		CPU_ZERO(&cpu_set);
		CPU_SET(smp_thread[i].core_id, &cpu_set);
		pthread_setaffinity_np(smp_thread[i].handle, sizeof(cpu_set_t), &cpu_set);
	}

	return;

} /* end of run_threads() */

int interface_init(char *name);
int interface_init(char *name)
{

	char cmd[256];
	snprintf(cmd, 256,  "/opt/dpdk/switch_intf_dpdk.sh %s", name);
	int ret = system(cmd);
	return ret;
}

int interface_deinit(char *name);
int interface_deinit(char *name)
{

	char cmd[256];
	snprintf(cmd, 256,  "/opt/dpdk/switch_intf_normal.sh %s", name);
	int ret = system(cmd);
	return ret;
}

void check_http_analyzer(void);
void check_http_analyzer(void)
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
		fd = open(buffer, O_RDONLY);
		if (fd == -1)
			continue;

		if ((len = read(fd, buffer, 4096)) > 1) {
			buffer[len] = '\0';
			if (strstr(buffer, "http_analyzer") != 0) {
				printf("\nThere is another running http_analyzer process\n");
				printf("Please exit it before continue.\n\n");
				exit(2);
			}
		}
	}

	return;
}

int check_for_pacifica(void)
{
	int fd;
	char buffer[8192];
	int found = 0;
	int len = 0;

	fd = open("/proc/cmdline", O_RDONLY);
	if (fd == -1)
		return found;

	len = read(fd, buffer, 8192);
	buffer[len] = '\0';
	if (len > 1 && strstr(buffer, "pacifica"))
		found = 1;

	close(fd);
	return found;
}

/********************************************************************************
 * function: main()
 ********************************************************************************/
int
main (int argc, const char **argv)
{
	int i;
	struct smp_pkt *smp_pkt;
	int total_transactions = 0;

	if (check_for_pacifica()) {
		while (1) {
			sleep(10000);
		}
	}

	/* Capture the Ctrl-C, to be able to clean up correctly when the user will stop the application */
	init_signals();

	check_http_analyzer();

	interface_deinit(argv[1]);

	if (set_huge_pages("http_analyzer", 1024) < 0) {
		return -1;
	}

        log_thread_start();

	config_and_run_counter_update();

	interface_init(argv[1]);

	// Basic DPI initialization
	dpi_init();

	// Performance tuning.
	dpi_performance_tuning();

	/* Initialize the DPDK module */
	dpdk_init(argc-1, argv+1);

	/* Run the threads */
	run_threads();

	/* Now start the packet receiving and processing engine */
	dpdk_main();

#if 0 /* Pcap mode is commented for now, need to support it thru command line option */
	pcap_main(argc, argv);
#endif /* 0 */

	/* Add a dummy packet at each thread packet list tail */
        for (i = 0; i < NUM_THREADS; i++) {
		if ((smp_pkt = calloc(1, sizeof(struct smp_pkt) + 1)) == NULL) {
			DBG_LOG(ERROR, MOD_DPI_TPROXY, "Can't calloc new packet");
			return -1;
		}
                smp_pkt->pkt.data = NULL;
#if 0 // COMMENTED OUT TO REPLACE WITH RING BUFFER - Ram
                pthread_spin_lock(&smp_thread[i].lock);
                list_add_tail((struct list_entry *)smp_pkt,
                              (struct list_entry *)&smp_thread[i].pkt_head);
                pthread_spin_unlock(&smp_thread[i].lock);
#endif /* 0 */

		/* Enqueue the packet info in the ring buffer */
		if(rte_ring_sp_enqueue(smp_thread[i].queue, smp_pkt) < 0) {
			/* Failed to add hence dropping */
			packets_dropped++;
			free (smp_pkt);
		}
		else
			packets_queued++;
       	 }

	/* wait for all threads to complete */
        for (i = 0; i < NUM_THREADS; i++) {
                pthread_join(smp_thread[i].handle, NULL);

		/* Total the per thread counters */
		total_transactions += thread_details[i].counter;
        }

	DBG_LOG(MSG, MOD_DPI_TPROXY,"The number of http PACKETS RECEIVED %llu", packets_count);
	DBG_LOG(MSG, MOD_DPI_TPROXY,"The number of http PACKETS DROPPED %llu", packets_dropped);
	DBG_LOG(MSG, MOD_DPI_TPROXY,"The number of http PACKETS QUEUED %llu", packets_queued);

        /* Flush packets in reassembly queue before cleanup */
        afc_expire_timers();

        /* unregister the session attributes asked to the application */
        event_handler_cleanup();

        dpi_engine_exit_nr(&deng_data, NUM_THREADS);
        /* clean all structure and memory used by DPI engine */
        uafc_cleanup();
	DESTROY_DPI_ENGINE_DATA_ENGINE(deng_data);

	/* Print the total transactions logged */
	DBG_LOG(MSG, MOD_DPI_TPROXY," Total Number of Transactions : %d", total_transactions);

#if 0
	interface_deinit( argv[1] );
#endif

        return 0;
} /* end of main() */

#endif /* !_STUB_ */

/* End of main.c */

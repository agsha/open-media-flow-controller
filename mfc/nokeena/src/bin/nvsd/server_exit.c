#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <glib.h>
#include <stdint.h>

#include "server.h"
#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "nvsd_mgmt.h"

extern GThread** nkn_sched_threads;
extern int DM_cleanup(void);
extern void exit_counters(void);
extern void nkn_unload_netstack(void);
extern void virt_cache_server_exit(void);
int server_exit(void);
/*
 * All exit functions should go here.
 * This function will be called right before the process is terminated.
 */
int server_exit(void)
{
	g_mem_profile();
	DBG_LOG(SEVERE, MOD_SYSTEM, "System is shutting down");
	DBG_LOG(ALARM, MOD_SYSTEM, "nvsd system is shutting down"); 
        
        /* Core thread join */
#if 0
        for(i = 0; i < glob_num_core_threads; i++) {
                g_thread_join(nkn_sched_threads[i]);
        }
#endif // 0
        free( nkn_sched_threads);
	exit_counters();

        virt_cache_server_exit();
	nvsd_mgmt_deinit();
	if(net_use_nkn_stack) nkn_unload_netstack();    // Initialize the user space net stack
	log_thread_end();
	exit(0);
}


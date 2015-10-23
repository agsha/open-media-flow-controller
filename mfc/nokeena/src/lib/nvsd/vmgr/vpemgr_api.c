#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <libgen.h>
#include <assert.h>

#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkn_errno.h"
#include "network.h"
#include "server.h"
#include "nkn_vpemgr_defs.h"


static pthread_t vpemgr_req_thread_id;
extern void *vpemgr_req_func(void *arg);
struct vpemgr_req_queue vpemgr_req_head;

unsigned long long glob_vpemgr_tot_reqs, glob_vpemgr_err_dropped;

/*
 * initialization
 */

int VPEMGR_init(void)
{
    int rv;
    pthread_attr_t attr;
    int stacksize = 128 * KiBYTES;

    LIST_INIT( &vpemgr_req_head );

    rv = pthread_attr_init(&attr);
    if (rv) {
	DBG_LOG(MSG, MOD_VPEMGR, "pthread_attr_init() failed, rv=%d", rv);
	return 0;
    }

    rv = pthread_attr_setstacksize(&attr, stacksize);
    if (rv) {
	DBG_LOG(MSG, MOD_VPEMGR, 
		"pthread_attr_setstacksize() failed, rv=%d", rv);
	return 0;
    }

    if( pthread_create(&vpemgr_req_thread_id, &attr, vpemgr_req_func, NULL)) {
        DBG_LOG(MSG, MOD_VPEMGR, "Failed to create thread %s VPEMGR Initialization unsuccessful", "");
        return 0;
    }

    glob_vpemgr_tot_reqs=0;
    glob_vpemgr_err_dropped=0;

    return 1;
}


int VPEMGR_shutdown (void)
{
    return 0;
}

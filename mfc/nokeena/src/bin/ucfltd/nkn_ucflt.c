/*
 * Filename :   nkn_ucfltd.c
 * Date:        04 July 2011
 * Author:      Muthukumar
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#undef __FD_SETSIZE
#define __FD_SETSIZE  65536

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/time.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <stdint.h>
#include <zlib.h>

#include <sys/resource.h>
#include <ares.h>

#include "nkn_defs.h"
#include "nkn_debug.h"
#include "network.h"
#include "nkn_mgmt_agent.h"
#include "nkn_cfg_params.h"
#include "nkn_log_strings.h"
#include "nkn_trace.h"
#include "pe_ucflt.h"


#define NKN_TS_DB_ACCESS_TYPE_HYBRID 1

#ifdef NKN_TS_DB_ACCESS_TYPE_HYBRID
#define NKN_TS_DB_FILE_NAME "/nkn/ucflt/sfcontrol"
#define NKN_TS_DB_LOC_FILE_NAME "/nkn/ucflt/sfcontrol.download"
#else
#define NKN_TS_DB_FILE_NAME "/nkn/ucflt/tscontrol"
#define NKN_TS_DB_LOC_FILE_NAME "/nkn/ucflt/tscontrol.download"
#endif


// Config variables from /config/nkn/nkn.ucflt.conf
extern int   ts_db_debug_enable;
extern int   ts_db_max_size ;
extern int   ts_db_max_age ;
extern int   ts_db_port_num ;
extern char *ts_db_server_name ;
extern char *ts_db_searial_num ;

NKNCNT_DEF(ucflt_lookup_called, AO_t, "", "total ucflt task")
NKNCNT_DEF(ucflt_lookup_completed, AO_t, "", "total ucflt task")
NKNCNT_DEF(ucflt_lookup_no_info, AO_t, "", "total ucflt tasks failed")
NKNCNT_DEF(ucflt_no_database, AO_t, "", "total ucflt tasks failed")
NKNCNT_DEF(ucflt_install_cnt, AO_t, "", "total ucflt database install")
NKNCNT_DEF(ucflt_install_completed_cnt, AO_t, "", "total ucflt install completed")
NKNCNT_DEF(ucflt_install_failed_cnt, AO_t, "", "total ucflt install failed")

pthread_mutex_t ucflt_mutex = PTHREAD_MUTEX_INITIALIZER;
extern int send_msg(ucflt_resp_t*);
static int gl_channel_ready = 0;



const char * error_string (int error)     ;


const char *
error_string (int error)
{
    switch(error) {
    default:
         return "UNKNOWN ERROR";
    }
}


/*Scheduler input function*/
void ucflt_lookup(ucflt_req_t *flt_req)
{
    return ;
}



int ucflt_open(uint32_t init_flags)
{
        // Setup timer thread for periodic database updates.

	return 0;
}

int ucflt_close(void){
	return 0;
}

int ucflt_version(char *ver, int length) {
        /**/
	return 0;
}

int parse_and_add_custom_site(char *buf);
int parse_and_add_custom_site(char *buf)
{
     return 0;
}


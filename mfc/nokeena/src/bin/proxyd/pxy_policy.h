/*
 *  Include this file only in pxy_policy.c
 */
#ifndef __PXY_POLICY__H
#define __PXY_POLICY__H

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <md_client.h>
#include <license.h>

#include "nkn_debug.h"
#include "pxy_defs.h"
#include "pxy_server.h"

/*
 * These 2 shared memory details has to be replicated in nvsd.
 */
#define PXY_HEALTH_CHECK_KEY 1234
typedef struct _nvsd_health {
       int good ; /* Just 1 field now */
} nvsd_health_td;

/*============================================================*/
#if 0 /* For now DBL/DWL handling is not present and hence commenting out this code.*/

void free_whole_ip_list(char * p);
void add_block_ip_list(char * p);
void add_cacheable_ip_list(char * p);
void del_ip_list(char * p);

typedef struct pxy_ip_list_t {
	struct pxy_ip_list_t * next;
	uint32_t ip;
	l4proxy_action action;
} PIL;

static PIL * p_global_ip_list = NULL;

static int check_ip_list(uint32_t ip);
static int add_into_ip_list(uint32_t ip, l4proxy_action action);
static void delete_from_ip_list(uint32_t ip);
static void add_ip_to_ip_list(char * p, l4proxy_action act);

#endif  /* DWL/DBL related changes */

#endif  /*__PXY_POLICY__H */



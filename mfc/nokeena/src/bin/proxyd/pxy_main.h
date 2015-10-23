#ifndef __PXY_MAIN__H
#define __PXY_MAIN__H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <stdint.h>
#include <netdb.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/resource.h>


#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "pxy_defs.h"

NKNCNT_DEF(shutdown_close_wait_socket, uint64_t, "", "version expured")

static char nkn_build_date[] =           ":Build Date: " __DATE__ " " __TIME__ ;
static char nkn_build_id[] =             ":Build ID: " NKN_BUILD_ID ;
static char nkn_build_prod_release[] =   ":Build Release: " NKN_BUILD_PROD_RELEASE ;
static char nkn_build_number[] =         ":Build Number: " NKN_BUILD_NUMBER ;
static char nkn_build_scm_info_short[] = ":Build SCM Info Short: " NKN_BUILD_SCM_INFO_SHORT ;
static char nkn_build_scm_info[] =       ":Build SCM Info: " NKN_BUILD_SCM_INFO ;
static char nkn_build_extra_cflags[] =   ":Build extra cflags: " EXTRA_CFLAGS_DEF ;


void catcher(int sig);

#endif /* __PXY_MAIN__H */

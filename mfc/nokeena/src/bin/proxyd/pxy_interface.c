#define _GNU_SOURCE
#include <stdio.h>              /* perror */
#include <stdlib.h>              /* perror */
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>         /* struct sockaddr_in */
#include <arpa/inet.h>          /* inet_ntoa */
#include <net/if_arp.h>
#include <net/if.h>
#include <unistd.h>             /* close */
#include <time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <sys/queue.h>

#include "pxy_defs.h"
#include "pxy_interface.h"
#include "nkn_stat.h"
#include "libpxy_common.h"

NKNCNT_DEF(tot_interface, uint16_t, "", "Total network interface found")

pxy_nkn_interface_t interface[MAX_NKN_INTERFACE];


/*
 * Get Link Aggregation Slave Interface Number
 */
int 
pxy_get_link_aggregation_slave_interface_num(char * if_name)
{
        return libpxy_get_link_aggregation_slave_interface_num(if_name) ;
}

/*
 *  Network Interface Management APIs
 */
int pxy_interface_init(void)
{
        return  libpxy_interface_init(interface, &glob_tot_interface) ;
}

void pxy_interface_display(void)
{
        libpxy_interface_display(interface, glob_tot_interface);
}


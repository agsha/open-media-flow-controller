
#ifndef __INTERFACE__H__
#define __INTERFACE__H__

#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <atomic_ops.h>
#include <libpxy_common.h>

//#define MAX_PORTLIST	       64
//#define MAX_NKN_INTERFACE      32

extern pxy_nkn_interface_t interface[MAX_NKN_INTERFACE];
extern char            http_listen_intfname[MAX_NKN_INTERFACE][16];
extern int             http_listen_intfcnt;

extern uint16_t        glob_tot_interface;

int       pxy_interface_init(void);
void      pxy_interface_display(void);
struct nkn_interface * pxy_interface_find_by_dstip(uint32_t dstip);
uint8_t * pxy_interface_find_mac_for_dstip(uint32_t ip, char * ifname);
int       pxy_get_link_aggregation_slave_interface_num(char * if_name);
void      pxy_http_interfacelist_callback(char * http_interface_list_str) ;
void      pxy_http_portlist_callback(char * http_port_list_str); 


#endif // __INTERFACE__H__


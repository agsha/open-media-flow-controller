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
#include <ifaddrs.h>
#include <netdb.h>
#include <pthread.h>

#include "nkn_cfg_params.h"
#include "interface.h"
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkn_cfg_params.h"
#include "nvsd_mgmt.h"
#include "libnknmgmt.h"
#include "nkn_debug.h"
#include "header_datatype.h"
#include "nkn_namespace.h"
#include "network.h"

NKNCNT_DEF(tot_interface, uint16_t, "", "Total network interface found")
NKNCNT_DEF(tot_alias_interface, uint16_t, "", "Total network interface aliases found")

uint32_t eth0_ip_address = 0;
nkn_interface_t interface[MAX_NKN_INTERFACE];
nkn_interface_t *if_alias_list;
pthread_mutex_t if_alias_list_lock = PTHREAD_MUTEX_INITIALIZER;

extern hash_entry_t hash_local_ip[128];
extern hash_table_def_t ht_local_ip;
extern int nkn_http_ipv6_enable;

extern int nkn_http_service_inited;

int get_link_aggregation_slave_interface_num(char * if_name);
void add_mac_to_list(uint32_t ip, char * ipmac);
void update_interface_bandwidth(const char *if_name, uint32_t if_speed);
int add_pns_to_alias_list(nkn_interface_t *pns, int);
int del_pns_from_alias_list(nkn_interface_t *pns, int);
int update_interface_events(void *if_change_info);
static int get_if_status (const char *name, char *);
int update_pns(nkn_interface_t *pns, if_info_state_change_t *if_info, int alias);
int del_if_from_pns(nkn_interface_t *pns, int alias);
int update_pns6(nkn_interface_t *pns, if_info_state_change_t *if_info, int alias);
int add_if_to_pns(if_info_state_change_t *if_info, int alias, nkn_interface_t **);
int wildcard_http_listen_call(int mode, int alias, nkn_interface_t *);

extern void nkn_setup_interface_parameter(nkn_interface_t *);
extern int http_if4_finit(nkn_interface_t *pns, int);
extern int http_if4_init(nkn_interface_t *pns);
extern int http_if6_finit(nkn_interface_t *pns);
extern int http_if6_init(nkn_interface_t *pns);

extern md_client_context *nvsd_mcc;

int add_pns_to_alias_list(nkn_interface_t *pns, int alias_locked /* 0 = not locked, 1 = locked*/) {
    char name[64];

    (!alias_locked)?pthread_mutex_lock(&if_alias_list_lock):0;
    if (if_alias_list) {
        if_alias_list->prev = pns;
    }
    pns->prev = NULL;
    pns->next = if_alias_list;
    if_alias_list = pns;
    (!alias_locked)?pthread_mutex_unlock(&if_alias_list_lock):0;

    glob_tot_alias_interface++;
    snprintf(name, 64, "net_port.%s.tot_sessions", pns->if_name);
    nkn_mon_add(name, NULL, &pns->tot_sessions, sizeof(pns->tot_sessions));
    snprintf(name, 64, "net_port.%s.vip_addr", pns->if_name);
    nkn_mon_add(name, NULL, &pns->if_addrv4, sizeof(pns->if_addrv4));

    return 0;
}

int del_pns_from_alias_list(nkn_interface_t *pns, int alias_locked /* 0 = not locked, 1 = locked*/) {

    char name[64] = {0};

    snprintf(name, 64, "net_port.%s.tot_sessions", pns->if_name);
    nkn_mon_delete(name, NULL);
    snprintf(name, 64, "net_port.%s.vip_addr", pns->if_name);
    nkn_mon_delete(name, NULL);

    (!alias_locked)?pthread_mutex_lock(&if_alias_list_lock):0;
    if (pns->prev) {
        pns->prev->next = pns->next;
    }
    if (pns->next) {
        pns->next->prev = pns->prev;
    }
    if (if_alias_list == pns) {
        if_alias_list = pns->next;
    }
    free(pns);
    (!alias_locked)?pthread_mutex_unlock(&if_alias_list_lock):0;
    glob_tot_alias_interface--;

    return 0;
}

/*
 * Get Link Aggregation Slave Interface Number
 */
int get_link_aggregation_slave_interface_num(char * if_name)
{
	FILE * fp;
	char filename[256];
	char * line = NULL;
	size_t len = 0;
	ssize_t l_read;
	int counter = 0;

	snprintf(filename, 256, "/proc/net/bonding/%s", if_name);
	if ((fp = fopen(filename, "r")) != NULL)
	{
		while ((l_read = getline(&line, &len, fp)) != -1)
		{
			if(strncmp(line, "Slave Interface", 15)== 0) {
				counter++;
			}
		}
		if (line)
			free(line);
		fclose(fp);
	}

	printf("counter=%d\n", counter);
	return counter;
}

/*
 *  Network Interface Management APIs
 */
int interface_init(void)
{
    int fd, i, s;
    //struct ifconf   ifc;
    nkn_interface_t * pns;
    struct ifreq     ifr;
    struct sockaddr_in sa;
    //char name[64];
    uint64_t if_speed;
    const char *intf_name = NULL;
    char *parent_intf = NULL;

    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];
    struct in6_addr if_addrv6;
    int if_count = 0;

    if ( (fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("[get_if_name] socket(AF_INET, SOCK_DGRAM, 0)");
        return -1;
    }

    if (getifaddrs(&ifaddr) == -1) {
        perror("[getifaddrs] error:");
        close(fd);
        return -1;
    }

    ifa = ifaddr;

    //printf("name\taddr\tsubnet\tnetmask\tmtu\tmac\n");
    for (; ifa; ifa = ifa->ifa_next) {

        strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));

        if ( ioctl(fd, SIOCGIFFLAGS, (char*)&ifr) == -1) {
            perror("DEBUG: [get_if_name] ioctl(SIOCGIFFLAGS)");
            continue;
        }

        /* check out if interface up or down */
        if ( !(ifr.ifr_flags & IFF_UP) )
        {
            printf("DOWN\n");
            continue;
        }

        /*
         * add a new server structure on this interface
         */
        if (strstr (ifa->ifa_name, ":")) {
            pns = (nkn_interface_t *)nkn_malloc_type (sizeof(nkn_interface_t), mod_nm_interface);
            if (!pns) {
                perror("DEBUG: malloc failed");
                continue;
            }
            memset((char *)pns, 0, sizeof(nkn_interface_t));
            pns->if_type = NKN_IF_ALIAS;
            strncpy(pns->if_name, ifa->ifa_name, sizeof(ifa->ifa_name));
            add_pns_to_alias_list(pns, 0 /* lock is not acquired*/);
            //assert(glob_tot_alias_interface < MAX_NKN_ALIAS);
        } 
        else {
            for (i = 0; i < glob_tot_interface; i++) {
                if (0 == strcmp(ifa->ifa_name, interface[i].if_name)) {
                    pns = &interface[i];
                    goto set_if_details;
                }
            }

            if (i == glob_tot_interface) {
                pns = &interface[glob_tot_interface];
                memset((char *)pns, 0, sizeof(nkn_interface_t));
                glob_tot_interface++;
            }
            /* get if bandwidth *
             * PLEASE ALSO REFER TO update_interface_bandwidth() below as it modifies the 
             * if_bandwidth field with a link up event any change done here has to 
             * done in that function too
             */
            strncpy(pns->if_name, ifa->ifa_name, sizeof(ifa->ifa_name));
            intf_name = pns->if_name;
            if(nvsd_mgmt_is_bond_intf(intf_name, nvsd_mcc)) {
                pns->if_bandwidth = nvsd_mgmt_get_bond_intf_bw(intf_name, nvsd_mcc);
                pns->if_type = NKN_IF_BOND;
            } else {
                /*eth or lo interface */
                if_speed = nvsd_mgmt_get_interface_link_speed(intf_name, nvsd_mcc);
                pns->if_bandwidth = (GBYTES/8) * (if_speed / 1000);
                if (0 == strcmp(pns->if_name, "lo")) {
                    pns->if_type = NKN_IF_LO;
                }
                else {
                    pns->if_type = NKN_IF_PHYSICAL;
                }
            }
        }

        if_count++;

        assert(glob_tot_interface < MAX_NKN_INTERFACE);

set_if_details:

        /* save if address */
        if (ifa->ifa_addr && (AF_INET == ifa->ifa_addr->sa_family) && 
            !( pns->if_addrv4)) {
            pns->if_addrv4 = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;

            if (strcmp(pns->if_name, "eth0") != 0) {
                eth0_ip_address = pns->if_addrv4; 
            }

            /* save if netmask */
            if ( ioctl(fd, SIOCGIFNETMASK, (char*)&ifr) == -1) {
                perror("DEBUG: [get_if_name] ioctl(SIOCGIFNETMASK)");
                exit(1);
            }
            memcpy(&sa, &ifr.ifr_netmask, sizeof(struct sockaddr_in));
            pns->if_netmask=sa.sin_addr.s_addr;

            /* calculate subnet */
            pns->if_subnet=pns->if_addrv4 & pns->if_netmask;

        }
        memset (&if_addrv6, 0, sizeof (if_addrv6));
        memset (host, 0, sizeof (host));

        if (ifa->ifa_addr && (AF_INET6 == ifa->ifa_addr->sa_family)) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6),
                    host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                perror("getnameinfo error");
                continue;
            }
            s = inet_pton(AF_INET6, host, &if_addrv6);
            if(s != 1 ) {
                perror("inet_pton error for ipv6");
                continue;
            } else {
                memcpy (&(pns->if_addrv6[pns->ifv6_ip_cnt].if_addrv6), &if_addrv6, sizeof (struct in6_addr));
                pns->ifv6_ip_cnt++;
            }
        }


        /* get if mtu */
        if ( ioctl(fd, SIOCGIFMTU, (char*)&ifr) == -1) {
            perror("Warning: [get_if_name] ioctl(SIOCGIFMTU)");
            exit(1);
        }
        if(ifr.ifr_mtu==0) { pns->if_mtu = 1500; }
        else { pns->if_mtu = ifr.ifr_mtu; }

        /* get hardware address */
        if ( ioctl(fd, SIOCGIFHWADDR, (char*)&ifr) == -1) {
            perror("DEBUG: [get_if_name] ioctl(SIOCGIFHWADDR)");
            exit(1);
        }
        memcpy(pns->if_mac, (unsigned char *)&ifr.ifr_hwaddr.sa_data[0], 6);

         safe_free(parent_intf);
    }
    freeifaddrs(ifaddr);
    close(fd);
    return 0;
}

void interface_display(void)
{
	int i;
	unsigned char * p;
	struct in_addr in;
	struct in_addr in_netmask;
	struct in_addr in_subnet;

	printf("  name bandwidth   mtu             addr           subnet          netmask mac\n");
	for(i=0; i<glob_tot_interface; i++)
	{
		in.s_addr=interface[i].if_addrv4;
		in_netmask.s_addr=interface[i].if_netmask;
		in_subnet.s_addr=interface[i].if_subnet;
		printf("%6s %8lu %6d %16s %16s %16s ",
			interface[i].if_name,
			interface[i].if_bandwidth/MBYTES,
			interface[i].if_mtu,
			inet_ntoa(in),
			inet_ntoa(in_netmask),
			inet_ntoa(in_subnet));

		p=(unsigned char *)&interface[i].if_mac;
		printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
			*p, *(p+1), *(p+2), *(p+3), *(p+4), *(p+5));

	}
}


/*
 *  Routing Table Management APIs
 */

struct nkn_interface * interface_find_by_dstip(uint32_t dstip)
{
	int i;

	for(i=0; i<glob_tot_interface; i++) {

		if( interface[i].if_netmask && 
		    ((dstip & interface[i].if_netmask) == interface[i].if_subnet) ) {
			//printf("found ifname=%s\n", prt->ifname);
			return &interface[i];
		}

	}
	return NULL;
}

/*
 *  ARP/Mac address Table Management APIs
 */

struct arp_table_t {
	struct arp_table_t * next;
	uint32_t ipaddr;
	uint8_t  ipmac[6];
};
static struct arp_table_t * parphead=NULL;

void add_mac_to_list(uint32_t ip, char * ipmac)
{
	struct arp_table_t * parp;

	/* TBD:
	 * I will change this code to be hash table based instead of link list.
	 */
	parp = parphead;
	while(parp) {
		if(parp->ipaddr == ip) return;
		parp = parp->next;
	}

	// Cache the Mac Address now
	parp =(struct arp_table_t *)nkn_malloc_type(sizeof(struct arp_table_t),
						    mod_net_arp_table_t);
	if(!parp) return ;

	parp->ipaddr=ip;
	memcpy(parp->ipmac, ipmac, 6);

	parp->next=parphead;
	parphead=parp;

	return;
}

/*
 * Function to update the bandwidth parameter in the 
 * interface data structure. It uses the link speed to 
 * calculate the bandwidth. 
 */
void update_interface_bandwidth(const char *if_name, uint32_t if_speed)
{
    uint32_t idx;
    nkn_interface_t * pns;


    /* Sanity test */
    if (!if_name) return;

    /* First find the interface structure */
    for (idx = 0; idx < glob_tot_interface; idx++)
    {
	pns = &interface[idx];

    	if (!strcmp (if_name, pns->if_name))
		break; /* Found the interface */
    }

    /* Sanity check */
    if (idx == glob_tot_interface)
    	return ;

    /* Set the bandwidth of the interface based on the link speed */
    /* Link speed in terms of Gbps is used in the calculation */
    pns->if_bandwidth = (GBYTES/8) * (if_speed / 1000);

    nkn_setup_interface_parameter(pns);

    /* Commenting out this logic as we can get the bond intfBW
    *  before this function call*/
#if 0
    /* Check for link aggregation to make the relevant change */
    if ((strncmp(pns->if_name, "eth", 3) != 0) &&
            (strncmp(pns->if_name, "lo", 2) != 0))
        pns->if_bandwidth = nvsd_mgmt_get_bond_intf_bw(pns->if_name);
#endif
    return;
}

#define M_PRINT_IF_INFO_FMT "Type: %d, \
IfName: %s, \
Cause: %s, \
IPv4: %s, \
v6Enable: %d, \
IPv6: %s, \
IfSrc: %s, \
oper_up: %d, \
link_up: %d"

static int get_if_status (const char *name, char *ipv4addr) {
    struct ifreq ifr;
    struct sockaddr_in sa;
    int fd;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (-1 == fd) {
        DBG_LOG(SEVERE, MOD_HTTP, "Unable to create socket");
        return -1;
    }

    memset(&ifr, 0, sizeof (ifr));
    strcpy(ifr.ifr_name, name);

    if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifr) == -1) {
        DBG_LOG(MSG, MOD_HTTP, "Unable to get flags for interface '%s'", name);
        close (fd);
        return -1;
    }

    if (!(IFF_UP & ifr.ifr_flags)) {
        close (fd);
        DBG_LOG(MSG, MOD_HTTP, "IF status for '%s' is DOWN", ifr.ifr_name);
        return -1;
    }

    if (ioctl(fd, SIOCGIFADDR, (char *)&ifr) == -1) {
        DBG_LOG(MSG, MOD_HTTP, "Unable to get IP for interface '%s'", name);
        close (fd);
        return -1;
    }

    memcpy(&sa, &ifr.ifr_addr, sizeof(struct sockaddr_in));
    inet_ntop(AF_INET, &sa.sin_addr, ipv4addr, 128);

    DBG_LOG(MSG, MOD_HTTP, "IF status for '%s => %s' is UP", ifr.ifr_name, ipv4addr);
    close(fd);
    return 0;
}

/*
if_info - points to Interface info structure from mgmtd event
alias - in this function, signifies alias interface (1) or non-alias interface (0)
ppns - ppns is not NULL, if a new interface add event is received.
ppns is not null when, an event (http enable) is received for an existing interface
*/
int add_if_to_pns(if_info_state_change_t *if_info, int alias, nkn_interface_t **pppns) {
    nkn_interface_t *pns;
    nkn_interface_t *ppns = *pppns;
    char *intf_name = NULL;
    uint64_t if_speed;
    //struct in6_addr if_addrv6;
    struct in_addr if_addrv4;
    const char *ipaddr_str;
    int ipaddr_strlen;
    int rv = -1, ret;

    if (ppns && (AO_load(&ppns->if_listening) == 1)) {
        DBG_LOG(MSG, MOD_HTTP, "ppns %p for if_name %s is already listening", ppns, ppns->if_name);
        return 1;
    }

    if (alias) {
        if (!ppns) {
            pns = (nkn_interface_t *)nkn_malloc_type(sizeof(nkn_interface_t), mod_nm_interface);
            if (!pns) {
                DBG_LOG(MSG, MOD_HTTP, "MALLOC failure");
                return -1;
            }
            memset(pns, 0, sizeof(nkn_interface_t));
            pns->if_type = NKN_IF_ALIAS;
            strncpy(pns->if_name, if_info->intfname, IFNAMSIZ);
            add_pns_to_alias_list(pns, alias /*alias lock is acquired */);
            *pppns = pns;
            //assert(glob_tot_alias_interface < MAX_NKN_ALIAS); 
        }
        else {
            pns = ppns;
        }
    }
    else {
        if (!ppns) {
            pns = &interface[glob_tot_interface];
            memset(pns, 0, sizeof(nkn_interface_t));
            glob_tot_interface++;
            assert(glob_tot_interface < MAX_NKN_INTERFACE);
            strncpy(pns->if_name, if_info->intfname, IFNAMSIZ);
            *pppns = pns;
        }
        else {
            // handle update to existing pns
            pns = ppns;
        }
        intf_name = pns->if_name;
        if(nvsd_mgmt_is_bond_intf(intf_name, nvsd_mcc)) {
            pns->if_bandwidth = nvsd_mgmt_get_bond_intf_bw(intf_name, nvsd_mcc);
            pns->if_type = NKN_IF_BOND;
            DBG_LOG(MSG, MOD_HTTP, "Bw for bond if %s is %lu", intf_name, pns->if_bandwidth);
        } else {
            /*eth or lo interface */
            if_speed = nvsd_mgmt_get_interface_link_speed(intf_name, nvsd_mcc);
            pns->if_bandwidth = (GBYTES/8) * (if_speed / 1000);
            if (0 == strcmp(pns->if_name, "lo")) {
                pns->if_type = NKN_IF_LO;
            }
            else {
                pns->if_type = NKN_IF_PHYSICAL;
            }
            DBG_LOG(MSG, MOD_HTTP, "Bw for interface %s is %lu", intf_name, pns->if_bandwidth);
        }
    }

    AO_store(&pns->if_status, 1);

    rv = inet_pton(AF_INET, if_info->ipv4addr, &if_addrv4);
    if (rv > 0) {
        pns->if_addrv4 = if_addrv4.s_addr;
        // Add fun only for new entries
        if (alias) {
            int val = 1;
            ipaddr_str = nkn_strdup_type(if_info->ipv4addr, mod_http_ipstr);
            ipaddr_strlen = strlen(ipaddr_str);
            ret = ht_local_ip.lookup_func(&ht_local_ip, ipaddr_str, ipaddr_strlen, &val);
            if (ret) {
                val = 1;
                ret = ht_local_ip.add_func(&ht_local_ip, ipaddr_str, ipaddr_strlen, val);
                nkn_namespace_config_update(NULL);
            }
            if (ret) {
                // Either IP addr exists or failed to add
                free((void *)ipaddr_str);
            }
        }
    // Trigger ipv4 init event
        // rv = http_if4_init(pns);
        return 0;
    }

    if (!alias && ppns && nkn_http_ipv6_enable) {
        // TODO: when support for IPv6 if_down/up is brought, consider having this logic in  a
        // a seperate IPv6 function
        // http_if6_init(pns);
    }
    DBG_LOG(MSG, MOD_HTTP, "Failed to add pns %p - if: %s", pns, if_info->intfname);

    return -1;
}

int del_if_from_pns(nkn_interface_t *pns, int alias) {
    struct in_addr inaddr;
    const char *ipaddr_str;
    int ipaddr_strlen;

    DBG_LOG(MSG, MOD_HTTP, "Deleting socket for pns %p on if: %s", pns, pns->if_name);

    if (alias && (AO_load(&pns->if_status) == 0)) {
        inaddr.s_addr = pns->if_addrv4;
        ipaddr_str = inet_ntoa(inaddr);
        if (ipaddr_str) {
            ipaddr_str = nkn_strdup_type(ipaddr_str, mod_http_ipstr);
            ipaddr_strlen = strlen(ipaddr_str);
            ht_local_ip.del_func(&ht_local_ip, ipaddr_str, ipaddr_strlen);
            nkn_namespace_config_update(NULL);

            free((void *)ipaddr_str);
        }
    }
    
    http_if4_finit(pns, alias);
    if (!alias && nkn_http_ipv6_enable) {
        // TODO: when support for IPv6 if_down/up is brought, consider having this logic in  a
        // a seperate IPv6 function
        // http_if6_finit(pns);
    }
    return 0;
}

int update_pns(nkn_interface_t *pns, if_info_state_change_t *if_info, int alias) {
    DBG_LOG(MSG, MOD_HTTP, "Updating socket for pns %p on if: %s", pns, pns->if_name);
    del_if_from_pns(pns, alias);
    if (0 == add_if_to_pns(if_info, alias, &pns)) {
        http_if4_init(pns);
    }
    return 0;
}

// IPv6 update event is tricky. Since multiple IPv6 addresses can exist for 
// an interface, all IPv6 events (add/del/mod) are seen as update (remember, 
// u ll find PNS always as the physical interface exists).
int update_pns6(nkn_interface_t *pns, if_info_state_change_t *if_info, int alias) {
    UNUSED_ARGUMENT(if_info);
    UNUSED_ARGUMENT(alias);
    struct ifaddrs *ifaddr, *ifa;
    char ipv6_host[NI_MAXHOST];
    struct in6_addr if_addrv6;
    int rv;

    DBG_LOG(MSG, MOD_HTTP, "Updating IPv6 socket for pns %p on if: %s", pns, pns->if_name);
    if (getifaddrs(&ifaddr) == -1) {
        DBG_LOG(SEVERE, MOD_HTTP, "Failed to get ifaddrs list");
        return -1;
    }

    ifa = ifaddr;
    while (ifa) {
        if (0 == strcmp(ifa->ifa_name, pns->if_name)) {
            if (ifa->ifa_addr && (AF_INET6 == ifa->ifa_addr->sa_family)) {
                rv = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), 
                    ipv6_host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

                if (!rv) {
                    break;
                }
                rv = inet_pton(AF_INET6, ipv6_host, &if_addrv6);
                if (rv > 0) {
                }
            }
        }

        ifa = ifa->ifa_next;
    }

    return 0;
}

// mode = 0 - shutdown
// mode = 1 - listen all
int wildcard_http_listen_call(int mode, int alias, nkn_interface_t *ppns) {
    nkn_interface_t *pns;

#if 1 // currently no support for non-alias interfaces
    int i = 0;
    for (i = 0; i < glob_tot_interface; i++) {
        pns = &interface[i];
        if (0 == strcmp (pns->if_name, "lo")) {
            continue;
        }
        switch (mode) {
            // shutdown all
            case 0:
                DBG_LOG(MSG, MOD_HTTP, "pns %p, ppns %p", pns, ppns);
                if (pns != ppns) {
                    if (AO_load(&pns->if_listening) == 1) {
                        pns->enabled = 0;
                        http_if4_finit(pns, alias);
                    }
                    if (nkn_http_ipv6_enable) {
                       // http_if6_finit(pns); 
                    }
                }
                break;
            // Listen on all
            case 1:
                if (AO_load(&pns->if_listening) == 0) {
                    pns->enabled = 1;
                    http_if4_init(pns);
                }
                if (nkn_http_ipv6_enable) {
                    // http_if6_init(pns);
                }
                break;
        }
    }
#endif

    for (pns = if_alias_list; pns; pns = pns->next) {
        switch (mode) {
            // shutdown all
            case 0:
                DBG_LOG(MSG, MOD_HTTP, "pns %p, ppns %p", pns, ppns);
                if ((pns != ppns) && (AO_load(&pns->if_listening) == 1)) {
                    pns->enabled = 0;
                    http_if4_finit(pns, alias);
                }
                break;
                // Listen on all
            case 1:
                if (AO_load(&pns->if_listening) == 0) {
                    pns->enabled = 1;
                    http_if4_init(pns);
                }
                break;
        }

    }

    return 0;
}

static int redo_bind_socket_to_interface(nkn_interface_t *pns);

static int redo_bind_socket_to_interface(nkn_interface_t *pns) {
    int i, j;

    if (!bind_socket_with_interface) {
        return 0;
    }

    if (pns && (AO_load(&pns->if_listening))) {
        for (i = 0; i < MAX_PORTLIST; i++) {
	    for (j = 0;  j < NM_tot_threads; j++) {
		if (pns->listenfd[i][j]) {
		    NM_bind_socket_to_interface(pns->listenfd[i][j], pns->if_name);
		}
		else {
		    break;
		}
	    }
        }
    }

    return 0;
}

int update_interface_events(void *if_change_info) {
    if_info_state_change_t if_info = *(if_info_state_change_t *)if_change_info;
    int if_status = -1;
    nkn_interface_t *pns = NULL;
    int alias_locked = 0;
    int ipv4addr_changed = 0;
    static int http_listen_all = -1;
    static int http_listen_cnt = -1;

    if (!nkn_http_service_inited) {
        // HTTP service is not yet initialized. Dropping interface events.
        DBG_LOG(SEVERE, MOD_HTTP, "WARNING! Dropping interface events (HTTP service thread is not yet initialized).");
        return 0;
    }

    if (-1 == http_listen_all) {
        // first time get this info mgmtd
        http_listen_all = !http_listen_intfcnt;
        http_listen_cnt = http_listen_intfcnt;
    }

    DBG_LOG(MSG, MOD_HTTP, "Got interface event: "M_PRINT_IF_INFO_FMT"\n", if_info.chng_type,
            if_info.intfname, if_info.cause, if_info.ipv4addr, 
            if_info.ipv6_enable, if_info.ipv6addr, if_info.intfsource, 
            if_info.oper_up, if_info.link_up);

    if_status = get_if_status(if_info.intfname, if_info.ipv4addr);
    if (strstr(if_info.intfname, ":")) {
        nkn_interface_t *temp;
        // Alias interface change
        pthread_mutex_lock(&if_alias_list_lock);
        alias_locked = 1;
        temp = if_alias_list;
        for (;temp;temp = temp->next) {
            if (0 == strcmp(temp->if_name, if_info.intfname)) {
                pns = temp;
                break;
            }
        }
    }
    else {
        int i;
        // non-alias changes
        for(i = 0; i < glob_tot_interface; i++) {
            if (0 == strcmp(interface[i].if_name, if_info.intfname)) {
                pns = &interface[i];
                break;
            }
        }
    }

    if (!pns && if_status) {
        DBG_LOG(MSG, MOD_HTTP, "No interface and no info with us. Just return...");
        goto if_exit;
    }

    if(pns) {
        int rv = 0;
        struct in_addr if_addrv4;
        DBG_LOG(MSG, MOD_HTTP, "Pns %p found for %s", pns, if_info.intfname);
        rv = inet_pton(AF_INET, if_info.ipv4addr, &if_addrv4);
        if (rv > 0) {
            if (pns->if_addrv4 != if_addrv4.s_addr) {
                ipv4addr_changed = 1;
            }
            else {
                ipv4addr_changed = 0;
            }
        }
    }

    DBG_LOG(MSG, MOD_HTTP, "http_listen_all: %d", http_listen_all);

    switch(if_info.chng_type) {
        case INTF_LINK_UP:
            // The bond device and TM callbacks are crazy. Add/remove slaves breaks device-bind-to-socket
            redo_bind_socket_to_interface(pns);
        case INTF_LINK_DOWN:
            // Never mind
            break;
        case INTF_LINK_CHNG:
            //if (!if_status && !pns && http_listen_all) {
            if (!if_status && !pns) {
                DBG_LOG(MSG, MOD_HTTP, "New interface addition case");
                // New interface addition case
                if ((0 == add_if_to_pns(&if_info, alias_locked, &pns)) && http_listen_all) {
                    pns->enabled = 1;
                    http_if4_init(pns);
                }
            }
            else if (pns && if_status) {
                AO_store(&pns->if_status, 0);
                DBG_LOG(MSG, MOD_HTTP, "Interface delete case");
                // Interface delete case
                del_if_from_pns(pns, alias_locked);
            }
            else if (pns && !if_status) {
                DBG_LOG(MSG, MOD_HTTP, "Interface param change case");
                // Interface param change case
                if (ipv4addr_changed && pns->enabled) {
                    update_pns(pns, &if_info, alias_locked);
                }
                else if (http_listen_all && pns->enabled && (AO_load(&pns->if_listening) == 0)) {
                    DBG_LOG(MSG, MOD_HTTP, "Interface no shut case");
                    update_pns(pns, &if_info, alias_locked);
                }
                else {
                    // This could be IPv6 add or del or mod :(
                    // This is one heck. Hate the way IPv6 interfaces are 
                    // maitained. We are not gonna get proper event from
                    // mgmtd for IPv6. This leads to running thro an array of size 32
                    // for 'n' number of IPv6 addresses times that I hate really.
                    // Not gonna support dynamic IPv6 add/delete.
                    //update_pns6(pns, &if_info, alias_locked);
                }
            }
            redo_bind_socket_to_interface(pns);
            break;
        case DELIVERY_ADD:
            http_listen_cnt++;
            http_listen_all = !http_listen_cnt;
            if (1 == http_listen_cnt) {
                DBG_LOG(MSG, MOD_HTTP, "shutdown others, if HTTP is not enabled");
                // shutdown others, if HTTP is not enabled
                if (pns) {
                    pns->enabled = 1;
                }
                wildcard_http_listen_call(0, alias_locked, pns);
            }
            if (!if_status) {
                DBG_LOG(MSG, MOD_HTTP, "HTTP enabled on this interface");
                // HTTP enabled on this interface
                if (0 == add_if_to_pns(&if_info, alias_locked, &pns)) {
                    pns->enabled = 1;
                    http_if4_init(pns);
                }
            }
            break;
        case DELIVERY_DEL:
            http_listen_cnt--;
            http_listen_all = !http_listen_cnt;
            if (0 == http_listen_cnt) {
                DBG_LOG(MSG, MOD_HTTP, "Listen on all configured interfaces");
                // Listen on all configured interfaces
                wildcard_http_listen_call(1, alias_locked, pns);
            }
            else if (pns && !if_status) {
                DBG_LOG(MSG, MOD_HTTP, "HTTP disabled on this interface");
                // HTTP disabled on this interface
                pns->enabled = 0;
                del_if_from_pns(pns, alias_locked);
            }
            break;
        default:
            DBG_LOG(MSG, MOD_HTTP, "Unknown interface event received");
            break;
    }

if_exit:
    if(alias_locked) {
        pthread_mutex_unlock(&if_alias_list_lock);
    }

    return 0;
}

static uint8_t * find_mac_from_os(uint32_t ip, char * if_name)
{
	int fd;
	struct sockaddr_in * sin;
	unsigned char * p;
	struct arpreq 	areq;
	static unsigned char ifmac[6];

	/* When useing user space TCP stack, this function never called. */
	if( net_use_nkn_stack ) return NULL;

	if ( (fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("[get_if_name] socket(AF_INET, SOCK_DGRAM, 0)");
		return NULL;
	}

	/* Make the ARP request. */
	memset(&areq, 0, sizeof(areq));
	sin = (struct sockaddr_in *) &areq.arp_pa;
	sin->sin_family = AF_INET;

        printf("ip=0x%x\n", ip);
	sin->sin_addr.s_addr = ip;
	sin = (struct sockaddr_in *) &areq.arp_ha;
	sin->sin_family = ARPHRD_ETHER;
	
        printf("if_name=%s\n", if_name);
	strcpy(areq.arp_dev, if_name);

	/* gets interfaces list */
	if ( ioctl(fd, SIOCGARP, (char*)&areq) == -1 ) {
		perror("[get_if_name] ioctl(SIOCGARP)");
		close(fd);
		return NULL;
	}

	p=(unsigned char *)&areq.arp_ha.sa_data;
	printf("Mac address: %02x:%02x:%02x:%02x:%02x:%02x\n", 
			*p, *(p+1), *(p+2),*(p+3),*(p+4),*(p+5));
        close(fd);

	if(memcmp(p, "000000", 6)==0) {
		//printf("not found\n");
		return NULL;
	}
	memcpy(ifmac, p, 6);

        return ifmac;
}


/*
 * Given ip address and interface name,
 * This function will return Mac address of this ip address.
 */
uint8_t * interface_find_mac_for_dstip(uint32_t ip, char * if_name)
{
	unsigned char * ipmac;
	struct arp_table_t * parp;

	/*
	 * It is a line search but the total number of ARP table
	 * should be limited to the total number of hosts
	 * in the local subnet.
	 */

	// Check out the cached Mac Addresses
	parp=parphead;
	while(parp) {
		if(parp->ipaddr == ip) {
			//printf("found cached mac address\n");
			return parp->ipmac;
		}
		parp=parp->next;
	}

	// Not cached yet, check out OS ARP entries
	ipmac=find_mac_from_os(ip, if_name);
	if(!ipmac) { 
		struct in_addr in;
		in.s_addr=ip;
		printf("ERROR: Cannot get Mac address from OS for %s\n", inet_ntoa(in));
		return NULL;
	}
	//printf("Not cached yet\n");

	add_mac_to_list(ip, (char *)ipmac);

	return ipmac;
}

#if 0
// convert format from 00:15:17:57:C1:CC to 0x00, 0x15, 0x17, 0x57, 0xC1, 0xCC
static int convert_str_to_mac(char * Mac, unsigned char * addr)
{
	char val;
	int i;

	for(i=0;i<6;i++) {
		val=0;

		if (*Mac<='9' && *Mac >='0') val = *Mac - '0';
		else if (*Mac<='F' && *Mac >='A') val = (*Mac - 'A') + 10;
		else return 0;
		Mac++;

		val <<= 4;

		if (*Mac<='9' && *Mac >='0') val += *Mac - '0';
		else if (*Mac<='F' && *Mac >='A') val += (*Mac - 'A')+10;
		else return 0;
		Mac++;

		*addr=val;
		addr++;

		Mac++; //skip :
	}
	return 1;
}
#endif // 0



#ifdef INTERFACE_DEBUG
/*
 * The following code is used for unit testing purpose.
 * Compilation command is:
 *      gcc -I../../include -DINTERFACE_DEBUG -o interface -g interface.c
 * Command to run is:
 *      ./interface
 */

int net_use_nkn_stack=0;

int main(int argc, char * argv[])
{
	unsigned char * p;
	char * ipaddr[]={"10.1.1.101", "10.1.2.101", "10.1.3.101", "10.1.4.101"};
	struct nkn_server * prt;
	unsigned int ip;
	int i;

	interface_init();
	interface_display();

	for(i=0;i<4;i++) {

		ip=inet_addr(ipaddr[i]);
		prt=interface_find_by_dstip(ip);
		if(!prt) {
			printf("ERROR: cannot found interface for ip address=%s\n", ipaddr[i]);
			continue;
		}

		p=interface_find_mac_for_dstip(ip, prt->if_name);
		if(p) {
			printf("ipaddr=%s ifname=%s Mac=", ipaddr[i], prt->if_name);
			printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
				*p, *(p+1), *(p+2),*(p+3),*(p+4),*(p+5));
		}
	}

}
#endif // INTERFACE_DEBUG

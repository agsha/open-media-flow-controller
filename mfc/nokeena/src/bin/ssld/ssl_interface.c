#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <unistd.h> 
#include <time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <sys/queue.h>
#include <ifaddrs.h>

#include "ssl_defs.h"
#include "ssl_interface.h"
#include "nkn_stat.h"

NKNCNT_DEF(tot_interface, uint16_t, "", "Total network interface found")

nkn_interface_t interface[MAX_NKN_INTERFACE];

/*
 * Get Link Aggregation Slave Interface Number
 */
int get_link_aggregation_slave_interface_num(char * if_name);
int get_link_aggregation_slave_interface_num(char * if_name)
{
	FILE *fp;
	char filename[256];
	char *line = NULL;
	size_t len = 0;
	ssize_t l_read;
	int counter = 0;

	snprintf(filename, 256, "/proc/net/bonding/%s", if_name);
	if ((fp = fopen(filename, "r")) != NULL) {
		while ((l_read = getline(&line, &len, fp)) != -1) {
			if(strncmp(line, "Slave Interface", 15)== 0) {
				counter++;
			}
		}
		if (line) free(line);

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
        int fd, i, if_counter,s;
        struct ifconf ifc;
	nkn_interface_t *pns;
	struct ifreq ibuf[MAX_NKN_INTERFACE], ifr, *ifrp, *ifend;
	struct sockaddr_in sa;
	char name[64];
	uint32_t if_speed;
	char *intf_name = NULL;
	char *parent_intf = NULL;

	struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];
    struct in6_addr if_addrv6;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
                perror("[get_if_name] socket(AF_INET, SOCK_DGRAM, 0)");
                return -1;
        }

        memset(ibuf, 0, sizeof(struct ifreq)*MAX_NKN_INTERFACE);
        ifc.ifc_len = sizeof(ibuf);
        ifc.ifc_buf = (caddr_t) ibuf;

        /* gets interfaces list */
        if (ioctl(fd, SIOCGIFCONF, (char*)&ifc) == -1 ||
			ifc.ifc_len < (int)sizeof(struct ifreq)) {
                perror("[get_if_name] ioctl(SIOCGIFCONF)");
                close(fd);
                return -1;
        }

        if (getifaddrs(&ifaddr) == -1) {
                perror("[getifaddrs] error:");
                close(fd);
                return -1;
        }

        /* ifrp points to buffer and ifend points to buffer's end */
        ifrp = ibuf;
        ifend = (struct ifreq*) ((char*)ibuf + ifc.ifc_len);

	//printf("name\taddr\tsubnet\tnetmask\tmtu\tmac\n");
        for (; ifrp < ifend; ifrp++) {

		// We are excluding non eth<num> subnet and any subnet including ":"
		/*
		if(strncmp(ifrp->ifr_name, "eth", 3)!=0 &&
		   strncmp(ifrp->ifr_name, "bond", 4)!=0 &&
		   strncmp(ifrp->ifr_name, "lo", 2)!=0) {
			continue;
		}
		*/
		for (i = 0; i < glob_tot_interface; i++) {
			if (strcmp(interface[i].if_name, ifrp->ifr_name)==0) {
				break;
			}
		}
		if (i != glob_tot_interface) continue;

		strncpy(ifr.ifr_name, ifrp->ifr_name, sizeof(ifr.ifr_name));

                if (ioctl(fd, SIOCGIFFLAGS, (char*)&ifr) == -1) {
                        perror("DEBUG: [get_if_name] ioctl(SIOCGIFFLAGS)");
                        continue;
                }

                /* check out if interface up or down */
                if (!(ifr.ifr_flags & IFF_UP)) {
                        printf("DOWN\n");
                        continue;
                }

		/* check out if interface IP address configured */
                if (ioctl(fd, SIOCGIFADDR, (char*)&ifr) == -1) {
                        perror("DEBUG: [get_if_name] ioctl(SIOCGIFADDR)");
			// No ip address configured.
			continue;
                }

		/*
		 * add a new server structure on this interface
		 */
        if (glob_tot_interface == MAX_NKN_INTERFACE) {
            break;
        }
		pns = &interface[glob_tot_interface];

		glob_tot_interface++;
		//assert(glob_tot_interface < MAX_NKN_INTERFACE);
		memset((char *)pns, 0, sizeof(nkn_interface_t));

		strncpy(pns->if_name, ifr.ifr_name, sizeof(ifr.ifr_name));

		/* add monitor counters */
		snprintf(name, 64, "net_port_%s_tot_sessions", pns->if_name);
		nkn_mon_add(name, NULL, &pns->tot_sessions, sizeof(pns->tot_sessions));
		snprintf(name, 64, "net_port_%s_available_bw", pns->if_name);
		nkn_mon_add(name, NULL, &pns->max_allowed_bw, sizeof(pns->max_allowed_bw));

                /* save if address */
                memcpy(&sa, &ifr.ifr_addr, sizeof(struct sockaddr_in));
		pns->if_addrv4=sa.sin_addr.s_addr;

               /* save ipv6 address */
                ifa = ifaddr;
                while (ifa != NULL) {
                        if (strcmp(ifa->ifa_name, pns->if_name) == 0) {
                                if (ifa->ifa_addr && (ifa->ifa_addr->sa_family == AF_INET6)) {
                                        s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6),
                                                host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                                        if (s!=0) {
                                                perror("getnameinfo error");
                                                break;
                                        }
                                        s = inet_pton(AF_INET6, host, &if_addrv6);
                                        if(s !=1 ) {
                                                perror("inet_pton error for ipv6");
                                        } else {
                                            memcpy(&(pns->if_addrv6[pns->ifv6_ip_cnt].if_addrv6), &if_addrv6, sizeof
                                                    (struct in6_addr));
                                            pns->ifv6_ip_cnt++;
                                        }
                                }
                        }
                        memset (&if_addrv6, 0, sizeof (if_addrv6));
                        memset (host, 0, sizeof (host));
                        ifa = ifa->ifa_next;
                }


                /* save if netmask */
                if (ioctl(fd, SIOCGIFNETMASK, (char*)&ifr) == -1) {
                        perror("DEBUG: [get_if_name] ioctl(SIOCGIFNETMASK)");
                        exit(1);
                }
                memcpy(&sa, &ifr.ifr_netmask, sizeof(struct sockaddr_in));
		pns->if_netmask=sa.sin_addr.s_addr;

                /* calculate subnet */
		pns->if_subnet=pns->if_addrv4 & pns->if_netmask;

                /* get if mtu */
                if (ioctl(fd, SIOCGIFMTU, (char*)&ifr) == -1) {
                        perror("Warning: [get_if_name] ioctl(SIOCGIFMTU)");
			exit(1);
		}
		if (ifr.ifr_mtu==0) { pns->if_mtu = 1500; }
                else { pns->if_mtu = ifr.ifr_mtu; }

                /* get hardware address */
                if (ioctl(fd, SIOCGIFHWADDR, (char*)&ifr) == -1) {
                        perror("DEBUG: [get_if_name] ioctl(SIOCGIFHWADDR)");
                        exit(1);
                }
		memcpy(pns->if_mac, (unsigned char *)&ifr.ifr_hwaddr.sa_data[0], 6);

                /* get if bandwidth *
		 * PLEASE ALSO REFER TO update_interface_bandwidth() below as it modifies the
		 * if_bandwidth field with a link up event any change done here has to
		 * done in that function too
		 */

		/*For Bond interfaces */
		if (strstr(pns->if_name,":")) {
			parent_intf = strndup(pns->if_name,
					strstr(pns->if_name, ":") - pns->if_name);
			intf_name = parent_intf;
			if_counter = get_link_aggregation_slave_interface_num(intf_name);
		} else {
			intf_name = pns->if_name;
			if_counter = 1;
		}

		/*eth or lo interface */
		pns->if_bandwidth = if_counter * (GBYTES/8); //(GBYTES/8) * (if_speed / 1000);
	}

	close(fd);
	freeifaddrs(ifaddr);
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
	for (i = 0; i < glob_tot_interface; i++) {
		in.s_addr=interface[i].if_addrv4;
		in_netmask.s_addr=interface[i].if_netmask;
		in_subnet.s_addr=interface[i].if_subnet;
		printf("%6s %8d %6d %16s %16s %16s ",
			interface[i].if_name,
			interface[i].if_bandwidth/MBYTES,
			interface[i].if_mtu,
			inet_ntoa(in),
			inet_ntoa(in_netmask),
			inet_ntoa(in_subnet));

		p = (unsigned char *)&interface[i].if_mac;
		printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
			*p, *(p+1), *(p+2), *(p+3), *(p+4), *(p+5));

	}

	return;
}

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>
#include <features.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <sys/prctl.h>
#include "nkn_osl.h"
#include "interface.h"
#include "nkn_ioctl.h"
#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "nkn_stat.h"

NKNCNT_DEF(tcp_tot_pkts_sent, int64_t, "", "Total Packets Sent")
NKNCNT_DEF(tcp_tot_pkts_received, int64_t, "", "Total Packets Received")
NKNCNT_DEF(err_write_driver_ioctl, int32_t, "", "Write Ioctl error")

static int skfd = -1;
/*
struct no_iovec_t {
	int tot_pkt;
	long serverid;
	//unsigned long write_cur_len;
        //int nextIn;
        //int nextOut;
	unsigned long begin_MMAP_offset;
	unsigned long end_MMAP_offset;
        //pthread_t ctid;
        //pthread_cond_t count_threshold_cv;
        pthread_mutex_t ioctl_mutex;
        struct nkn_server * prt;
} noiovec[MAX_DRV_THREAD];
*/

#define MAX_ITEMS 1024

extern void * p_drv_mmap, * p_copy_mmap;
extern size_t l_drv_mmap, l_copy_mmap;
extern int (* so_ip_input)(char * p, int len);
extern void (* so_netfree)(void ** p);
extern void sig_timer(int nn);
extern void add_mac_to_list(uint32_t ip, char * ipmac);
extern void print_hex(const char * name, unsigned char * buf, int size);

//void nkn_run_Tx_thread(void);
//static void nkn_init_items_sg(void);
//static void close_nkn_dev_sg(void);
//static void Tx_ioctl_func_sg(void * arg);
//int write_nkn_dev_sg(void * head, int headlen, void * pbuf, int lbuf);
static void nkn_init_items(nkn_interface_t * prt);
static void close_nkn_dev(void);
static void Tx_ioctl_func(nkn_interface_t * prt);
void init_nkn_dev(void);
void end_nkn_dev(void);
void * read_nkn_dev(void * arg);
int write_nkn_dev(char * phead, int lhead, char * pbuf, int lbuf);


//////////////////////////////////////////////////////////////////////////////
// Tool functions
//////////////////////////////////////////////////////////////////////////////
/* Like strncpy but make sure the resulting string is always 0 terminated. */
static inline double
tv_diff (struct timeval *tv1, struct timeval *tv2)
{
    return ((tv2->tv_sec - tv1->tv_sec)
          + (tv2->tv_usec - tv1->tv_usec) / 1e6);
}


//////////////////////////////////////////////////////////////////////////////
// change our driver configuration
//////////////////////////////////////////////////////////////////////////////
void init_nkn_dev(void )
{
	char cmd[100];

	/*
	 * echo 80 > /sys/module/igb/parameters/nkn_sysfs_tcp_dport
	 */
	snprintf(cmd, 100, "echo %d > /sys/module/igb/parameters/nkn_sysfs_tcp_dport", nkn_http_serverport[0]);
	system(cmd);

	/*
	 * Open a basic socket.
	 */
	if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        	DBG_LOG(SEVERE, MOD_TCP, "socket creation error %d", errno);
		assert(0);
    	}

#if 0
	nkn_init_items_sg();
#endif // 0
}

void end_nkn_dev(void)
{
	/*
	 * echo 0 > /sys/module/igb/parameters/nkn_sysfs_tcp_dport
	 */
	system("echo 0 > /sys/module/igb/parameters/nkn_sysfs_tcp_dport");

	close(skfd);
#if 0
	close_nkn_dev_sg();
#endif // 0
	close_nkn_dev();
        DBG_LOG(MSG, MOD_TCP, "nokeena TCP stack shutdown successfully%s", "");
}


//////////////////////////////////////////////////////////////////////////////
// No IOVEC
//////////////////////////////////////////////////////////////////////////////
#if 0
void nkn_run_Tx_thread(void)
{
	if(useIOVEC==0) Tx_ioctl_func(NULL);
	else Tx_ioctl_func_sg(NULL);
}
#endif //0

static void nkn_init_items(nkn_interface_t * prt)
{
	static int tot=0;
	int rlen = RX_TOT_LEN / MAX_DRV_THREAD;
	int wlen = TX_TOT_LEN / MAX_DRV_THREAD;

	assert(tot<MAX_DRV_THREAD);
	prt->tot_pkt=0;
	pthread_mutex_init(&(prt->ioctl_mutex), NULL);
	prt->rx_begin_offset = p_mmap_header->rx_init_offset + tot * rlen;
	prt->rx_end_offset = prt->rx_begin_offset;
	prt->tx_begin_offset = p_mmap_header->tx_init_offset + tot * wlen;
	prt->tx_end_offset = prt->tx_begin_offset;
	DBG_LOG(MSG, MOD_TCP, "Network thread %d is created for packet sending", tot);
	tot++;
}

void * read_nkn_dev(void * arg)
{
	int ret, len;
	struct ifreq ifr;
	int i;
	long serverid = (long)arg;
	nkn_interface_t * prt;
	nkn_packet_t * ppacket;
	nkn_ioctl_fmt_t iofmt;
	unsigned int ip;
	char * p;
	prctl(PR_SET_NAME, "nvsd-nkndev", 0, 0, 0);
	//struct timeval tv1, tv2;

	prt = &interface[serverid];
	nkn_init_items(prt);

	DBG_LOG(MSG, MOD_TCP, "read_nkn_dev: ifname=%s thread.", prt->if_name);
	printf("read_nkn_dev: serverid=%ld ifname=%s thread.\n", serverid, prt->if_name);
	strcpy(ifr.ifr_name, prt->if_name);
	ifr.ifr_data = (void *)&iofmt;

	iofmt.numOfPacket = 0;
	iofmt.begin_offset = prt->rx_begin_offset;
	iofmt.end_offset = prt->rx_end_offset;

	while(1){
		//gettimeofday(&tv1, NULL);
		if (((ret=ioctl(skfd, NKN_USR_READ_IOCTL, &ifr)) < 0) || (iofmt.numOfPacket==0)) {
			Tx_ioctl_func(prt);
			sig_timer(0);
			usleep(100000);
			continue;
    		}
		glob_tcp_tot_pkts_received+=iofmt.numOfPacket;
		//printf("read_nkn_dev: dev=%s iofmt.numOfPacket=%d\n", prt->if_name,iofmt.numOfPacket);

		/* process each packet */
		p = (char *)p_drv_mmap + iofmt.begin_offset;
		for(i=0; i<iofmt.numOfPacket; i++) {
			//gettimeofday(&tv2, NULL);
       			//printf("rcv ioctl: tv1=%ld tv2=%ld\n", tv1.tv_usec, tv2.tv_usec);

			ppacket = (nkn_packet_t *)p;
			len = ppacket->len;
			if(len<=(SIZEOF_ETHERHEAD+40) || len>1514) break;
			p += nkn_packet_s;

			ip=*(unsigned int *)(p+SIZEOF_ETHERHEAD+12);
			add_mac_to_list(ip, p+6);
			//gettimeofday(&tv1, NULL);
			//print_hex("read_nkn_dev", (unsigned char *)p, len);
			so_ip_input(p+SIZEOF_ETHERHEAD, len-SIZEOF_ETHERHEAD);
			//gettimeofday(&tv2, NULL);
       			//printf("gen_pkt_time(user): tv1=%ld tv2=%ld delta=%lf\n", 
			//	tv1.tv_usec, tv2.tv_usec, tv_diff(&tv1, &tv2));

			p+=len;
		}

        	//pthread_cond_signal(&count_threshold_cv);
		Tx_ioctl_func(prt);
		sig_timer(0);

		iofmt.numOfPacket = 0;
		iofmt.begin_offset = prt->rx_begin_offset;
		iofmt.end_offset = prt->rx_end_offset;
	}
	return 0;
}

int write_nkn_dev(char * phead, int lhead, char * pbuf, int lbuf)
{
	char etherhead[]={0x08, 0x00};
	unsigned int ip;
	struct nkn_interface * prt;
	unsigned char * pdstmac;
	nkn_packet_t * ppacket;
	char * p;
	//struct timeval tv1;

        //printf("write_nkn_dev: lbuf=%d\n", lbuf);

	// The first 18 bytes are reserved for filling head
	// So we could avoid a copy

	// Checkout routing table to get source and destination Mac address
	//print_hex("write_nkn_dev", (unsigned char*)phead, lhead);
	ip=*(unsigned int *)&phead[16]; // IP header
	prt=interface_find_by_dstip(ip);
	if(!prt) {
                struct in_addr in;
                in.s_addr=ip;
		DBG_LOG(MSG, MOD_TCP, 
                	"ERROR: Cannot found routing interface for %s", inet_ntoa(in));
		//so_netfree((void **)&phead);
		return -1;
	}

	// persistent on the same interface, purpose only is to pick up one thread.
	// BUG in case of interface name is not eth<num>
        //printf("find_rt_by_dstip: ifname=%s\n", prt->ifname);
	pdstmac=interface_find_mac_for_dstip(ip, prt->if_name);
	if(pdstmac==NULL) {
		DBG_LOG(MSG, MOD_TCP, 
                	"ERROR: Cannot found MAC address for interface %s", prt->if_name);
		//so_netfree((void **)&phead);
		return -1;
	}
 
	pthread_mutex_lock(&prt->ioctl_mutex);
	// The first 4 bytes are length of the packet in host order
	// fill in the length of packet
	p = (char *)p_drv_mmap + prt->tx_end_offset;
	ppacket = (nkn_packet_t *)p;
	ppacket->len = lhead+lbuf+SIZEOF_ETHERHEAD;
	p += nkn_packet_s;
	memcpy(p, pdstmac, 6);  // destinatin Mac
	memcpy(p+6, prt->if_mac, 6);// source Mac 
	memcpy(p+12, etherhead, 2);// ethernet type
	p += SIZEOF_ETHERHEAD;
	memcpy(p, phead, lhead);
	if(pbuf) {
		/* when packet with data */
		p += lhead;
		memcpy(p, pbuf, lbuf);
	}
	//print_hex("write_nkn_dev", 
	//	(unsigned char*)p_drv_mmap + prt->tx_end_offset, 
	//	ppacket->len + nkn_packet_s);
	prt->tx_end_offset += ppacket->len + nkn_packet_s;
	prt->tot_pkt ++;

	/* Update counters */
	glob_tcp_tot_pkts_sent ++;

	//so_netfree((void **)&phead);
	pthread_mutex_unlock(&prt->ioctl_mutex);

	if(1 || prt->tot_pkt >= 10) {
		Tx_ioctl_func(prt);
	}
	return lhead+lbuf;
}

static void Tx_ioctl_func(nkn_interface_t * prt)
{
	struct ifreq ifr;
	int ret;
	nkn_ioctl_fmt_t iofmt;
	//struct timeval tv1, tv2;

	pthread_mutex_lock(&prt->ioctl_mutex);
	if(prt->tot_pkt) {
		/*
		 * Create the ioctl structure
		 */
		iofmt.numOfPacket = prt->tot_pkt;
		iofmt.begin_offset = prt->tx_begin_offset;
		iofmt.end_offset = prt->tx_end_offset;

		ifr.ifr_data = (void *)&iofmt;
		strncpy(ifr.ifr_name, prt->if_name, 16);

		//gettimeofday(&tv1, NULL);
		ret = ioctl(skfd, NKN_USR_WRITE_IOCTL, &ifr);
		//gettimeofday(&tv2, NULL); // somehow we need to give up CPU
		//printf("write_ioctl: tv1=%ld tv2=%ld delta=%f\n", tv1.tv_usec, tv2.tv_usec, tv_diff(&tv1, &tv2));
		// usleep(10);
		if (ret < 0) {
			DBG_LOG(MSG, MOD_TCP, "ERROR: ioctl return %d", ret);
			glob_err_write_driver_ioctl++;
		}
		prt->tot_pkt = 0;
		prt->tx_end_offset = prt->tx_begin_offset;
	}
        pthread_mutex_unlock(&prt->ioctl_mutex);
}

static void close_nkn_dev(void)
{
	int i;
	for(i=0;i<MAX_NKN_INTERFACE;i++) {
		if(interface[i].rx_begin_offset) {
			pthread_mutex_destroy(&interface[i].ioctl_mutex);
		}
	}
}

#if 0
//////////////////////////////////////////////////////////////////////////////
// IOVEC
//////////////////////////////////////////////////////////////////////////////
struct items_t {
        char head[96];
        char * ppayload;
        int headlen;
        int payloadlen;
};

struct mynkn_t {
        //int fd;
        int nextIn;
        int nextOut;
        int tot_pkt_in_queue;
        struct items_t items_sg[MAX_ITEMS];
        pthread_t ctid;
        //pthread_cond_t count_threshold_cv;
        pthread_mutex_t ioctl_mutex;
        pthread_mutex_t count_mutex;
        struct nkn_server * prt;
} mynkn[4];
pthread_mutex_t copy_mmap_mutex;

static void nkn_init_items_sg()
{
	int i;

	pthread_mutex_init(&copy_mmap_mutex, NULL);
	for(i=0;i<4;i++) {

		memset((char *)&mynkn[i], 0, sizeof(struct mynkn_t));
		pthread_mutex_init(&mynkn[i].count_mutex, NULL);
	}
	DBG_LOG(MSG, MOD_TCP, 
		"Thread is created for packet sending%s", "");
}



/* pointer of head includes Mac address */
int write_nkn_dev_sg(void * head, int headlen, void * pbuf, int lbuf)
{
        struct ifreq ifr;
        int ret;
        char etherhead[]={0x08, 0x00};
        unsigned int ip;
        struct nkn_server * prt;
        unsigned char * pdstmac;
	struct items_t * pitem;
	struct mynkn_t * pnkn;
	char *pch;
	int ch;
	nkn_tx_packet_t * p_ioctl_packet;

	//print_hex("write_nkn_dev_sg head", head, headlen);
	//print_hex("write_nkn_dev_sg body", pbuf, lbuf);
	if(pbuf && 
	    (((char*)pbuf < (char*)p_drv_mmap) || ((char*)pbuf+lbuf > (char*)p_drv_mmap + l_drv_mmap) ))  {
		char * pbuftmp;
		static unsigned int copy_buf_offset = 0;

		//printf("ERROR: MMAP buffer is not used. pbuf=%p, lbuf=%d\n", pbuf, lbuf);
		/* 
		 * solution: we make a copy to the copy buffer from this pbuf.
		 * TBD: We have two bugs.
		 * 1. memory leak of pbuf.
		 * 2. 100 MBytes is not enough and memory overwritten.
		 */
        	pthread_mutex_lock(&copy_mmap_mutex);
		pbuftmp = (char *)pbuf;
		if(copy_buf_offset + lbuf >= l_copy_mmap) {
			copy_buf_offset = 0;
		}
		pbuf = (char *)p_copy_mmap + copy_buf_offset;
		memcpy(pbuf, pbuftmp, lbuf);
		copy_buf_offset += lbuf;
        	pthread_mutex_unlock(&copy_mmap_mutex);
		//printf("ERROR: after pbuf=%p, lbuf=%d\n", pbuf, lbuf);
 
	}
        //pthread_mutex_lock(&ioctl_mutex);

	/* the reason that we need to back up head is that ip_output() will do m_freem(m) */

        // Checkout routing table to get source and destination Mac address
	pch=(char *)head;
        ip=*(unsigned int *)&pch[14+16];
        prt=interface_find_by_dstip(ip);
        if(!prt) {
                struct in_addr in;
                in.s_addr=ip;
		DBG_LOG(MSG, MOD_TCP, 
                	"ERROR: Cannot found routing interface for %s\n", inet_ntoa(in));
        	//pthread_mutex_unlock(&pnk->ioctl_mutex);
                return -1;
        }
	ch=(int)(prt->if_name[3]) % 4;	// persistent on the same interface, purpose only is to pick up one thread.
				// BUG in case of interface name is not eth<num>
	pnkn=&mynkn[ch];
	pnkn->prt=prt;

        pdstmac=interface_find_mac_for_dstip(ip, prt->if_name);
        if(pdstmac==NULL) {
		DBG_LOG(MSG, MOD_TCP, 
                	"ERROR: Cannot found MAC address for interface %s \n", prt->if_name);
        	//pthread_mutex_unlock(&pnk->ioctl_mutex);
		return -1;
	}

	/* pick up one slot */
        pitem=(struct items_t *)&pnkn->items_sg[pnkn->nextIn];

	memcpy(pitem->head, head, headlen); // in ip_output, it will call m_freem() to free the header
	pitem->headlen=headlen;
	pitem->ppayload=pbuf;
	pitem->payloadlen=lbuf;
	
        // The first 14 bytes are reserved for filling head
        memcpy(&pitem->head[0], pdstmac, 6);  // destinatin Mac
        memcpy(&pitem->head[6], prt->if_mac, 6);// source Mac
        memcpy(&pitem->head[12], etherhead, 2);// ethernet type

	/* Update counters */
	glob_tcp_tot_pkts_sent ++;
        pthread_mutex_lock(&pnkn->count_mutex);
        pnkn->tot_pkt_in_queue++; // should use atomic decrease
        pthread_mutex_unlock(&pnkn->count_mutex);

        pnkn->nextIn = (pnkn->nextIn + 1) % MAX_ITEMS;
        pitem=(struct items_t *)&pnkn->items_sg[pnkn->nextIn];
        if (pitem->headlen != 0)
        {
                //printf("ERROR: nkn_sendquque is full and sleep here. size=%d\n", MAX_ITEMS);
		Tx_ioctl_func_sg(NULL);
        }

        //pthread_mutex_unlock(&pnkn->ioctl_mutex);

        return lbuf+headlen;
}


static void Tx_ioctl_func_sg(void * arg)
{
   struct mynkn_t * pnkn;
   struct items_t * pitem;
   //struct timeval tv1, tv2;
   int totpkt;
   struct ifreq ifr;
   char packet[nkn_tx_packet_sg_s];
   int i, ret;
   nkn_tx_packet_sg_t * p_nkn_tx_packet_sg;
   nkn_iovec_sg_t * piov;

   p_nkn_tx_packet_sg = (nkn_tx_packet_sg_t *)&packet[0];

   for(i=0;i<4;i++) {

	pnkn=&mynkn[i];
        pthread_mutex_lock(&pnkn->ioctl_mutex);

        pitem=(struct items_t *)&pnkn->items_sg[pnkn->nextOut];
        if (pitem->headlen == 0)
        {
        	pthread_mutex_unlock(&pnkn->ioctl_mutex);
		continue;
        }

again:
	//printf("Tx_ioctl_func_sg: i=%d headlen=%d\n", i, pitem->headlen);
        pitem=(struct items_t *)&pnkn->items_sg[pnkn->nextOut];
	//gettimeofday(&tv2, NULL); // somehow we need to give up CPU
       	//printf("Tx_ioctl_func_sg: tv2=%ld\n", tv2.tv_usec);

	p_nkn_tx_packet_sg->totpkt=0;
        strncpy(ifr.ifr_name, pnkn->prt->if_name, 16);
        ifr.ifr_data = packet;
	piov = (nkn_iovec_sg_t *) &p_nkn_tx_packet_sg->ppkt[0];
        while (pitem->headlen) {
		int num;

		piov->header_ptr = pitem->head;
		piov->header_len = pitem->headlen;
		piov->payload_len = pitem->payloadlen;
		if(pitem->payloadlen) {
			piov->payload_ptr = (unsigned long)((char *)pitem->ppayload-(char *)p_drv_mmap);
		} else {
			piov->payload_ptr = 0; // This line is unnecessary
		}

		piov++;

		//pitem->headlen=0;
                pnkn->nextOut = (pnkn->nextOut + 1) % MAX_ITEMS;
        	pitem=(struct items_t *)&(pnkn->items_sg[pnkn->nextOut]);

		/* update counters */
		pthread_mutex_lock(&pnkn->count_mutex);
		pnkn->tot_pkt_in_queue--; // should use atomic decrease
		pthread_mutex_unlock(&pnkn->count_mutex);

		p_nkn_tx_packet_sg->totpkt++;
		if(p_nkn_tx_packet_sg->totpkt>10) break;
	}
        if(p_nkn_tx_packet_sg->totpkt) {
		//gettimeofday(&tv1, NULL);
		ret = ioctl(skfd, NKN_USR_WRITE_IOCTL_SG, &ifr);
		//gettimeofday(&tv2, NULL); // somehow we need to give up CPU
		//printf("ioctl_time (drv):   tv1=%ld tv2=%ld delta=%ld\n", 
		//	tv1.tv_usec, tv2.tv_usec, tv_diff(&tv1, &tv2));
		if (ret < 0) {
			DBG_LOG(MSG, MOD_TCP, 
				"ERROR: IOVEC ioctl return %d errno=%d\n", ret, errno);
			glob_err_write_driver_ioctl++;
		}

		goto again;
	}
        pthread_mutex_unlock(&pnkn->ioctl_mutex);
    }

}

void close_nkn_dev_sg(void)
{
	int i;
	for(i=0;i<4;i++) {
		pthread_mutex_destroy(&(mynkn[i].ioctl_mutex));
		pthread_mutex_destroy(&(mynkn[i].count_mutex));
	}
}
#endif // 0

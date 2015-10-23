#ifndef __NKNS__OSL__
#define __NKNS__OSL__

//
// defined in the Linux
//
struct nkn_osl_funcs_t {
	void * (* nkn_linux_malloc)(size_t size);
	void   (* nkn_linux_free)(void *ptr);
	long int (* nkn_random)(void);
	void (* nkn_exit)(int __status) __attribute__ ((__noreturn__));
	int (* nkn_usleep)( __useconds_t  usec);
	int (* nkn_pthread_mutex_lock)(pthread_mutex_t *mutex);
	int (* nkn_pthread_mutex_trylock)(pthread_mutex_t *mutex);
	int (* nkn_pthread_mutex_unlock)(pthread_mutex_t *mutex);
	void (* nkn_print_trace) (void);
	int (* nkn_gettimeofday)(struct timeval *tv, struct timezone *tz);
	int (* nkn_send_packet)(char * phead, int lhead, char * pbuf, int lbuf);
};


//
// Defined in nkn_route.c
//
struct route_table_t {
        struct route_table_t * next;
        char ifname[10];
        unsigned int ifaddr;
        unsigned int subnet;   // Network order
        unsigned int netmask;   // Network order
        unsigned int gw;        // Network order
        unsigned char ifmac[6];
        int ifmtu;
};
extern unsigned char * get_mac_for_ip(unsigned int ip, char * ifname);
extern struct route_table_t * find_rt_by_dstip(unsigned int dstip);
extern void init_route_table(void);
extern void display_route_table(void);
extern struct route_table_t * getNextRT(struct route_table_t * prt, char * ifname);


#endif // __NKNS__OSL__


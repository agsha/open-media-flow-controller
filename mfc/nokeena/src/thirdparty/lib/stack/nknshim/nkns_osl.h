#ifndef __NKNS__OSL__
#define __NKNS__OSL__


/*
 * defined in the Linux, function pointers passed in through nkn_init_func().
 */

extern void * (* nkn_linux_malloc)(size_t size);
extern void   (* nkn_linux_free)(void *ptr);
extern long int (* nkn_random)(void);
extern void (* nkn_exit)(int __status) __attribute__ ((__noreturn__));
extern int (* nkn_usleep)(useconds_t usec);
extern int (* nkn_pthread_mutex_lock)(pthread_mutex_t *mutex);
extern int (* nkn_pthread_mutex_trylock)(pthread_mutex_t *mutex);
extern int (* nkn_pthread_mutex_unlock)(pthread_mutex_t *mutex);
extern void (* nkn_print_trace) (void);
extern int (* nkn_gettimeofday)(struct timeval *tv, struct timezone *tz);
extern int (* nkn_send_packet)(char * phead, int lhead, char * pbuf, int lbuf);

struct nkn_osl_funcs_t {
	void * (* nkn_linux_malloc)(size_t size);
	void   (* nkn_linux_free)(void *ptr);
	long int (* nkn_random)(void);
	void (* nkn_exit)(int __status) __attribute__ ((__noreturn__));
	int (* nkn_usleep)(useconds_t usec);
	int (* nkn_pthread_mutex_lock)(pthread_mutex_t *mutex);
	int (* nkn_pthread_mutex_trylock)(pthread_mutex_t *mutex);
	int (* nkn_pthread_mutex_unlock)(pthread_mutex_t *mutex);
	void (* nkn_print_trace) (void);
	int (* nkn_gettimeofday)(struct timeval *tv, struct timezone *tz);
	int (* nkn_send_packet)(char * phead, int lhead, char * pbuf, int lbuf);
};


/*
 * Defined in nkn_debug.c
 */

#define NKN_VERBOSE_MBUF_DUMP		0x00000002
#define NKN_VERBOSE_HEXDATA		0x00000004
#define NKN_VERBOSE_TCP			0x00000008
#define NKN_VERBOSE_DELTA		0x00000010
#define NKN_VERBOSE_MEM			0x00000020
#define NKN_VERBOSE_WRP			0x00000040
#define NKN_VERBOSE_SYSLOG		0x00000080
#define NKN_VERBOSE_PKT_FLOW		0x00000100
#define NKN_VERBOSE_CACHE_D		0x00000200
#define NKN_VERBOSE_TUNNEL		0x00000400
#define NKN_VERBOSE_PROXY		0x00000800
#define NKN_VERBOSE_POLICY		0x00001000
#define NKN_VERBOSE_APPPPLICATION	0x00002000
#define NKN_VERBOSE_CIFS		0x00004000
#define NKN_VERBOSE_KERN		0x00008000
#define NKN_VERBOSE_OSL			0x00010000
#define NKN_VERBOSE_FLAG_MAX		0x80000000

extern void net_print_hex(char * name, unsigned char * buf, int size);
extern void net_assert(char * file, int line, char * failedexpr);
extern void net_printf(char * sFmt, ...);

extern int nkn_verbose;
extern int curcpu;
extern void * nkn_netmalloc(int size);
extern void nkn_netfree(void **ptr);

#define NKN_MTU		1460		// Each mbuf packet size


#define DBG_MBUF_DUMP(A, B) 	if(nkn_verbose & (NKN_VERBOSE_MBUF_DUMP|B)) { m_print A; }
#define DBG_HEXDATA(A)		if(nkn_verbose & NKN_VERBOSE_HEXDATA)    { net_print_hex A; }
#define DBG_TCP(A)		if(nkn_verbose & NKN_VERBOSE_TCP) { net_printf A; }
#define DBG_DELTA(A)		if(nkn_verbose & NKN_VERBOSE_DELTA) { net_printf A; }
#define DBG_MEM(A)		if(nkn_verbose & NKN_VERBOSE_MEM) { net_printf A; }
#define DBG_WRP(A)		if(nkn_verbose & NKN_VERBOSE_WRP) { net_printf A; }
#define DBG_SYSLOG(A)		if(nkn_verbose & NKN_VERBOSE_SYSLOG) { printf A; }
#define DBG_PKT_FLOW(A)		if(nkn_verbose & NKN_VERBOSE_PKT_FLOW) { net_printf A; }
#define DBG_CACHE_D(A)		if(nkn_verbose & NKN_VERBOSE_CACHE_D) { net_printf A; }
#define DBG_TUNNEL(A)		if(nkn_verbose & NKN_VERBOSE_TUNNEL) { net_printf A; }
#define DBG_PROXY(A)		if(nkn_verbose & NKN_VERBOSE_PROXY) { net_printf A; }
#define DBG_POLICY(A)		if(nkn_verbose & NKN_VERBOSE_POLICY) { net_printf A; }
#define DBG_APP(A)		if(nkn_verbose & NKN_VERBOSE_APPPPLICATION) { net_printf A; }
#define DBG_CIFS(A)		if(nkn_verbose & NKN_VERBOSE_CIFS) { net_printf A; }
#define DBG_KERN(A)		if(nkn_verbose & NKN_VERBOSE_KERN) { net_printf A; }
#define DBG_OSL(A)		if(nkn_verbose & NKN_VERBOSE_OSL) { net_printf A; }
#define DBG_MAX(A)		if(nkn_verbose & NKN_VERBOSE_FLAG_MAX) { net_printf A; }


#endif // __NKNS__OSL__


/*
 * net.c -- libnet API 
 */
#include <stdio.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <unistd.h>
#include <libgen.h>
#include <malloc.h>
#include <assert.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netdb.h>

#include "network.h"
#include "nkn_lockstat.h"
#include "network_int.h"
#include "libnet/libnet.h"
#include "memalloc.h"


extern void *__libc_malloc(size_t n);
extern void *__libc_realloc(void *ptr, size_t size);
extern void *__libc_calloc(size_t n, size_t size);
extern void __libc_free(void *ptr);

extern int check_out_hmoq(int num_of_epoll);
extern void check_out_cp_queue(int num_of_epoll);
extern void check_out_fp_queue(int num_of_epoll);
extern void check_out_tmrq(int num_of_epoll);
extern void check_out_pe_req_queue(int num_of_epoll);
extern void nkn_setup_interface_parameter(nkn_interface_t *intf);

int NM_tot_threads = 2;
int nkn_max_client = MAX_GNM;
uint16_t glob_tot_interface = 0;
int nm_lb_policy = 0;
int net_use_nkn_stack = 0;
int close_socket_by_RST_enable = 0;
nkn_interface_t interface[MAX_NKN_INTERFACE];
time_t nkn_cur_ts;
char myhostname[1024];
int myhostname_len;
char **http_listen_intfname;
int http_listen_intfcnt = 0;
int nkn_http_ipv6_enable = 0;
int nkn_http_serverport[MAX_PORTLIST];

int http_idle_timeout = 60;
int max_http_req_size = 16384;

volatile sig_atomic_t srv_shutdown;

nkn_interface_t *if_alias_list;
int bind_socket_with_interface = 1;

int libnet_def_log_level = MSG3;
int *libnet_log_level;
int (*pnet_dbg_log)(int, long, const char *, ...)
		    __attribute__ ((format (printf, 3, 4)));
void *(*pnet_malloc_type)(size_t n, nkn_obj_type_t type);
void *(*pnet_realloc_type)(void *ptr, size_t size, nkn_obj_type_t type);
void *(*pnet_calloc_type)(size_t n, size_t size, nkn_obj_type_t type);
void (*pnet_free)(void *ptr);
int (*pnet_recv_data_event)(libnet_conn_handle_t *handle, 
			    char *data, int datalen, 
			    const libnet_conn_attr_t *attr);
int (*pnet_recv_data_event_err)(libnet_conn_handle_t *handle, 
				char *data, int datalen, 
			   	const libnet_conn_attr_t *attr, int reason);
int (*pnet_send_data_event)(libnet_conn_handle_t *handle);
int (*pnet_timeout_event)(libnet_conn_handle_t *handle);
int (*so_shutdown)(int s, int how) = 0;

static void close_conn(con_t *con);

static pthread_t timer_1sec_thread_id;
static pthread_t net_parent_thread_id;

pthread_mutex_t hmoq_mutex[MAX_EPOLL_THREADS] = 
{ 
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER,
PTHREAD_MUTEX_INITIALIZER
};

pthread_cond_t hmoq_cond[MAX_EPOLL_THREADS] = { 
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER,
PTHREAD_COND_INITIALIZER
};

nkn_lockstat_t nm_lockstat[MAX_EPOLL_THREADS];

NKNCNT_DEF(tot_svr_sockets, uint16_t, "", "Total server sockets")
NKNCNT_DEF(tot_svr_sockets_ipv6, uint16_t, "", "Total ipv6 server sockets")
NKNCNT_DEF(tot_alias_interface, uint16_t, "", "Total network interface aliases found")
NKNCNT_DEF(accept_queue_100, uint64_t, "", "times of accept queue when larger than 100")
NKNCNT_DEF(vip_parent_if_hit, uint64_t, "", "Successful getsockopt for VIP")
NKNCNT_DEF(vip_parent_if_miss, uint64_t, "", "Failed getsockopt for VIP")
NKNCNT_DEF(overflow_socket_due_afr, uint64_t, "", "num of sockets closed due to AFR limit")
NKNCNT_DEF(tot_vip_hit, uint64_t, "", "Total conns received for VIPs")

NKNCNT_DEF(tot_http_sockets, AO_t, "", "Total HTTP sockets")
NKNCNT_DEF(tot_http_sockets_ipv6, AO_t, "", "Total HTTP sockets ipv6")
NKNCNT_DEF(cur_open_http_socket, AO_t, "", "Total currently open HTTP sockets")
NKNCNT_DEF(cur_open_http_socket_ipv6, AO_t, "", "Total currently open HTTP sockets for ipv6")
NKNCNT_DEF(enlarge_cb_size, uint64_t, "", "enlarge network cb size")
NKNCNT_DEF(tot_bytes_recv, uint64_t, "Bytes", "Total received bytes from socket")
NKNCNT_DEF(client_recv_tot_bytes, AO_t, "", "read data")
NKNCNT_DEF(warn_socket_no_recv_data, uint64_t, "", "num of sockets closed without any data received")
NKNCNT_DEF(warn_socket_no_send_data, uint64_t, "", "num of sockets closed without any data sent")


#ifndef IP_INDEV
#define IP_INDEV 125
#endif

#define F_FILE_ID LT_SERVER

#define CONF_FIRST_TASK		0x0000000000000001
#define CONF_CANCELD		0x0000000000000004
#define CONF_TASK_POSTED	0x0000000000000008
#define CONF_RECV_DATA		0x0000000000000010
#define CONF_SEND_DATA		0x0000000000000020
#define CONF_BLOCK_RECV		0x0000000000000040
#define CONF_NO_CONTENT_LENGTH	0x0000000000000080
#define CONF_RESP_PARSER_ERROR  0x0000000000000100
#define CONF_TASK_TIMEOUT	0x0000000000000200
#define CONF_HAS_SET_TOS	0x0000000000000800
#define CONF_RETRY_HDL_REQ	0x0000000000001000
#define CONF_REDIRECT_REQ	0x0000000000002000
#define CONF_DNS_DONE		0x0000000000004000
#define CONF_RECV_ERROR         0x0000000000008000
#define CONF_CLIENT_CLOSED      0x0000000000010000
#define CONF_TUNNEL_PROGRESS	0x0000000000020000
#define CONF_RESUME_TASK	0x0000000000040000
#define CONF_INTRL_REQ_SRC	0x0000000000100000
#define CONF_INTRL_HTTPS_REQ	0x0000000000200000
#define CONF_HTTPS_HDR_RECVD	0x0000000000400000
#define CONF_REFETCH_REQ	0x0000000000800000
#define CONF_GEOIP_TASK		0x0000000001000000
#define CONF_PE_HOST_HDR	0x0000000002000000
#define CONF_DNS_HOST_IS_IP	0x0000000004000000
#define CONF_UCFLT_TASK		0x0000000008000000

#define CON_MAGIC_FREE          0x123456789deadbee
#define CON_MAGIC_USED          0x1111111111111111

#define IPv4(x)  x.addr.v4.s_addr
#define IPv6(x)  x.addr.v6.s6_addr

#define TYPICAL_HTTP_CB_BUF 4096

#define SET_CON_FLAG(x, f) (x)->con_flag |= (f)
#define UNSET_CON_FLAG(x, f) (x)->con_flag &= ~(f)
#define CHECK_CON_FLAG(x, f) ((x)->con_flag & (f))

#define CONF_INTRL_REQ_SRC      0x0000000000100000

#define MK_HANDLE(_hdl, _con) \
	(_hdl)->opaque[0] = (uint64_t)(_con); \
	(_hdl)->opaque[1] = (uint64_t) \
	    (((uint64_t)(gnm[(_con)->fd].incarn) << 32) | (_con)->fd);

#define HANDLE_TO_DATA(_hdl, _con, _fd, _incarn) \
	(_con) = (con_t *)(_hdl)->opaque[0]; \
	(_fd) = (int32_t)(_hdl)->opaque[1]; \
	(_incarn) = (int32_t)((_hdl)->opaque[1] >> 32);

#define VALID_HANDLE(_con, _fd, _incarn) \
	(((_con)->fd == (_fd)) && (gnm[(_con)->fd].incarn == (_incarn)))

int libnet_dbg_log(int level, long module, const char *fmt, ...)
		    __attribute__ ((format (printf, 3, 4)));

int libnet_dbg_log(int level, long module, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    printf("[Level=%d Module=0x%lx]:", level, module);
    vprintf(fmt, ap);
    printf("\n");
    return 0;
}

static void *libnet_malloc_type(size_t size, nkn_obj_type_t type)
{
    UNUSED_ARGUMENT(type);
    return __libc_malloc(size);
}

static void *libnet_realloc_type(void *ptr, size_t size, nkn_obj_type_t type)
{
    UNUSED_ARGUMENT(type);
    return __libc_realloc(ptr, size);
}

static void *libnet_calloc_type(size_t nmemb, size_t size, nkn_obj_type_t type)
{
    UNUSED_ARGUMENT(type);
    return __libc_calloc(nmemb, size);
}

static void libnet_free(void *ptr)
{
    return __libc_free(ptr);
}

static int libnet_recv_data_event(libnet_conn_handle_t *handle, 
				  char *data, int datalen, 
				  const libnet_conn_attr_t *attr)
{
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(datalen);
    UNUSED_ARGUMENT(attr);

    con_t *con;
    int32_t fd;
    uint32_t incarn;

    HANDLE_TO_DATA(handle, con, fd, incarn);
    if (!VALID_HANDLE(con, fd, incarn)) {
    	return 1; // Invalid handle
    }

    close_conn(con);
    return 0;
}

static int libnet_recv_data_event_err(libnet_conn_handle_t *handle, 
				      char *data, int datalen, 
				      const libnet_conn_attr_t *attr, 
				      int reason)
{
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(datalen);
    UNUSED_ARGUMENT(attr);
    UNUSED_ARGUMENT(reason);

    con_t *con;
    int32_t fd;
    uint32_t incarn;

    HANDLE_TO_DATA(handle, con, fd, incarn);
    if (!VALID_HANDLE(con, fd, incarn)) {
    	return 1; // Invalid handle
    }

    close_conn(con);
    return 0;
}

static int libnet_send_data_event(libnet_conn_handle_t *handle)
{
    con_t *con;
    int32_t fd;
    uint32_t incarn;

    HANDLE_TO_DATA(handle, con, fd, incarn);
    if (!VALID_HANDLE(con, fd, incarn)) {
    	return 1; // Invalid handle
    }

    close_conn(con);
    return 0;
}

static int libnet_timeout_event(libnet_conn_handle_t *handle)
{
    con_t *con;
    int32_t fd;
    uint32_t incarn;

    HANDLE_TO_DATA(handle, con, fd, incarn);
    if (!VALID_HANDLE(con, fd, incarn)) {
    	return 1; // Invalid handle
    }

    close_conn(con);
    return 0;
}

/*
 *******************************************************************************
 * Internal functions called from network.c
 *******************************************************************************
 */
int check_out_hmoq(int num_of_epoll)
{
    UNUSED_ARGUMENT(num_of_epoll);
    return 0;
}

void check_out_cp_queue(int num_of_epoll)
{
    UNUSED_ARGUMENT(num_of_epoll);
    return;
}

void check_out_fp_queue(int num_of_epoll)
{
    UNUSED_ARGUMENT(num_of_epoll);
    return;
}

void check_out_tmrq(int num_of_epoll)
{
    UNUSED_ARGUMENT(num_of_epoll);
    return;
}

void check_out_pe_req_queue(int num_of_epoll)
{
    UNUSED_ARGUMENT(num_of_epoll);
    return;
}

void nkn_setup_interface_parameter(nkn_interface_t *pns)
{
    AO_store(&pns->max_allowed_sessions, nkn_max_client);
    AO_store(&pns->max_allowed_bw, pns->if_bandwidth);
}

void NM_set_socket_active(network_mgr_t *pnm)
{
    int slot;

    if (pnm->f_timeout == NULL) {
	return;
    }

    pnm->last_active = nkn_cur_ts;

    // Timeout Queue operation
    NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnm->pthr->num]);
    if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
	LIST_REMOVE(pnm, toq_entries);
    }
    slot = (pnm->pthr->cur_toq + pnm->timeout_slot) % MAX_TOQ_SLOT;
    LIST_INSERT_HEAD(&(pnm->pthr->toq_head[slot]), pnm, toq_entries);
    NM_SET_FLAG(pnm, NMF_IN_TIMEOUTQ);
    NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);

    return;
}

void nkn_mem_create_thread_pool(const char *pname)
{
    UNUSED_ARGUMENT(pname);
    return;
}

/*
 *******************************************************************************
 * External network.c thread functions
 *******************************************************************************
 */
static
void *timer_1sec(void *arg)
{
    UNUSED_ARGUMENT(arg);
    prctl(PR_SET_NAME, "lnet-1sec", 0, 0, 0);

    int count2sec = 0;

    while (1) { // Begin while

    time(&nkn_cur_ts);

    count2sec++;
    if (count2sec >= 2) {
    	count2sec = 0;
	NM_timer();
    }

    sleep(1);

    } // End while
    return NULL;
}

static void *net_parent_func(void *arg)
{
    UNUSED_ARGUMENT(arg);
    prctl(PR_SET_NAME, "lnet-parent", 0, 0, 0);

    NM_main();

    return 0;
}

/*
 *******************************************************************************
 * Internal interface[] initialization functions
 *******************************************************************************
 */
static int add_pns_to_alias_list(nkn_interface_t *pns)
{
    if (if_alias_list) {
        if_alias_list->prev = pns;
    }
    pns->prev = NULL;
    pns->next = if_alias_list;
    if_alias_list = pns;

    glob_tot_alias_interface++;
    return 0;
}

static int del_pns_from_alias_list(nkn_interface_t *pns)
{
    assert(!"del_pns_from_alias_list() invalid call");

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
    glob_tot_alias_interface--;

    return 0;
}

static int interface_init(void)
{
    int fd, i, s;
    nkn_interface_t *pns;
    struct ifreq ifr;
    struct sockaddr_in sa;
    const char *intf_name = NULL;

    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];
    struct in6_addr if_addrv6;
    int if_count = 0;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("[get_if_name] socket(AF_INET, SOCK_DGRAM, 0)");
        return 1;
    }

    if (getifaddrs(&ifaddr) == -1) {
        perror("[getifaddrs] error:");
        close(fd);
        return 2;
    }

    ifa = ifaddr;
    for ( ; ifa; ifa = ifa->ifa_next) {
        strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));

        if (ioctl(fd, SIOCGIFFLAGS, (char*)&ifr) == -1) {
            perror("DEBUG: [get_if_name] ioctl(SIOCGIFFLAGS)");
            continue;
        }

        /* check out if interface up or down */
        if (!(ifr.ifr_flags & IFF_UP)) {
            printf("DOWN\n");
            continue;
        }

        /*
         * add a new server structure on this interface
         */
        if (strstr (ifa->ifa_name, ":")) {
            pns = (nkn_interface_t *)nkn_malloc_type(sizeof(nkn_interface_t), 
						     mod_nm_interface);
            if (!pns) {
                perror("DEBUG: malloc failed");
                continue;
            }
            memset((char *)pns, 0, sizeof(nkn_interface_t));
            pns->if_type = NKN_IF_ALIAS;
            strncpy(pns->if_name, ifa->ifa_name, sizeof(ifa->ifa_name));
            add_pns_to_alias_list(pns);

        } else {
            for (i = 0; i < glob_tot_interface; i++) {
                if (!strcmp(ifa->ifa_name, interface[i].if_name)) {
                    pns = &interface[i];
                    goto set_if_details;
                }
            }

            if (i == glob_tot_interface) {
                pns = &interface[glob_tot_interface];
                memset((char *)pns, 0, sizeof(nkn_interface_t));
                glob_tot_interface++;
            }

            strncpy(pns->if_name, ifa->ifa_name, sizeof(ifa->ifa_name));
            intf_name = pns->if_name;
	    pns->if_bandwidth = 0;
	    if (!strcmp(pns->if_name, "lo")) {
		pns->if_type = NKN_IF_LO;
	    } else {
		pns->if_type = NKN_IF_PHYSICAL;
	    }
        }
        if_count++;

        assert(glob_tot_interface < MAX_NKN_INTERFACE);

set_if_details:

        /* save if address */
        if (ifa->ifa_addr && (AF_INET == ifa->ifa_addr->sa_family) && 
            !( pns->if_addrv4)) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
			    host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
	    	host[0] = '\0';
	    }
	    strcpy(pns->if_addrv4_numeric, host);
            pns->if_addrv4 = 
	    	((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;

            /* save if netmask */
            if ( ioctl(fd, SIOCGIFNETMASK, (char*)&ifr) == -1) {
                perror("DEBUG: [get_if_name] ioctl(SIOCGIFNETMASK)");
                return 3;
            }
            memcpy(&sa, &ifr.ifr_netmask, sizeof(struct sockaddr_in));
            pns->if_netmask=sa.sin_addr.s_addr;

            /* calculate subnet */
            pns->if_subnet = pns->if_addrv4 & pns->if_netmask;
        }
        memset(&if_addrv6, 0, sizeof (if_addrv6));
        memset(host, 0, sizeof (host));

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
		strcpy(pns->if_addrv6_numeric[pns->ifv6_ip_cnt].addr, host);
                memcpy(&(pns->if_addrv6[pns->ifv6_ip_cnt].if_addrv6), 
		       &if_addrv6, sizeof (struct in6_addr));
                pns->ifv6_ip_cnt++;
            }
        }

        /* get if mtu */
        if (ioctl(fd, SIOCGIFMTU, (char*)&ifr) == -1) {
            perror("Warning: [get_if_name] ioctl(SIOCGIFMTU)");
            return 4;
        }

        if (ifr.ifr_mtu == 0) { 
	    pns->if_mtu = 1500; 
	} else { 
	    pns->if_mtu = ifr.ifr_mtu; 
	}

        /* get hardware address */
        if (ioctl(fd, SIOCGIFHWADDR, (char*)&ifr) == -1) {
            perror("DEBUG: [get_if_name] ioctl(SIOCGIFHWADDR)");
            return 5;
        }
        memcpy(pns->if_mac, (unsigned char *)&ifr.ifr_hwaddr.sa_data[0], 6);
    }
    freeifaddrs(ifaddr);
    close(fd);

    return 0;
}

/*
 *******************************************************************************
 * HTTP Client handler functions
 *******************************************************************************
 */
static void close_conn(con_t *con)
{
    DBG_LOG(MSG, MOD_HTTP, "con=%p fd=%d", con, con->fd);

    NM_EVENT_TRACE(con->fd, FT_CLOSE_CONN);
    NM_close_socket(con->fd);

    if(!CHECK_CON_FLAG(con, CONF_RECV_DATA)) {
	glob_warn_socket_no_recv_data++;
    }

    if (!CHECK_CON_FLAG(con, CONF_SEND_DATA)) {
	glob_warn_socket_no_send_data++;
    }

    AO_fetch_and_sub1(&con->pns->tot_sessions);
    if (con->ip_family == AF_INET6) {
	AO_fetch_and_sub1(&glob_cur_open_http_socket_ipv6);
    } else {
	AO_fetch_and_sub1(&glob_cur_open_http_socket);
    }

    if (con->v_pns) {
    	AO_fetch_and_sub1(&con->v_pns->tot_sessions);

    	if (AO_load(&con->v_pns->if_refcnt) > 0) {
	    AO_fetch_and_sub1(&con->v_pns->if_refcnt);
    	}
    	if ((!AO_load(&con->v_pns->if_refcnt)) 
	    && AO_load(&con->v_pns->if_status) == 0) {
	    del_pns_from_alias_list(con->v_pns);
    	}
    	con->v_pns = NULL;
    }

    free(con->http.cb_buf);

    assert(con->magic == CON_MAGIC_USED);
    con->magic = CON_MAGIC_FREE;
    free(con);
}

static int http_handle_request(con_t *con)
{
    libnet_conn_handle_t hdl;
    libnet_conn_attr_t attr;
    int rv;

    MK_HANDLE(&hdl, con);

    attr.DestIP = con->http.remote_ipv4v6_numeric;
    attr.destip = (net_ip_addr_t *)&con->http.remote_ipv4v6;
    attr.DestPort = &con->http.remote_port;
    attr.ClientIP = con->src_ipv4v6_numeric;
    attr.clientip = (net_ip_addr_t *)&con->src_ipv4v6;
    attr.ClientPort = &con->src_port_hs;

    SET_CON_FLAG(con, CONF_BLOCK_RECV);
    rv = (*pnet_recv_data_event)(&hdl, con->http.cb_buf, 
			    	 con->http.cb_totlen, &attr);
    if (rv == 1) { // More data
    	UNSET_CON_FLAG(con, CONF_BLOCK_RECV);
	return 1;
    }
    else if (rv == 2) { // await EPOLLOUT event
    	NM_add_event_epollout(con->fd);
    	return 1;
    }
    return 1;
}

static int send_err_on_large_request(con_t *con)
{
    libnet_conn_handle_t hdl;
    libnet_conn_attr_t attr;
    int rv;

    MK_HANDLE(&hdl, con);

    attr.DestIP = con->http.remote_ipv4v6_numeric;
    attr.destip = (net_ip_addr_t *)&con->http.remote_ipv4v6;
    attr.DestPort = &con->http.remote_port;
    attr.ClientIP = con->src_ipv4v6_numeric;
    attr.clientip = (net_ip_addr_t *)&con->src_ipv4v6;
    attr.ClientPort = &con->src_port_hs;

    rv = (*pnet_recv_data_event_err)(&hdl, con->http.cb_buf, 
				     con->http.cb_totlen, &attr, 1);
    return 0;
}

static int nkn_check_http_cb_buf(http_cb_t *phttp)
{
    int rlen;
    char *p;
    int size;

    rlen = phttp->cb_max_buf_size - phttp->cb_totlen;
    if (rlen <= 10) {
	glob_enlarge_cb_size++;
	if (phttp->cb_max_buf_size < TYPICAL_HTTP_CB_BUF) {
	    assert(0);
	}
	size = phttp->cb_max_buf_size * 2;
	if (size > max_http_req_size) {
	    return rlen;
	}
	p = (char *)nkn_realloc_type(phttp->cb_buf, size + 1, mod_network_cb);
	if (p == NULL) {
	    return -1;
	}
	phttp->cb_max_buf_size = size;
	phttp->cb_buf = p;
	rlen = phttp->cb_max_buf_size - phttp->cb_totlen;
    }
    return rlen;
}

static int http_epollin(int sockfd, void *private_data)
{
    con_t * con = (con_t *)private_data;
    int rlen;
    int n;
    int err = 0;
    int ret = -1;

    NM_EVENT_TRACE(sockfd, FT_HTTP_EPOLLIN);

    /*
     * We do not support pipeline request and POST method for now
     */
    if (CHECK_CON_FLAG(con, CONF_BLOCK_RECV) || 
	CHECK_CON_FLAG(con, CONF_TASK_POSTED)) {
	/*
	 * BUG 5381: Don't read any data from socket for now.
	 * When this transaction is finished and it is keep-alive
	 * request, another event will trig to call http_epollin()
	 * when this socket is added back to epoll loop.
	 */
	goto close_socket;
    }

    if (CHECK_CON_FLAG(con, CONF_TUNNEL_PROGRESS)) {
	return TRUE;
    }

    rlen = nkn_check_http_cb_buf(&con->http);

    /* BZ 6558:
     * In some cases, rlen comes back as 0 because the
     * buffer exhausted or is packed exactly to its limit.
     * In such cases, we never respond to the client with
     * an error and attempt to read from the socket with
     * length 0. This will cause the socket to close,
     * without us responding with a 413 to the client.
     */
    if ((rlen <= 0)) {
	// if request entity too large.. we would have responded
	// with a 413
	(void) send_err_on_large_request(con);
	err = 1;
	goto close_socket;
    }

    n = recv(sockfd, &con->http.cb_buf[con->http.cb_totlen], rlen, 0);
    if(n <= 0) {
	err = 2;
	SET_CON_FLAG(con, CONF_RECV_ERROR);
	goto close_socket;
    }

    glob_tot_bytes_recv += n;
    AO_fetch_and_add(&glob_client_recv_tot_bytes, n);

    // valid data
    con->http.cb_totlen += n;

    if (n > max_http_req_size) {
	(void) send_err_on_large_request(con);
	err = 3;
	goto close_socket;
    }
#if 0
    else if(n == 0) {
	/*
	 * BUG TBD:
	 * Don't understand what root cause it is.
	 * errno returns ENOENT (2), which is un-documentated 
	 * in the man recv page.
	 * On google, someone suggests this errno could be because of
	 * the code is not compiled with -D_REENTRANT
	 *
	 * Workaround: do nothing and skip this epollin signal.
	 */
	return TRUE;
    }
#endif // 0

    SET_CON_FLAG(con, CONF_RECV_DATA);

    // Normal HTTP Transaction request

    ret = http_handle_request(con);

    return TRUE;

close_socket:

    /*
     * socket needs to be closed.
     */
    if(CHECK_CON_FLAG(con, CONF_TASK_POSTED)) {
	NM_del_event_epoll(sockfd);
    } else {
	close_conn(con);
    }
    return TRUE;
}

int http_try_to_sendout_data(con_t *con)
{
    libnet_conn_handle_t hdl;
    int rv;

    MK_HANDLE(&hdl, con);

    rv = (*pnet_send_data_event)(&hdl);
    if (rv == -1) {
    	NM_add_event_epollout(con->fd);
    }
    return TRUE;
}

static int http_epollout(int sockfd, void *private_data)
{
    UNUSED_ARGUMENT(sockfd);
    con_t *con = (con_t *)private_data;

    http_try_to_sendout_data(con);
    return TRUE;
}

static int http_epollerr(int sockfd, void *private_data)
{
    UNUSED_ARGUMENT(sockfd);
    con_t *con = (con_t *)private_data;

    close_conn(con);
    return TRUE;
}

static int http_epollhup(int sockfd, void *private_data)
{
    return http_epollerr(sockfd, private_data);
}

static int http_timer(int sockfd, void *private_data,
                      net_timer_type_t timer_type)
{
    UNUSED_ARGUMENT(sockfd);
    UNUSED_ARGUMENT(private_data);
    UNUSED_ARGUMENT(timer_type);
    return 0;
}

static int http_timeout(int sockfd, void *private_data, double timeout)
{
    UNUSED_ARGUMENT(sockfd);
    UNUSED_ARGUMENT(timeout);
    con_t *con;
    libnet_conn_handle_t hdl;
    int rv;

    con = (con_t *)private_data;
    MK_HANDLE(&hdl, con);

    rv = (*pnet_timeout_event)(&hdl);

    return TRUE;
}

/*
 *******************************************************************************
 * HTTP Server handler functions
 *******************************************************************************
 */
static void init_conn(int svrfd, int sockfd, nkn_interface_t *pns, 
		      nkn_interface_t *ppns, struct sockaddr_storage *addr, 
		      int thr_num)
{
    con_t *con;
    int j;
    struct sockaddr_in *addrptr;
    struct sockaddr_in6 *addrptrv6;
    char host[NI_MAXHOST];
    int s;
    
    if (addr->ss_family == AF_INET6) {
	addrptrv6 = (struct sockaddr_in6 *)addr;
    } else {
	addrptr = (struct sockaddr_in *)addr;
    }

    con = nkn_calloc_type(1, sizeof(con_t), mod_http_con_t);
    if (!con) {
	nkn_close_fd(sockfd, NETWORK_FD);
	return;
    }
    NM_EVENT_TRACE(sockfd, FT_INIT_CONN);

    /* PNS choosing logic:
       The interface could be ALIAS or non-alias. 
       Alias interfaces are reached only through a physical or bond interface.
       This means, alias (VIP) can be reached through any of the 
       physical interfaces. 
       Thus the physical/bond interface through which the alias is 
       reached should be accounted for bw, sessions, afr computations. 
    1. If the connection request received is for physical interface,
        pns = physical interface, parent pns - ppns = NULL
        Simply use 'pns' for computations.
    2. If the request received is for alias interface, then
        pns = alias interface, ppns = parent physcial interface
        Now, we should use ppns for the aforementioned computation.
        So, set pns = ppns and v_pns = pns in con.
        However, make sure FDs, IP addresses are all that belong to the 
	alias interface and not that of parent's
    */

    if (ppns) {
        con->pns = ppns;
        con->v_pns = pns;
        AO_fetch_and_add1(&pns->if_refcnt);
    } else {
        con->pns = pns;
        con->v_pns = NULL;
    }
		
    con->magic = CON_MAGIC_USED;
    con->fd = sockfd;
    con->src_ipv4v6.family = addr->ss_family;
    con->dest_ipv4v6.family = addr->ss_family;	
    con->http.remote_ipv4v6.family = addr->ss_family;

    if (addr->ss_family == AF_INET) {
        con->src_port = addrptr->sin_port;
        con->src_port_hs = ntohs(addrptr->sin_port);

	s = getnameinfo((struct sockaddr *)addrptr, sizeof(struct sockaddr_in),
			host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
	if (s != 0) {
	    host[0] = '\0';
	}
	strcpy(con->src_ipv4v6_numeric, host);

        IPv4(con->src_ipv4v6) = addrptr->sin_addr.s_addr;
        IPv4(con->dest_ipv4v6) = pns->if_addrv4;
        IPv4(con->http.remote_ipv4v6) = pns->if_addrv4;
	con->http.remote_ipv4v6_numeric = pns->if_addrv4_numeric;
    } else {
	con->src_port = addrptrv6->sin6_port;
	con->src_port_hs = ntohs(addrptrv6->sin6_port);

	s = getnameinfo((struct sockaddr *)addrptrv6, 
			sizeof(struct sockaddr_in6),
			host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
	if (s != 0) {
	    host[0] = '\0';
	}
	strcpy(con->src_ipv4v6_numeric, host);

	memcpy(&IPv6(con->src_ipv4v6), &addrptrv6->sin6_addr, 
	       sizeof(struct in6_addr));
    }
	
    for (j = 0; j < MAX_PORTLIST; j++) {
	if (addr->ss_family == AF_INET) {
	    if (pns->listenfd[j] == svrfd) {
		con->http.remote_port = pns->port[j];
		con->dest_port = htons(pns->port[j]);
		break;
	    }
	} else {
	    uint32_t ifv6_cnt = 0, found = 0;

	    for (ifv6_cnt = 0; ifv6_cnt < pns->ifv6_ip_cnt; ifv6_cnt++) {
		if (pns->if_addrv6[ifv6_cnt].listenfdv6[j] && 
		   (pns->if_addrv6[ifv6_cnt].listenfdv6[j] == svrfd)) {
		    con->http.remote_port = pns->if_addrv6[ifv6_cnt].port[j];
		    con->dest_port = htons(pns->if_addrv6[ifv6_cnt].port[j]);
		    found = 1;
		    break;
	    	}
	    }

	    if (found) {
		memcpy(&IPv6(con->dest_ipv4v6), 
		       &pns->if_addrv6[ifv6_cnt].if_addrv6, 
		       sizeof(struct in6_addr));
		memcpy(&IPv6(con->http.remote_ipv4v6), 
		       &pns->if_addrv6[ifv6_cnt].if_addrv6, 
		       sizeof(struct in6_addr));
		con->http.remote_ipv4v6_numeric = 
		    pns->if_addrv6_numeric[ifv6_cnt].addr;
		break;
	    }
	}
    }

    con->http.cb_max_buf_size = TYPICAL_HTTP_CB_BUF;
    con->http.cb_buf = (char *)
    	nkn_malloc_type(con->http.cb_max_buf_size * sizeof(char) + 1,
			mod_network_cb);

#if 0
    // Setup session bandwidth and ABR
    con->max_bandwidth = sess_bandwidth_limit;
    AO_fetch_and_add(&con->pns->max_allowed_bw, -con->max_bandwidth);
    con->min_afr = sess_assured_flow_rate;
    con->max_client_bw = 100 << 20;	// 100 MBytes/sec.  hard-coded by now.
    con->max_allowed_bw = AO_load(&con->pns->max_allowed_bw);
    con->max_send_size = 0;
    con->max_faststart_buf = 0;

    DBG_LOG(MSG, MOD_HTTP, "con=%p fd=%d", con, sockfd);

    if (con->min_afr) {
	/* 
	 * If assured_bit_rate for this session is enabled
	 * We set up the max session bandwidth for this session.
	 * The extra .2 is the insurance co-efficiency factor.
	 */
	con_set_afr(con, con->min_afr);
    }

    if (sess_bandwidth_limit && 
    	(con->max_client_bw > (uint64_t)sess_bandwidth_limit) ) {
	/*
	 * if max session bandwidth is configured,
	 * we pick up the min(sess_bandwidth_limit, max_client_bw)
	 * as max client bw.
	 */
	con->max_client_bw = sess_bandwidth_limit;
    }
#endif

#ifdef SOCKFD_TRACE
    if (debug_fd_trace && gnm[sockfd].p_trace == NULL) {
	gnm[sockfd].p_trace = 
	    nkn_calloc_type(1, sizeof(nm_trace_t), mod_nm_trace_t);
#ifdef DBG_EVENT_TRACE
	pthread_mutex_init(&gnm[sockfd].p_trace->mutex, NULL);
#endif
    }
#endif

    pthread_mutex_lock(&gnm[sockfd].mutex);
    NM_TRACE_LOCK(&gnm[sockfd], LT_SERVER);
    gnm[sockfd].accepted_thr_num = thr_num;
    if (addr->ss_family == AF_INET6) {
	//The flags will be overwritten in register_nm_socket
	NM_SET_FLAG(&gnm[sockfd], NMF_IS_IPV6);
    }
    if (register_NM_socket(sockfd,
		    (void *)con,
		    http_epollin,
		    http_epollout,
		    http_epollerr,
		    http_epollhup,
		    http_timer,
		    http_timeout,
		    http_idle_timeout,
		    USE_LICENSE_TRUE,
		    TRUE) == FALSE)
    {
	free(con->http.cb_buf);
	nkn_close_fd(sockfd, NETWORK_FD);
	free(con);
	NM_TRACE_UNLOCK(&gnm[sockfd], LT_SERVER);
	pthread_mutex_unlock(&gnm[sockfd].mutex);
	return;
    }

    /* 
     * We need to increase counter first before calling NM_add_event_epollin().
     * otherwise pns->tot_sessions could be zero if NM_add_event_epollin 
     * returns FALSE.
     */
    AO_fetch_and_add1(&con->pns->tot_sessions);
    if (con->v_pns) {
        AO_fetch_and_add1(&con->v_pns->tot_sessions);
        AO_fetch_and_add1(&glob_tot_vip_hit);
    }

    if (addr->ss_family == AF_INET6) {
	AO_fetch_and_add1(&glob_tot_http_sockets_ipv6);
	AO_fetch_and_add1(&glob_cur_open_http_socket_ipv6);
    } else {
	AO_fetch_and_add1(&glob_tot_http_sockets);
	AO_fetch_and_add1(&glob_cur_open_http_socket);
    }

    con->ip_family = addr->ss_family;
    if (NM_CHECK_FLAG(&gnm[sockfd], NMF_IN_USE)) {
	/*
	 * To fix BUG 1786, 429.
	 * Inside successful register_NM_socket() function call,
	 * register_NM_socket() puts this socket into TIMEOUT QUEUE.
	 * Then this thread is swapped out.
	 * Then timeout function called first which calls close_conn() 
	 * and free the con memory.
	 * Then swap thread back to here.
	 * If this is the case,  NM_CHECK_FLAG(pnm, NMF_IN_USE) 
	 * should not be set.
	 * Otherwise, it means timeout function has not been called.
	 *
	 * Why timeout is called first, I have no idea so far.
	 *
	 * -- Max
	 */
	if( NM_add_event_epollin(sockfd) == FALSE ) {
	    close_conn(con);
	    NM_TRACE_UNLOCK(&gnm[sockfd], LT_SERVER);
	    pthread_mutex_unlock(&gnm[sockfd].mutex);
	    return;
	}
    }

    NM_TRACE_UNLOCK(&gnm[sockfd], LT_SERVER);
    pthread_mutex_unlock(&gnm[sockfd].mutex);
}

static int httpsvr_epollin(int sockfd, void *private_data)
{
    int clifd;
    int cnt;
    struct sockaddr_in addr;
    socklen_t addrlen;
    nkn_interface_t *pns = (nkn_interface_t *)private_data;
    nkn_interface_t *ppns = NULL;
    int i32index = 0;
    unsigned if_len = IFNAMSIZ;
    char if_name[IFNAMSIZ] = {0};

    /* always returns TRUE for this case */

    /* BUG 9645: we are in the infinite loop, and watch-dog killed nvsd
     * Solution: set a limit 5000 for max one time accept().
     */
    for (cnt = 0; cnt < 5000; cnt++) { // Begin for loop

    addrlen = sizeof(struct sockaddr_in);
    clifd = accept(sockfd, (struct sockaddr *)&addr, &addrlen);
    if (clifd == -1) {
	if(cnt > 100) {
	    glob_accept_queue_100++;
	}
	return TRUE;
    }
    nkn_mark_fd(clifd, NETWORK_FD);

    if (NKN_IF_ALIAS == pns->if_type) {
    	if (getsockopt(clifd, SOL_IP, IP_INDEV, if_name, &if_len) < 0) {
	    DBG_LOG(MSG, MOD_HTTP, "getsockopt failed to get indev. Error: %d",
		    errno);
    	} else {
	    DBG_LOG(MSG, MOD_HTTP, "getsockopt[indev] if_name: %s, pns_if: %s", 
		    if_name, pns->if_name);
    	}

    	if ('\0' == if_name[0]) {
	    AO_fetch_and_add1(&glob_vip_parent_if_miss);
    	} else {
	    AO_fetch_and_add1(&glob_vip_parent_if_hit);
    	}

    	for (i32index = 0; i32index < MAX_NKN_INTERFACE; i32index++) {
	    if (!strcmp(interface[i32index].if_name, if_name)) {
	    	DBG_LOG(MSG, MOD_HTTP, 
		    	"getsockopt[indev] if_name: %s, pns_if: %s", 
		    	if_name, pns->if_name);
	    	break;
	    }
    	}
    	assert(i32index < MAX_NKN_INTERFACE);
    	ppns = &interface[i32index];
    }

    if (NKN_IF_ALIAS == pns->if_type) {
    	// max_allowed_sessions is calculated based on ABR
    	if (AO_load(&ppns->tot_sessions) > ppns->max_allowed_sessions) {
	    glob_overflow_socket_due_afr++;
	    nkn_close_fd(clifd, NETWORK_FD);
	    return TRUE;
    	}
    } else {
    	if (AO_load(&pns->tot_sessions) > pns->max_allowed_sessions) {
	    glob_overflow_socket_due_afr++;
	    nkn_close_fd(clifd, NETWORK_FD);
	    return TRUE;
    	}
    }

    // valid socket
    if(NM_set_socket_nonblock(clifd) == FALSE) {
	DBG_LOG(MSG, MOD_HTTP, 
		"Failed to set socket %d to be nonblocking socket.", clifd);
	nkn_close_fd(clifd, NETWORK_FD);
	return TRUE;
    }

    // Bind this socket into this accepted interface 
    if(bind_socket_with_interface) {
	NM_bind_socket_to_interface(clifd, pns->if_name);
    }

    init_conn(sockfd, clifd, pns, ppns, 
	      (struct sockaddr_storage*)&addr, gnm[sockfd].pthr->num);

    } // End for loop

    return TRUE;
}

static int httpsvr_epollout(int sockfd, void *private_data)
{
    UNUSED_ARGUMENT(private_data);

    DBG_LOG(SEVERE, MOD_HTTP, 
	    "epollout should never called. sockid=%d", sockfd);
    DBG_ERR(SEVERE, "epollout should never called. sockid=%d", sockfd);
    assert(0); // should never called
    return FALSE;
}

static int httpsvr_epollerr(int sockfd, void *private_data)
{
    UNUSED_ARGUMENT(private_data);

    DBG_LOG(SEVERE, MOD_HTTP, 
	    "epollerr should never called. sockid=%d", sockfd);
    DBG_ERR(SEVERE, "epollerr should never called. sockid=%d", sockfd);
    assert(0); // should never called
    return FALSE;
}

static int httpsvr_epollhup(int sockfd, void *private_data)
{
    UNUSED_ARGUMENT(private_data);

    DBG_LOG(SEVERE, MOD_HTTP, 
	    "epollhup should never called. sockid=%d", sockfd);
    DBG_ERR(SEVERE, "epollhup should never called. sockid=%d", sockfd);
    assert(0); // should never called
    return FALSE;
}

static int http_if4_init(nkn_interface_t *pns)
{
    struct sockaddr_in srv;
    int ret, val, j;
    int listenfd;
    network_mgr_t *pnm = NULL;

    if (!pns) {
        DBG_LOG(SEVERE, MOD_HTTP, "Invalid pns received for init");
        return -1;
    }

    AO_store(&pns->if_status, 1);

    if (!pns->if_addrv4) {
        goto set_if_params;
    }

    for (j = 0; j < MAX_PORTLIST; j++) {
        if (0 == nkn_http_serverport[j]) {
            continue;
        }

        if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            DBG_LOG(SEVERE, MOD_HTTP, 
	            "Failed to create IPv4 socket. errno = %d", errno);
            continue;
        }
        nkn_mark_fd(listenfd, NETWORK_FD);
        val = 1;
        ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
        if (-1 == ret) {
            DBG_LOG(SEVERE, MOD_HTTP, 
	    	    "Failed to set socket option. errno = %d", errno);
            nkn_close_fd(listenfd, NETWORK_FD);
            continue;
        }

#if 0
        ret = setsockopt(listenfd, SOL_IP, IP_TRANSPARENT, &val, sizeof(val));
        if (-1 == ret) {
            DBG_LOG(SEVERE, MOD_HTTP, 
	    	    "Failed to se socket option (IP_TRANSPARENT). errno = %d", 
		    errno);
            nkn_close_fd(listenfd, NETWORK_FD);
            continue;
        }
#endif

        memset(&srv, 0, sizeof(srv));
        srv.sin_family = AF_INET;
        srv.sin_addr.s_addr = pns->if_addrv4;
        srv.sin_port = htons(nkn_http_serverport[j]);

        if (bind(listenfd, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
            DBG_LOG(SEVERE, MOD_HTTP, 
		    "Failed to bind server socket to port %d. errno = %d", 
		    nkn_http_serverport[j], errno);
            nkn_close_fd(listenfd, NETWORK_FD);
            continue;
        }
        DBG_LOG(MSG, MOD_HTTP, 
		"[port_cnt: %d] bind to server socket succ. to port %d, "
		"listenfd: %d", j, ntohs(srv.sin_port), listenfd);

        // make listen socket non-blocking
        NM_set_socket_nonblock(listenfd);

        // bind this socket into this accepted interface
        NM_bind_socket_to_interface(listenfd, pns->if_name);

        listen(listenfd, 10000);
        pns->port[j] = nkn_http_serverport[j];
        pns->listenfd[j] = listenfd;

        if (register_NM_socket(listenfd, 
            pns,
            httpsvr_epollin,
            httpsvr_epollout,
            httpsvr_epollerr,
            httpsvr_epollhup,
            NULL,
            NULL,
            0,
            USE_LICENSE_FALSE,
            FALSE) == FALSE) {
            nkn_close_fd(listenfd, NETWORK_FD);
            continue;
        }

        NM_add_event_epollin(listenfd);
        pnm = &gnm[listenfd];
        gnm[listenfd].accepted_thr_num = gnm[listenfd].pthr->num;
        glob_tot_svr_sockets++;
        AO_store(&pns->if_listening, 1);
    }

set_if_params:

    if (NKN_IF_ALIAS != pns->if_type) {
        /* Setup max_allowed_sessions and max_allowed_bw */
        nkn_setup_interface_parameter(pns);
    } 

    return 0;
}

static int httpsvrv6_epollin(int sockfd, void *private_data)
{
    int clifd;
    int cnt;
    struct sockaddr_storage addr;
    socklen_t addrlen;
    nkn_interface_t * pns = (nkn_interface_t *)private_data;
    nkn_interface_t * ppns = NULL;
    int i32index = 0;
    unsigned if_len = IFNAMSIZ;
    char if_name[IFNAMSIZ] = {0};

    /* always returns TRUE for this case */

    for(cnt=0; ; cnt++) { // Begin for loop

        //printf("cnt=%d\n", cnt);
        addrlen=sizeof(struct sockaddr_in6);
        clifd = accept(sockfd, (struct sockaddr *)&addr, &addrlen);
        if (clifd == -1) {
            if(cnt>100) {
                glob_accept_queue_100++;
            }
            return TRUE;
        }
        nkn_mark_fd(clifd, NETWORK_FD);

        // Well. Alias is not applicable for IPv6.
        if (NKN_IF_ALIAS == pns->if_type) {
            if (getsockopt(clifd, SOL_IP, IP_INDEV, if_name, &if_len) < 0) {
                DBG_LOG(MSG, MOD_HTTP, 
			"getsockopt failed to get indev. Error: %d", errno);
            } else {
                DBG_LOG(MSG, MOD_HTTP, 
			"getsockopt[indev] if_name: %s, pns_if: %s", 
			if_name, pns->if_name);
            }

            if ('\0' == if_name[0]) {
                AO_fetch_and_add1(&glob_vip_parent_if_miss);
            } else {
                AO_fetch_and_add1(&glob_vip_parent_if_hit);
            }

            for(i32index = 0; i32index < MAX_NKN_INTERFACE; i32index++) {
                if (!strcmp(interface[i32index].if_name, if_name)) {
                    break;
                }
            }
            assert(i32index < MAX_NKN_INTERFACE);
            ppns = &interface[i32index];
        }

        // max_allowed_sessions is calculated based on ABR
        if (NKN_IF_ALIAS == pns->if_type) {
            if (AO_load(&ppns->tot_sessions) > ppns->max_allowed_sessions) {
                glob_overflow_socket_due_afr++;
                nkn_close_fd(clifd, NETWORK_FD);
                return TRUE;
            }
        }
        else {
            if (AO_load(&pns->tot_sessions) > pns->max_allowed_sessions) {
                glob_overflow_socket_due_afr++;
                nkn_close_fd(clifd, NETWORK_FD);
                return TRUE;
            }
        }

        // valid socket
        if(NM_set_socket_nonblock(clifd) == FALSE) {
            DBG_LOG(MSG, MOD_HTTP,
                    "Failed to set socket %d to be nonblocking socket.",
                    clifd);
            nkn_close_fd(clifd, NETWORK_FD);
            return TRUE;
        }

        // Bind this socket into this accepted interface
        if(bind_socket_with_interface) {
            NM_bind_socket_to_interface(clifd, pns->if_name);
        }
        addr.ss_family = AF_INET6;
        init_conn(sockfd, clifd, pns, ppns, &addr, gnm[sockfd].pthr->num);

    } // End for loop

    return TRUE;
}

static int httpsvrv6_epollout(int sockfd, void *private_data)
{
    UNUSED_ARGUMENT(private_data);

    DBG_LOG(SEVERE, MOD_HTTP, 
	    "epollout_v6 should never called. sockid=%d", sockfd);
    DBG_ERR(SEVERE, "epollout_v6 should never called. sockid=%d", sockfd);
    assert(0); // should never called
    return FALSE;
}

static int httpsvrv6_epollerr(int sockfd, void *private_data)
{
    UNUSED_ARGUMENT(private_data);

    DBG_LOG(SEVERE, MOD_HTTP, 
	    "epollerr_v6 should never called. sockid=%d", sockfd);
    DBG_ERR(SEVERE, "epollerr_v6 should never called. sockid=%d", sockfd);
    assert(0); // should never called
    return FALSE;
}

static int httpsvrv6_epollhup(int sockfd, void *private_data)
{
    UNUSED_ARGUMENT(private_data);

    DBG_LOG(SEVERE, MOD_HTTP, 
	    "epollhup_v6 should never called. sockid=%d", sockfd);
    DBG_ERR(SEVERE, "epollhup_v6 should never called. sockid=%d", sockfd);
    assert(0); // should never called
    return FALSE;
}

static int http_if6_init(nkn_interface_t *pns)
{
    struct sockaddr_in6 srv_v6;
    int j;
    uint32_t ipv6_cnt;
    int val, ret;
    int listenfdv6;
    network_mgr_t *pnm = NULL;

    if (!pns) {
        DBG_LOG(SEVERE, MOD_HTTP, "Invalid pns received");
        return -1;
    }

    for (ipv6_cnt = 0; ipv6_cnt < pns->ifv6_ip_cnt; ipv6_cnt++) {

        if (1 == AO_load(&pns->if_addrv6[ipv6_cnt].if_listening)) {
            continue;
        }
        for (j = 0; j < MAX_PORTLIST; j++) {
            if (0 == nkn_http_serverport[j]) {
                continue;
            }

            if ((listenfdv6 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
                DBG_LOG(SEVERE, MOD_HTTP, 
			"Failed to create IPv6 socket. errno = %d", errno);
                continue;
            }
            nkn_mark_fd(listenfdv6, NETWORK_FD);
            val = 1;
            ret = setsockopt(listenfdv6, SOL_SOCKET, SO_REUSEADDR, &val, 
	    		     sizeof(val));
            if (-1 == ret) {
                DBG_LOG(SEVERE, MOD_HTTP, 
			"Failed to set ipv6 socket options (SO_REUSEADDR). "
			"errno = %d", errno);
                nkn_close_fd(listenfdv6, NETWORK_FD);
                continue;
            }

#if 0
            val = 1;
            ret = setsockopt(listenfdv6, SOL_IP, IP_TRANSPARENT, &val, 
			     sizeof(val));
            if (-1 == ret) {
                DBG_LOG(SEVERE, MOD_HTTP, 
			"Failed to set ipv6 socket option (IP_TRANSPARENT). "
			"errno = %d", errno);
                nkn_close_fd(listenfdv6, NETWORK_FD);
                continue;
            }
#endif

            memset(&srv_v6, 0, sizeof(srv_v6));
            srv_v6.sin6_family = AF_INET6;
            memcpy(&srv_v6.sin6_addr, &pns->if_addrv6[ipv6_cnt].if_addrv6, 
		   sizeof(struct in6_addr));
            srv_v6.sin6_port = htons(nkn_http_serverport[j]);

            if (bind(listenfdv6, (struct sockaddr *)&srv_v6, 
		    sizeof(srv_v6)) < 0) {
                DBG_LOG(SEVERE, MOD_HTTP, 
			"Failed to bind server ipv6 socket to port %d. "
			"errno = %d", nkn_http_serverport[j], errno);
                nkn_close_fd(listenfdv6, NETWORK_FD);
                continue;
            }
            DBG_LOG(MSG, MOD_HTTP, 
	    	    "[ifcnt: %d, port_cnt: %d] bind to server socket succ. "
		    "to port %d. listenfdv6: %d", 
		    ipv6_cnt, j, ntohs(srv_v6.sin6_port), listenfdv6);

            // Make socket non-blocking
            NM_set_socket_nonblock(listenfdv6);

            NM_bind_socket_to_interface(listenfdv6, pns->if_name);

            listen(listenfdv6, 10000);
            pns->if_addrv6[ipv6_cnt].port[j] = nkn_http_serverport[j];
            pns->if_addrv6[ipv6_cnt].listenfdv6[j] = listenfdv6;
            // It is enuf to set it once for one port.
            // But it is fine to do it repeatedly for all configured 
	    // ports during startup
            AO_store(&pns->if_addrv6[ipv6_cnt].if_status, 1);
            AO_store(&pns->if_addrv6[ipv6_cnt].if_listening, 1);

            if (NKN_IF_ALIAS != pns->if_type) {
                // Setup max_allowed_sessions and max_allowed_bw
                nkn_setup_interface_parameter(pns);
            }
            pnm = &gnm[listenfdv6];
            NM_SET_FLAG(pnm, NMF_IS_IPV6);

            if (register_NM_socket(listenfdv6,
                pns,
                httpsvrv6_epollin,
                httpsvrv6_epollout,
                httpsvrv6_epollerr,
                httpsvrv6_epollhup,
                NULL,
                NULL,
                0,
                USE_LICENSE_FALSE,
                FALSE) == FALSE) {
                nkn_close_fd(listenfdv6, NETWORK_FD);
                continue;
            }
            NM_add_event_epollin(listenfdv6);
            gnm[listenfdv6].accepted_thr_num = gnm[listenfdv6].pthr->num;
            glob_tot_svr_sockets_ipv6++;
        }
    }

    return 0;
}

static int HTTPserver_init(void)
{
    int i, j;
    unsigned int ipv6_cnt = 0;
    int listen_all = 1;
    nkn_interface_t *pns = NULL;
    char ipv6host[NI_MAXHOST];

    gethostname(myhostname, sizeof(myhostname)-1);
    myhostname_len = strlen(myhostname);

    /*
     * Check if any interface is enabled else listen on all interface
     */
    listen_all = !http_listen_intfcnt;
    for (i = 0; i < glob_tot_interface; i++) {
    	if (listen_all) {
	    interface[i].enabled = 1;
	    continue;
    	}
    	for (j = 0; j < http_listen_intfcnt; j++) {
	    if (!strcmp(interface[i].if_name, http_listen_intfname[j])) {
	    	interface[i].enabled = 1;
	    	break;
	    }
    	}
    }

    for (i = 0; i < glob_tot_interface; i++) {
	if ((listen_all == 0) &&
	    (interface[i].enabled != 1) &&
	    (strcmp(interface[i].if_name, "lo"))) {
	    continue;
	}
    	http_if4_init(&interface[i]);

	if (nkn_http_ipv6_enable == 0) {
	    continue;
	}

	for (ipv6_cnt = 0; ipv6_cnt < interface[i].ifv6_ip_cnt; ipv6_cnt++) {
	    inet_ntop(AF_INET6, &interface[i].if_addrv6[ipv6_cnt].if_addrv6, 
		      ipv6host, NI_MAXHOST);
    	}
    	http_if6_init(&interface[i]);
    } 

    for (pns = if_alias_list; pns; pns = pns->next) {
    	if (listen_all) {
	    pns->enabled = 1;
	    continue;
    	}
    	for (i = 0; i < http_listen_intfcnt; i++) {
	    if (!strcmp(pns->if_name, http_listen_intfname[i])) {
	    	pns->enabled = 1;
	    	break;
	    }
    	}
    }

    for (pns = if_alias_list; pns; pns = pns->next) {
    	if (!listen_all && !pns->enabled) {
	    continue;
    	}
    	http_if4_init(pns);
    }

    return 0;
}

static int reset_http_cb(con_t *con)
{
    con->con_flag = 0;
    con->http.cb_totlen = 0;

    return 0;
}

/*
 *******************************************************************************
 * libnet API functions
 *******************************************************************************
 */
int libnet_init(const libnet_config_t *cfg, int *err, int *suberr,
		pthread_t *pnet_parent_thread_id)
{
#define RETURN(_errcode, _suberrcode) \
	if ((_errcode) == 0) { \
	    *err = 0; \
	    *suberr = 0; \
	    return 0; \
	} else { \
	    *err = (_errcode); \
	    *suberr = (_suberrcode); \
	    return 1; \
	}

    int i_am_root;
    struct rlimit rlim;

    if (!cfg) {
	RETURN(1000, 0);
    }

    if (cfg->interface_version != LIBNET_INTF_VERSION) {
	RETURN(1001, 0);
    }

    if (cfg->log_level) {
	libnet_log_level = cfg->log_level;
    } else {
	libnet_log_level = &libnet_def_log_level;
    }

    if (cfg->libnet_dbg_log) {
    	pnet_dbg_log = cfg->libnet_dbg_log;
    } else {
    	pnet_dbg_log = libnet_dbg_log;
    }

    if (cfg->libnet_malloc_type) {
    	pnet_malloc_type =
	    (void *(*)(size_t, nkn_obj_type_t))cfg->libnet_malloc_type;
    } else {
    	pnet_malloc_type = libnet_malloc_type;
    }

    if (cfg->libnet_realloc_type) {
    	pnet_realloc_type =
	    (void *(*)(void *, size_t, nkn_obj_type_t))cfg->libnet_realloc_type;
    } else {
    	pnet_realloc_type = libnet_realloc_type;
    }

    if (cfg->libnet_calloc_type) {
    	pnet_calloc_type =
	    (void *(*)(size_t, size_t, nkn_obj_type_t))cfg->libnet_calloc_type;
    } else {
    	pnet_calloc_type = libnet_calloc_type;
    }

    if (cfg->libnet_free) {
    	pnet_free = cfg->libnet_free;
    } else {
    	pnet_free = libnet_free;
    }

    if (cfg->libnet_recv_data_event) {
    	pnet_recv_data_event = cfg->libnet_recv_data_event;
    } else {
    	pnet_recv_data_event = libnet_recv_data_event;
    }

    if (cfg->libnet_recv_data_event_err) {
    	pnet_recv_data_event_err = cfg->libnet_recv_data_event_err;
    } else {
    	pnet_recv_data_event_err = libnet_recv_data_event_err;
    }

    if (cfg->libnet_send_data_event) {
    	pnet_send_data_event = cfg->libnet_send_data_event;
    } else {
    	pnet_send_data_event = libnet_send_data_event;
    }

    if (cfg->libnet_timeout_event) {
    	pnet_timeout_event = cfg->libnet_timeout_event;
    } else {
    	pnet_timeout_event = libnet_timeout_event;
    }

    memset(nkn_http_serverport, 0, sizeof(nkn_http_serverport));
    memcpy(nkn_http_serverport, cfg->libnet_listen_ports, 
	   MIN(LIBNET_MAX_LISTEN_PORTS, MAX_PORTLIST) * sizeof(int));

    pthread_attr_t attr;
    int stacksize = 128 * 1024;
    int rv;

    i_am_root = (getuid() == 0);
    if (i_am_root) {
	if (!(cfg->options & LIBNET_OPT_VALGRIND)) {
	    rlim.rlim_cur = MAX_GNM;
	    rlim.rlim_max = MAX_GNM;
	    if (setrlimit(RLIMIT_NOFILE, &rlim)) {
	    	RETURN(1, 0);
	    }
	} else {
	    if (getrlimit(RLIMIT_NOFILE, &rlim)) {
	    	RETURN(2, 0);
	    }
	}
    } else {
        if (getrlimit(RLIMIT_NOFILE, &rlim)) {
	    RETURN(3, 0);
	}
    }
    if ((int)rlim.rlim_max < nkn_max_client) {
	nkn_max_client = rlim.rlim_cur;
    }

    rv = interface_init();
    if (rv) {
    	RETURN(100, rv);
    }

    NM_init();

    rv = pthread_attr_init(&attr);
    if (rv) {
    	RETURN(200, rv);
    }

    rv = pthread_attr_setstacksize(&attr, stacksize);
    if (rv) {
    	RETURN(300, rv);
    }

    // Create one second timer thread
    rv = pthread_create(&timer_1sec_thread_id, 0, timer_1sec, 0);
    if (rv) {
    	RETURN(400, rv);
    }

    // Create parent network thread
    rv = pthread_create(&net_parent_thread_id, &attr, net_parent_func, 0);
    if (rv) {
    	RETURN(500, rv);
    }

    // Setup HTTP server listeners
    rv = HTTPserver_init();
    if (rv) {
    	RETURN(600, rv);
    }

    if (pnet_parent_thread_id) {
    	*pnet_parent_thread_id = net_parent_thread_id;
    }
    RETURN(0, 0);
}

int libnet_send_data(libnet_conn_handle_t *handle, char *data, int datalen)
{
    con_t *con;
    int32_t fd;
    uint32_t incarn;

    int ret;

    HANDLE_TO_DATA(handle, con, fd, incarn);
    if (!VALID_HANDLE(con, fd, incarn)) {
    	return -3; // Invalid handle
    }

    ret = send(fd, data, datalen, 0);
    if (ret == -1) {
    	if (errno == EAGAIN) {
	    return -1;
	} else {
	    return -2;
	}
    }
    SET_CON_FLAG(con, CONF_SEND_DATA);

    return ret;
}

int libnet_close_conn(libnet_conn_handle_t *handle, int keepalive_close)
{
    con_t *con;
    int32_t fd;
    uint32_t incarn;

    HANDLE_TO_DATA(handle, con, fd, incarn);
    if (VALID_HANDLE(con, fd, incarn)) {
    	if (keepalive_close) {
	    UNSET_CON_FLAG(con, CONF_BLOCK_RECV);
	    reset_http_cb(con);
	} else {
	    close_conn(con);
	}
    } else {
    	return 1; // Invalid handle
    }
    return 0;
}

/*
 * End of net.c
 */

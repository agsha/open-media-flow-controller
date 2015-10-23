#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "nkn_osl.h"
#include "nkn_elf.h"
#include "interface.h"
#include "nkn_debug.h"

int netverbose=0;
const char * cfg_ifnames="eth2 eth3 eth4 eth5";
//const int useIOVEC=0; // This value should be alwys zero
static pthread_t readthr[MAX_NKN_INTERFACE];

extern void * p_tcp_mmap;
extern size_t l_tcp_mmap;

extern void init_nkn_dev(void);
extern int write_nkn_dev(char * phead, int lhead, char * pbuf, int lbuf);
//extern int write_nkn_dev_sg(void * head, int headlen, void * pbuf, int lbuf);
extern void * read_nkn_dev(void * arg);
extern void nkndev_init(void);
extern void end_nkn_dev(void);
extern int nkn_init_mmap(void);
extern void nkn_cleanup_mmap(void);


/*
 * This is Linux User Space TCP/IP stack shared library setup module
 * It includes four parts:
 * 1. Initialize the TCP/IP stacks.
 * 2. Provide TCP/IP stack shim layer. TCP/IP stack calls some Linux functions.
 * 3. Setup TCP/IP stack API interface. So Linux Applicatin can call exported functions 
 *    from TCP/IP stack.
 * 4. Unload Linux User Space TCP/IP stack shared library.
 */
static void nkn_setup_linux_shim_layer(void);
static int nkn_setup_so_api(void);
static struct nkn_osl_funcs_t funcs;
static void * tcp_stack_handle=NULL;

/*
 * ============================== 1 =================================
 * Setup shim layer
 * ============================== 1 =================================
 */
extern void print_trace(void);
static struct nkn_osl_funcs_t funcs = {
        malloc,
        free,
	random,
	exit,
	usleep,
        pthread_mutex_lock,
        pthread_mutex_trylock,
        pthread_mutex_unlock,
	print_trace,
	gettimeofday,
	write_nkn_dev
};

static void nkn_setup_linux_shim_layer(void) {
        char *error;
	void (* so_init_funcs)(struct nkn_osl_funcs_t * posl, void * pmem, long lmem) = NULL;

	DBG_LOG(MSG, MOD_TCP, "nkn_setup_linux_shim_layer called%s.", "");
	so_init_funcs = dlsym(tcp_stack_handle, "nkn_init_funcs");
        if ((error = dlerror()) != NULL)  {
		DBG_LOG(SEVERE, MOD_TCP, 
			"failed to find symbol nkn_init_funcs errno=%d", errno);
		DBG_ERR(SEVERE,  
			"failed to find symbol nkn_init_funcs errno=%d", errno);
		assert(0);
        }
	so_init_funcs(&funcs, p_tcp_mmap, l_tcp_mmap);
}


/*
 * ============================== 3 =================================
 * Setup TCP/IP stack API interface. 
 * ============================== 3 =================================
 */
void (* so_100ms_timer)(void * tv) = NULL;
void (* so_1s_timer)(void * tv) = NULL;
int (* so_add_ifip)(unsigned long ifip, unsigned long netmask, unsigned long subnet) = NULL;
int (* so_ip_input)(char * p, int len) = NULL;
void (* so_debug_verbose)(int verbose) = NULL;
void * (* so_netmalloc)(int size) = NULL;
void (* so_netfree)(void ** p) = NULL;
int (* so_geterrno)(void) = NULL;
int (* so_shutdown)(int s, int how) = NULL;

struct replace_api_from_sys_to_nkn {
	const char * sys_fname;
	void * psysapi;
	const char * nkn_fname;
	void * pnknapi;
};
static struct replace_api_from_sys_to_nkn func_list[] = 
{
  {"accept", (void *)accept, "nkn_accept", NULL},
  {"bind", (void *)bind, "nkn_bind", NULL},
  {"connect", (void *)connect, "nkn_connect", NULL},
  {"getpeername", (void *)getpeername, "nkn_getpeername", NULL},
  {"getsockname", (void *)getsockname, "nkn_getsockname", NULL},
  {"getsockopt", (void *)getsockopt, "nkn_getsockopt", NULL},
  {"listen", (void *)listen, "nkn_listen", NULL},
  {"recv", (void *)recv, "nkn_recv", NULL},
//  {"read", (void *)read, "nkn_read", NULL},
  {"recvfrom", (void *)recvfrom, "nkn_recvfrom", NULL},
//  {"recvmsg", (void *)recvmsg, "nkn_recvmsg", NULL},
  {"send", (void *)send, "nkn_send", NULL},
//  {"write", (void *)write, "nkn_write", NULL},
//  {"sendmsg", (void *)sendmsg, "nkn_sendmsg", NULL},
  {"sendto", (void *)sendto, "nkn_sendto", NULL},
  {"setsockopt", (void *)setsockopt, "nkn_setsockopt", NULL},
  {"shutdown", (void *)shutdown, "nkn_shutdown", NULL},
//  {"close", (void *)close, "nkn_close", NULL},
  {"socket", (void *)socket, "nkn_socket", NULL},
//  {"sockatmark", (void *)sockatmark, "nkn_sockatmark", NULL},
 // {"socketpair", (void *)socketpair, "nkn_socketpair", NULL},
//  {"ioctl", (void *)ioctl, "nkn_ioctl", NULL},
  {"epoll_create", (void *)epoll_create, "nkn_epoll_create", NULL},
  {"epoll_ctl", (void *)epoll_ctl, "nkn_epoll_ctl", NULL},
  {"epoll_wait", (void *)epoll_wait, "nkn_epoll_wait", NULL},
//  {"epoll_close", (void *)epoll_close, "nkn_epoll_close", NULL},
//  {"readv", (void *)readv, "nkn_readv", NULL},
//  {"writev", (void *)writev, "nkn_writev", NULL},
//  {"getprotobyname", (void *)getprotobyname, "nkn_getprotobyname", NULL},
  {NULL, NULL, NULL, NULL}
};

static int nkn_setup_so_api(void) {
        char *error;
	void * pfunc;
	struct replace_api_from_sys_to_nkn * p;

	// get each functin api
        so_100ms_timer = dlsym(tcp_stack_handle, "nkn_100ms_timer");
        if ((error = dlerror()) != NULL)  goto errdone;

        so_1s_timer = dlsym(tcp_stack_handle, "nkn_1s_timer");
        if ((error = dlerror()) != NULL)  goto errdone;

        so_add_ifip = dlsym(tcp_stack_handle, "nkn_add_ifip");
        if ((error = dlerror()) != NULL)  goto errdone;

        so_ip_input = dlsym(tcp_stack_handle, "nkn_ip_input");
        if ((error = dlerror()) != NULL)  goto errdone;

        so_debug_verbose = dlsym(tcp_stack_handle, "nkn_debug_verbose");
        if ((error = dlerror()) != NULL)  goto errdone;

        so_netmalloc = dlsym(tcp_stack_handle, "nkn_netmalloc");
        if ((error = dlerror()) != NULL)  goto errdone;

        so_netfree = dlsym(tcp_stack_handle, "nkn_netfree");
        if ((error = dlerror()) != NULL)  goto errdone;

        so_geterrno = dlsym(tcp_stack_handle, "nkn_geterrno");
        if ((error = dlerror()) != NULL)  goto errdone;

        so_shutdown = dlsym(tcp_stack_handle, "nkn_shutdown");
        if ((error = dlerror()) != NULL)  goto errdone;


#if 0 
	/*
	 *
	 */
        f_accept = dlsym(tcp_stack_handle, "nkn_accept");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_bind = dlsym(tcp_stack_handle, "nkn_bind");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_connect = dlsym(tcp_stack_handle, "nkn_connect");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_getpeername = dlsym(tcp_stack_handle, "nkn_getpeername");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_getsockopt = dlsym(tcp_stack_handle, "nkn_getsockopt");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_listen = dlsym(tcp_stack_handle, "nkn_listen");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_recv = dlsym(tcp_stack_handle, "nkn_recv");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_recvfrom = dlsym(tcp_stack_handle, "nkn_recvfrom");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_recvmsg = dlsym(tcp_stack_handle, "nkn_recvmsg");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_send = dlsym(tcp_stack_handle, "nkn_send");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_sendmsg = dlsym(tcp_stack_handle, "nkn_sendmsg");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_sendto = dlsym(tcp_stack_handle, "nkn_sendto");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_setsockopt = dlsym(tcp_stack_handle, "nkn_setsockopt");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_shutdown = dlsym(tcp_stack_handle, "nkn_shutdown");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_socket = dlsym(tcp_stack_handle, "nkn_socket");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_sockatmark = dlsym(tcp_stack_handle, "nkn_sockatmark");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_socketpair = dlsym(tcp_stack_handle, "nkn_socketpair");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_epoll_create = dlsym(tcp_stack_handle, "nkn_epoll_create");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_epoll_ctl = dlsym(tcp_stack_handle, "nkn_epoll_ctl");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_epoll_wait = dlsym(tcp_stack_handle, "nkn_epoll_wait");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_readv = dlsym(tcp_stack_handle, "nkn_readv");
        if ((error = dlerror()) != NULL)  goto errdone;

        f_writev = dlsym(tcp_stack_handle, "nkn_writev");
        if ((error = dlerror()) != NULL)  goto errdone;

#endif // 0
	for(p=&func_list[0]; p->sys_fname != NULL; p++) {
		pfunc = dlsym(tcp_stack_handle, p->nkn_fname);
		if ((error = dlerror()) != NULL)  {
			DBG_LOG(MSG, MOD_TCP, "dlsym: cannot find function %s\n", p->nkn_fname);
			goto errdone;
		}
		p->pnknapi = pfunc;
		p->psysapi = nkn_replace_func(p->sys_fname, pfunc);
	}

	return 0;
errdone:
	fputs(error, stderr);
	exit(1);
}

/*
 * ============================== 4 =================================
 * Unload Linux User Space TCP/IP stack shared library.
 * ============================== 4 =================================
 */

void nkn_load_netstack(void);
void nkn_load_netstack(void)
{
	nkn_init_mmap();

	/*
	 * using NKN device driver
	 * At this case, we need two sockets.
	 * one is for reading packet from NKN's driver
	 * The other one is for sending packet out.
	 * Whether it works, it needs testing.
	 */

	init_nkn_dev();

	/*
	 * Load shared library
	 */
        tcp_stack_handle = dlopen ("/lib/nkn/libstack.so.1.0.1", RTLD_NOW|RTLD_GLOBAL/*RTLD_LAZY*/);
        if (!tcp_stack_handle) {
		DBG_LOG(SEVERE, MOD_TCP, 
			"Failed to open shared library /lib/nkn/libstack.so.1.0.1");
		DBG_ERR(SEVERE, 
			"Failed to open shared library /lib/nkn/libstack.so.1.0.1");
		assert(0);
		assert(0);
        }

	/*
	 * setup api interfaces 
	 */
	nkn_setup_linux_shim_layer();
	nkn_setup_so_api();

	// Setup debug information
	so_debug_verbose(netverbose);

	DBG_LOG(SEVERE, MOD_TCP, "Linux User Space TCP stack is ready%s.", "");
	DBG_ERR(SEVERE, "Linux User Space TCP stack is ready%s.", "");
}

void nkn_start_driver(void);
void nkn_start_driver(void)
{
	long i;
	for(i=0; i<glob_tot_interface; i++) {
		if(strstr(cfg_ifnames, interface[i].if_name) != NULL) {
			/* we do not need to listen on this port */
			DBG_LOG(MSG, MOD_TCP, 
				"Thread %ld created for reading packet from interface %s", 
				i, interface[i].if_name);
			if(pthread_create(&readthr[i], NULL, read_nkn_dev, (void *)i) != 0) {
				DBG_LOG(SEVERE, MOD_TCP, "Failed to create threads%s.", "");
				DBG_ERR(SEVERE, "Failed to create threads%s.", "");
				assert(0);
			}
		}
	}

}

void nkn_unload_netstack(void);
void nkn_unload_netstack(void)
{
	end_nkn_dev();
	if(tcp_stack_handle) {
        	dlclose(tcp_stack_handle);
		tcp_stack_handle=NULL;
	}
}


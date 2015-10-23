#ifndef __PXY_NETWORK__H
#define __PXY_NETWORK__H

#include <sys/queue.h>
#include <sys/socket.h>
#include <signal.h>
#include "md_client.h"
#include "pxy_defs.h"
#include "pxy_server.h"
#include "nkn_lockstat.h"
#include "libpxy_common.h"

#define MAX_EPOLL_THREADS         16

#define LB_ROUNDROBIN	          0
#define LB_STICK	          1	/* 1 network thread bound to 1 network port */

#define USE_LICENSE_FALSE	  0
#define USE_LICENSE_TRUE	  1
#define USE_LICENSE_ALWAYS_TRUE	  2

#define MAX_TOQ_SLOT 	          300			// Timeout Queue slot

//extern int NM_tot_threads;



/* 
 * socket is registered by each application module.
 * register_NM_socket() inside handles to acquire/release mutex.
 * multiple application modules can call in the same time.
 */
int pxy_NM_register_socket(int fd,
        uint32_t pd_type,    // Type of private_data
        void * private_data,
        NM_func f_epollin,
        NM_func f_epollout,
        NM_func f_epollerr,
        NM_func f_epollhup,
        NM_func_timeout f_timeout,
        int timeout);	// in seconds

/* 
 * socket is unregistered by each application module.
 * pxy_NM_unregister_socket()/pxy_NM_close_socket() inside handle to acquire/release mutex.
 * multiple application modules can call in the same time.
 * The difference between these two functions are close socket only.
 * pxy_NM_close_socket() closes the socket.
 */
void pxy_NM_close_socket(int fd);
int  pxy_NM_unregister_socket(int fd);

/*
 * Before calling any following functions, caller should get the gnm.mutex locker first
 */
void pxy_NM_change_socket_timeout_time(network_mgr_t * pnm, 
                                        int next_timeout_slot); // one slot = 5seconds
int pxy_NM_set_socket_nonblock(int fd);
int pxy_NM_bind_socket_to_interface(int fd, char * p_interface);
int pxy_NM_add_event_epollin(int fd);
int pxy_NM_add_event_epollout(int fd);
int pxy_NM_del_event_epoll(int fd);
int pxy_NM_del_event_epoll_inout(int fd);
void pxy_NM_set_socket_active(network_mgr_t * pnm);
void * pxy_NM_main_loop( void * arg ) ;
/*
 * Mutex is managed within these functions.
 * Caller has no need to acquire the mutex.
 */
void pxy_NM_init(void);
void pxy_NM_timer(void);
void pxy_NM_main(void);

extern struct nm_threads g_NM_thrs[MAX_EPOLL_THREADS];

/*
 * All the timer related APIs
 */
void pxy_server_timer_init(void);

/*
 * All http proxyd related APIs.
 */
void pxy_httpsvr_init(void) ;

/*
 * Proxy config related APIs
 */
int      read_pxy_cfg(char * configfile);
uint64_t convert_hexstr_2_int64(char * s);
int      check_pxy_cfg(void);
int pxy_http_cfg_query(void);

int pxy_http_module_cfg_handle_change (bn_binding_array *old_bindings,
                                bn_binding_array *new_bindings);
int pxy_http_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
                        bn_binding *binding, void *data);
int pxy_http_cfg_server_port(const bn_binding_array *arr, uint32 idx,
                        bn_binding *binding, void *data);
int pxy_http_cfg_server_interface(const bn_binding_array *arr, uint32 idx,
                        bn_binding *binding, void *data);

extern void config_and_run_counter_update(void );
extern void cfg_params_init(void);
extern void pxy_mgmt_init(void);
extern int pxy_mgmt_initiate_exit(void);

extern uint16_t glob_tot_svr_sockets;

extern int nkn_choose_cpu(int cpu);
extern void nkn_setup_interface_parameter(int i);

//extern int nkn_http_serverport[MAX_PORTLIST] ;
extern void pxy_server_timer_1sec(void) ;

#endif // __PXY_NETWORK__H


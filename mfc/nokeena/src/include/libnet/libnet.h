/*
 * libnet.h -- API definitions
 */

#ifndef _LIBNET_H
#define _LIBNET_H
#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>

#define LIBNET_MAX_LISTEN_PORTS 64

typedef struct net_ip_addr {
    uint8_t    family;
    union {
        struct  in_addr v4;
        struct  in6_addr v6;
    } addr;
} net_ip_addr_t;

typedef struct libnet_conn_attr {
    const char *DestIP;
    net_ip_addr_t *destip;
    const uint16_t *DestPort;
    const char *ClientIP;
    net_ip_addr_t *clientip;
    const uint16_t *ClientPort;
} libnet_conn_attr_t;

typedef struct libnet_conn_handle {
    uint64_t opaque[2];
} libnet_conn_handle_t;

#define LIBNET_INTF_VERSION 1

#define LIBNET_OPT_VALGRIND (1 << 0)

typedef struct libnet_config {
    int interface_version; // Set to LIBNET_INTF_VERSION
    uint64_t options; // See LIBNET_OPT_ defines
    /*
     * Logging interface
     */
    int *log_level;
    int (*libnet_dbg_log)(int, long, const char *, ...);

    /*
     * Memory allocation interfaces
     */
    void *(*libnet_malloc_type)(size_t n, int type);
    void *(*libnet_realloc_type)(void *ptr, size_t size, int type);
    void *(*libnet_calloc_type)(size_t n, size_t size, int type);
    void (*libnet_free)(void *ptr);

    /*
     * Data received callout (libnet => User).
     *  Input:
     *    handle - Context handle, ptr only valid within scope of callout
     *    data, datalen - Received data
     *    attr - Connection attributes, ptr only valid within scope of callout
     *
     *  Return:
     *    0 => No action
     *    1 => Callback with more data
     *    2 => Post send data event (i.e. write failed with EAGAIN)
     */
    int (*libnet_recv_data_event)(libnet_conn_handle_t *handle, 
				  char *data, int datalen, 
				  const libnet_conn_attr_t *attr);

    /*
     * Data received error callout (libnet => User).
     *  Input:
     *    handle - Context handle, ptr only valid within scope of callout
     *    data, datalen - Received data
     *    attr - Connection attributes, ptr only valid within scope of callout
     *    reason - Specific error
     *      0 => Unknown 
     *      1 => Request to large
     *
     *  Return:
     *    0 => No action
     *    1 => Error
     */
    int (*libnet_recv_data_event_err)(libnet_conn_handle_t *handle, 
				      char *data, int datalen, 
				      const libnet_conn_attr_t *attr, 
				      int reason);

    /*
     * Data send event callout (libnet => User).
     *  Input:
     *    handle - Context handle, ptr only valid within scope of callout
     *  Return:
     *   -1 => Post data send event (i.e. write failed with EAGAIN)
     *    0 => Success
     *    1 => Error
     */
    int (*libnet_send_data_event)(libnet_conn_handle_t *handle);

    /*
     * Connection timeout callout (libnet => User).
     *  Input:
     *    handle - Context handle, ptr only valid within scope of callout
     *  Return:
     *    0 => Success
     *    1 => Error
     */
    int (*libnet_timeout_event)(libnet_conn_handle_t *handle);

    /* 
     * Listen ports (zero entries are ignored)
     */
    int libnet_listen_ports[LIBNET_MAX_LISTEN_PORTS];

    char pad[128];
} libnet_config_t;

/*
 * Subsystem initialization
 *  Return
 *   ==0 Success
 *   !=0 Error; err/suberr denote the specific error
 */
int libnet_init(const libnet_config_t *cfg, int *err, int *suberr, 
		pthread_t *net_parent_thread_id);

/*
 * Send data on connection
 *  Input:
 *    handle - Context handle
 *    data, datalen - Data to send
 *  Return:
 *   >=0 => Success, bytes sent
 *    -1 => EAGAIN, retry by scheduling send data event
 *    -2 => Error
 */
int libnet_send_data(libnet_conn_handle_t *handle, char *data, int datalen);

/*
 * Close connection associated with handle
 *  Input:
 *    handle - Context handle
 *    keepalive_close - Consider HTTP keep alive on close
 *  Return:
 *     0 => Success
 *     1 => Invalid handle
 */
int libnet_close_conn(libnet_conn_handle_t *handle, int consider_keepalive);

#endif /* _LIBNET_H */
/*
 * End of libnet.h
 */

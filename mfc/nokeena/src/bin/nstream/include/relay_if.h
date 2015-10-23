/* File : relay_if.h Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _RELAY_IF_H_
/** 
 * @file relay_if.h
 * @brief Relay Interface
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-13
 */

#define _RELAY_IF_H_
#include <sys/types.h>  
#include <sys/socket.h>

#define RELAY_READ_FLAG  (0x01)
#define RELAY_WRITE_FLAG (0x10)

/** 
 * @brief Type definition of Registration of Event Call back function
 * 		The file discriptor ready to Read/Write will wake up the call back
 * 		with respective flags
 * 
 * @param fd File Descriptor
 * @param relay_id Relay ID
 * @param rw_flag - bit 0 => read, bit 1 => write
 * @param cb_func - Call back function to invoke
 * 
 * @return  -  0 - Success
 * 			 (-1) - Failure
 */
typedef int (* reg_relay_rw_ev_cb_t)(int fd, int relay_id, int rw_flag , relay_rw_event_cb_t *cb_func);

/** 
 * @brief Set Flags for event call back
 * 
 * @param fd File descriptor
 * @param rw_flag R/W Flag
 * 
 * @return   0 - Success
 * 		   (-1) - Failure
 */
typedef int (* rw_ev_set_flag_cb_t) (int fd, int rw_flag);

/** 
 * @brief Get Flags for event call back
 * 
 * @param fd File descriptor
 * @param rw_flag pointer to returning flag value
 * 
 * @return   0 - Success
 * 		   (-1) - Failure
 */
typedef int (* rw_ev_get_flag_cb_t) (int fd, int *rw_flag);

/** 
 * @brief Type definition of Read/Write Event Call back (Callback invoked by async scheduler)
 * 
 * @param relay_id - Relay Identifier
 * @param rw_flag - Readiness indicator bit 0 => ready to read, bit 1 => ready to write
 * 
 * @return  - 0 - Success
 */
typedef int (* relay_rw_event_cb_t)(int relay_id, int rw_flag);


/** 
 * @brief Type definition of Recv Call back
 * 
 * @param relay_id - Recv channel
 * @param buf - Recv Data
 * @param len - Data length
 * 
 * @return - 0 - Success
 */
typedef int (* relay_recv_cb_t)(int relay_id, const char *buf, int len);


/** 
 * @brief Typedef of Configuration for the relay interface
 */
typedef relay_if_conf_t {
	reg_relay_rw_ev_cb_t reg_event_cb; 		/* Registration function for even call backs */
	rw_event_set_flag_cb_t set_flag_cb; 	/* write flags */
	rw_event_get_flag_cb_t get_flag_cb; 	/* read flags */
}relay_if_conf_t;

/** 
 * @brief Initialize Relay Interface
 * 
 * @param conf Parameters for Initialization
 * 
 * @return 	 0 - Success
 * 			-1 - Failure
 */
int relay_if_init(relay_if_conf_t *conf);

/** 
 * @brief Connection configuration details
 */
typedef struct relay_conn_conf_t{
	const char *host;			/*!< Host to connect */
	const char *service;		/*!< Port/service to connect */	
	const char *sock_type;		/*!< Socket type, Ref socket(2) */
	const char *app_prot_type;  /*!< "RTSP" , "RTP" ... */
	/* Addition Conf Params Added Here */
}relay_conn_conf_t;

/** 
 * @brief Open a socket with specified params and connect to it
 * 
 * @param conf - Connection Configuration
 * 
 * @return - relay session id
 * 		   - negative value on error
 */
int relay_connect_host(relay_conn_conf_t *conf);

/** 
 * @brief send Relay Identifier returned by 'relay_connect_host'
 * 
 * @param relay_id
 * @param buf Buffer To Send
 * @param len Buffer To Recv
 * 
 * @return 	0  - Success
 *  		(-1) - RELAY Busy
 *  		(-2) - Buffer Too long 
 *  		(-3) - Invalid Relay Id
 *  		(-4) - Invalid buf
 */
int relay_send(int relay_id, const char *buf, int len);


/** 
 * @brief Registration of a read call back. Registered function is called on
 * 			Data recv on a relay id
 * 
 * @param recv_fn 
 * 
 * @return   0 - Success
 * 			-1 - Failure
 */
int relay_register_rd_cb(relay_recv_cb_t recv_fn);

/** 
 * @brief Cleanup function
 * 
 * @return  none
 */
void relay_if_exit(void );

#endif 

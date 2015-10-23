/*
 * @file diameter_base_api.h
 *
 * diameter_base_api.h - DiameterBase API
 *
 * Vitaly Dzhitenov, July 2011
 *
 * Copyright (c) 2011-2013, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef _DIAMETER_BASE_H_
#define _DIAMETER_BASE_H_

#define DIAMETER_DATA_STORE_MAX_KEYS  4
#define DIAMETER_VERSION 1
#define DIAMETER_MIN_COMPATIBLE_VERSION 1

typedef struct diameter_application_id_s {
    uint32_t vendor;
    uint32_t application;
} diameter_application_id_t;

/*
 *  address family, either IPv4 or IPv6
 */
typedef enum diameter_address_family_enum {
    DIAMETER_IPV4 = 4,
    DIAMETER_IPV6 = 6
} diameter_address_family_t;

typedef enum diameter_address_protocol_enum {
    DIAMETER_TCP = 1,
    DIAMETER_SCTP = 2
} diameter_address_protocol_t;

typedef struct diameter_address_s {
    diameter_address_family_t family;
    union {
        uint32_t ipv4;
        unsigned char ipv6[16];
    } u;
    uint32_t port;
    diameter_address_protocol_t protocol;
    uint32_t reserved;
} diameter_address_t;

/*
 *  opaque handle types used in the API
 */
typedef struct diameter_peer_s*             diameter_peer_handle;
typedef struct diameter_peergroup_s*        diameter_peergroup_handle;
typedef struct diameter_system_s*           diameter_system_handle;
typedef struct diameter_peer_event_s*       diameter_peer_event_handle;
typedef struct diameter_messenger_observer_s*
                                            diameter_messenger_observer_handle;
typedef void*                               diameter_incoming_request_context;
/*
 * handles provided by caller
 *  values understood only by the caller, stored as handles (cookies)
 */
typedef void*                               diameter_transport_handle;
typedef uint64_t                            diameter_messenger_handle;
typedef uint64_t                            diameter_data_store_handle;
typedef uint64_t                            diameter_data_store_iterator_handle;
typedef void*                               diameter_messenger_data_handle;


typedef struct diameter_request_handler_s*  diameter_request_handler_handle;
typedef void* diameter_request_observer_handle;
typedef struct diameter_response_observer_s* diameter_response_observer_handle;
typedef struct diameter_peer_observer_s*    diameter_peer_observer_handle;
typedef struct diameter_peergroup_observer_s* diameter_peergroup_observer_handle;

/*
 * wrapper around read/write lock
 */
typedef void*                               diameter_lock_handle;
/*
 * wrapper around jbuf
 */
typedef void*           diameter_buffer_handle;
/*
 * wrapper around mspapp_timer
 */
typedef struct diameter_timer_s*            diameter_timer_handle;

/*
 *  stored as a handle (cookie) in the peer object
 */
typedef void* diameter_peer_user_handle;
/*
 *  stored as a handle (cookie) in the peer group object
 */
typedef void* diameter_peergroup_user_handle;
/*
 *  result values returned from all API functions 
 */
typedef enum diameter_return_code_enum {
    DIAMETER_OK = 0,
    DIAMETER_INVALID_POINTER,
    DIAMETER_INVALID_HANDLE,
    DIAMETER_INVALID_DIAMETER_MESSAGE,
    DIAMETER_INVALID_ARGUMENT,
    DIAMETER_SYSTEM_START_FAILED,
    DIAMETER_SYSTEM_STOP_FAILED,
    DIAMETER_SYSTEM_FAILURE,
    DIAMETER_WRONG_STATE,
    DIAMETER_NO_FAILOVER,
    /*
     *  the buffer passed to the function cannot accomodate the data that
     *  needs to be returned;  the amount of buffer space needed is
     *  returned as an output argument from the function
     */
    DIAMETER_BUFFER_TOO_SMALL,

    DIAMETER_NO_TRANSPORTS,
    DIAMETER_NO_ADDRESSES,
    /*
     *  an internal call resulted in an unexpected condition that cannot
     *  be mapped to a documented result value;
     *  if this value is returned from any api function, it indicates a
     *  bug in the implementation
     */
    DIAMETER_UNEXPECTED_ERROR,

    DIAMETER_SYSTEM_NOT_STARTED,
    /* call must be made before system starts */
    DIAMETER_SYSTEM_STARTED,
    DIAMETER_DUPLICATE_REQUEST,
    DIAMETER_MEMORY_FAILURE,
    DIAMETER_RESOURCE_FAILURE

} diameter_return_code_t;

enum diameter_send_request_flags {
    DIAMETER_ALLOW_FAILOVER = 1,
    DIAMETER_ALLOW_PAUSE
};

/*
 *  this values can be returned from message observer callback 
 */
typedef enum diameter_message_action_enum {
    /*
     *  continue to make progress
     */
    DIAMETER_MESSAGE_CONTINUE = 0,
    /*
     *  cancel the message
     */
    DIAMETER_MESSAGE_CANCEL,
    DIAMETER_MESSAGE_DETACH
} diameter_message_action_t;

/*
 *  request status 
 */
typedef enum diameter_request_state_enum {
    /*
     *  message is a request ready to be (re)submitted
     */
    DIAMETER_REQUEST_IDLE = 1,
    /*
     *  message is a request waiting in peer's pending queue
     */
    DIAMETER_REQUEST_QUEUED = 3,
    /*
     *  the message is a request that has been passed into a send buffer,
     *  from which it may have been sent out on the network.
     *  if a response is received while the message is in this
     *  state, the response handler is called.
     */
    DIAMETER_REQUEST_SENT = 4,
    /*
     *  a response to request has been received and has been dispatched
     *  to the application code. this is a final state.
     */
    DIAMETER_REQUEST_DONE = 6,
    /*
     *  includes peer not open shutdown/disconnect/close.
     *  this is a final state.
     */
    DIAMETER_REQUEST_ERROR = 7,
    /*
     *  the message has been removed from all queues and
     *  no response will be dispatched for it.
     *  this is a final state.
     */
    DIAMETER_REQUEST_CANCELLED = 8

} diameter_request_state_t;

/*
 *  response status 
 */
typedef enum diameter_response_state_enum {
    /*
     *  the message has not yet been sent
     */
    DIAMETER_RESPONSE_IDLE = 0,
    /*
     *  message is a response waiting in peer's pending queue
     */
    DIAMETER_RESPONSE_QUEUED = 2,
    /*
     *  the mesage is a response that has been passed into a send buffer,
     *  from which it may have been sent out on the network.
     */
    DIAMETER_RESPONSE_DONE = 5,
    /*
     *  includes peer not open shutdown/disconnect/close.
     *  this is a final state.
     */
    DIAMETER_RESPONSE_ERROR = 7,
    /*
     *  the message has been removed from all queues and
     *  no response will be dispatched for it.
     *  this is a final state.
     */
    DIAMETER_RESPONSE_CANCELLED = 8

} diameter_response_state_t;

typedef enum diameter_peer_disconnect_reason_enum {
    /*
     *  remote
     */
    /*
     *  default. covers remote transport close
     */
    DIAMETER_PEER_DISCONNECT_REMOTE = 0,
    /*
     *  DPR received. had to close
     */
    DIAMETER_PEER_DISCONNECT_REMOTE_DPR = 1,

    /*
     *  local
     */
    /*
     *  watchdog suspect
     */
    DIAMETER_PEER_DISCONNECT_WATCHDOG = 2,
    /*
     *  DPR was sent
     */
    DIAMETER_PEER_DISCONNECT_DPR = 3

} diameter_peer_disconnect_reason_t;

/*
 *  log groups, identifying categories of log messages
 */
typedef enum diameter_log_group_enum {
    DIAMETER_LOG_TRANSACTION_PROCESSING = 0,
    DIAMETER_LOG_PACKET_TRACE = 1,
    DIAMETER_LOG_PEER_STATE_MACHINE = 2,
    DIAMETER_LOG_CONFIGURATION = 3
} diameter_log_group_t;
/*
 *  log levels, one of these can be selected for each log group
 */
typedef enum diameter_log_level_enum {
    DIAMETER_LOG_NO_MESSAGES = 0,
    DIAMETER_LOG_SEVERE_MESSAGES = 1,
    DIAMETER_LOG_NORMAL_MESSAGES = 2,
    DIAMETER_LOG_DEBUG_MESSAGES = 3
} diameter_log_level_t;


typedef struct diameter_incoming_queue_config_s {
         uint32_t size;
} diameter_incoming_queue_config_t;

typedef struct diameter_outgoing_queue_config_s {
         uint32_t size;
         uint32_t high_watermark;
         uint32_t low_watermark;
} diameter_outgoing_queue_config_t;

/* ------------------------- Peer ----------------------------------------- */

int diameter_peer_release(diameter_peer_handle peer);
/*
 *  on return *pnhandle contains user handle value specified in create_peer
 */
int diameter_peer_get_user_data_handle(diameter_peer_handle peer,
                                       diameter_peer_user_handle* handle);
/*
 *  specifies time interval between connect attempts
 */
int diameter_peer_set_reconnect_timeout(diameter_peer_handle peer,
                                        uint32_t reconnect_timeout_ms);

/*
 *  specifies time interval between connect attempts after
 *  DO_NOT_WANT_TO_TALK_TO_YOU
 */
int diameter_peer_set_repeat_timeout(diameter_peer_handle peer,
                                     uint32_t repeat_timeout_ms);
/*
 *  specifies timeout value for watchdog algorithm
 */
int diameter_peer_set_watchdog_timeout(diameter_peer_handle peer,
                                       uint32_t watchdog_timeout_ms);
/*
 *  specifies timeout value for peer state machine as in rfc 3588
 */
int diameter_peer_set_state_machine_timeout(diameter_peer_handle peer,
                                            uint32_t state_machine_timeout_ms);

int diameter_peer_set_connect_timeout(diameter_peer_handle peer,
                                      uint32_t timeout_ms);
int diameter_peer_set_cea_timeout(diameter_peer_handle peer,
                                  uint32_t timeout_ms);
int diameter_peer_set_dpa_timeout(diameter_peer_handle peer,
                                  uint32_t timeout_ms);

/*
 *  configures peer with all supported vendor ids
 */
int diameter_peer_set_supported_vendor_ids(diameter_peer_handle peer,
                                           uint32_t* supported_vendor_id,
                                           uint32_t size);
/*
 *  configures peer with all supported auth application ids
 */
int diameter_peer_set_auth_application_ids(diameter_peer_handle peer,
                                           diameter_application_id_t* ids,
                                           uint32_t size);
/*
 *  configures peer with all supported acct application ids
 */
int diameter_peer_set_acct_application_ids(diameter_peer_handle peer,
                                           diameter_application_id_t* ids,
                                           uint32_t size);
/*
 *  configures peer with all remote host addresses in the numeric form 
 */
int diameter_peer_set_remote_addresses(diameter_peer_handle peer,
                                       diameter_address_t* address,
                                       uint32_t size);
/*
 *  configures peer queue limit 
 *  exceeding the limit will result in onqueuelimitexceeded called 
 */
int diameter_peer_set_outgoing_queue_config(diameter_peer_handle peer,
                                            diameter_outgoing_queue_config_t* config);

int diameter_peer_set_origin_host_prefix(diameter_peer_handle peer,
                                         const char* origin_host_prefix);
/*
 *  specifies values to use for Host-IP-Address in capabilities exchange;
 */
int diameter_peer_set_local_addresses(diameter_peer_handle peer,
                                      diameter_address_t* numeric_address,
                                      uint32_t size);
/*
 *  retrieves peer queue limit 
 */
int diameter_peer_get_outgoing_queue_config(diameter_peer_handle peer,
                                            diameter_outgoing_queue_config_t* config);
/*
 *  retrieves all supported vendor ids for this peer
 */
int diameter_peer_get_supported_vendor_ids(diameter_peer_handle peer,
                                           uint32_t* supported_vendor_ids,
                                           uint32_t size,
                                           uint32_t* return_size);
/*
 *  retrieves all supported auth application ids for this peer
 */
int diameter_peer_get_auth_application_ids(diameter_peer_handle peer,
                                           diameter_application_id_t* ids,
                                           uint32_t size,
                                           uint32_t* return_size);
/*
 *  retrieves all supported acct application ids for this peer
 */
int diameter_peer_get_acct_application_ids(diameter_peer_handle peer,
                                           diameter_application_id_t* ids,
                                           uint32_t size,
                                           uint32_t* return_size);
/*
 *  retrieves all remote addresses for this peer
 */
int diameter_peer_get_remote_addresses(diameter_peer_handle peer,
                                       diameter_address_t* host_addresses,
                                       uint32_t size,
                                       uint32_t* return_size);
/*
 *  retrieves remote peer vendor-id
 */
int diameter_peer_get_remote_vendor_id(diameter_peer_handle peer,
                                       uint32_t* vendor_id);
/*
 *  retrieves remote peer firmware-revision 
 */
int diameter_peer_get_remote_firmware_revision(diameter_peer_handle peer,
                                               uint32_t* firmware_revision);
/*
 *  retrieves remote peer product-name
 */
int diameter_peer_get_remote_product_name(diameter_peer_handle peer,
                                          char* product_tname,
                                          uint32_t size,
                                          uint32_t* returnsize);
/*
 *  retrieves remote peer origin-host
 */
int diameter_peer_get_remote_origin_host(diameter_peer_handle peer,
                                         char* origin_host,
                                         uint32_t size,
                                         uint32_t* return_size);
/*
 *  retrieves remote peer origin-realm
 */
int diameter_peer_get_remote_origin_realm(diameter_peer_handle peer,
                                          char* origin_realm,
                                          uint32_t size,
                                          uint32_t* return_size);
/*
 *  finalize peer, start participating in state machine
 */
int diameter_peer_start(diameter_peer_handle peer);
/*
 *  controls whether the peer actively tries to connect; 
 *  may be called repeatedly without requiring restart()
 */
int diameter_peer_set_active(diameter_peer_handle peer, int active);
/*
 *  close connection, use new configuration parameters for next connection
 */
int diameter_peer_restart(diameter_peer_handle peer, int close_cause);
/*
 *  close connection if open, shut down peer and clear all queues, 
 *  peer cannot be used afterwards
 */
int diameter_peer_stop(diameter_peer_handle peer, int close_cause);
/*
 *  stops receiving incoming messages
 */
int diameter_peer_flow_off(diameter_peer_handle peer);
/*
 *  resumes receiving incoming messages
 */
int diameter_peer_flow_on(diameter_peer_handle peer);
/*
 *  configures local transport to be used to connect
 */
int diameter_peer_set_transport(diameter_peer_handle peer,
                                diameter_transport_handle transport);
/*
 *  configures peer incoming queue limit 
 *  exceeding the limit will result in on_incoming_queue_limit_exceeded called
 */
int diameter_peer_set_incoming_queue_config(diameter_peer_handle peer,
                                            diameter_incoming_queue_config_t* config);
/*
 *  retrieves peer incoming queue limit 
 */
int diameter_peer_get_incoming_queue_config(diameter_peer_handle peer,
                                            diameter_incoming_queue_config_t* config);

/* ------------------------- IncomingRequest ------------------------------ */
typedef struct diameter_base_avp_s {
    uint32_t  avp_code;
    bool      avp_has_header;
    union {
        const void*                 p;
        struct diameter_base_avp_s* group;
    } avp_u;

    uint32_t  avp_len;
    uint32_t  avp_flags;
    uint32_t  avp_vendor;
} diameter_base_avp_t;
static inline
void diameter_base_set_avp (uint32_t    avp_code,
                            void*       avp_value,
                            uint32_t    avp_length,
                            uint32_t    avp_flags,
                            uint32_t    avp_vendor,
                            diameter_base_avp_t* avp)
{
    avp->avp_code   = avp_code;
    avp->avp_has_header  = false;
    avp->avp_u.p = avp_value;
    avp->avp_len    = avp_length;
    avp->avp_flags  = avp_flags;
    avp->avp_vendor  = avp_vendor;
}

bool diameter_parse_avp (uint8_t* ptr,
                         uint32_t length,
                         uint32_t* position,
                         uint8_t**   avp_value,
                         uint32_t*   avp_length,
                         diameter_base_avp_t*   avp);



int diameter_add_incoming_request (diameter_peer_handle peer,
                                   diameter_incoming_request_context context,
                                   uint32_t end_to_end,
                                   const char* origin_host_ptr,
                                   uint32_t origin_host_len,
                                   uint64_t* pid);

/*
 *  sends response to the same peer that it was received from, 
 *  matching hop-by-hop ID
 */
int
diameter_incoming_request_send_response(uint64_t id,
                                        uint32_t n, struct iovec* request,
                                        bool remove_from_cache,
                                        diameter_response_observer_handle h);

/*
 *  sends response in the same stack frame where request is received
 *  (without storing request in DSI)
 */
int
diameter_send_response(diameter_incoming_request_context connection,
                       uint32_t n, struct iovec* data,
                       diameter_response_observer_handle h);

/*
 * diameter_error_response
 *
 * @brief
 *
 * @param peer
 * @param messenger
 * @param info
 * @param result_code
 * @param count
 * @param failed_avp_code
 * @param failed_avp_data
 * @param failed_avp_len
 *
 * @return void
 */

int diameter_incoming_request_error_response (diameter_peer_handle  peer,
                                diameter_incoming_request_context connection,
                                uint32_t                          application,
                                uint32_t                          command,
                                uint32_t                          end_to_end,
                                uint32_t                          hop_by_hop,
                                uint32_t                          result_code,
                                uint32_t                          avp_count,
                                diameter_base_avp_t*              avps,
                                diameter_response_observer_handle h);

/* ------------------------- RequestHandler ------------------------------- */

/*
 *  supplies received request along with the interface to reply to the request
 */
typedef int
diameter_cb_receive_request(diameter_request_handler_handle     h,
                            uint32_t                            application,
                            uint32_t                            command,
                            uint32_t                            end_to_end,
                            uint32_t                            hop_by_hop,
                            diameter_buffer_handle              packet,
                            diameter_incoming_request_context   context,
                            diameter_peer_handle                peer);

typedef struct diameter_request_handler_cb_table_s {
    diameter_cb_receive_request* cb_receive_request;
} diameter_request_handler_cb_table;

/* ------------------------- RequestObserver ------------------------------ */

/*
 *  delivers received response to previously sent request
 *  because response and timeout events are concurrent this callback
 *  does not return any value and release callback may happen later (as in the
 *  case when timeout event was in progress when response is received)
 */
typedef void
diameter_cb_request_on_response(diameter_request_observer_handle h,
                                uint16_t n, struct iovec* request,
                                diameter_buffer_handle response_message,
                                diameter_peer_handle peer,
                                diameter_peergroup_handle peergroup,
                                diameter_messenger_data_handle messenger_data);
/*
 *  called at the end of the life time of the request; application should
 *  release resources associated with the request observer
 */
typedef void
diameter_cb_request_on_release(diameter_request_observer_handle h,
                               uint16_t n, struct iovec* request,
                               diameter_peergroup_handle peergroup,
                               diameter_request_state_t state,
                               diameter_message_action_t action);
/*
 *  signals timeout in request processing when message is in a peer group
 *  return value specifies the handling of the request
 */

typedef diameter_message_action_t
diameter_cb_request_on_peer_timeout(diameter_request_observer_handle h,
                                    uint16_t n, struct iovec* request,
                                    diameter_peer_handle peer,
                                    diameter_peergroup_handle peergroup,
                                    diameter_request_state_t state,
                                    uint32_t time_left_ms);

/*
 *  signals error in request processing
 *  return value specifies the handling of the request
 */
typedef diameter_message_action_t
diameter_cb_request_on_error(diameter_request_observer_handle h,
                             uint16_t n, struct iovec* request,
                             diameter_peer_handle peer,
                             diameter_peergroup_handle peergroup,
                             diameter_request_state_t state,
                             const char* error_message,
                             int errorcode,
                             uint32_t time_left_ms);
/*
 *  signals overall timeout in message processing
 *  message goes into final state
 */
typedef diameter_message_action_t
diameter_cb_request_on_timeout(diameter_request_observer_handle h,
                               uint16_t n, struct iovec* request,
                               diameter_peergroup_handle peergroup,
                               diameter_request_state_t state);

/*
 *  signals absence of available peers in request processing when message
 *  is in a peer group.
 *  return value specifies the handling of the request
 */
typedef diameter_message_action_t
diameter_cb_request_on_pause(diameter_request_observer_handle h,
                             uint16_t n, struct iovec* request,
                             diameter_peergroup_handle peergroup,
                             diameter_request_state_t state,
                             uint32_t time_left_ms);

/*
 * out of all callbacks the following two are mutually exclusive and the only
 * one possible callback after them is cb_on_release:
 *      - cb_on_response
 *      - cb_on_timeout
 * the following are transient and can precede cb_on_response or cb_on_timeout:
 *      - cb_on_peer_timeout
 *      - cb_on_error
 * cb_on_release is final callabck after which no callbacks can be called
 */
typedef struct diameter_request_cb_table_s {
    diameter_cb_request_on_response*            cb_on_response;
    diameter_cb_request_on_release*             cb_on_release;
    diameter_cb_request_on_peer_timeout *       cb_on_peer_timeout;
    diameter_cb_request_on_error*               cb_on_error;
    diameter_cb_request_on_timeout *            cb_on_timeout;
    diameter_cb_request_on_pause*               cb_on_pause;
} diameter_request_cb_table;

/* ------------------------- ResponseObserver ------------------------------ */

/*
 *  called at the end of the life time of the message; application should
 *  release resources associated with the message user handle
 */
typedef void
diameter_cb_response_on_release(diameter_response_observer_handle h);
/*
 *  signals timeout in message processing
  */
typedef void
diameter_cb_response_on_timeout(diameter_response_observer_handle h,
                               diameter_buffer_handle message,
                               diameter_peer_handle peer,
                               diameter_response_state_t state);
/*
 *  signals error in message processing
 *  return value specifies the handling of the message
 */
typedef diameter_message_action_t
diameter_cb_response_on_error(diameter_response_observer_handle h,
                             diameter_buffer_handle message,
                             diameter_peer_handle peer,
                             diameter_response_state_t state,
                             const char* error_message,
                             int errorcode);
// TODO: cb_on_timeout
typedef struct diameter_response_cb_table_s {
    diameter_cb_response_on_release*        cb_on_release;
    diameter_cb_response_on_timeout*        cb_on_timeout;
    diameter_cb_response_on_error*          cb_on_error;
} diameter_response_cb_table;

/* ------------------------- PeerObserver --------------------------------- */

/*
 *  called at the end of the life time of the peer; application should
 *  release resources associated with the peer user handle
 */
typedef void
diameter_cb_peer_on_release(diameter_peer_observer_handle h,
                            diameter_peer_user_handle user_handle);
/*
 *  signals _timeout trying to connect peer
 *  returns 0 to cancel connecting, non-zero to continue connecting
 */
typedef int
diameter_cb_peer_on_connect_timeout(diameter_peer_observer_handle h,
                                    diameter_peer_handle peer,
                                    uint32_t elapsed_time_ms);
/*
 *  signals peer connect
 */
typedef void
diameter_cb_peer_on_connect(diameter_peer_observer_handle h,
                            diameter_peer_handle peer);
/*
 *  signals peer disconnect
 */
typedef void
diameter_cb_peer_on_disconnect(diameter_peer_observer_handle h,
                               diameter_peer_handle peer,
                               diameter_peer_disconnect_reason_t reason);
/*
 *  signals peer queue size increasing over configured limit
 */
typedef void
diameter_cb_peer_on_outgoing_queue_limit_exceeded(diameter_peer_observer_handle h,
                                                  diameter_peer_handle peer);
/*
 *  signals peer queue size decreasing under configured limit
 */
typedef void
diameter_cb_peer_on_outgoing_queue_limit_recovered(diameter_peer_observer_handle h,
                                                   diameter_peer_handle peer);
/*
 *  signals peer connect failure
 */
typedef int
diameter_cb_peer_on_connect_failure(diameter_peer_observer_handle h,
                                    diameter_peer_handle peer);
/*
 *  signals peer connect failure
 */
typedef int
diameter_cb_peer_on_ce_failure(diameter_peer_observer_handle h,
                               diameter_peer_handle peer,
                               unsigned long result_code,
                               const char* error_message);
/*
 *  signals 'negative' DPA
 */
typedef void
diameter_cb_peer_on_remote_error(diameter_peer_observer_handle h,
                                 diameter_peer_handle peer,
                                 unsigned long result_code,
                                 const char* error_message);
/*
 *  signals DPR
 */
typedef void
diameter_cb_peer_on_dpr(diameter_peer_observer_handle h,
                        diameter_peer_handle peer,
                        int disconnect_cause);
/*
 *  signals incoming undispatched work queue size increasing over configured limit
 */
typedef void
diameter_cb_peer_on_incoming_queue_limit_exceeded(diameter_peer_observer_handle h,
                                                  diameter_peer_handle peer,
                                                  uint32_t queue_size);
/*
 *  signals incoming undispatched work queue size decreasing under configured limit
 */
typedef void
diameter_cb_peer_on_incoming_queue_limit_recovered(diameter_peer_observer_handle h,
                                                   diameter_peer_handle peer,
                                                   uint32_t queue_size);
/*
 *  signals peer state change
 */
typedef void
diameter_cb_peer_on_state_change(diameter_peer_observer_handle h,
                                 diameter_peer_handle peer,
                                 diameter_peer_state_t old,
                                 diameter_peer_state_t);

typedef struct diameter_peer_cb_table_s {
    diameter_cb_peer_on_release*            cb_on_release;
    diameter_cb_peer_on_connect_timeout *   cb_on_connect_timeout;
    diameter_cb_peer_on_connect*            cb_on_connect;
    diameter_cb_peer_on_disconnect*         cb_on_disconnect;
    diameter_cb_peer_on_outgoing_queue_limit_exceeded*
                                            cb_on_outgoing_queue_limit_exceeded;
    diameter_cb_peer_on_outgoing_queue_limit_recovered*
                                            cb_on_outgoing_queue_limit_recovered;
    diameter_cb_peer_on_connect_failure *   cb_on_connect_failure;
    diameter_cb_peer_on_ce_failure*         cb_on_ce_failure;
    diameter_cb_peer_on_remote_error*       cb_on_remote_error;
    diameter_cb_peer_on_dpr*                cb_on_dpr;
    diameter_cb_peer_on_incoming_queue_limit_exceeded*
                                            cb_on_incoming_queue_limit_exceeded;
    diameter_cb_peer_on_incoming_queue_limit_recovered*
                                            cb_on_incoming_queue_limit_recovered;
    diameter_cb_peer_on_state_change*       cb_on_state_change;
} diameter_peer_cb_table;

/* ------------------------- PeerGroup ------------------------------------ */

int diameter_peergroup_release(diameter_peergroup_handle peergroup);
/*
 *  on return *pnHandle contains user handle value specified in CreatePeerGroup
 */
int diameter_peergroup_get_user_data_handle(diameter_peergroup_handle peergroup,
                                            diameter_peergroup_user_handle* h);
/*
 *  cancel any pending requests, stop all peers, deactivate peer group 
 */
int diameter_peergroup_stop(diameter_peergroup_handle peergroup,
                            int closecause);
/*
 *  This function adds peer object to the peer group. 
 *  Note that Diameter applications supported by the peer are not checked
 *  for compatibility with the rest of peers in the group.
 */
int diameter_peergroup_add_peer(diameter_peergroup_handle peergroup,
                                diameter_peer_handle peer,
                                uint16_t priority,
                                uint32_t timeout);

int diameter_peergroup_set_auth_application_ids(diameter_peergroup_handle pg,
                                                diameter_application_id_t* ids,
                                                uint32_t size);
int diameter_peergroup_set_acct_application_ids(diameter_peergroup_handle pg,
                                                diameter_application_id_t* ids,
                                                uint32_t size);
int diameter_peergroup_get_auth_application_ids(diameter_peergroup_handle peergroup,
                                                diameter_application_id_t* ids,
                                                uint32_t size,
                                                uint32_t* return_size);
int diameter_peergroup_get_acct_application_ids(diameter_peergroup_handle peergroup,
                                                diameter_application_id_t* ids,
                                                uint32_t size,
                                                uint32_t* return_size);
/*
 *  finalize peergroup, start all peers
 */
int diameter_peergroup_start(diameter_peergroup_handle peergroup);
/*
 *  send a request on any open peer; if none are open, queue the request
 */
int diameter_peergroup_send_request(diameter_peergroup_handle peergroup,
                                    uint16_t n, struct iovec* request,
                                    uint32_t timeout,
                                    uint32_t nFlags,
                                    uint32_t dh_offset,
                                    diameter_request_observer_handle h,
                                    diameter_peer_handle peer);

int diameter_peergroup_get_supported_vendor_ids(diameter_peergroup_handle pg,
                                                uint32_t* supported_vendor_ids,
                                                uint32_t size,
                                                uint32_t* return_size);

/* ------------------------- PeerGroupObserver ---------------------------- */

/*
 *  called at the end of the life time of the peer group; the application should
 *  release resources associated with the peer group user handle
 */
typedef void
diameter_cb_peergroup_on_release(diameter_peergroup_observer_handle h,
                                 diameter_peergroup_user_handle user_handle);
/*
 *  This callback is called when a peer in the group reaches the connected state. 
 *  Note that the peer observer will also be called, if one was registered
 *  when the grouped peer was created.
 */
typedef void
diameter_cb_peergroup_on_connect(diameter_peergroup_observer_handle h,
                                 diameter_peergroup_handle peergroup,
                                 diameter_peer_handle peer);
/*
 *  called when messages are retransmitted to a peer in the group, either
 *  because they were queued (or sent) on a peer that failed or because no peer
 *  was available at the time they were originally submitted;
 *  the argument NumRetransmittedRequests is the number of request messages
 *  that were sent to the peer
 */
typedef void
diameter_cb_peergroup_on_failover(diameter_peergroup_observer_handle h,
                                  diameter_peergroup_handle peergroup,
                                  diameter_peer_handle hpeer,
                                  uint32_t num_retransmitted_requests);

/*
 *  called when a peer in the group goes to disconnected state;  
 *  the argument NumPendingRequests is the number of request messages that were
 *  queued on the peer at the time that it became disconnected (these will be
 *  retransmitted to the next available *  peer);
 *  the argument NumRestartedRequests is the number of requests that were
 *  previously sent to the peer but which will be restarted because the
 *  connection they were sent on is closed (these will be retransmitted to the
 *  next available peer);
 *  NumConnectedPeers is the number of connected peers in this group 
 *  (after the peer is disconnected)
 */
typedef void
diameter_cb_peergroup_on_disconnect(diameter_peergroup_observer_handle h,
                                    diameter_peergroup_handle peergroup,
                                    diameter_peer_handle peer,
                                    diameter_peer_disconnect_reason_t reason,
                                    uint32_t num_pending_requests,
                                    uint32_t num_restarted_requests);

typedef struct diameter_peergroup_cb_table_s {
    diameter_cb_peergroup_on_release*      cb_on_release;
    diameter_cb_peergroup_on_connect*      cb_on_connect;
    diameter_cb_peergroup_on_failover*     cb_on_failover;
    diameter_cb_peergroup_on_disconnect*   cb_on_disconnect;
} diameter_peergroup_cb_table;

/* ------------------------- Statistics ---------------------------------- */
typedef enum diameter_peer_stat_enum {
    DIAMETER_PEER_STAT_LAST_DISC_CAUSE,      //Integer32
    DIAMETER_PEER_STAT_WHO_INIT_DISCONNECT,  //Integer32
    DIAMETER_PEER_STAT_DW_CURRENT_STATUS,    //Integer32
    DIAMETER_PEER_STAT_REQUEST_IN,
    DIAMETER_PEER_STAT_RESPONSE_OUT,
    DIAMETER_PEER_STAT_REQUEST_OUT,
    DIAMETER_PEER_STAT_RESPONSE_IN,
    DIAMETER_PEER_STAT_CONNECT_FAILURES,    //Counter32
    DIAMETER_PEER_STAT_REQUEST_TIMEOUTS,    //Counter32
    DIAMETER_PEER_STAT_ASR_IN,              //Counter32
    DIAMETER_PEER_STAT_ASA_OUT,             //Counter32
    DIAMETER_PEER_STAT_ASR_OUT,                 //Counter32
    DIAMETER_PEER_STAT_ASA_IN,                  //Counter32
    DIAMETER_PEER_STAT_ACR_IN,              //Counter32
    DIAMETER_PEER_STAT_ACR_OUT,             //Counter32
    DIAMETER_PEER_STAT_ACA_IN,              //Counter32
    DIAMETER_PEER_STAT_ACA_OUT,             //Counter32
    DIAMETER_PEER_STAT_CER_IN,              //Counter32
    DIAMETER_PEER_STAT_CER_OUT,             //Counter32
    DIAMETER_PEER_STAT_CEA_IN,              //Counter32
    DIAMETER_PEER_STAT_CEA_OUT,             //Counter32
    DIAMETER_PEER_STAT_DWR_IN,              //Counter32
    DIAMETER_PEER_STAT_DWR_OUT,             //Counter32
    DIAMETER_PEER_STAT_DWA_IN,              //Counter32
    DIAMETER_PEER_STAT_DWA_OUT,             //Counter32
    DIAMETER_PEER_STAT_DPR_IN,              //Counter32
    DIAMETER_PEER_STAT_DPR_OUT,             //Counter32
    DIAMETER_PEER_STAT_DPA_IN,              //Counter32
    DIAMETER_PEER_STAT_DPA_OUT,             //Counter32
    DIAMETER_PEER_STAT_RAR_IN,              //Counter32
    DIAMETER_PEER_STAT_RAA_OUT,             //Counter32
    DIAMETER_PEER_STAT_RAR_OUT,                 //Counter32
    DIAMETER_PEER_STAT_RAA_IN,                  //Counter32
    DIAMETER_PEER_STAT_CCR_IN,                  //Counter32
    DIAMETER_PEER_STAT_CCA_OUT,                 //Counter32
    DIAMETER_PEER_STAT_CCR_OUT,                 //Counter32
    DIAMETER_PEER_STAT_CCA_IN,                  //Counter32
    DIAMETER_PEER_STAT_MESSAGE_OUT,             //Counter32
    DIAMETER_PEER_STAT_MESSAGE_IN,              //Counter32
    DIAMETER_PEER_STAT_REDIRECT_EVENTS,      //Counter32
    DIAMETER_PEER_STAT_MALFORMED_MESSAGES,      //Counter32
    DIAMETER_PEER_STAT_RETRANSMISSIONS,         //Counter32
    DIAMETER_PEER_STAT_DROPPED_RESPONSES,       //Counter32
    DIAMETER_PEER_STAT_DROPPED_REQUESTS,        //Counter32
    DIAMETER_PEER_STAT_DUPLICATE_REQUESTS,      //Counter32
    DIAMETER_PEER_STAT_UNKNOWN_TYPES,        //Counter32
    DIAMETER_PEER_STAT_PROTOCOL_ERRORS_OUT,     //Counter32
    DIAMETER_PEER_STAT_TRANSIENT_FAILURES_OUT,  //Counter32
    DIAMETER_PEER_STAT_PERMANENT_FAILURES_OUT,  //Counter32
    DIAMETER_PEER_STAT_PROTOCOL_ERRORS_IN,      //Counter32
    DIAMETER_PEER_STAT_TRANSIENT_FAILURES_IN,   //Counter32
    DIAMETER_PEER_STAT_PERMANENT_FAILURES_IN,   //Counter32
    DIAMETER_PEER_STAT_TRANSPORT_FAILURE,        //Counter32
    DIAMETER_PEER_STAT_HIGH_WATERMARK_HIT,    //Counter32
    DIAMETER_PEER_STAT_LOW_WATERMARK_HIT,     //Counter32
    DIAMETER_PEER_STAT_DW_FAILURES,         //Counter32
    DIAMETER_PEER_STAT_CE_FAILURES,         //Counter32
    DIAMETER_PEER_STAT_UNKNOWN_APPLICATION, //Counter32

    /* must be the last one */
    DIAMETER_PEER_STAT_NUM_COUNTERS
} diameter_peer_stat_t;

typedef void
diameter_cb_peer_stat_increment(diameter_peer_handle peer,
                                diameter_peer_stat_t counter);
typedef void
diameter_cb_peer_stat_decrement(diameter_peer_handle peer,
                                diameter_peer_stat_t counter);

typedef void
diameter_cb_peer_stat_set(diameter_peer_handle peer,
                          diameter_peer_stat_t counter,
                          int value);

typedef enum diameter_peergroup_stat_enum {
    DIAMETER_PEERGROUP_STAT_REQUEST_OUT,
    DIAMETER_PEERGROUP_STAT_RESPONSE_IN,
    DIAMETER_PEERGROUP_STAT_REQUEST_TIMEOUT,     //Counter32
    DIAMETER_PEERGROUP_STAT_REQUEST_CANCEL,     //Counter32
    DIAMETER_PEERGROUP_STAT_NUM_COUNTERS
} diameter_peergroup_stat_t;

typedef void
diameter_cb_peergroup_stat_increment(diameter_peergroup_handle peergroup,
                                     diameter_peergroup_stat_t counter);
typedef void
diameter_cb_peergroup_stat_decrement(diameter_peergroup_handle peergroup,
                                     diameter_peergroup_stat_t counter);

typedef struct diameter_statistics_cb_table_s {
    diameter_cb_peer_stat_increment*       cb_peer_stat_increment;
    diameter_cb_peer_stat_decrement*       cb_peer_stat_decrement;
    diameter_cb_peer_stat_set*             cb_peer_stat_set;

    diameter_cb_peergroup_stat_increment*       cb_peergroup_stat_increment;
    diameter_cb_peergroup_stat_decrement*       cb_peergroup_stat_decrement;
} diameter_statistics_cb_table;

/* ------------------------- LogObserver ---------------------------------- */

/*
 *  receives a UTF8 log message issued by the diameter-base system
 */
typedef void diameter_cb_log_on_log_message(diameter_log_group_t group,
                                            const char* log_message);

typedef struct diameter_log_cb_table_s {
    diameter_cb_log_on_log_message* cb_on_log_message;
} diameter_log_cb_table;

/* ------------------------- Lock callbacks ------------------------------- */
typedef enum diameter_cb_return_code_e {
    DIAMETER_CB_OK = 0,
    DIAMETER_CB_ERROR
} diameter_cb_return_code_t;

typedef uint32_t diameter_cb_lock_create(diameter_lock_handle* lock);
typedef uint32_t diameter_cb_lock_destroy(diameter_lock_handle lock);
/*
 *  Acquires read lock on the given handle
 */
typedef uint32_t diameter_cb_lock_read_lock(diameter_lock_handle lock);
/*
 *  Acquires write lock on the given handle
 */
typedef uint32_t diameter_cb_lock_write_lock(diameter_lock_handle lock);
/*
 *  Releases read lock on the given handle
 */
typedef uint32_t diameter_cb_lock_read_unlock(diameter_lock_handle lock);
/*
 *  Releases write lock on the given handle
 */
typedef uint32_t diameter_cb_lock_write_unlock(diameter_lock_handle lock);

typedef struct diameter_lock_cb_table_s {
    diameter_cb_lock_create*        cb_create;
    diameter_cb_lock_destroy*       cb_destroy;
    diameter_cb_lock_read_lock*     cb_read_lock;
    diameter_cb_lock_write_lock*    cb_write_lock;
    diameter_cb_lock_read_unlock*   cb_read_unlock;
    diameter_cb_lock_write_unlock*  cb_write_unlock;
} diameter_lock_cb_table;

/* ------------------------- Buffer callbacks ------------------------------- */

/*
 *  Returns size of data
 */
typedef uint32_t diameter_cb_buffer_get_size(diameter_buffer_handle buffer,
                                           uint32_t* pnReturnSize);
/*
 *  Returns pointer to data
 */
typedef uint32_t diameter_cb_buffer_get_ptr(diameter_buffer_handle buffer,
                                          uint8_t** ppData);
/*
 *  Releases reference to the buffer. This follows special convention
 *  by which buffer can be referenced only by one entity
 */
typedef uint32_t diameter_cb_buffer_release(diameter_buffer_handle buffer);

typedef struct diameter_buffer_cb_table_s {
    diameter_cb_buffer_get_size*  cb_get_size;
    diameter_cb_buffer_get_ptr*   cb_get_ptr;
    diameter_cb_buffer_release*   cb_release;
} diameter_buffer_cb_table;

/* ------------------------- Schedule ------------------------------------- */
typedef uint32_t diameter_cb_schedule(diameter_peer_handle peer,
                                  diameter_peer_event_handle event);

/* ------------------------- Timer ------------------------------------- */
enum diameter_timer_type_e {
    DIAMETER_TIMER_PEER_FSM,
    DIAMETER_TIMER_PEER_REQUEST,
    DIAMETER_TIMER_PEERGROUP_REQUEST,
    DIAMETER_TIMER_INCOMING_REQUEST
};
typedef uint32_t diameter_cb_create_timer (uint16_t type,
                                           void* ctx,
                                           diameter_timer_handle* out);
typedef uint32_t diameter_cb_start_timer (diameter_timer_handle h,
                                          uint32_t timeout,
                                          uint32_t jitter_percent);
typedef uint32_t diameter_cb_restart_timer (diameter_timer_handle h,
                                            uint32_t nTimeout);
typedef uint32_t diameter_cb_stop_timer (diameter_timer_handle h);
typedef uint32_t diameter_cb_destroy_timer (diameter_timer_handle h);
typedef uint32_t diameter_cb_get_remaining_time (diameter_timer_handle h,
                                                 uint32_t *remaining_time);
typedef struct diameter_timer_cb_table_s {
    diameter_cb_create_timer*   cb_create_timer;
    diameter_cb_start_timer*    cb_start_timer;
    diameter_cb_restart_timer*  cb_restart_timer;
    diameter_cb_stop_timer*     cb_stop_timer;
    diameter_cb_destroy_timer*  cb_destroy_timer;
    diameter_cb_get_remaining_time* cb_get_remaining_time;
} diameter_timer_cb_table;
/* ------------------------- Data Lookups --------------------------------- */
/*
 * Creates hash table (for example dsi).
 */
typedef enum diameter_data_store_key_type_s {
    DIAMETER_DATA_STORE_KEY_STRING, /* string              */
    DIAMETER_DATA_STORE_KEY_INT16, /* 16-bit Integer      */
    DIAMETER_DATA_STORE_KEY_INT32, /* 32-bit Integer      */
    DIAMETER_DATA_STORE_KEY_INT64,
/* 64-bit Integer      */
} diameter_data_store_key_type_t;
typedef enum diameter_data_store_type_s {
    DIAMETER_DATA_STORE_TYPE_HASH,            /* Hash table           */
    DIAMETER_DATA_STORE_TYPE_ITABLE16,        /* Itable 16        */
    DIAMETER_DATA_STORE_TYPE_ITABLE32,        /* Itable 32        */
    DIAMETER_DATA_STORE_TYPE_PTREE,           /* Patricia Tree        */
} diameter_data_store_type_t;

typedef struct diameter_data_store_parameters_s {
    uint32_t total_keys;
    diameter_data_store_key_type_t key_type[DIAMETER_DATA_STORE_MAX_KEYS];
    uint32_t key_offset[DIAMETER_DATA_STORE_MAX_KEYS];
    uint32_t key_len[DIAMETER_DATA_STORE_MAX_KEYS];
    void (*findcb_after)(void *obj, void *args);
} diameter_data_store_parameters_t;

typedef uint32_t
        diameter_cb_data_store_create(diameter_data_store_type_t type,
                                      const char* name,
                                      diameter_data_store_parameters_t* parameters,
                                      diameter_data_store_handle* phandle);

/*
 * Destroys hash table represented by diameter_avp_table_handle.
 * The handle is no longer valid after this call.
 */
typedef uint32_t diameter_cb_data_store_destroy(diameter_data_store_handle handle);
/*
 * Inserts entry into table represented by diameter_avp_table_handle
 */
typedef uint32_t diameter_cb_data_store_insert(diameter_data_store_handle handle,
                                     void* obj,
                                     void* key[DIAMETER_DATA_STORE_MAX_KEYS]);
/*
 * Removes entry from the table represented by diameter_avp_table_handle
 */
typedef uint32_t diameter_cb_data_store_remove(diameter_data_store_handle handle,
                                      void* key[DIAMETER_DATA_STORE_MAX_KEYS],
                                      void** obj);
/*
 * Looks up entry in the table represented by diameter_avp_table_handle
 */
typedef uint32_t diameter_cb_data_store_find(diameter_data_store_handle handle,
                               void* key[DIAMETER_DATA_STORE_MAX_KEYS],
                               void** obj);

/*
 * Looks up entry in the table represented by diameter_avp_table_handle
 * Invokes callback under tree (bucket) lock
 */
typedef uint32_t
diameter_cb_data_store_callback_find(diameter_data_store_handle handle,
                                     void* key[DIAMETER_DATA_STORE_MAX_KEYS],
                                     void** obj,
                                     void* args);

typedef enum diameter_data_store_iterator_flags_s {
    DIAMETER_DATA_STORE_ITERATOR_EXTRACT_ALL
} diameter_data_store_iterator_flags_t;

typedef uint32_t
diameter_cb_data_store_create_iterator(diameter_data_store_handle handle,
                                       uint32_t flags,
                                       void* params,
                                       diameter_data_store_iterator_handle* p);

typedef uint32_t
diameter_cb_data_store_destroy_iterator(diameter_data_store_handle handle,
                                       diameter_data_store_iterator_handle p);

typedef uint32_t
diameter_cb_data_store_iterator_get_next (diameter_data_store_handle handle,
                                          diameter_data_store_iterator_handle it,
                                          void** p_obj_out);

typedef struct diameter_data_store_cb_table_s {
    diameter_cb_data_store_create*    cb_create;
    diameter_cb_data_store_destroy*   cb_destroy;
    diameter_cb_data_store_insert*    cb_insert;
    diameter_cb_data_store_remove*    cb_remove;
    diameter_cb_data_store_find*      cb_find;
    diameter_cb_data_store_callback_find*       cb_callback_find;
    diameter_cb_data_store_create_iterator*     cb_create_iterator;
    diameter_cb_data_store_destroy_iterator*    cb_destroy_iterator;
    diameter_cb_data_store_iterator_get_next*   cb_get_next;
} diameter_data_store_cb_table;
/* ------------------------- Memory management ---------------------------- */
typedef enum diameter_object_type_e {
    DIAMETER_TYPE_INCOMING_REQ_CTX = 0,
    DIAMETER_TYPE_PEER_EVENT,
    DIAMETER_TYPE_PEER,
    DIAMETER_TYPE_PEER_CONNECTION,
    DIAMETER_TYPE_PEERGROUP,
    DIAMETER_TYPE_SERVER,
    /* must be the last */
    DIAMETER_TYPE_TOTAL
 } diameter_object_type_t;
/*
 *  Allocates memory of the given size
 */
typedef void* diameter_cb_memory_allocate(uint32_t nSize, bool critical);

/*
 *  frees memory
 */
typedef void diameter_cb_memory_free(void*, bool critical);


uint32_t diameter_object_size (diameter_object_type_t type);

/* ------------------------- Outgoing buffer ------------------------- */
/*
 *  Allocates memory of the given size for sending over transport
 */
/*
 * TODO: does VSIZE support reference counting?
 */
#define DIAMETER_DEFAULT_IOV_SIZE   512

typedef void* diameter_cb_iov_create(uint32_t nSize);

/*
 *  frees memory allocated for sending over transport
 */
typedef void diameter_cb_iov_release(void* iov_base);

typedef int diameter_cb_iov_edit(struct iovec* iov);

/* ------------------------- Reference-counted objects -------------------- */
/*
 *  Creates reference-counted object of the given size
 */
typedef void* diameter_cb_object_allocate(diameter_object_type_t type,
                                        uint32_t nSize);
/*
 *  Increments object's reference count.
 */
typedef int diameter_cb_object_addref(diameter_object_type_t type,
                                           void* object);
/*
 *  Decrements reference count.
 */
typedef int diameter_cb_object_release(diameter_object_type_t type,
                                            void* object);
/*
 *  Frees memory allocated for the object
 */
typedef uint32_t diameter_cb_object_free(diameter_object_type_t type,
                                            void* object);
typedef struct diameter_memory_cb_table_s {
    diameter_cb_memory_allocate*            cb_allocate;
    diameter_cb_memory_free*                cb_free;
    diameter_cb_iov_create*                 cb_iov_create;
    diameter_cb_iov_release*                cb_iov_release;
    diameter_cb_iov_edit*                   cb_iov_edit;
    diameter_cb_object_allocate*            cb_object_allocate;
    diameter_cb_object_addref*              cb_object_addref;
    diameter_cb_object_release*             cb_object_release;
    diameter_cb_object_free*                cb_object_free;
} diameter_memory_cb_table;

/* ------------------------- Transport callbacks --------------------------- */

typedef uint32_t
diameter_cb_create_messenger(diameter_transport_handle transport,
                             diameter_incoming_queue_config_t* incoming,
                             diameter_outgoing_queue_config_t* outgoing,
                             diameter_messenger_observer_handle observer_handle,
                             diameter_messenger_handle* pMessenger);

typedef struct diameter_transport_cb_table_s {
    diameter_cb_create_messenger* cb_create_messenger;

} diameter_transport_cb_table;

/* ------------------------- Messenger callbacks --------------------------- */

/*
 *  Connects the messenger (a socket in TCP or associatation in SCTP) to the 
 *  specified remote address and port. 
 *  Note that the socket will bound to the local address configured for the 
 *  transport which has created the messenger.
 */
typedef uint32_t
diameter_cb_messenger_connect(diameter_messenger_handle messenger,
                              const diameter_address_t* address);

/*
 *  sends (or schedules for sending) data
 */
typedef uint32_t
diameter_cb_messenger_send_data(diameter_messenger_handle messenger,
                                uint32_t n, struct iovec* request,
                                void* uap);

/*
 *  Stops receiving data thus causing back pressure
 */
typedef uint32_t
diameter_cb_messenger_flow_off(diameter_messenger_handle messenger);
/*
 *  Resumes receiving data
 */
typedef uint32_t
diameter_cb_messenger_flow_on(diameter_messenger_handle messenger);
/*
 *  Gracefully disconnects the messenger
 */
typedef uint32_t
diameter_cb_messenger_disconnect(diameter_messenger_handle messenger);
/*
 *  Hard stops the messenger
 */
typedef uint32_t
diameter_cb_messenger_close(diameter_messenger_handle messenger);

typedef uint32_t
diameter_cb_messenger_configure_incoming_queue(diameter_messenger_handle messenger,
                                               diameter_incoming_queue_config_t* incoming);
typedef uint32_t
diameter_cb_messenger_configure_outgoing_queue(diameter_messenger_handle messenger,
                                               diameter_outgoing_queue_config_t* outgoing);

typedef struct diameter_messenger_cb_table_s {
    diameter_cb_messenger_connect*      cb_connect;
    diameter_cb_messenger_send_data*    cb_send_data;
    diameter_cb_messenger_flow_off*     cb_flow_off;
    diameter_cb_messenger_flow_on*      cb_flow_on;
    diameter_cb_messenger_disconnect*   cb_disconnect;
    diameter_cb_messenger_close*        cb_close;
    diameter_cb_messenger_configure_incoming_queue*    cb_configure_incoming_queue;
    diameter_cb_messenger_configure_outgoing_queue*    cb_configure_outgoing_queue;

} diameter_messenger_cb_table;

/* ------------------------- System --------------------------------------- */

typedef struct diameter_system_cb_table_s {
    diameter_log_cb_table           log_cb_table;
    diameter_transport_cb_table     transport_cb_table;
    diameter_messenger_cb_table     messenger_cb_table;
    diameter_memory_cb_table        memory_cb_table;
    diameter_lock_cb_table          lock_cb_table;
    diameter_buffer_cb_table        buffer_cb_table;
    diameter_timer_cb_table         timer_cb_table;
    diameter_data_store_cb_table    data_store_cb_table;
    diameter_statistics_cb_table    statistics_cb_table;

    diameter_cb_schedule*           cb_schedule;
} diameter_system_cb_table;

/*
 *  retrieves version of the system API
 */
int diameter_system_get_version(int* version, int* min_compatible_version);
/*
 starts the system, may only be called once
 */
int
diameter_system_start(diameter_system_cb_table*     system_cb_table,
                      diameter_peer_cb_table*       peer_cb_table,
                      diameter_peergroup_cb_table*  peergroup_cb_table,
                      diameter_request_cb_table*    request_cb_table,
                      diameter_response_cb_table*   response_cb_table);
/*
 *  alternative implementation of peer/peergroup request cache
 *  using fixed size array and internal resource management
 */
int
diameter_system_set_request_cache_size(uint32_t size);

/*
 *  specifies handler for incoming requests 
 */
int
diameter_system_set_default_request_handler(diameter_request_handler_cb_table* cb_table,
                                            diameter_request_handler_handle request_handler);
/*
 *  set Vendor-ID, Product-Name, Firmware-Revision
 */
int diameter_system_set_local_settings(uint32_t vendor_id,
                                       const char* product_name,
                                       uint32_t firmware_revision);
/*
 *  set Origin-Host, Origin-Realm
 */
int diameter_system_set_local_origin(const char* origin_host,
                                     const char* origin_realm);
int diameter_system_set_log_level(diameter_log_group_t group,
                                  diameter_log_level_t level);
/*
 *  creates peer, call multiple times
 */
int diameter_system_create_peer(const char* name,
                                diameter_peer_user_handle user_handle,
                                diameter_peer_observer_handle h,
                                /*out*/diameter_peer_handle* ppeer);
/*
 *  creates peer group, name is meaningful only for logging and statistics, call multiple times
 */
int
diameter_system_create_peergroup(const char* groupname,
                                 diameter_peergroup_user_handle user_handle,
                                 diameter_peergroup_observer_handle h,
                                 uint32_t id,
                                 /*out*/diameter_peergroup_handle* ppeergroup);
/*
 *  stops the system
 */
int diameter_system_shutdown(void);
/*
 *  When thread is available to process peer's event DiameterBase is called
 */
int diameter_peer_event(diameter_peer_handle peer,
                        diameter_peer_event_handle event);
/*
 *  When timer is expired
 */
int diameter_timer_elapsed(uint16_t type, void* context);

/* ------------------------- transport API functions ---------------------- */
/*
 *  Indicates transport error
 */
int diameter_transport_error_indication(diameter_transport_handle hTransport,
                                        const char * cszErrorMessage,
                                        uint32_t nError);
/*
 *  Indicates fatal transport error
 */
int diameter_transport_failure_indication(diameter_transport_handle hTransport,
                                          const char * cszErrorMessage,
                                          uint32_t nError);

/* ------------------------- messenger API functions ---------------------- */
/*
 *  Indicates successful connection (initiated by diameter_cb_messenger_connect)
 */
int
diameter_messenger_connect_confirmation(diameter_messenger_handle messenger,
                                        diameter_messenger_observer_handle h);
/*
 *  Indicates connect failure (initiated by diameter_cb_messenger_connect)
 */
int
diameter_messenger_connect_failure(diameter_messenger_handle messenger,
                                   diameter_messenger_observer_handle h);
/*
 *  Indicates disconnect initiated by the remote peer
 */
int
diameter_messenger_disconnect_indication(diameter_messenger_handle messenger,
                                         diameter_messenger_observer_handle h);
/*
 *  Indicates successful disconnect (initiated by diameter_cb_messenger_disconnect)
 */
int
diameter_messenger_disconnect_confirmation(diameter_messenger_handle messenger,
                                           diameter_messenger_observer_handle h);
/*
 *  Indicates incoming data. There is another call to get the size of the buffer. 
 */
int
diameter_messenger_data_indication(diameter_messenger_handle messenger,
                                   diameter_messenger_observer_handle h,
                                   diameter_buffer_handle data,
                                   diameter_messenger_data_handle messenger_data);
/*
 *  Indicates messenger error
 */
int
diameter_messenger_error_indication(diameter_messenger_handle messenger,
                                    diameter_messenger_observer_handle h,
                                    const char * cszErrorMessage,
                                    uint32_t nError);
/*
 *  Indicates that send queue limit exceeded. 
 *  Internally DiameterBase may wait for the limit to be restored before sending more messages.
 */
int
diameter_messenger_send_queue_pain_indication(diameter_messenger_handle messenger,
                                              diameter_messenger_observer_handle h);
/*
 *  Indicates that send queue limit is restored.
 */
int
diameter_messenger_send_queue_pleasure_indication(diameter_messenger_handle messenger,
                                                  diameter_messenger_observer_handle h);

/*
 * If given length is less than the size of Diameter header
 * this function returns DIAMETER_INVALID_DIAMETER_MESSAGE
 * Otherwise, it reads the length field from the header.
 * It then returns DIAMETER_INVALID_DIAMETER_MESSAGE if the length is valid
 * or DIAMETER_OK otherwise
 *
 */
int diameter_get_message_length (uint8_t* ptr, uint32_t size, uint32_t* length);

/*
 * diameter_reformat_avp changes AVP specified by
 * the_avp_code, the_avp_vendor, and *offset
 * to the the_avp_flags and value specified by new_data, new_length
 *
 */
int diameter_reformat_avp (struct iovec* message,
                           uint32_t size,
                           uint32_t the_avp_code,
                           uint32_t the_avp_flags,
                           uint32_t the_avp_vendor,
                           const uint8_t* new_data, uint32_t new_length,
                           uint32_t* offset);

/*
 * diameter_messenger_accept_indication associates given peer with the
 * given messenger
 * Returns newly created Messenger Observer
 */
int
diameter_messenger_accept_indication(diameter_messenger_handle          messenger,
                                     diameter_peer_handle               peer,
                                     diameter_messenger_observer_handle *h);
/*
 * diameter_messenger_close_indication destroys given Messenger Observer
 * It should be called before Messenger destruction
 */
int
diameter_messenger_close_indication(diameter_messenger_handle           messenger,
                                    diameter_messenger_observer_handle  h);

int is_ropen_state(struct diameter_peer_s *peer);
#endif

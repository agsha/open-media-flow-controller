/*
 *
 *
 */

#ifndef __BNODE_PROTO_H_
#define __BNODE_PROTO_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "bnode.h"
#include "gcl.h"

/**
 * \file src/include/bnode_proto.h Binding node: API for working with
 * GCL binding node messages.
 * \ingroup gcl
 */

/* Body message types for body protocol "BNODE" */
enum {
    bbmt_request_response_offset = 1
};

/**
 * Message type.
 *
 * NOTE: bn_msg_type_map (in bnode_proto.c) must be kept in sync.
 */
typedef enum {

    /* WARNING: do not change these numbers, only add new ones */

    /** No message type specified / invalid */
    bbmt_none =            0,

    /**
     * Query request message.  A query request message may contain
     * several binding node names, each of which specifies a different
     * query suboperation (::bn_query_subop) and set of query node
     * flags (::bn_query_node_flags).
     */
    bbmt_query_request =   1,

    /**
     * Query response message.  This will contain a list of bindings
     * which are the combined response to all node names listed in the
     * query request we are responding to.
     */
    bbmt_query_response =  bbmt_query_request + bbmt_request_response_offset,

    /**
     * Set request message.
     */
    bbmt_set_request =     3,

    /**
     * Set response message.
     */
    bbmt_set_response =    bbmt_set_request + bbmt_request_response_offset,

    /**
     * Action request message.
     */
    bbmt_action_request =  5,

    /**
     * Action response message.
     */
    bbmt_action_response = bbmt_action_request + bbmt_request_response_offset,

    /**
     * Event request message.
     */
    bbmt_event_request =   7, 

    /**
     * Event response message.
     */
    bbmt_event_response =  bbmt_event_request + bbmt_request_response_offset,
    bbmt_LAST
} bn_msg_type;


extern const lc_enum_string_map bn_msg_type_map[];


/* ========== Query message data types ========== */

/**
 * Query suboperation: specifies a particular type of query to perform.
 *
 * NOTE: this is mirrored by
 * src/java/packages/com/tallmaple/gcl/BnQuerySubop.java, which
 * must be kept in sync.
 *
 * NOTE: bn_query_subop_map (in bnode_proto.c) must be kept in sync.
 */
typedef enum {

    /** Invalid/unassigned */
    bqso_none =     0,

    /** Get a single specified node */
    bqso_get =      1,

    /** Iterate: retrieve children or descendants of a specified node.
     * Default is to retrieve only immediate children unless other
     * flags are specified.
     */
    bqso_iterate =  2,

    /** Retrieve registration information for a specified node */
    bqso_reg_info = 3,

    /* xxx-later: compares */
} bn_query_subop;

extern const lc_enum_string_map bn_query_subop_map[];

/** A reserved value for a db_name which refers to the persistent copy
 * of the current active database.
 */
#define BN_DBSPEC_SAVED " * SAVED * "
extern const char bn_dbspec_saved[];

/* 
 * XXX/EMT: is the initial db cached in mgmtd?  What if the initial db
 * functions might return different nodes at different times?
 */

/** A reserved value for a db_name which refers to the initial database.
 * Note that this does not refer to any existing database file; but if
 * a request is issued against the initial db, mgmtd will create one
 * on the fly to use for this request.
 */
#define BN_DBSPEC_INITIAL " * INITIAL * "
extern const char bn_dbspec_initial[];

/** A reserved value for a db_name which refers to the in-memory copy
 * of the currently active database.  Note that this is equivalent to
 * passing NULL for the db_name; it exists to allow clients to be more
 * explicit in their wishes.
 */
#define BN_DBSPEC_RUNNING " * RUNNING * "
extern const char bn_dbspec_running[];

/**
 * Option flags that apply to an entire query request.
 *
 * NOTE: this is mirrored by
 * src/java/packages/com/tallmaple/gcl/BnQueryFlags.java, which
 * must be kept in sync.
 *
 * NOTE: bn_query_flags_map (in bnode_proto.c) must be kept in sync.
 */
typedef enum {
    bqf_NONE = 0,

    /**
     * Do not follow default mgmtd barrier rules, instead providing for
     * full async non-barrier behavior.  This means that this request
     * will not be delayed by any in-flight mgmtd operations or
     * messages, nor will it be a barrier to other mgtmd message
     * processing.  This flag can be applied to messages which query any
     * kind of nodes: config, internal monitoring, or external monitoring.
     * Normally mgmtd enforces various rules to make query - set -
     * query style patterns behave as expected without extra work.
     *
     * Additionally, if you are using this flag for the first time on
     * nodes served by an external daemon of your own, you should make
     * certain that if the daemon re-enters its event loop, that it
     * deals correctly with incoming (async) non-barrier queries.
     */
    bqf_msg_nonbarrier = 1 << 0,

    bqf_MASK = (bqf_msg_nonbarrier),

} bn_query_flags;

/* Bit field of ::bn_query_flags ORed together */
typedef uint32 bn_query_flags_bf;

extern const lc_enum_string_map bn_query_flags_map[];


/**
 * Flags that can be attached to a single node in a query request.
 * Note that those with "iterate" in their names are only appropriate
 * for iterate requests (those whose ::bn_query_subop is
 * ::bqso_iterate).
 *
 * What is the difference between these flags and mdb_db_query_flags?
 * These are for use in GCL requests, while those are for querying 
 * the database directly from within mgmtd using libmdb.
 *
 * (Implementation note: unfortunately, these flags were originally
 * sometimes internally used interchangeably with mdb_db_query_flags,
 * with implicit conversions between the two.  You'd think that you'd
 * only see bn_query_node_flags in libgcl, and only see
 * mdb_db_query_flags in libmdb, and that mgmtd would convert from one
 * to the other while processing incoming requests.  But in fact, it's
 * more intertwined than that: mdb_query() takes an array of
 * bn_request_per_node_settings, and the higher-level APIs in libmdb
 * (implicitly) convert from mdb_db_query_flags to bn_query_node_flags
 * so they can call it.  At the time the sets of flags diverged (the
 * addition of bqnf_cleartext and mdqf_obfuscate), we wanted to sort
 * them all out, but decided disentangling it would be too much
 * work/risk.  Instead, we gave those two flags different values, and
 * reserved the value for each in the other's value space, so the bit
 * field types can continue to be used interchangeably.  Libmdb will
 * need to have a default if neither flag is specified, and that will
 * be to NOT obfuscate.  When mgmtd is interpreting a query request,
 * it will do that one transformation to enforce the default of
 * obfuscation in replies to GCL requests: if bqnf_cleartext is not
 * specified, we add mdqf_obfuscate.)
 *
 * NOTE: this is mirrored by
 * src/java/packages/com/tallmaple/gcl/BnQueryNodeFlags.java, which
 * must be kept in sync.
 *
 * NOTE: bn_query_node_flags_map (in bnode_proto.c) must be kept in sync.
 */ 
typedef enum {
    /** Return only monitoring nodes; exclude any config nodes */
    bqnf_sel_class_no_config =    0x0100,

    /** Return only config nodes; exclude any monitoring/state nodes */
    bqnf_sel_class_no_state =     0x0200,

    /** Allow this query to traverse into passthrough trees */
    bqnf_sel_class_enter_passthrough =  0x0400,

    /**
     * Omit nodes which (a) are config literals, (b) have a ba_value
     * attribute equal to their registered initial value, and (c) are
     * registered with ::mrf_omit_defaults_eligible.
     *
     * This is generally most interesting with Iterate requests, but also
     * works with Get requests.
     *
     * Performance note: there is a slight cost to using this flag
     * (mainly the cost of bn_attrib_compare() on each value against its
     * initial value).  Test queries using this flag, on large numbers of
     * nodes where all nodes had non-default values (and so none were
     * filtered out), took about 2.5% longer than the same query without
     * this flag.  Actual cost will vary depending on the specific data
     * involved.  Of course, once some nodes do start getting filtered
     * out, this cost is quickly recouped.
     */
    bqnf_sel_class_omit_defaults =     0x0800,

    /**
     * For an iterate query, get all descendants, not just immediate
     * children.
     */
    bqnf_sel_iterate_subtree =           0x01000,

    /**
     * For an iterate query, retrieve the node named also, not just
     * its children or descendants.
     */
    bqnf_sel_iterate_include_self =      0x02000,

    /**
     * For an iterate query, only return nodes if they are leaves;
     * generally only appropriate in conjuction with
     * bqnf_sel_iterate_subtree.
     *
     * NOTE: this means nodes whose corresponding registration node is
     * a leaf in the registration tree.  So e.g. if you have an instance
     * of "/hosts/ipv4/ *" with no children, it would not show up, because
     * "/hosts/ipv4/ *" has children in the registration tree.
     */
    bqnf_sel_iterate_leaf_nodes_only =   0x04000,

    /**
     * For an iterate query over a config wildcard, return only a 
     * count of the number of instances, not the actual data.
     * (Note: this does not take into account advanced query
     * filtering; see bug 11836)
     */
    bqnf_sel_iterate_count =             0x10000,

    /*
     * Do not use 0x20000 here; it is reserved for an "obfuscate" flag
     * in mdb_db_query_flags.  See comment at top of this enum
     * definition for explanation of why that matters.
     */

    /**
     * Do not obfuscate the contents of nodes registered with the 
     * mrf_exposure_obfuscate flag.  By default, they are obfuscated.
     *
     * NOTE that unlike all the other flags in this enum (so far),
     * this one is NOT identical to the corresponding one in
     * bn_query_node_flags, so it is actully important to use the 
     * correct set of flags.
     */
    bqnf_cleartext =                     0x40000,

    /**
     * Return bindings exactly as they would be written to persistent
     * storage.  Currently, this means only "if the mrf_exposure_encrypt
     * flag is set, return the binding encrypted", since that is the way 
     * in which saving a config db to persistent storage varies by default
     * from servicing queries.  When a binding is encrypted, the ba_value
     * attribute is removed, and replaced with a ba_encrypted_value
     * attribute of type bt_binary, which holds the old contents of the
     * ba_value in encrypted form.
     *
     * Note that this flag cannot be used with bqnf_cleartext.  If the
     * combination is attempted, a warning will be logged and the
     * bqnf_as_saved flag will be ignored.
     */
    bqnf_as_saved =                      0x80000,
} bn_query_node_flags;

/* Bit field of ::bn_query_node_flags ORed together */
typedef uint32 bn_query_node_flags_bf;

extern const lc_enum_string_map bn_query_node_flags_map[];

typedef struct bn_query_request_per_node bn_query_request_per_node;

typedef struct bn_query_request_body bn_query_request_body;

typedef struct bn_query_response_per_node bn_query_response_per_node;

typedef struct bn_query_response_body bn_query_response_body;

typedef union bn_request_per_node_settings bn_request_per_node_settings;

typedef union bn_request_body_settings bn_request_body_settings;

typedef union bn_response_per_node_settings bn_response_per_node_settings;

typedef union bn_response_body_settings bn_response_body_settings;



/* ========== Set message data types ========== */


/*
 * NOTE: this is mirrored by
 * src/java/packages/com/tallmaple/gcl/BnSetFlags.java, which
 * must be kept in sync.
 *
 * NOTE: bn_set_flags_map (in bnode_proto.c) must be kept in sync.
 */
typedef enum {
    bsf_db_specified =           0x0004, /* NYI: use a non-"running" db */
    bsf_continue_after_error =   0x0008, /* continue commit after 1st err */
} bn_set_flags;

/* Bit field of ::bn_set_flags ORed together */
typedef uint32 bn_set_flags_bf;

extern const lc_enum_string_map bn_set_flags_map[];


/* Use when query or set is not conditional */
enum { bn_db_revision_id_none = 0 };

/**
 * Flags that can be attached to a single node in a set request.
 *
 * (NOTE: these are sometimes used interchangeably with
 * mdb_db_set_flags, and therefore corresponding flags should have the
 * same values where possible.  If for some reason this is not, find
 * and fix all of the places where implicit conversion occurs.)
 *
 * NOTE: this is mirrored by
 * src/java/packages/com/tallmaple/gcl/BnSetNodeFlags.java, which
 * must be kept in sync.
 *
 * NOTE: bn_set_node_flags_map (in bnode_proto.c) must be kept in sync.
 */
typedef enum {
    bsnf_NONE =                  0,

    /**
     * Do copy instead of move.  Only applicable if suboperation is
     * bsso_reparent.
     */
    bsnf_reparent_by_copy =      0x400,
} bn_set_node_flags;

extern const lc_enum_string_map bn_set_node_flags_map[];

/* Bit field of ::bn_set_node_flags ORed together */
typedef uint32 bn_set_node_flags_bf;

/* the per-node info for a set.  must be written via send function */
typedef struct bn_set_request_per_node bn_set_request_per_node;


/* note: this struct must be output via its send function xxx */
typedef struct bn_set_request_body bn_set_request_body;

typedef struct bn_set_response_per_node bn_set_response_per_node;

typedef struct bn_set_response_body bn_set_response_body;



/* ========== Action message data types ========== */

typedef struct bn_action_request_per_node bn_action_request_per_node;

typedef struct bn_action_request_body bn_action_request_body;

typedef struct bn_action_response_per_node bn_action_response_per_node;

typedef struct bn_action_response_body bn_action_response_body;


/* ========== Event message data types ========== */

typedef struct bn_event_request_per_node bn_event_request_per_node;

typedef struct bn_event_request_body bn_event_request_body;

typedef struct bn_event_response_per_node bn_event_response_per_node;

typedef struct bn_event_response_body bn_event_response_body;



/* ==================== generic request and response structures */

/* ========== request */

/* a request or response has a header, and then an op specific body */

/* the per-node info for a request.  must be written via send function  */
typedef struct bn_request_per_node bn_request_per_node;


/* body of a request.  note: this struct must be output via send function */
typedef struct bn_request bn_request;

/* structure to hold fields changed in a request by sending it */
typedef struct bn_request_preserves bn_request_preserves;

TYPED_ARRAY_HEADER_NEW_NONE(bn_request_per_node, bn_request_per_node *);

TYPED_ARRAY_HEADER_NEW_NONE(bn_request, bn_request *);

int bn_request_array_new(bn_request_array **ret_arr);


/* ========== response */

/* the per-node info for a response.  must be written via send function  */
typedef struct bn_response_per_node bn_response_per_node;


/* body of a response.  note: this struct must be output via send function */
typedef struct bn_response bn_response;

TYPED_ARRAY_HEADER_NEW_NONE(bn_response_per_node, bn_response_per_node *);

/**
 * Callback to be called when a request message is received.
 * \param sess GCL session on which the request was received.
 * \param request Request message received.  Note that the caller
 * retains ownership of the message structure, so if you want to keep
 * a copy of the message after your callback returns, you have to make
 * your own copy using bn_request_msg_dup().
 * \param arg The argument passed in when this callback function
 * was registered with the GCL.
 */
typedef int (*bn_msg_request_callback_func)
     (gcl_session *sess, 
      bn_request **inout_request,
      void *arg);

/**
 * Callback to be called when a response message is received.
 * \param sess GCL session on which the response was received.
 * \param response Response message received.  Note that the caller
 * retains ownership of the message structure, so if you want to keep
 * a copy of the message after your callback returns, you have to make
 * your own copy using bn_response_msg_dup().
 * \param arg The argument passed in when this callback function
 * was registered with the GCL.
 */
typedef int (*bn_msg_response_callback_func)
     (gcl_session *sess, 

      /* xxx should have the request msg info */

      bn_response **inout_response,
      void *arg);




typedef enum {
    bss_none = 0,
    bss_invalid = 1,
    bss_init,
    bss_opened,
    bss_closed
} bn_state;


/**
 * @name Message handler callbacks
 * The functions in this section register callback functions which the
 * GCL will call whenever it receives a message.  The function to be
 * called can be determined based on message type and/or which GCL 
 * session it is received on.
 *
 * NOTE: callbacks set globally on a GCL handle only affect sessions
 * created after the global callback is registered.  Each GCL session
 * contains a copy of the callback pointers, which is made from the
 * GCL handle when it is created.  If you want to change callbacks
 * after a session is created, you must change them on the session
 * directly.
 */
/*@{*/

/**
 * Specify a callback to be called whenever a query request message comes
 * in on any GCL session created after this call.
 * \param gclh GCL handle from gcl_init().
 * \param callback_func Function to call when query request message
 * is received.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_global_callback_query_request_msg(gcl_handle *gclh, 
                                      bn_msg_request_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called when a query request message comes
 * in on the specified GCL session.
 * \param sess GCL session from gcl_session_client_new() or other
 * function to open a new session.
 * \param callback_func Function to call when query request message
 * is received on the specified session.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_session_callback_query_request_msg(gcl_session *sess, 
                                      bn_msg_request_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called whenever a query response message comes
 * in on any GCL session created after this call.
 * \param gclh GCL handle from gcl_init().
 * \param callback_func Function to call when query response message
 * is received.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_global_callback_query_response_msg(gcl_handle *gclh, 
                                      bn_msg_response_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called when a query response message comes
 * in on the specified GCL session.
 * \param sess GCL session from gcl_session_client_new() or other
 * function to open a new session.
 * \param callback_func Function to call when query response message
 * is received on the specified session.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_session_callback_query_response_msg(gcl_session *sess, 
                                      bn_msg_response_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called whenever a set request message comes
 * in on any GCL session created after this call.
 * \param gclh GCL handle from gcl_init().
 * \param callback_func Function to call when set request message
 * is received.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_global_callback_set_request_msg(gcl_handle *gclh, 
                                      bn_msg_request_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called when a set request message comes
 * in on the specified GCL session.
 * \param sess GCL session from gcl_session_client_new() or other
 * function to open a new session.
 * \param callback_func Function to call when set request message
 * is received on the specified session.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_session_callback_set_request_msg(gcl_session *sess, 
                                      bn_msg_request_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called whenever a set response message comes
 * in on any GCL session created after this call.
 * \param gclh GCL handle from gcl_init().
 * \param callback_func Function to call when set response message
 * is received.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_global_callback_set_response_msg(gcl_handle *gclh, 
                                      bn_msg_response_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called when a set response message comes
 * in on the specified GCL session.
 * \param sess GCL session from gcl_session_client_new() or other
 * function to open a new session.
 * \param callback_func Function to call when set response message
 * is received on the specified session.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_session_callback_set_response_msg(gcl_session *sess, 
                                      bn_msg_response_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called whenever an event request message comes
 * in on any GCL session created after this call.
 * \param gclh GCL handle from gcl_init().
 * \param callback_func Function to call when event request message
 * is received.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_global_callback_event_request_msg(gcl_handle *gclh, 
                                      bn_msg_request_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called when an event request message comes
 * in on the specified GCL session.
 * \param sess GCL session from gcl_session_client_new() or other
 * function to open a new session.
 * \param callback_func Function to call when event request message
 * is received on the specified session.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_session_callback_event_request_msg(gcl_session *sess, 
                                      bn_msg_request_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called whenever an event response message comes
 * in on any GCL session created after this call.
 * \param gclh GCL handle from gcl_init().
 * \param callback_func Function to call when event response message
 * is received.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_global_callback_event_response_msg(gcl_handle *gclh, 
                                      bn_msg_response_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called when an event response message comes
 * in on the specified GCL session.
 * \param sess GCL session from gcl_session_client_new() or other
 * function to open a new session.
 * \param callback_func Function to call when event response message
 * is received on the specified session.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_session_callback_event_response_msg(gcl_session *sess, 
                                      bn_msg_response_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called whenever an action request message comes
 * in on any GCL session created after this call.
 * \param gclh GCL handle from gcl_init().
 * \param callback_func Function to call when action request message
 * is received.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_global_callback_action_request_msg(gcl_handle *gclh, 
                                      bn_msg_request_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called when an action request message comes
 * in on the specified GCL session.
 * \param sess GCL session from gcl_session_client_new() or other
 * function to open a new session.
 * \param callback_func Function to call when action request message
 * is received on the specified session.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_session_callback_action_request_msg(gcl_session *sess, 
                                      bn_msg_request_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called whenever an action response message comes
 * in on any GCL session created after this call.
 * \param gclh GCL handle from gcl_init().
 * \param callback_func Function to call when action response message
 * is received.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_global_callback_action_response_msg(gcl_handle *gclh, 
                                      bn_msg_response_callback_func 
                                      callback_func,
                                      void *callback_arg);

/**
 * Specify a callback to be called when an action response message comes
 * in on the specified GCL session.
 * \param sess GCL session from gcl_session_client_new() or other
 * function to open a new session.
 * \param callback_func Function to call when action response message
 * is received on the specified session.
 * \param callback_arg Data to be passed to callback_func.
 */
int bn_set_session_callback_action_response_msg(gcl_session *sess, 
                                      bn_msg_response_callback_func 
                                      callback_func,
                                      void *callback_arg);

/*@}*/


/* ================================================================= */
/* bnode message creation */
/* ================================================================= */

/* for debugging */
int bn_request_msg_dump(const gcl_session *sess,
                        const bn_request *query_msg, 
                        int log_level, 
                        const char *prefix);

/** @name General message manipulation */

/*@{*/

/**
 * Create a request message (seldom used).
 * Note that generally, instead of this 
 * function, you should call the APIs specific to the message type 
 * you are creating:
 * bn_query_request_msg_create(), bn_set_request_msg_create(), 
 * bn_action_request_msg_create(), or bn_event_request_msg_create().
 */
int bn_request_msg_create(bn_request **ret_req_msg, bn_msg_type req_type);

/**
 * Duplicate a request message.
 * \param req Request message to be duplicated.
 * \retval ret_req A new copy of the specified request message.
 * This is dynamically allocated and is the caller's responsibility
 * to free.
 */
int bn_request_msg_dup(const bn_request *req, bn_request **ret_req);

/**
 * Send a request message on a GCL session.
 * \param sess The GCL session on which to send the request.
 * \param inout_req_msg The request message to send.  Note that this
 * message will be modified, because one of its fields, the message ID,
 * is not set until the message is sent.
 */
int bn_request_msg_send(gcl_session *sess, 
                        bn_request *inout_req_msg);

/**
 * Send a request message on a GCL session, but first save the
 * contents of whatever fields we overwrite in the message.  A call to
 * this, followed by a call to bn_request_msg_restore_preserved(),
 * will leave the bn_request structure identical to when it started.
 * This is useful if you plan to send the same message to more than
 * one party, and want to save the cost of duplicating it.
 */
int bn_request_msg_preserve_send(gcl_session *sess, bn_request *inout_req_msg,
                                 bn_request_preserves **ret_preserves);

/**
 * Reset a request message to the way it was before the call to
 * bn_request_msg_preserve_send() that produced the preserves structure
 * provided.
 */
int bn_request_msg_restore_preserved(bn_request *inout_req_msg,
                                     const bn_request_preserves *preserves);

/**
 * Free the memory associated with a request preserves structure.
 */
int bn_request_preserves_free(bn_request_preserves **inout_preserves);

/**
 * Free the memory associated with a request message.
 * \param inout_request_msg Request message to be freed.  The pointer 
 * will be set to NULL after it is freed to prevent accidental use of
 * the dangling pointer.
 */
int bn_request_msg_free(bn_request **inout_request_msg);

/**
 * A wrapper around bn_request_msg_free() which can be passed as 
 * the deletion function field ::ao_elem_free_func in an
 * ::array_options structure. 
 */
void bn_request_msg_free_for_array(void *ptr);

/**
 * Create a response message (seldom used).
 * Note that generally, instead of this 
 * function, you should call the APIs specific to the message type 
 * you are creating:
 * bn_query_response_msg_create(), bn_set_response_msg_create(), 
 * bn_action_response_msg_create(), or bn_event_response_msg_create().
 */
int bn_response_msg_create(bn_response **ret_req_msg, bn_msg_type req_type,
                           uint32 request_msg_id,
                           uint32 return_code,
                           const tstring *return_message);

/**
 * Send a response message on a GCL session.
 * \param sess The GCL session on which to send the response.
 * \param inout_resp_msg The response message to send.  Note that this
 * message will be modified, because one of its fields, the message ID,
 * is not set until the message is sent.
 */
int bn_response_msg_send(gcl_session *sess, bn_response *inout_resp_msg);

/**
 * Create a response message, empty except for a return code and message,
 * and send it.
 *
 * \param sess The GCL session on which to send the response.
 * \param resp_type The type of message to send, assumed to be 
 * a response type.
 * \param request_msg_id The request message ID of the request to which
 * we are replying.
 * \param return_code The error return code to include in the response.
 * Use zero for success, and nonzero for failure.
 * \param return_msg The return message to include in the response, if any.
 * This may be NULL.
 * \param service_id The service ID to use for this response.  You may
 * pass zero (bap_service_none) to not set the service ID, thus taking
 * whatever is the default for this session (which will be the last
 * service opened on that session).
 */
int bn_response_msg_create_send(gcl_session *sess, bn_msg_type resp_type, 
                                uint32 request_msg_id, uint32 return_code,
                                const char *return_msg,
                                bap_service_id service_id);

/**
 * Duplicate a response message.
 * \param resp Response message to be duplicated.
 * \retval ret_resp A new copy of the specified response message.
 * This is dynamically allocated and is the caller's responsibility
 * to free.
 */
int bn_response_msg_dup(const bn_response *resp, bn_response **ret_resp);

/**
 * Free the memory associated with a response message.
 * \param inout_response_msg Response message to be freed.  The pointer 
 * will be set to NULL after it is freed to prevent accidental use of
 * the dangling pointer.
 */
int bn_response_msg_free(bn_response **inout_response_msg);

/*@}*/

/**
 * Replace the return code and message in an already-created response
 * message.
 * \param resp The response message whose return code and message
 * to change.
 * \param code An integer return code; normally 0 for success,
 * nonzero for failure.
 * \param msg A user-visible message to be included with the
 * response.  Usually empty except in failure cases.  The string is
 * not modified and the caller retains ownership of the memory.
 */
int bn_response_msg_set_return_code_msg(bn_response *resp,
                                        uint32 code, const char *msg);

/**
 * Append a binding to a response message, usually to a query or
 * action response.  Use this if for some reason you don't know
 * what kind of response it is; otherwise, for additional sanity
 * checking, use bn_query_response_msg_append_binding() or
 * bn_action_response_msg_append_binding().
 *
 * Makes its own copy of the binding provided.
 */
int bn_response_msg_append_binding(bn_response *msg,
                                   const bn_binding *binding,
                                   int32 node_id);

/**
 * Like bn_response_msg_append_binding() except that it assumes ownership
 * of the binding provided, and sets the caller's copy to NULL.
 */
int bn_response_msg_append_binding_takeover(bn_response *msg,
                                            bn_binding **inout_binding,
                                            int32 node_id);

/**
 * Like bn_response_msg_append_binding() except creates a binding
 * for you, setting the ba_value attribute from components you provide.
 */
int bn_response_msg_append_new_binding(bn_response *msg,
                                       const char *name,
                                       bn_type type,
                                       uint32 type_flags,
                                       const char *value,
                                       int32 node_id);

/* send functions */


/**
 * Set the service ID to which the request message should be directed.
 * This is a rarely-used function.  Normally a client will only have
 * one GCL service open per provider, and this will be automatically
 * used for any requests sent.  But if a client has more than one
 * service open on a given session, this function can be used to specify
 * which service the message should be directed to.  If this function
 * is not called and multiple services are open, the last service opened
 * is used by default.
 * \param inout_req The request whose service ID to set.  Note that the
 * function must be called before the request is sent.
 * \param service_id The service ID to set on the request.
 */
int bn_request_set_service_id(bn_request *inout_req, 
                              bap_service_id service_id);


/** @name Extracting contents of requests */

/*@{*/

/**
 * Extract information out of a request.
 * \param req The request from which to extract data.
 * \retval ret_req_type The type of request message that was presented.
 * \retval ret_req_body_msg_specific (Seldom used)
 * \retval want_binding_name_parts Should the bindings in the binding
 * array provided have their name components field filled out?
 * This is mainly provided as a performance aid.
 * \retval ret_node_bindings Array of bindings found in the request.
 * \retval ret_node_ids An array parallel to that returned in 
 * ret_node_bindings; each element tells the node ID of the binding
 * at the corresponding index.
 * \retval ret_node_settings (Seldom used)
 */
int
bn_request_get(const bn_request *req,
               bn_msg_type *ret_req_type,
               const bn_request_body_settings **ret_req_body_msg_specific,
               tbool want_binding_name_parts,
               bn_binding_array **ret_node_bindings,
               uint32_array **ret_node_ids,
               array **ret_node_settings);
               /* XXXX bn_request_per_node_settings */


/**
 * Similar to bn_request_get() except more efficient.  The bindings
 * which are part of the request are detached from the incoming
 * bn_request and taken over by the returned binding array.  The other
 * message contents are not modified or freed.
 */
int
bn_request_get_takeover(bn_request *req,
               bn_msg_type *ret_req_type,
               const bn_request_body_settings **ret_req_body_msg_specific,
               tbool want_binding_name_parts,
               bn_binding_array **ret_node_bindings,
               uint32_array **ret_node_ids,
               array **ret_node_settings);


/**
 * Get the service ID to which a request will be directed.
 */
int bn_request_get_service_id(const bn_request *req, 
                              bap_service_id *service_id);

/**
 * Get the type of a request message (query, set, action, or event request)
 */
bn_msg_type bn_request_get_msg_type(const bn_request *req);

/**
 * Get the message type of the appropriate response to a particular 
 * message (e.g. query response, if this was a query request).
 */
bn_msg_type bn_request_msg_response_msg_type(const bn_request *req);

uint32 bn_request_get_msg_id(const bn_request *req);

/**
 * Get the message id, returning -1 if req is NULL
 */
uint32 bn_request_get_msg_id_quiet(const bn_request *req);

/* XXX/EMT: should this be called bn_request_get_binding()? */
int bn_request_get_param(bn_binding_array *bindings, uint32_array *node_ids,
                         const char *name, char **ret_param,
                         uint32 *ret_node_id);

int bn_request_get_binding_by_name(const bn_request *req,
                                   const char *binding_name,
                                   tbool want_binding_name_parts,
                                   bn_binding **ret_binding,
                                   int32 *ret_node_id);

/*@}*/

int bn_request_set_transaction_id(bn_request *inout_req, 
                                  uint32 transaction_id);
int bn_request_get_transaction_id(const bn_request *req, 
                                  uint32 *transaction_id);


/* XXX: add accessors for other message types here too */


/* all the response functions */

/**
 * Dump the contents of a response message to the logs.
 */
int bn_response_msg_dump(const gcl_session *sess,
                         const bn_response *resp_msg,
                         int log_level, 
                         const char *prefix);

int bn_response_set_service_id(bn_response *inout_req, 
                               bap_service_id service_id);
int bn_response_get_service_id(const bn_response *req, 
                               bap_service_id *service_id);

int bn_response_set_transaction_id(bn_response *inout_req, 
                                  uint32 transaction_id);
int bn_response_get_transaction_id(const bn_response *req, 
                                  uint32 *transaction_id);

/** Get the message type of a response message. */
int bn_response_get_msg_type(const bn_response *req, bn_msg_type *msg_type);

int bn_response_get_msg_ids(const bn_response *resp, uint32 *request_msg_id,
                            uint32 *response_msg_id);

uint32 bn_response_get_msg_id_quiet(const bn_response *resp);

uint32 bn_response_get_req_msg_id_quiet(const bn_response *resp);

int bn_response_set_req_msg_id(bn_response *inout_resp, uint32 request_msg_id);

int bn_response_set_msg_id(bn_response *inout_resp, uint32 msg_id);


/**
 * Function to be called for every binding in a response message.
 * \param resp The response message whose bindings are being iterated over.
 * \param binding A binding from the response.
 * \param binding_name_parts A tstring array containing the name of the
 * binding broken down into parts.
 * \param data The void * parameter passed to the iterator function.
 */
typedef int (*bn_response_foreach_binding_func)
                                    (const bn_response *resp,
                                     const bn_binding *binding,
                                     const tstr_array *binding_name_parts,
                                     void *data);

/** @name Extracting contents of responses */

/*@{*/

/**
 * Extract contents of a response message.
 * \param resp Response message from which to extract information.
 * \param want_binding_name_parts Specify whether or not to populate
 * the binding name parts fields in the bindings returned.
 * \retval ret_return_code Response code extracted from the message.
 * \retval ret_return_message Response message string extracted from 
 * the message.
 * \retval ret_bindings Bindings extracted from the message.  Usually
 * only present in query responses and action responses.
 * \retval ret_node_ids An array of node IDs parallel to the ret_bindings
 * array.  The member at location n in this array contains the node ID
 * of the binding at location n in the binding array.  The node ID
 * allows you to correlate response bindings with nodes in the request:
 * see bn_query_request_msg_append_str() for elaboration.
 */
int bn_response_get(const bn_response *resp,
                    uint32 *ret_return_code,
                    tstring **ret_return_message,
                    tbool want_binding_name_parts,
                    bn_binding_array **ret_bindings,
                    uint32_array **ret_node_ids);

/**
 * Like bn_response_get() except removes the retrieved bindings from the
 * response.  This is more efficient than bn_response_get() for responses
 * with large bindings.  It does not destroy the entire response, just
 * those items it returns as pointers.
 */
int bn_response_get_takeover(bn_response *inout_resp,
                             uint32 *ret_return_code,
                             tstring **ret_return_message,
                             tbool want_binding_name_parts,
                             bn_binding_array **ret_bindings,
                             uint32_array **ret_node_ids);

/**
 * Like bn_response_get() except does not return bindings from the
 * response message.
 */
int bn_response_get_return(const bn_response *resp,
                           uint32 *ret_return_code,
                           tstring **ret_return_message);

/**
 * Like bn_response_get() except does not return the code or message
 * string from the response message.
 */
int bn_response_get_bindings(const bn_response *resp,
                             tbool want_binding_name_parts,
                             bn_binding_array **ret_bindings,
                             uint32_array **ret_node_ids);

/**
 * Like bn_response_get_bindings() except removes the retrieved bindings
 * from the response.  This is more efficient than
 * bn_response_get_bindings() for responses with large bindings.
 */
int bn_response_get_bindings_takeover(bn_response *inout_resp,
                                      tbool want_binding_name_parts,
                                      bn_binding_array **ret_bindings,
                                      uint32_array **ret_node_ids);

/**
 * Look up a binding in a response message based on the binding's name.
 */
int bn_response_get_binding_by_name(const bn_response *resp,
                                    const char *binding_name,
                                    tbool want_binding_name_parts,
                                    bn_binding **ret_binding,
                                    int32 *ret_node_id);

/**
 * Look up a binding in a response message based on the node id originally
 * used in the request message.
 */
int bn_response_get_bindings_by_node_id(const bn_response *resp,
                                        int32 node_id,
                                        tbool want_binding_name_parts,
                                        bn_binding_array **ret_bindings);

/*
 * Get the number of nodes (bindings) in a response message.
 */
int bn_response_get_num_nodes(const bn_response *msg, uint32 *ret_node_count);

/*
 * Get the nth binding and node id of a response message.  Returns
 * lc_err_not_found if there is no such index in the response.
 */
int bn_response_get_nth_node_const(const bn_response *req,
                                   uint32 node_index,
                                   const bn_binding **ret_binding,
                                   uint32 *ret_node_id);

/*
 * Get the nth binding of a response message.  Returns lc_err_not_found
 * if there is no such index in the response.
 */
int
bn_response_get_nth_binding_const(const bn_response *req,
                                  uint32 binding_index,
                                  const bn_binding **ret_binding);

/**
 * Iterate over each binding in a response message, calling a function
 * for each one.
 */
int bn_response_foreach_matching_binding
                           (const bn_response *resp, const char *pattern,
                            bn_response_foreach_binding_func foreach_func,
                            void *foreach_data);

/**
 * Same as bn_response_foreach_matching_binding() except the iteration
 * is done in reverse order.
 * 
 * XXX/EMT: the logic is not actually quite the same, but it doesn't
 * look like it would affect behavior.
 */
int bn_response_foreach_matching_binding_rev
                           (const bn_response *resp, const char *pattern,
                            bn_response_foreach_binding_func foreach_func,
                            void *foreach_data);

/*@}*/

/* ========== Query request ========== */

/**
 * @name Query request construction
 */

/*@{*/

/**
 * Create a query request message (see ::bbmt_query_request).
 * This just creates the empty message, but does not specify any
 * nodes to query.  Use functions like bn_query_request_msg_append()
 * et al. to populate the message with specific queries.
 * \param query_flags A bit field of ::bn_query_flags ORed together.
 * \param db_name Name of database to operate on, or NULL for the 
 * current active database.  The name may be a filename of a file
 * in the configuration directory /config/db, or it may be a 
 * special reserved name: ::bn_dbspec_saved, ::bn_dbspec_initial, or
 * ::bn_dbspec_running.
 * \retval ret_req_msg A pointer to the newly created message structure.
 * This points to dynamically-allocated memory which must be freed with
 * bn_request_msg_free() when you are finished with it.
 */
int bn_query_request_msg_create(bn_request **ret_req_msg, uint32 query_flags,
                                const char *db_name);

int bn_query_request_msg_set_flags(bn_request *query_msg,
                                   uint32 query_flags);

int bn_query_request_msg_get_flags(const bn_request *query_msg, 
                                 uint32 *ret_query_flags);

int bn_query_request_msg_set_db_name(bn_request *query_msg, 
                                     const char *db_name);

int bn_query_request_msg_get_db_name(const bn_request *query_msg, 
                                     const tstring **ret_db_name);

/** \cond */
/* These functions are NYI */
int bn_query_request_msg_set_option_max_nodes(bn_request *query_msg,
                                              uint32 max_nodes);
int bn_query_request_msg_set_option_db_spec(bn_request *query_msg,
                                            const char *db_spec);
/** \endcond */

/**
 * Set a db revision ID condition on a query request.  If the 
 * database revision number at the time the request is received
 * does not match the specified revision ID, the request will fail.
 * Pass bn_db_revision_id_none to specify no constraint
 * (this is the default).
 */
int bn_query_request_msg_set_option_cond_db_revision_id(bn_request *
                                                        query_msg,
                                                        int32 
                                                        cond_db_revision_id);

int bn_query_request_msg_get_option_cond_db_revision_id(const bn_request *
                                                      query_msg,
                                                      int32 *
                                                      ret_cond_db_revision_id);

/* nodes and attributes */

/* common */

/** \cond */
/* NOT YET IMPLEMENTED */
int bn_query_request_msg_set_cookie(bn_request *query_msg,
                            int32 node_id, /* can be -1 for last */
                            uint8 *cookie, uint32 cookie_length);
/** \endcond */

/* gets */

/**
 * Append a single node name to a query request message.
 * \param query_msg Message to which to add this node name.
 * \param subop What kind of query to do
 * \param node_query_flags Bit field of ::bn_query_node_flags ORed 
 * together.
 * \param node_name Full name of binding node to query.
 * \retval ret_node_id An integer which will uniquely identify this 
 * node name within this query request.  When the response comes back,
 * each binding in the response will be associated with a node ID that
 * tells which node in the query it came from.  If you do not need
 * help correlating bindings like this, you may pass NULL here or 
 * ignore the returned value.
 */
int bn_query_request_msg_append_str(bn_request *query_msg,
                                    bn_query_subop subop,
                                    bn_query_node_flags_bf node_query_flags,
                                    const char *node_name, 
                                    int32 *ret_node_id);

int bn_query_request_msg_append_binding(bn_request *query_msg,
                                        bn_query_subop subop,
                                       bn_query_node_flags_bf node_query_flags,
                                        const bn_binding *binding,
                                        int32 *ret_node_id);

/** \cond */
/* Deprecated, use bn_query_request_msg_append_binding() */
int bn_query_request_msg_append(bn_request *query_msg,
                                bn_query_subop subop,
                                bn_query_node_flags_bf node_query_flags,
                                const bn_binding *binding,
                                int32 *ret_node_id);
/** \endcond */

/**
 * Like bn_query_request_msg_append_binding() except that it assumes ownership
 * of the binding provided, and sets the caller's copy to NULL.
 */
int bn_query_request_msg_append_binding_takeover(bn_request *query_msg,
                                                 bn_query_subop subop,
                                       bn_query_node_flags_bf node_query_flags,
                                                 bn_binding **inout_binding,
                                                 int32 *ret_node_id);

/** \cond */
int bn_query_request_msg_append_with_attribs(bn_request *query_msg,
                                             bn_query_subop subop,
                                       bn_query_node_flags_bf node_query_flags,
                                             const char *node_name, 
                                             const bn_attrib_array *attribs,
                                             int32 *ret_node_id);

int bn_request_per_node_settings_get_query_request(
                            const bn_request_per_node_settings *rpns,
                            bn_query_subop *ret_query_subop,
                            bn_query_node_flags_bf *ret_query_node_flags);

/** \endcond */

/**
 * Structure to hold advanced query information for a single wildcard
 * pattern under a single query node.
 */
typedef struct bn_query_node_data_per_wildcard {
    /**
     * Registration node name pattern, relative to the main query node
     * name (bn_query_node_data::bqnd_node_name), to which these
     * criteria pertain.  For example, if there are node
     * registrations:
     *   - /users/ *
     *   - /users/ * /first_name
     *   - /users/ * /last_name
     *   - /users/ * /accounts/ *
     *   - /users/ * /accounts/ * /login
     *
     * and the query node is "/users", then this could be "*", or 
     * "* /accounts/ *".
     */
    tstring *bqnw_pattern;

    /**
     * Tokenization of bqnw_pattern.  The caller may fill out either
     * bqnw_pattern, bqnw_pattern_parts, or both; but if both are
     * filled out, they must be consistent, as the API will assume
     * this and not catch any inconsistencies.
     */
    tstr_array *bqnw_pattern_parts;

    /**
     * An expression to be evaluated for every candidate wildcard
     * instance matching the pattern specified in bqnw_pattern.  It
     * should evaluate to true or false, indicating whether or not we
     * should return this instance.  It is roughly analogous to the
     * "WHERE" clause in a SQL query.
     *
     * See doc/dev/adv-queries.txt for documentation of the expression
     * syntax, and functions you can call from it.
     */
    tstring *bqnw_match_expr;

    /**
     * An optional restriction on which child nodes to return for each
     * wildcard instance matched by bqnw_match_expr.  If this array is
     * NULL, there is no restriction, and all nodes underneath each
     * matching wildcard will be returned (modulo any other
     * restrictions imposed by the query flags).  But if this array is
     * allocated, only those child nodes whose names are listed in it
     * are eligible to be returned.  The names are relative to the
     * wildcard instance matched by bqnw_pattern.  So in the example
     * above, if the pattern was "*", possible values for this array
     * would include "first_name" and "last_name".
     *
     * Note that "accounts/ *" is not.  Whether or not we descend into
     * the second-level wildcard is determined by the presence of
     * another bn_query_node_data_per_wildcard record with a pattern
     * of "* /accounts/ *".
     *
     * This array does NOT come preallocated with a new instance of
     * this structure: it starts as NULL, meaning no restrictions.
     */
    tstr_array *bqnw_fields;

    /**
     * Tokenizations of the names in bqnw_fields.  As with
     * bqnw_pattern_parts, the caller may provide either this, or
     * bqnw_fields, or both.  But if something is provided, it must be
     * complete.  i.e. if bqnw_fields is non-NULL, it will be assumed
     * to include ALL names; and if bqnw_fields_parts is non-NULL, it
     * will be assumed to include name parts for ALL names.
     */
    tstr_array_array *bqnw_fields_parts;

} bn_query_node_data_per_wildcard;


/**
 * Structure to hold advanced query information for a single query
 * node.
 */
typedef struct bn_query_node_data {
    /** 
     * Node name to query. 
     */
    tstring *bqnd_node_name;

    /**
     * Number of wildcards requested when this structure was
     * allocated.  This is filled in by bn_query_node_data_new() and
     * should not be changed.  It reflects how many slots are
     * allocated in the bqnd_wildcards array.
     */
    uint32 bqnd_num_wildcards;

    /**
     * Array of per-wildcard query data.  This has the number of
     * entries specified by bqnd_num_wildcards (plus a NULL one at the
     * end to catch overruns).
     */
    bn_query_node_data_per_wildcard **bqnd_wildcards;

    /*
     * Things we could add: min/max depth, and max num nodes to
     * return.  Per-wildcard, we could also have start_child_num, and
     * max_num_nodes.  These were in the old mdb_iterate_full_binding(),
     * which was spec'd but never implemented.
     */
    
} bn_query_node_data;


/**
 * Allocate a new structure to hold data for a single node to go into
 * a query request.  Fill this out, pass it to
 * bn_query_request_msg_append_full_takeover(), which will take ownership 
 * of it.
 *
 * \param num_wildcards The number of per-wildcard query criteria to
 * allocate space for in the structure.  This represents the number of
 * wildcard registrations (not wildcard instances) underneath the
 * query node for which you want to specify advanced query criteria.
 * If you don't know ahead of time how many wildcards you need, you
 * can always guess or start at zero, and later call
 * bn_query_node_data_resize().
 *
 * \retval ret_query_node_data The newly-allocated structure for you to
 * fill out.
 */
int bn_query_node_data_new(uint32 num_wildcards, 
                           bn_query_node_data **ret_query_node_data);

/**
 * Change the number of spaces allocated for wildcards in a 
 * pre-existing bn_query_node_data structure.  This can be useful
 * if you do not know ahead of time how many wildcards you will need
 * to add to it.
 */
int bn_query_node_data_resize(bn_query_node_data *query_node_data,
                              uint32 num_wildcards);

/**
 * Make a duplicate copy of a bn_query_node_data structure.
 */
int bn_query_node_data_dup(const bn_query_node_data *orig,
                           bn_query_node_data **ret_dup);

/**
 * Free the memory associated with a bn_query_node_data structure,
 * and set the pointer to NULL.
 */
int bn_query_node_data_free(bn_query_node_data **inout_query_node_data);

/**
 * A more powerful way to append a node name to a query, which lets
 * you specify additional options not offered by other
 * bn_query_request_msg_append_...() variants.  
 *
 * NOTE: the query_node_data you pass is taken over by this function,
 * so you do not have to free it afterwards.
 */
int bn_query_request_msg_append_full_takeover(bn_request *query_msg, 
                                    bn_query_subop subop,
                                    bn_query_node_flags node_query_flags,
                                    bn_query_node_data **inout_query_node_data,
                                    int32 *ret_node_id);

/*@}*/


/* ========== Query response ========== */

/**
 * @name Query response construction
 */

/*@{*/

/**
 * Create a query response message (see ::bbmt_query_response).
 * This just creates the empty message, but does not specify any
 * binding nodes to be included in the response.  Use functions like
 * bn_query_response_msg_append_binding() et al. to populate the
 * message with data.
 * \param request_msg_id The message ID of the request message to
 * which we are responding, from bn_request_get_msg_id().
 * \param return_code An integer return code; normally 0 for success,
 * nonzero for failure.
 * \param return_message A user-visible message to be included with the
 * response.  Usually empty except in failure cases.  The string is
 * not modified and the caller retains ownership of the memory.
 * \retval ret_req_msg A pointer to the newly created message structure.
 * This points to dynamically-allocated memory which must be freed with
 * bn_response_msg_free() when you are finished with it.
 */
int bn_query_response_msg_create(bn_response **ret_req_msg, 
                                 uint32 request_msg_id,
                                 uint32 return_code,
                                 const tstring *return_message);


int bn_query_response_msg_set_db_revision_id(bn_response *
                                             query_msg,
                                             int32 db_revision_id);

int bn_query_response_msg_get_db_revision_id(const bn_response *
                                             query_msg,
                                             int32 *ret_db_revision_id);

/**
 * Append a single binding to a query response message.
 * \param query_msg Query response message to which to add this binding.
 * \param binding Binding to add to this message.  Note that this function
 * makes its own copy of the binding, so the caller retains memory ownership
 * of the binding parameter.
 * \param node_id An integer identifying the node from the query request
 * that this binding is in response to.
 */
int bn_query_response_msg_append_binding(bn_response *query_msg,
                                         const bn_binding *binding,
                                         int32 node_id);

/**
 * Same as bn_query_response_msg_append_binding() except that this
 * function takes memory ownership of the specified binding and NULLs
 * out your copy of the pointer to it as a measure of protection against
 * double-frees.
 */
int bn_query_response_msg_append_binding_takeover(bn_response *query_msg,
                                                  bn_binding **inout_binding,
                                                  int32 node_id);


/*@}*/

/* ========== Set request  ========== */

/**
 * @name Set request construction
 */

/*@{*/

/**
 * Create a set request message (see ::bbmt_set_request).
 * This just creates the empty message, but does not specify any
 * bindings to set.  Use functions like bn_set_request_msg_append()
 * et al. to populate the message with specific set requests.
 * \param set_flags A bit field of ::bn_set_flags ORed together.
 * \retval ret_set_msg A pointer to the newly created message structure.
 * This points to dynamically-allocated memory which must be freed with
 * bn_request_msg_free() when you are finished with it.
 */
int bn_set_request_msg_create(bn_request **ret_set_msg, uint32 set_flags);

int bn_set_request_msg_set_flags(bn_request *set_msg, uint32 set_flags);
int bn_set_request_msg_get_flags(const bn_request *set_msg, 
                                 uint32 *ret_set_flags);

/** \cond */
int bn_set_request_msg_set_option_max_nodes(bn_request *set_msg,
                                            uint32 max_nodes);
int bn_set_request_msg_set_option_db_spec(bn_request *set_msg,
                                          const char *db_spec);
/** \endcond */

/**
 * Set a db revision ID condition on a query set.  If the 
 * database revision number at the time the request is received
 * does not match the specified revision ID, the request will fail.
 * Pass bn_db_revision_id_none to specify no constraint
 * (this is the default).
 */
int bn_set_request_msg_set_option_cond_db_revision_id(bn_request *
                                                      set_msg,
                                                      int32 
                                                      cond_db_revision_id);

int bn_set_request_msg_get_option_cond_db_revision_id(const bn_request *
                                                      set_msg,
                                                      int32 *
                                                      ret_cond_db_revision_id);


/* create, delete, modify, and reset */

/**
 * Append a single binding node to a set request.  This can be 
 * used for creating nodes (::bsso_create), deleting nodes 
 * (::bsso_delete), resetting nodes (::bsso_reset), and modifying
 * nodes (::bsso_modify).  It cannot be used for reparents 
 * (::bsso_reparent): use bn_set_request_msg_append_reparent()
 * for those instead.
 * \param set_msg Set request message to append to.
 * \param subop Set sub-operation to perform with this binding.
 * Cannot be ::bsso_reparent.
 * \param node_set_flags Bit field of ::bn_set_node_flags ORed together.
 * \param binding Binding to append to the set request.  If subop is
 * ::bsso_delete or ::bsso_reset, the type and value of the binding
 * does not matter, as the name is all we care about.
 * \retval ret_node_id Node ID assigned to this binding in the set
 * request.  If the set response wants to comment on a particular
 * binding from the set request, it will use the node ID to identify it.
 */
int bn_set_request_msg_append_binding(bn_request *set_msg,
                                      bn_set_subop subop,
                                      bn_set_node_flags_bf node_set_flags,
                                      const bn_binding *binding,
                                      int32 *ret_node_id);


/** \cond */
/* Deprecated, use bn_set_request_msg_append_binding() */
int bn_set_request_msg_append(bn_request *set_msg,
                              bn_set_subop subop,
                              bn_set_node_flags_bf node_set_flags,
                              const bn_binding *binding,
                              int32 *ret_node_id);
/** \endcond */

/**
 * Like bn_set_request_msg_append_binding() except that it assumes ownership
 * of the binding provided, and sets the caller's copy to NULL.
 */
int bn_set_request_msg_append_binding_takeover(bn_request *set_msg,
                                               bn_set_subop subop,
                                           bn_set_node_flags_bf node_set_flags,
                                               bn_binding **inout_binding,
                                               int32 *ret_node_id);



/**
 * Construct a new binding and add it to a set request message.
 * This is the same as bn_set_request_msg_append() except that 
 * it takes the binding components for the ba_value attribute,
 * rather than a pre-formed binding object.
 * \param set_msg Set request message to append to.
 * \param set_subop Set sub-operation to perform with this binding.
 * Cannot be ::bsso_reparent.
 * \param node_set_flags Bit field of ::bn_set_node_flags ORed together.
 * \param name Name of node to set.
 * \param type Type of node to set.  Note that if the subop is
 * ::bsso_delete or ::bsso_reset, the type and value of the binding
 * does not matter, as the name is all we care about.
 * \param type_flags Modifier flags for the type to set.
 * \param str_value A string representation of the value to set.
 * \retval ret_node_id Node ID assigned to this binding in the set
 * request.  If the set response wants to comment on a particular
 * binding from the set request, it will use the node ID to identify it.
 */
int bn_set_request_msg_append_new_binding(bn_request *set_msg,
                                          bn_set_subop set_subop,
                                          bn_set_node_flags_bf node_set_flags,
                                          const char *name,
                                          bn_type type,
                                          uint32 type_flags,
                                          const char *str_value,
                                          int32 *ret_node_id);

/**
 * Like bn_set_request_msg_append_new_binding() except that the name of
 * the binding to set is constructed from a printf()-style format 
 * string and a variable argument list, rather than a single string
 * to use as-is.
 *
 * XXX: this does not handle escaping if any of the strings to be
 * substituted in have '/' in them.  Should have a ..._va() variant
 * for those cases.
 */
int bn_set_request_msg_append_new_binding_fmt(bn_request *set_msg,
                                              bn_set_subop set_subop,
                                           bn_set_node_flags_bf node_set_flags,
                                              bn_type type,
                                              uint32 type_flags,
                                              const char *str_value,
                                              int32 *ret_node_id,
                                              const char *name_fmt, ...)
     __attribute__ ((format (printf, 8, 9)));

/* Reparent / rename */

/**
 * Append a reparent request to a set request message.
 * \param set_msg Set request message to append to.
 * \param node_set_flags Bit field of ::bn_set_node_flags ORed together.
 * \param source_node_name Name of node to be reparented.
 * \param graft_under_name Name of new parent for the node being 
 * moved/renamed, or NULL if it is to remain under the same parent.
 * \param new_self_name New name to be given to the node being 
 * moved/renamed, or NULL if it is to retain the same name.  Note that
 * this is not the fully-qualified node name, but just the last 
 * component of the name.
 * \retval ret_node_id Node ID assigned to this binding in the set
 * request.  If the set response wants to comment on a particular
 * binding from the set request, it will use the node ID to identify it.
 */
int bn_set_request_msg_append_reparent(bn_request *set_msg,
                                       bn_set_node_flags_bf node_set_flags,
                                       const char *source_node_name,
                                       const char *graft_under_name,
                                       const char *new_self_name,
                                       int32 *ret_node_id);

/**
 * Helper function to append a reparent request when the caller has only
 * the old and new names of the binding.
 */
int bn_set_request_msg_append_reparent_rename(bn_request *set_msg,
                                              bn_set_node_flags_bf
                                              node_set_flags,
                                              const char *source_node_name,
                                              const char *dest_node_name,
                                              int32 *ret_node_id);
                                      

/*@}*/

/* ========== Set Response ========== */

/**
 * @name Set response construction
 */

/*@{*/

/**
 * Create a set response message.
 */
int bn_set_response_msg_create(bn_response **ret_req_msg, 
                               uint32 request_msg_id,
                               uint32 return_code,
                               const tstring *return_message);

int bn_set_response_msg_set_db_revision_id(bn_response *set_msg,
                                           int32 db_revision_id);

int bn_set_response_msg_get_db_revision_id(const bn_response *set_msg,
                                           int32 *ret_db_revision_id);

/**
 * Append a binding to a set response.  A copy is made of the binding, 
 * so the caller retains memory ownership of the binding passed.
 */
int bn_set_response_msg_append_binding(bn_response *set_msg,
                                       const bn_binding *binding,
                                       int32 node_id);

/**
 * Same as bn_set_response_msg_append_binding() except that it takes
 * ownership of the binding passed and sets the caller's copy of 
 * the pointer to NULL.
 */
int bn_set_response_msg_append_binding_takeover(bn_response *set_msg,
                                                bn_binding **inout_binding,
                                                int32 node_id);


/*@}*/

/* ========== Actions ========== */

/**
 * @name Action request construction
 */

/*@{*/

/**
 * Create an action request message.
 */
int bn_action_request_msg_create(bn_request **ret_req_msg, 
                                 const char *action_name);

/* XXX/EMT: are action flags really on a per-binding basis? */

/**
 * Append a binding to an action request message.
 */
int bn_action_request_msg_append_binding(bn_request *action_msg,
                                         uint32 node_action_flags,
                                         const bn_binding *binding,
                                         int32 *ret_node_id);

/** \cond */
/* Deprecated, use bn_action_request_msg_append_binding() */
int bn_action_request_msg_append(bn_request *action_msg,
                                 uint32 node_action_flags,
                                 const bn_binding *binding,
                                 int32 *ret_node_id);
/** \endcond */

/**
 * Like bn_action_request_msg_append_binding() except that it assumes ownership
 * of the binding provided, and sets the caller's copy to NULL.
 */
int bn_action_request_msg_append_binding_takeover(bn_request *action_msg,
                                                  uint32 node_action_flags,
                                                  bn_binding **inout_binding,
                                                  int32 *ret_node_id);

/**
 * Append a new binding to an action request message, given the name,
 * type, and value of the \c ba_value attrbute of the binding.
 */
int bn_action_request_msg_append_new_binding(bn_request *action_msg,
                                             uint32 node_action_flags,
                                             const char *name, bn_type type,
                                             const char *value,
                                             int32 *ret_node_id);

int bn_action_request_msg_append_new_binding_ex(bn_request *action_msg,
                                                uint32 node_action_flags,
                                                const char *name, bn_type type,
                                                uint32 type_flags,
                                                const char *value,
                                                int32 *ret_node_id);

/**
 * Return a copy of the action name of a provided action request message.
 * \param action_msg The action message whose name to retrieve.
 * \retval ret_action_name A copy of the action request message's name.
 * Because this is a copy, it is the caller's responsibility to free
 * it when done.
 */
int bn_action_request_msg_get_action_name(const bn_request *action_msg, 
                                          tstring **ret_action_name);

/**
 * Change the name of an existing action message.
 */
int bn_action_request_msg_set_action_name(bn_request *action_msg, 
                                          const char *new_action_name);

/*@}*/

/**
 * @name Action response construction
 */

/*@{*/

/**
 * Create an action response message.
 * \retval ret_resp_msg The newly-created action response message.
 * \param request_msg_id The message ID of the action request message
 * to which this response corresponds.
 * \param return_code The error response code to include with the 
 * action response.  Pass 0 for success, and any other number for
 * failure.
 * \param return_message The return message string.  This string should be
 * fit for end-user consumption as it may be displayed verbatim in the UI.
 * A copy is made of the message passed, so the caller retains memory
 * ownership of it.
 */
int bn_action_response_msg_create(bn_response **ret_resp_msg, 
                                  uint32 request_msg_id,
                                  uint32 return_code,
                                  const tstring *return_message);

/**
 * Append the specified binding to an action response message.
 * \param action_msg The action message to which to append the binding.
 * \param binding The binding to add.  A copy is made of this binding,
 * so the caller retains memory ownership of his binding.
 * \param node_id The node id of the binding to append.  This is optional;
 * pass 0 if you don't care.  This could be used to help correlate 
 * response bindings with the bindings in the request to which they
 * correspond, if there is such a relation.
 */
int bn_action_response_msg_append_binding(bn_response *action_msg,
                                          const bn_binding *binding,
                                          int32 node_id);

/**
 * Same as bn_action_response_msg_append_binding() except that it takes
 * memory ownership of the binding provided.
 */
int bn_action_response_msg_append_binding_takeover(bn_response *action_msg,
                                                   bn_binding **inout_binding,
                                                   int32 node_id);

/* XXX/EMT: should be called bn_action_response_msg_append_new_binding() */
int bn_action_response_set_param(bn_response *resp, const char *name,
                                 const char *param, int32 node_id);

/**
 * Shortcut API that calls bn_action_response_msg_create(),
 * bn_action_response_msg_append_binding_takeover(),
 * bn_response_msg_send(), and bn_response_msg_free() for you.
 * The binding array is consumed by this call.
 */
int bn_action_response_msg_create_send_takeover(gcl_session *sess,
                                            uint32 req_id,
                                            uint32 code,
                                            const char *msg,
                                            bn_binding_array **inout_bindings);
                                             

/*@}*/

/* ========== Events ========== */

/**
 * @name Event request construction
 */

/*@{*/

/**
 * Create an event request message (also known as an event message).
 */
int bn_event_request_msg_create(bn_request **ret_req_msg,
                                const char *event_name);

int bn_event_request_msg_set_event_name(bn_request *req,
                                        const char *event_name);

/**
 * Append a binding to an event request message.
 */
int bn_event_request_msg_append_binding(bn_request *event_msg,
                                        uint32 node_event_flags,
                                        const bn_binding *binding,
                                        int32 *ret_node_id);

/** \cond */
/* Deprecated, use bn_event_request_msg_append_binding() */
int bn_event_request_msg_append(bn_request *event_msg,
                                uint32 node_event_flags,
                                const bn_binding *binding,
                                int32 *ret_node_id);
/** \endcond */

/**
 * Like bn_event_request_msg_append_binding() except that it assumes ownership
 * of the binding provided, and sets the caller's copy to NULL.
 */
int bn_event_request_msg_append_binding_takeover(bn_request *event_msg,
                                                 uint32 node_event_flags,
                                                 bn_binding **inout_binding,
                                                 int32 *ret_node_id);

/**
 * Construct a new binding and append it to an event request message.
 */
int bn_event_request_msg_append_new_binding(bn_request *event_msg,
                                            uint32 node_event_flags,
                                            const char *name, bn_type type,
                                            const char *value,
                                            int32 *ret_node_id);

int bn_event_request_msg_append_new_binding_ex(bn_request *event_msg,
                                               uint32 node_event_flags,
                                               const char *name, bn_type type,
                                               uint32 type_flags,
                                               const char *value,
                                               int32 *ret_node_id);

int bn_event_request_msg_get_event_name(const bn_request *event_msg, 
                                          tstring **ret_event_name);

/*@}*/

/** \cond */

int bn_event_response_msg_create(bn_response **ret_req_msg, 
                                 uint32 request_msg_id,
                                 uint32 return_code,
                                 const tstring *return_message);

int bn_event_response_msg_append_binding(bn_response *event_msg,
                                         const bn_binding *binding,
                                         int32 node_id);

int bn_event_response_msg_append_binding_takeover(bn_response *event_msg,
                                                  bn_binding **inout_binding,
                                                  int32 node_id);

/** \endcond */


/* ========== Common ========== */

int bn_request_msg_append_binding(bn_request *msg,
                                  const bn_binding *binding,
                                  int32 *ret_node_id);

/** \cond */
/* Deprecated, use bn_request_msg_append_binding() */
int bn_request_msg_append(bn_request *msg,
                          const bn_binding *binding,
                          int32 *ret_node_id);
/** \endcond */

int bn_request_msg_append_binding_takeover(bn_request *msg,
                                           bn_binding **inout_binding,
                                           int32 *ret_node_id);

int bn_request_msg_append_new_binding(bn_request *msg,
                                      const char *name,
                                      bn_type type,
                                      uint32 type_flags,
                                      const char *value,
                                      int32 *ret_node_id);

int bn_request_msg_remove_binding(bn_request *msg, const char *name);

int bn_request_msg_remove_binding_prefix(bn_request *msg, 
                                         const char *name, tbool invert_sense);

int bn_request_msg_get_node_count(const bn_request *msg, 
                                  uint32 *ret_node_count);

int bn_request_msg_convert_ipv6_attribs(bn_request *msg, int log_level,
                                        const char *log_prefix);

/*
 * Get the number of nodes (bindings) in a response message.
 */
int bn_request_get_num_nodes(const bn_request *msg, uint32 *ret_node_count);

/*
 * Get the nth binding, node id and per-node settings from a request
 * message.  Returns lc_err_not_found if there is no such index in the
 * request.
 */
int bn_request_get_nth_node_const(const bn_request *req,
                                  uint32 node_index,
                                  const bn_binding **ret_binding,
                                  uint32 *ret_node_id,
                                  const bn_request_per_node_settings
                                  **ret_node_settings);

/*
 * Get the nth binding of a request message.  Returns lc_err_not_found
 * if there is no such index in the request.
 */
int bn_request_get_nth_binding_const(const bn_request *req,
                                     uint32 binding_index,
                                     const bn_binding **ret_binding);


int bn_set_request_msg_set_cookie(bn_request *set_msg,
                                  int32 node_id, /* can be -1 for last */
                                  uint8 *cookie, uint32 cookie_length);

/*
 * Cuts a string from the beginning of the name of each binding in the
 * given message.  This is done in place. 
 */
int bn_request_msg_strip_binding_prefix(bn_request *msg, 
                                        const char *prefix);
int bn_response_msg_strip_binding_prefix(bn_response *msg, 
                                         const char *prefix);

int bn_response_msg_append_binding_prefix(bn_response * msg,
                                          const char *prefix);

int bn_response_msg_convert_ipv6_attribs(bn_response *msg, int log_level,
                                         const char *log_prefix);

/**
 * Make a response message look like it goes with the specified
 * request message.  Most useful in proxying situations where the
 * proxy has gotten a response from the real provider, but needs to
 * fix it up before passing it back to the original requestor to make
 * it look like it was a response to the original request, rather than
 * to the proxy's request.  This involves copying the following fields 
 * from the request into the response:
 *   - Transaction ID
 *   - Request ID
 *   - Service ID
 *
 * It does not do anything with node IDs; the caller needs to take care
 * of those manually.
 *
 * \param req The request to which we are formulating a direct response.
 * \param inout_resp A response message which we wish to adapt for use
 * as a response to the specified request.  There must already be a valid
 * response message here; one will not be allocated by this function.
 */
int bn_response_fix_for_request(const bn_request *req,
                                bn_response *inout_resp);

/**
 * Like bn_response_fix_for_request() except takes the individual items
 * that are required, instead of needing the full message.
 */
int bn_response_fix_for_request_by_params(bn_response *inout_resp,
                                          uint32 request_msg_id,
                                          uint32 transaction_id,
                                          bap_service_id service_id);

#ifdef __cplusplus
}
#endif

#endif /* __BNODE_PROTO_H_ */

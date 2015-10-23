/*
 *
 * src/bin/snmp/tms/sn_sets.h
 *
 *
 *
 */

#ifndef __SN_SETS_H_
#define __SN_SETS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "node_enums.h"
#include "bnode_proto.h"
#include "sn_mod_reg.h"
#include "sn_config.h"


int sn_set_init(void);
int sn_set_deinit(void);


/* ------------------------------------------------------------------------- */
int sn_custom_set_response_new(sn_custom_set_response **ret_resp);


/* ------------------------------------------------------------------------- */
int sn_custom_set_response_free(sn_custom_set_response **inout_resp);


/* ------------------------------------------------------------------------- */
void sn_set_params_free(sn_set_params **inout_set_params);


/* ------------------------------------------------------------------------- */
int sn_set_handle_custom_resp(sn_set_request_state *req_state,
                              sn_custom_set_response *sresp,
                              int err_return,
                              tbool async_response);

/* ------------------------------------------------------------------------- */
int sn_set_handler_scalar(int action, u_char * var_val,
                          u_char var_val_type, size_t var_val_len,
                          u_char * statP, oid * name, size_t name_len);

/* ------------------------------------------------------------------------- */
int sn_set_handler_columnar(int action, u_char * var_val,
                            u_char var_val_type, size_t var_val_len,
                            u_char * statP, oid * name, size_t name_len);

/* ------------------------------------------------------------------------- */
tbool sn_set_in_progress(void);


/* ------------------------------------------------------------------------- */
int sn_set_async_cancel(const char *owner, const char *reqid, tbool forget,
                        uint32 *ret_code, tstring **ret_msg);

/* ------------------------------------------------------------------------- */
/* Cancel all outstanding requests initiated by the specified user.
 * This includes async ones (in sn_set_reqs), as well as the active 
 * synchronous one, if any (sn_set_state.sss_curr_req).
 */
int sn_set_cancel_user_requests(const sn_snmp_v3_user *user);


/* ------------------------------------------------------------------------- */
/* Status for a single active SNMP SET request.  One of these is
 * created when a request is received, and kept indefinitely, even
 * after the request has completed, so we can continue to report
 * the status and outcome of the request
 *
 * It is a pointer to this structure which is passed as the opaque
 * 'void *' given to each custom SET handler.
 */
struct sn_set_request_state {

    /* ........................................................................
     * Runtime state: this is the information we need while the request is
     * being processed.
     */

    /*
     * Should we allow this request to be serviced?  This is only set
     * after we have checked the authentication credentials.
     */
    tbool ssrs_permit_set;

    /*
     * Temporary internal state variables, which can be used e.g. to hold
     * parameters for action requests included in the same SET message.
     * This is cleared automatically at the end of any SET request
     * (and also at the beginning, as an extra precaution).
     */
    bn_binding_array *ssrs_vars_temp;

    /*
     * List of names of certain sticky variables which have been set
     * during the processing of this SET request.  We only care about
     * ones which map a single internal variable to multiple SNMP
     * variables (hence the "multivars" name here), so those are the
     * only ones tracked here.
     *
     * Because this doesn't come up very much, it is only lazily
     * allocated if needed.
     */
    tstr_array *ssrs_multivars_sticky_set;

    /*
     * GCL set request to be sent to mgmtd as an outcome of this SNMP SET
     * request.  Note that all variables contained in a single SET request
     * which map to mgmtd sets are handled in a single request.
     *
     * This can coexist with one SNMP variable with a custom SET handler.
     */
    bn_request *ssrs_set_req;

    /*
     * GCL action or event request to be sent as an outcome of this
     * SNMP SET request.  Note that we only allow one such request per
     * SNMP SET.  Unlike multiple GCL node sets, which can be combined
     * into a single request message, it is not possible to combine
     * multiple actions/events into a single message, so invoking more
     * than a single SNMP SET mapping to an action/event request will
     * provoke an error.
     *
     * Note also that this (and ssrs_action_event_req_scalar) is
     * mutually exclusive with SNMP variables with custom SET handlers.
     */
    bn_request *ssrs_action_event_req;

    /*
     * The scalar registration structure which created the action or
     * event request held in sss_action_event_req.  Between these two
     * fields, either both or neither should be set.
     */
    const sn_scalar *ssrs_action_event_req_scalar;

    /*
     * The user account (or community) as which this request is
     * operating.  This has various useful context info, like the GCL
     * session.
     */
    sn_snmp_v3_user *ssrs_user;

    /*
     * If ssrs_status is nssas_running, track whether or not we have
     * an asynchronous message pending.  This starts off false, is set
     * to true when the module calls sn_send_mgmt_msg_async_takeover(),
     * and is set back to false again when the async message completes
     * and their callback is called.  Therefore, we know the SET
     * request is finished iff one of their callbacks (either the
     * original custom SET processing callback, or the one to handle
     * the async reply) returns with this still set to false.
     */
    tbool ssrs_async_pending;

    /*
     * If ssrs_async_pending is true, this holds the message ID of the
     * request, whose response we are awaiting.
     */
    uint32 ssrs_req_msg_id;

    /*
     * If ssrs_async_pending is true, these hold the callback and data
     * provided when the message was sent.
     */
    tstring *ssrs_async_callback_str;
    tstring *ssrs_async_callback_str_old;
    sn_async_msg_callback ssrs_async_callback;
    void *ssrs_async_data;

    /*
     * The former (ssrs_oid) holds the OID of the variable that was
     * requested whenever we call a custom handler.  The sole purpose
     * of this is so that sn_send_mgmt_msg_async_takeover() can copy
     * it onto the latter (ssrs_async_oid).  The latter thus represents
     * the OID of the variable whose handler went async.  This is 
     * served as a monitoring node, as an aid to identifying the various
     * async actions.
     */
    tstring *ssrs_oid;
    tstring *ssrs_async_oid;

    /* ........................................................................
     * Permanent state: this is the information we keep after a request
     * is finished, to show its status and outcome.
     */

    /*
     * Tracking information, either synthesized or copied from
     * temporary variables when the request finishes its first phase,
     * and is considered complete by NET-SNMP.
     *
     * ssrs_dup_id is set if the ssrs_owner and ssrs_reqid are
     * detected to be a duplicate of a pre-existing request.
     * This means we won't save the record when it fails, but just
     * toss it out instead.
     *
     * ssrs_owner_reqid_set just tracks when we have been through
     * the official setting process.  We want to do that exactly
     * once, at least to log something even if they were already set
     * by someone else.
     */
    tstring *ssrs_owner;
    tstring *ssrs_reqid;
    tbool ssrs_dup_id;
    tbool ssrs_owner_reqid_set;

    /*
     * Overall status of the request.  If it is nssas_complete, the
     * contents of this structure are basically static, and are just
     * being kept so we can serve status info.
     */
    nen_snmp_state_request_status ssrs_status;

    /*
     * Progress tracking info.
     */
    uint32 ssrs_progress_pct_done;
    uint32 ssrs_progress_pct_curr_weight;
    tstring *ssrs_progress_oper_id;
    
    /*
     * When the request started and finished processing.
     */
    lt_time_ms ssrs_started;
    lt_time_ms ssrs_finished;

    /*
     * Result information.  This is in basically four groups:
     *
     *   - ssrs_snmp_err: an SNMP error code chosen from the SNMP_ERR_
     *     series in snmp.h.  This reflects what was returned to the
     *     SNMP client who made the request.  If the request was handled
     *     synchronously, this should accurately reflect whether or not
     *     the result was an error (though if it was an error, the details
     *     may be unclear due to SNMP's limited vocabulary).  Note that
     *     if a request goes async, this cannot change as a result of any
     *     processing after the response is sent to the client.
     *
     *   - ssrs_gcl_resp: the full GCL response message which concluded
     *     this request, whether synchronous or asynchronous.  We never
     *     let there be more than one request outstanding at a time, and
     *     we never proceed after an error response (unless a custom
     *     handler chooses to stifle an error it encounters).  So although
     *     multiple GCL messages may get sent throughout a request, there
     *     can only be a maximum of one error.
     *
     *   - ssrs_result_code and ssrs_result_msg: just a code and message,
     *     which is basically a subset of ssrs_gcl_resp.  If ssrs_gcl_resp
     *     is set, these will contain the code and message extracted from
     *     it.  But these may also be set alone by a custom callback's
     *     response, which clears out ssrs_gcl_resp.
     *
     *   - ssrs_cancelled: a flag set if we bail out of one of our
     *     phases with lc_err_cancelled.  Cancellation needs special
     *     treatment: because of how it is treated specially by our bail
     *     macros, code may not notice the cancellation happening.
     *     Anyway, none of our other mechanisms are quite right for
     *     reporting this anyway.  If we see lc_err_cancelled at the end
     *     of an action/phase, we set this flag.  Then, when the request
     *     is being wrapped up, we'll set its status to "cancelled"
     *     instead of "completed".
     */
    int ssrs_snmp_err;
    bn_response *ssrs_gcl_resp;
    uint32 ssrs_result_code;
    tstring *ssrs_result_msg;
    tbool ssrs_cancelled;

};


TYPED_ARRAY_HEADER_NEW_NONE(sn_set_request_state,
                            struct sn_set_request_state *);

extern sn_set_request_state_array *sn_set_reqs;


/* ------------------------------------------------------------------------- */
/* Global status for SNMP SET requests.  A single instance of this
 * structure is always allocated, and is shared across all SNMP SET
 * requests.
 *
 * Note that per-request state is held in a separate sn_set_request_state
 * structure, which is allocated when the request starts and freed when
 * it finishes.
 */
struct sn_set_state {

    /*
     * Currently active SET request, if any.  Note that multiple SET
     * requests may be "outstanding", since they can go async.  But
     * only a single one may be "active", meaning that a response has
     * not yet been sent.
     */
    sn_set_request_state *sss_curr_req;

    /*
     * "Sticky" internal state variables.  Like ssrs_vars_temp, this is
     * used to hold parameters for action or event requests; the
     * difference is that this is global across all requests, and 
     * therefore not cleared automatically at the beginning/end of a
     * request.  It is cleared only on explicit request, or when the
     * snmpd process exits.  Thus it can provide parameters to 
     * action/event requests which come in a separate SET message.
     *
     * Note that whether a value goes into the temporary vs. the sticky
     * variable pool is up to the registration on the variable, NOT the
     * request.
     */
    bn_binding_array *sss_vars_sticky;
    
    /*
     * The last "action" code with which our SET handler was called.
     * Currently, this is only used to help with our logging, and does
     * not affect anything else.
     */
    int sss_last_action;
};


#ifdef __cplusplus
}
#endif

#endif /* __SN_SETS_H_ */

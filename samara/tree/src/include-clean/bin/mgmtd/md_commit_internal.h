/*
 *
 * src/bin/mgmtd/md_commit_internal.h
 *
 *
 *
 */

#ifndef __MD_COMMIT_INTERNAL_H_
#define __MD_COMMIT_INTERNAL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "mdb_db.h"
#include "md_mod_commit.h"
#include "proc_utils.h"
#include "md_message.h"

/*
 * The state needed to track a child process that is completing an
 * action or event for an incoming request. 
 */
typedef struct md_commit_completion_child {
    pid_t mdcc_pid;      /* pid of child process */
    int mdcc_status;     /* wait_status of child process (if done) */
    tbool mdcc_done;     /* 'false' if the child is still running */

    /*
     * What to do with this child if mgmtd is shutting down before the
     * child has exited?  If shutdown type is mccs_signal_sigterm,
     * we send the signal, and mdcc_shutdown_wait tells us what to do
     * next.  If zero, we just proceed immediately; if greater than
     * zero, we wait up to that amount of time for the process to
     * exit.
     */
    md_commit_completion_shutdown_type mdcc_shutdown_type;
    lt_dur_ms mdcc_shutdown_wait;

    /* Description of this process, for logging */
    char *mdcc_proc_descr;

    /* The module's completion func */
    mdm_action_std_async_completion_proc_func mdcc_action_finalize;
    mdm_event_std_async_completion_proc_func mdcc_event_finalize;
    void *mdcc_finalize_arg;
} md_commit_completion_child;

typedef int (*md_commit_completion_callback_func)(md_commit *inout_commit,
                                                  void *arg);

/*
 * Possible ways that a commit can be initiated.  Note that this listing
 * is not comprehensive (for one thing it leaves out GCL Set requests!) --
 * these are only the particular cases that we care about for now.
 */
typedef enum {
    mcct_none = 0,

    /* Initiated by /mgmtd/notify/revert action */
    mcct_revert,

    /* Initiated by /mgmtd/notify/new_in_place action */
    mcct_new_in_place,

    /* Initiated by /mgmtd/notify/import action */
    mcct_import,

} md_commit_context_type;

/* 
 * This struct is given to modules at commit time.  It is a handle to
 * extract information about the commiter, as well as to allow the 
 * module to set return codes.
 */
struct md_commit {
    gcl_handle *mdc_gcl_handle;        /* Should not be NULL */
    int_ptr mdc_gcl_session_id;
    gcl_session *mdc_gcl_session;      /* Can be NULL */
    int64 mdc_request_msg_id;          /* -1 if we don't have one */
    int32 mdc_uid;
    int32 mdc_gid;
    tstring *mdc_username;
    tbool mdc_is_interactive;
    tbool mdc_is_synth_username;
    uint32 mdc_capabilities;
    uint32_array *mdc_acl_auth_groups;
    tstr_array *mdc_acl_roles;

    /*
     * The idea of a local username is not ACL-specific, but it is
     * currently only calculated, and subsequently consulted, under
     * PROD_FEATURE_ACLS.
     *
     * (Note: we now usually also have mss_username_local, and perhaps
     * should be using that instead.  They should generally only be
     * different in the case of uid 0 users other than "admin" himself;
     * and with the way in which this field is currently being used,
     * that distinction does not matter.  Keeping it for now, just to
     * not rock the boat, and for easier handling of edge cases where
     * we don't actually have mss_username_local because we did not get
     * a TRUSTED_AUTH_INFO.)
     */
    tstring *mdc_acl_local_username;

    md_aaa_auth_type mdc_auth_method;

    tbool mdc_mode_one_shot;
    tbool mdc_mode_no_apply;
    tbool mdc_mode_initial;
    tbool mdc_mode_upgrade;
    tbool mdc_doing_apply;
    uint16 mdc_commit_return_code;
    uint16 mdc_apply_return_code;
    tstring *mdc_return_message;
    bn_binding_array *mdc_return_bindings;
    uint32_array *mdc_return_node_ids;
    mdb_db **mdc_main_db;

    gcl_session *mdc_orig_gcl_session;
    int32 mdc_orig_uid;
    int32 mdc_orig_gid;
    tstring *mdc_orig_username;
    tbool mdc_orig_is_interactive;
    tbool mdc_orig_is_synth_username;

    /* 
     * ==================================================
     * Child process async completion
     * ================================================== 
     */
    
    tbool mdc_completion_proc_async_allowed;  /* If we'll let the module
                                               * request async completion.
                                               */
    tbool mdc_completion_proc_async;    /* 
                                         * If handling an incoming request
                                         * will be broken up into start,
                                         * waitpid, finalize .  Currently
                                         * only supported for actions.
                                         */

    /*
     * For actions, the action node that matched the caller's request.
     */
    const mdb_node_reg *mdc_action_called;

    /*
     * Unique Action Audit Number to correlate async audit log messages.
     */
    uint32 mdc_action_audit_number;

    /*
     * Unique AAA accounting transaction ID for action audit accounting.
     * This number is incremented along with mdc_action_audit_number, but
     * is also incremented for change auditing, and is similarly used to
     * provide correlation of async action requests with their completions
     * under the same transaction ID.
     */
    uint64 mdc_action_transaction_id;

    /*
     * Logging level for all audit log messages related to this action.
     */
    int mdc_action_audit_log_level;

    /*
     * When unaudited actions fail, we have no visibility of a failure
     * beyond any commit status messages (which are at LOG_INFO), unless
     * the action itself or the entity that invoked the action logs a
     * message of its own.  When PROD_FEATURE_ACTION_AUDITING is enabled
     * but auditing is suppressed for an action, this flag will cause an
     * "Unaudited action..." message to be logged in the event of action
     * failure.  This is useful wherever auditing of actions is
     * suppressed, e.g. via maaf_suppress_logging or similar flags, and and
     * the calling entity does not take responsibility for logging failures.
     */
    tbool mdc_action_audit_log_unaudited_failures;

    /*
     * Flag for actions that are completed later (asynchronously) after
     * returning a successful commit status.  This comes from the module
     * registration flag maaf_completion_not_audit_logged.  This may be
     * deprecated when a better mechanism comes along (see bug 13426).
     */
    tbool mdc_action_audit_completion_not_logged;

    /*
     * Flag for actions that should or should not have their failure response
     * messages logged since the response may contain sensitive information.
     */
    tbool mdc_action_audit_response_msg_sensitive;

    /*
     * Flag for actions whose overall success or failure should be based only
     * on the commit status code (excluding the apply status code, which is
     * non-fatal in the case of a database commit).
     */
    tbool mdc_action_audit_status_is_from_commit;

    /* State to help finish up an async commit */
    /*
     * XXXX/EMT: should be able to remove this, as long as we'll
     * always have mdc_request_msg_id.
     */
    uint32 mdc_completion_proc_request_msg_id;
    mdb_node_reg *mdc_completion_proc_reg_node;

    /* 
     * The infrastructure's child process completion func.  This is called
     * per child that is reaped, and on the last child, should clean up, and
     * send a response message.
     */
    lc_child_completion_callback_func mdc_completion_proc_core_finalize;
    void *mdc_completion_proc_core_finalize_arg;

    array *mdc_completion_proc_children; /* Of md_commit_completion_child *s */


    /* 
     * ==================================================
     * External messaging based async completion
     * ================================================== 
     */

    tbool mdc_completion_msg_async_allowed;  /* If we'll let anyone request
                                              * async message completion.
                                              */
    tbool mdc_completion_msg_async;    /* 
                                        * If handling an incoming request
                                        * will be broken up into start, wait
                                        * for response, finalize .
                                        * Currently only supported for
                                        * queries and non-barrier actions.
                                        */

    uint32 mdc_completion_msg_request_msg_id;

    array *mdc_completion_msg_request_state; /* md_msg_ext_request_state * */

    /* Only for queries and nonbarrier actions right now */
    md_commit_completion_callback_func mdc_completion_msg_core_finalize;
    void *mdc_completion_msg_core_finalize_arg;

    /* 
     * ==================================================
     * Commit context info: regarding how the commit was initiated
     *
     * Note: these fields will not always be filled out.  mdc_context 
     * will be 0 (mcct_none) if we don't know the context.  The info
     * is only actionable if this is nonzero.
     * ================================================== 
     */
    md_commit_context_type mdc_ctxt_type;
    char *mdc_ctxt_import_id;      /* For mcct_new_in_place and mcct_import  */
    char *mdc_ctxt_db_name;        /* For mcct_import                        */
    char *mdc_ctxt_backup_db_name; /* For mcct_new_in_place                  */
};


int md_commit_commit_state_new(gcl_handle *gclh, gcl_session *sess,
                               int64 request_msg_id,
                               mdb_db **main_db, tbool commit_mode_one_shot,
                               md_commit **ret_commit);

/*
 * Propagate user credential fields from one commit to another.
 * There are two operations we can do:
 *
 *   1. Copy main credential fields to corresponding 'mdc_orig_' fields.
 *      We always do this.
 *
 *   2. Copy main credential fields to corresponding main credential
 *      fields.  We do this only if 'also_replace_main' is set.
 */
int md_commit_commit_state_copy_credentials(const md_commit *src_commit,
                                            md_commit *inout_dest_commit,
                                            tbool also_replace_main);

int md_commit_set_commit_mode(md_commit *commit, tbool one_shot);

int md_commit_set_commit_initial(md_commit *commit, tbool initial);

int md_commit_handle_commit(md_commit *commit, mdb_db **main_db,
                            mdb_db **proposed_db, tbool using_snapshot);

int md_commit_handle_commit_request(md_commit *commit, mdb_db **main_db, 
                                    bn_request *inout_request);
int md_commit_authentication_authorization_map(int32 uid, int32 gid,
                                               const char *username,
                                               /* Mask of mdm_capabilities */
                                               uint32 *ret_capability_mask,
                                               tbool *ret_enforce_cap);

int md_commit_handle_set_session_request(gcl_session *sess, mdb_db **main_db,
                                     bn_request *inout_request);

int md_commit_handle_query_request(md_commit *commit, mdb_db **main_db, 
                                   bn_request *inout_request);
int md_commit_handle_query_session_request(gcl_session *sess, 
                                           mdb_db **main_db, 
                                           bn_request *inout_request);

int md_commit_handle_action(md_commit *commit, mdb_db **main_db, 
                            bn_request *inout_request);
int md_commit_handle_action_completion_cleanup(md_commit **inout_commit,
                                               uint32 request_msg_id,
                                               tbool *did_cleanup);
int md_commit_handle_action_session_request(gcl_session *sess, 
                                            mdb_db **main_db, 
                                            bn_request *inout_request);

/*
 * Handle an event.  The request is non-const and may be modified
 * during this call, but is not freed.  So the caller retains
 * ownership, but cannot rely on the contents of the event on return.
 *
 * (In practice, it is currently thought that the event will be left
 * untouched.  We send it, which sets some message ID fields, but then
 * supposedly restore the previous contents of those fields.)
 */
int md_commit_handle_event(md_commit *commit, const mdb_db *main_db, 
                           bn_request *inout_request);

int md_commit_handle_event_completion_cleanup(md_commit **inout_commit,
                                              uint32 request_msg_id,
                                              tbool *did_cleanup);
int md_commit_handle_event_session_request(gcl_session *sess, 
                                           mdb_db **main_db, 
                                           bn_request *inout_request);


int
md_commit_handle_action_session_response(gcl_session *sess, mdb_db **main_db, 
                                         bn_response *inout_response);

int
md_commit_handle_event_session_response(gcl_session *sess, mdb_db **main_db, 
                                        bn_response *inout_response);

int md_commit_create_initial_db(mdb_db **ret_db, const char *db_name, 
                                mdb_reg_state *reg_state);
int md_commit_add_initial_db_nodes(mdb_db *inout_db, 
                                   mdb_reg_state *reg_state,
                                   void *vmod); /* really an 'md_module *' */

int md_commit_fixup_active_db(tstring **ret_db_name, 
                              tbool db_thought_damamged);
int md_commit_switch_db(mdb_db **inout_main_db, mdb_db **inout_switch_to_db);

#define GT_MOD(msg, node) (GT_((msg), ((md_module *)((node)->mnr_module))->mdm_gettext_domain))

#ifdef __cplusplus
}
#endif

#endif /* __MD_COMMIT_INTERNAL_H_ */

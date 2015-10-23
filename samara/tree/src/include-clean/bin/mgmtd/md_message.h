/*
 *
 * src/bin/mgmtd/md_message.h
 *
 *
 *
 */

#ifndef __MD_MESSAGE_H_
#define __MD_MESSAGE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "gcl.h"
#include "bnode_proto.h"
#include "tree.h"
#include "libevent_wrapper.h"
#include "mdb_db.h"

int md_session_request_cb(gcl_session *sess, 
                          bn_request **inout_request,
                          void *arg);

int md_session_response_cb(gcl_session *sess, 
                           bn_response **inout_response,
                           void *arg);


extern tbool md_waitfor_response_active;
extern tbool md_waitfor_response_done;
extern gcl_session *md_waitfor_response_session;
extern uint32 md_waitfor_response_request_msgid;
extern bn_response **md_waitfor_response_msg;
extern array *md_backlog_message_queue;
extern int md_backlog_message_queue_pipe[2];
int md_waitfor_timedout(int fd, short ev_type, void *data,
                        lew_context *ctxt);

extern lew_event *md_blocklog_message_queue_event;

void md_backlog_message_free_for_array(void *elem);
int md_handle_backlog_message_queue(int fd, short ev_type, void *data,
                                    lew_context *ctxt);


typedef struct md_ep_request_state {
    char *mers_provider_name;
    tree *mers_remote_reg_tree;
    bn_request *mers_request;
} md_ep_request_state;

typedef struct md_ext_query_finalize {
    uint32_array *meqf_orig_node_ids;
    uint32_array *meqf_new_node_ids;
} md_ext_query_finalize;

TYPED_ARRAY_HEADER_NEW_NONE(md_ep_request_state, md_ep_request_state *);

int md_ep_request_state_array_new(md_ep_request_state_array **ret_arr);

int md_ep_request_state_lookup_by_name(md_ep_request_state_array *ep_request_array,
                                       const char *provider_name,
                                       md_ep_request_state **ret_mrs);

int md_ep_request_state_new_elem(md_ep_request_state **ret_mrs,
                                 const char *provider_name);

typedef struct md_msg_ext_request_state md_msg_ext_request_state;

typedef int (*md_msg_ext_request_finalize_func)(md_commit *commit,
                                 md_msg_ext_request_state *req_state,
                                 void *finalize_arg);


int md_message_ext_commit_lookup_message(int_ptr ext_session_id, 
                                         int32 ext_req_msgid, 
                                         md_msg_ext_request_state **
                                         ret_req_state);

int md_message_ext_commit_check_num_response_left(md_commit *commit,
                                                  int32 *ret_num_resp_left);

int md_message_ext_response_session_close(int_ptr ext_session_id);

int md_message_ext_commit_counts(uint32 *ret_total_commits,

                                 /*
                                  *  Normal queries, or a mix of normal
                                  *  and non-barrier queries
                                  */
                                 uint32 *ret_normal_commits,       

                                 /*
                                  * Fully async, non-barrier queries or actions.
                                  */
                                 uint32 *ret_nonbarrier_commits);

int md_request_interact_external(md_commit *commit,
                                 gcl_session *session,
                                 bn_request *request,

                                 /* 
                                  * If we'll wait to get a response or
                                  * timeout the request before
                                  * returning, or go async.
                                  */
                                 tbool wait_for_response,

                                 /*
                                  * If wait_for_response is false, then
                                  * if this parameter is true, the
                                  * message we send should (if allowed
                                  * by registration state) not be a
                                  * barrier to other requests.
                                  */
                                 tbool non_barrier,

                                 /*
                                  * Timeout for this interaction.
                                  * Pass 0 to use default (from md_client.h)
                                  */
                                 lt_dur_sec timeout,

                                 void *non_wait_arg,

                                 bn_response **ret_response,

                                 /*
                                  * If !wait_for_response, and we don't 
                                  * have a response yet, and the request
                                  * is still running.
                                  */
                                 tbool *ret_async_msg_active);


extern array *md_message_ext_commits_in_progress; /* of const md_commit *'s */

void md_msg_ext_request_state_free_for_array(void *elem);

#ifdef __cplusplus
}
#endif

#endif /* __MD_MESSAGE_H_ */
